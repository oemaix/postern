/*
 * Copyright 2026 oemaix
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "postern.h"

#include "postern_encode.h"
#include "postern_file.h"

#include <string.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/i2c_slave.h"
#include "driver/i2s_std.h"
#include "esp_a2dp_api.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "postern";

static const char *POSTERN_DEVICE_NAME = "Postern";

enum {
    POSTERN_EVT_START = 1,
    POSTERN_EVT_STOP,
    POSTERN_EVT_A2D,
    POSTERN_EVT_FOUND,
    POSTERN_EVT_DISCOVERY_STOPPED,
    POSTERN_EVT_PROFILE,
    POSTERN_EVT_CAPS,
    POSTERN_EVT_ENCODE,
};

enum {
    POSTERN_PREF_AAC = 1,
    POSTERN_PREF_SBC,
};

enum {
    POSTERN_MEDIA_IDLE = 0,
    POSTERN_MEDIA_PREF,
    POSTERN_MEDIA_CHECK,
    POSTERN_MEDIA_STARTING,
    POSTERN_MEDIA_STARTED,
    POSTERN_MEDIA_STOPPING,
};

typedef struct {
    uint8_t kind;
    esp_a2d_cb_event_t a2d_event;
    esp_a2d_cb_param_t a2d;
    esp_bd_addr_t bda;
    esp_a2d_init_state_t profile;
    esp_err_t encode_err;
} postern_evt_t;

static postern_config_t s_cfg;
static postern_file_t s_file;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static QueueHandle_t s_queue;
static QueueHandle_t s_cmd;
static TaskHandle_t s_task;
static i2c_slave_dev_handle_t s_i2c;
static i2s_chan_handle_t s_rx;
static bool s_i2s_ready;
static bool s_i2s_on;
static volatile bool s_pcm_open;
static uint32_t s_i2s_hz;
static bool s_inited;

static bool s_connected;
static bool s_connecting;
static bool s_discovering;
static bool s_chosen;
static bool s_sbc_valid;
static bool s_aac_valid;
static bool s_aac_sep_ok;
static bool s_sbc_sep_ok;
static bool s_aac_give_up;
static bool s_start_pending;
static bool s_start_acked;
static bool s_mcc_valid;
static esp_bd_addr_t s_peer;
static esp_a2d_conn_hdl_t s_conn;
static uint16_t s_audio_mtu;
static esp_a2d_cie_sbc_t s_sbc_caps;
static esp_a2d_cie_m24_t s_aac_caps;
static esp_a2d_mcc_t s_mcc;
static uint32_t s_stream_hz;
static int s_media;
static int s_pref_kind;
static int s_profile_tries;
static int s_sep_wait;
static bool s_sep_gate;

bool postern_load_config(postern_config_t *config)
{
    if (config == NULL) {
        return false;
    }

    config->bclk_gpio = CONFIG_POSTERN_I2S_BCLK_GPIO;
    config->ws_gpio = CONFIG_POSTERN_I2S_WS_GPIO;
    config->din_gpio = CONFIG_POSTERN_I2S_DIN_GPIO;
    config->sda_gpio = CONFIG_POSTERN_I2C_SDA_GPIO;
    config->scl_gpio = CONFIG_POSTERN_I2C_SCL_GPIO;
    config->i2c_addr = CONFIG_POSTERN_I2C_ADDRESS;

    return config->bclk_gpio >= 0 && config->ws_gpio >= 0 && config->din_gpio >= 0
        && config->sda_gpio >= 0 && config->scl_gpio >= 0 && config->i2c_addr != 0;
}

static void postern_wake(void)
{
    if (s_task != NULL) {
        xTaskNotify(s_task, 1, eSetBits);
    }
}

static void postern_wake_from_isr(BaseType_t *wake)
{
    if (s_task != NULL) {
        xTaskNotifyFromISR(s_task, 1, eSetBits, wake);
    }
}

static void postern_send(const postern_evt_t *evt)
{
    if (s_queue == NULL || evt == NULL) {
        return;
    }
    if (xQueueSend(s_queue, evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full");
        return;
    }
    postern_wake();
}

static void postern_set_link(bool up)
{
    portENTER_CRITICAL(&s_mux);
    postern_file_set_link(&s_file, up);
    portEXIT_CRITICAL(&s_mux);
}

static void postern_set_running(bool running)
{
    portENTER_CRITICAL(&s_mux);
    postern_file_set_running(&s_file, running);
    portEXIT_CRITICAL(&s_mux);
}

static bool postern_take_rate(uint32_t *hz)
{
    bool ok;
    portENTER_CRITICAL(&s_mux);
    ok = s_file.rate_hz != 0 && !s_file.rate_rejected && !s_file.running;
    *hz = s_file.rate_hz;
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

static void postern_pcm_close(void)
{
    if (!postern_encode_stop()) {
        postern_set_running(false);
        return;
    }
    s_pcm_open = false;
    if (s_i2s_on && s_rx != NULL) {
        i2s_channel_disable(s_rx);
        s_i2s_on = false;
    }
    postern_set_running(false);
}

static esp_err_t postern_pcm_open(uint32_t hz)
{
    i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(hz);
    esp_err_t err;

    if (!s_i2s_ready) {
        i2s_std_config_t std = {
            .clk_cfg = clk,
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = (gpio_num_t)s_cfg.bclk_gpio,
                .ws = (gpio_num_t)s_cfg.ws_gpio,
                .dout = I2S_GPIO_UNUSED,
                .din = (gpio_num_t)s_cfg.din_gpio,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false,
                },
            },
        };
        err = i2s_channel_init_std_mode(s_rx, &std);
        if (err != ESP_OK) {
            return err;
        }
        s_i2s_ready = true;
        s_i2s_hz = hz;
    } else if (s_i2s_hz != hz) {
        if (s_i2s_on) {
            s_pcm_open = false;
            i2s_channel_disable(s_rx);
            s_i2s_on = false;
        }
        err = i2s_channel_reconfig_std_clock(s_rx, &clk);
        if (err != ESP_OK) {
            return err;
        }
        s_i2s_hz = hz;
    }

    if (!s_i2s_on) {
        err = i2s_channel_enable(s_rx);
        if (err != ESP_OK) {
            return err;
        }
        s_i2s_on = true;
    }
    s_pcm_open = true;
    return ESP_OK;
}

static bool postern_on_receive(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_rx_done_event_data_t *edata, void *arg)
{
    postern_file_action_t action;
    uint8_t cmd;
    BaseType_t wake = pdFALSE;

    (void)i2c_slave;
    (void)arg;
    if (edata == NULL || edata->buffer == NULL || edata->length == 0) {
        return false;
    }

    portENTER_CRITICAL_ISR(&s_mux);
    action = postern_file_write(&s_file, edata->buffer, edata->length);
    portEXIT_CRITICAL_ISR(&s_mux);

    if (action == POSTERN_FILE_NONE || s_cmd == NULL) {
        return false;
    }
    cmd = action == POSTERN_FILE_START ? POSTERN_EVT_START : POSTERN_EVT_STOP;
    if (xQueueSendFromISR(s_cmd, &cmd, &wake) != pdTRUE) {
        return wake == pdTRUE;
    }
    postern_wake_from_isr(&wake);
    return wake == pdTRUE;
}

static bool postern_on_request(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_request_event_data_t *edata, void *arg)
{
    uint8_t buf[POSTERN_REG_FILE_LEN];
    size_t have;
    uint32_t wrote = 0;

    (void)edata;
    (void)arg;
    /* ESP32 calls this once per read. The whole remainder has to be queued here.
       Eight bytes fit in the hardware FIFO, so nothing is left in the ring buffer. */
    portENTER_CRITICAL_ISR(&s_mux);
    have = postern_file_copy(&s_file, buf, sizeof(buf));
    portEXIT_CRITICAL_ISR(&s_mux);
    if (have == 0 || i2c_slave_write(i2c_slave, buf, have, &wrote, 0) != ESP_OK || wrote == 0) {
        return false;
    }
    portENTER_CRITICAL_ISR(&s_mux);
    postern_file_commit(&s_file, wrote);
    portEXIT_CRITICAL_ISR(&s_mux);
    return false;
}

static bool postern_is_renderer(esp_bt_gap_cb_param_t *param)
{
    uint32_t cod = 0;
    bool have_cod = false;

    for (int i = 0; i < param->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *prop = param->disc_res.prop + i;
        if (prop->type == ESP_BT_GAP_DEV_PROP_COD && prop->val != NULL) {
            memcpy(&cod, prop->val, sizeof(cod));
            have_cod = true;
        }
    }
    if (!have_cod || !esp_bt_gap_is_valid_cod(cod)) {
        return false;
    }
    return (esp_bt_gap_get_cod_srvc(cod) & ESP_BT_COD_SRVC_RENDERING) != 0;
}

static void postern_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    postern_evt_t evt;

    memset(&evt, 0, sizeof(evt));
    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT:
        if (param != NULL && postern_is_renderer(param)) {
            evt.kind = POSTERN_EVT_FOUND;
            memcpy(evt.bda, param->disc_res.bda, sizeof(evt.bda));
            postern_send(&evt);
        }
        break;
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        if (param != NULL && param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            evt.kind = POSTERN_EVT_DISCOVERY_STOPPED;
            postern_send(&evt);
        }
        break;
    case ESP_BT_GAP_PIN_REQ_EVT: {
        esp_bt_pin_code_t pin = {'1', '2', '3', '4', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'};
        if (param == NULL) {
            break;
        }
        if (param->pin_req.min_16_digit) {
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin);
        } else {
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin);
        }
        break;
    }
    case ESP_BT_GAP_CFM_REQ_EVT:
        if (param != NULL) {
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        }
        break;
    default:
        break;
    }
}

static void postern_store_mcc(const esp_a2d_mcc_t *mcc);

static void postern_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    postern_evt_t evt;

    memset(&evt, 0, sizeof(evt));
    evt.kind = POSTERN_EVT_A2D;
    evt.a2d_event = event;
    if (event == ESP_A2D_REPORT_SNK_ALL_CODEC_CAPS_EVT && param != NULL
        && param->a2d_report_snk_all_codec_caps_stat.sep_mcc != NULL) {
        uint8_t n = param->a2d_report_snk_all_codec_caps_stat.sep_num;
        esp_a2d_sep_mcc_t *seps = param->a2d_report_snk_all_codec_caps_stat.sep_mcc;
        for (uint8_t i = 0; i < n; i++) {
            postern_store_mcc(&seps[i].mcc);
        }
        evt.kind = POSTERN_EVT_CAPS;
        postern_send(&evt);
        return;
    }
    if (param != NULL) {
        evt.a2d = *param;
    }
    if (event == ESP_A2D_PROF_STATE_EVT && param != NULL) {
        evt.kind = POSTERN_EVT_PROFILE;
        evt.profile = param->a2d_prof_stat.init_state;
    }
    postern_send(&evt);
}

static void postern_begin_discovery(void)
{
    esp_err_t err;

    if (s_connected || s_connecting || s_discovering) {
        return;
    }
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    s_discovering = true;
    err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
    if (err != ESP_OK) {
        s_discovering = false;
        ESP_LOGW(TAG, "discovery did not start: %s", esp_err_to_name(err));
    }
}

static void postern_cancel_discovery(void)
{
    if (!s_discovering) {
        return;
    }
    esp_bt_gap_cancel_discovery();
}

static bool postern_load_peer(esp_bd_addr_t bda)
{
    nvs_handle_t handle;
    size_t len = ESP_BD_ADDR_LEN;
    esp_err_t err;

    if (nvs_open("postern", NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    err = nvs_get_blob(handle, "bda", bda, &len);
    nvs_close(handle);
    return err == ESP_OK && len == ESP_BD_ADDR_LEN;
}

static void postern_save_peer(const esp_bd_addr_t bda)
{
    nvs_handle_t handle;

    if (nvs_open("postern", NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    if (nvs_set_blob(handle, "bda", bda, ESP_BD_ADDR_LEN) == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);
}

static void postern_page_or_discover(void)
{
    esp_bd_addr_t saved;

    if (postern_load_peer(saved)) {
        memcpy(s_peer, saved, sizeof(s_peer));
        s_chosen = true;
        s_connecting = true;
        if (esp_a2d_source_connect(s_peer) == ESP_OK) {
            return;
        }
        s_chosen = false;
        s_connecting = false;
    }
    postern_begin_discovery();
}

static uint8_t postern_sbc_freq(uint32_t hz)
{
    if (hz == POSTERN_RATE_HZ_44K) {
        return ESP_A2D_SBC_CIE_SF_44K;
    }
    if (hz == POSTERN_RATE_HZ_48K) {
        return ESP_A2D_SBC_CIE_SF_48K;
    }
    return 0;
}

static uint8_t postern_sbc_channel(const esp_a2d_cie_sbc_t *caps)
{
    if ((caps->ch_mode & ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO) != 0) {
        return ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO;
    }
    if ((caps->ch_mode & ESP_A2D_SBC_CIE_CH_MODE_STEREO) != 0) {
        return ESP_A2D_SBC_CIE_CH_MODE_STEREO;
    }
    if ((caps->ch_mode & ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL) != 0) {
        return ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL;
    }
    return 0;
}

static uint8_t postern_sbc_pick(uint8_t caps, uint8_t first, uint8_t second, uint8_t third, uint8_t fourth)
{
    if ((caps & first) != 0) {
        return first;
    }
    if ((caps & second) != 0) {
        return second;
    }
    if ((caps & third) != 0) {
        return third;
    }
    if ((caps & fourth) != 0) {
        return fourth;
    }
    return 0;
}

static void postern_store_mcc(const esp_a2d_mcc_t *mcc)
{
    portENTER_CRITICAL(&s_mux);
    if (mcc->type == ESP_A2D_MCT_SBC) {
        s_sbc_caps = mcc->cie.sbc_info;
        s_sbc_valid = true;
    } else if (mcc->type == ESP_A2D_MCT_M24) {
        s_aac_caps = mcc->cie.m24_info;
        s_aac_valid = true;
    }
    portEXIT_CRITICAL(&s_mux);
}

static bool postern_load_aac(esp_a2d_cie_m24_t *out)
{
    bool ok;
    portENTER_CRITICAL(&s_mux);
    ok = s_aac_valid;
    if (ok) {
        *out = s_aac_caps;
    }
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

static bool postern_load_sbc(esp_a2d_cie_sbc_t *out)
{
    bool ok;
    portENTER_CRITICAL(&s_mux);
    ok = s_sbc_valid;
    if (ok) {
        *out = s_sbc_caps;
    }
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

static void postern_put_aac_bitrate(esp_a2d_cie_m24_t *m24)
{
    m24->br1 = (POSTERN_AAC_BITRATE_BPS >> 16) & ESP_A2D_M24_CIE_BR1_MSK;
    m24->br2 = (POSTERN_AAC_BITRATE_BPS >> 8) & ESP_A2D_M24_CIE_BR2_MSK;
    m24->br3 = POSTERN_AAC_BITRATE_BPS & ESP_A2D_M24_CIE_BR3_MSK;
}

static void postern_register_seps(void)
{
    esp_a2d_mcc_t aac;
    esp_a2d_mcc_t sbc;

    memset(&aac, 0, sizeof(aac));
    aac.type = ESP_A2D_MCT_M24;
    aac.cie.m24_info.drc = ESP_A2D_M24_CIE_DRC_NS;
    aac.cie.m24_info.obj_type = ESP_A2D_M24_CIE_OBJ_TYPE_2_AAC_LC | ESP_A2D_M24_CIE_OBJ_TYPE_4_AAC_LC;
    aac.cie.m24_info.samp_freq1 = ESP_A2D_M24_CIE_SF1_44K;
    aac.cie.m24_info.samp_freq2 = ESP_A2D_M24_CIE_SF2_48K;
    aac.cie.m24_info.ch = ESP_A2D_M24_CIE_CH_2;
    aac.cie.m24_info.vbr = ESP_A2D_M24_CIE_VBR_NS;
    postern_put_aac_bitrate(&aac.cie.m24_info);
    s_sep_gate = true;
    s_sep_wait = 0;
    s_aac_sep_ok = true;
    s_sbc_sep_ok = true;
    if (esp_a2d_source_register_stream_endpoint(0, &aac) != ESP_OK) {
        s_aac_sep_ok = false;
        ESP_LOGW(TAG, "AAC endpoint was not queued");
    } else {
        s_sep_wait++;
    }

    memset(&sbc, 0, sizeof(sbc));
    sbc.type = ESP_A2D_MCT_SBC;
    sbc.cie.sbc_info.samp_freq = ESP_A2D_SBC_CIE_SF_44K | ESP_A2D_SBC_CIE_SF_48K;
    sbc.cie.sbc_info.ch_mode = ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO | ESP_A2D_SBC_CIE_CH_MODE_STEREO
        | ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL;
    sbc.cie.sbc_info.block_len = ESP_A2D_SBC_CIE_BLOCK_LEN_16 | ESP_A2D_SBC_CIE_BLOCK_LEN_12
        | ESP_A2D_SBC_CIE_BLOCK_LEN_8 | ESP_A2D_SBC_CIE_BLOCK_LEN_4;
    sbc.cie.sbc_info.num_subbands = ESP_A2D_SBC_CIE_NUM_SUBBANDS_8 | ESP_A2D_SBC_CIE_NUM_SUBBANDS_4;
    sbc.cie.sbc_info.alloc_mthd = ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS | ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR;
    sbc.cie.sbc_info.min_bitpool = 2;
    sbc.cie.sbc_info.max_bitpool = 53;
    if (esp_a2d_source_register_stream_endpoint(1, &sbc) != ESP_OK) {
        s_sbc_sep_ok = false;
        ESP_LOGW(TAG, "SBC endpoint was not queued");
    } else {
        s_sep_wait++;
    }
    if (s_sep_wait == 0) {
        s_sep_gate = false;
        postern_page_or_discover();
    }
}

static void postern_sep_arrived(void)
{
    if (!s_sep_gate) {
        return;
    }
    if (s_sep_wait > 0) {
        s_sep_wait--;
    }
    if (s_sep_wait == 0) {
        s_sep_gate = false;
        postern_page_or_discover();
    }
}

static void postern_try_stream(void);

static void postern_pref_refused(void)
{
    s_media = POSTERN_MEDIA_IDLE;
    if (s_pref_kind == POSTERN_PREF_AAC) {
        s_aac_give_up = true;
        postern_try_stream();
        return;
    }
    s_start_pending = false;
}

static bool postern_fill_aac(esp_a2d_mcc_t *pref, uint32_t hz)
{
    esp_a2d_cie_m24_t caps;
    uint8_t obj;

    if (!s_aac_sep_ok || s_aac_give_up || !postern_load_aac(&caps)) {
        return false;
    }
    if ((caps.ch & ESP_A2D_M24_CIE_CH_2) == 0) {
        return false;
    }
    if ((caps.obj_type & ESP_A2D_M24_CIE_OBJ_TYPE_2_AAC_LC) != 0) {
        obj = ESP_A2D_M24_CIE_OBJ_TYPE_2_AAC_LC;
    } else if ((caps.obj_type & ESP_A2D_M24_CIE_OBJ_TYPE_4_AAC_LC) != 0) {
        obj = ESP_A2D_M24_CIE_OBJ_TYPE_4_AAC_LC;
    } else {
        return false;
    }

    memset(pref, 0, sizeof(*pref));
    pref->type = ESP_A2D_MCT_M24;
    pref->cie.m24_info.drc = ESP_A2D_M24_CIE_DRC_NS;
    pref->cie.m24_info.obj_type = obj;
    pref->cie.m24_info.ch = ESP_A2D_M24_CIE_CH_2;
    pref->cie.m24_info.vbr = ESP_A2D_M24_CIE_VBR_NS;
    postern_put_aac_bitrate(&pref->cie.m24_info);
    if (hz == POSTERN_RATE_HZ_44K) {
        if ((caps.samp_freq1 & ESP_A2D_M24_CIE_SF1_44K) == 0) {
            return false;
        }
        pref->cie.m24_info.samp_freq1 = ESP_A2D_M24_CIE_SF1_44K;
        return true;
    }
    if (hz == POSTERN_RATE_HZ_48K && (caps.samp_freq2 & ESP_A2D_M24_CIE_SF2_48K) != 0) {
        pref->cie.m24_info.samp_freq2 = ESP_A2D_M24_CIE_SF2_48K;
        return true;
    }
    return false;
}

static bool postern_fill_sbc(esp_a2d_mcc_t *pref, uint32_t hz)
{
    esp_a2d_cie_sbc_t caps;
    uint8_t freq;
    uint8_t channel;
    uint8_t blocks;
    uint8_t subbands;
    uint8_t alloc;
    uint8_t min_pool;
    uint8_t max_pool;

    if (!s_sbc_sep_ok || !postern_load_sbc(&caps)) {
        return false;
    }
    freq = postern_sbc_freq(hz);
    channel = postern_sbc_channel(&caps);
    blocks = postern_sbc_pick(caps.block_len, ESP_A2D_SBC_CIE_BLOCK_LEN_16, ESP_A2D_SBC_CIE_BLOCK_LEN_12,
        ESP_A2D_SBC_CIE_BLOCK_LEN_8, ESP_A2D_SBC_CIE_BLOCK_LEN_4);
    subbands = postern_sbc_pick(caps.num_subbands, ESP_A2D_SBC_CIE_NUM_SUBBANDS_8, ESP_A2D_SBC_CIE_NUM_SUBBANDS_4, 0, 0);
    alloc = postern_sbc_pick(caps.alloc_mthd, ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS, ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR, 0, 0);
    if (freq == 0 || (caps.samp_freq & freq) == 0 || channel == 0 || blocks == 0 || subbands == 0 || alloc == 0) {
        return false;
    }
    min_pool = caps.min_bitpool < 2 ? 2 : caps.min_bitpool;
    max_pool = caps.max_bitpool > 53 ? 53 : caps.max_bitpool;
    if (min_pool > max_pool) {
        return false;
    }

    memset(pref, 0, sizeof(*pref));
    pref->type = ESP_A2D_MCT_SBC;
    pref->cie.sbc_info.samp_freq = freq;
    pref->cie.sbc_info.ch_mode = channel;
    pref->cie.sbc_info.block_len = blocks;
    pref->cie.sbc_info.num_subbands = subbands;
    pref->cie.sbc_info.alloc_mthd = alloc;
    pref->cie.sbc_info.min_bitpool = min_pool;
    pref->cie.sbc_info.max_bitpool = max_pool;
    return true;
}

static void postern_try_stream(void)
{
    esp_a2d_mcc_t pref;
    uint32_t hz = 0;
    int kind = 0;

    if (!s_start_pending || !s_connected || s_media != POSTERN_MEDIA_IDLE) {
        return;
    }
    if (!s_sbc_valid && !s_aac_valid) {
        return;
    }
    if (!postern_take_rate(&hz)) {
        return;
    }
    if (postern_fill_aac(&pref, hz)) {
        kind = POSTERN_PREF_AAC;
    } else if (postern_fill_sbc(&pref, hz)) {
        kind = POSTERN_PREF_SBC;
    } else if (!s_sbc_valid && s_sbc_sep_ok) {
        return;
    } else {
        ESP_LOGW(TAG, "sink cannot take %u Hz stereo", (unsigned)hz);
        s_start_pending = false;
        return;
    }

    s_stream_hz = hz;
    s_mcc_valid = false;
    s_start_acked = false;
    s_pref_kind = kind;
    s_media = POSTERN_MEDIA_PREF;
    if (esp_a2d_source_set_pref_mcc(s_conn, &pref) != ESP_OK) {
        ESP_LOGW(TAG, "set preferred codec failed");
        postern_pref_refused();
    }
}

static void postern_become_idle(void)
{
    s_media = POSTERN_MEDIA_IDLE;
    postern_try_stream();
}

static void postern_stop_stream(void)
{
    s_start_pending = false;
    s_start_acked = false;
    postern_pcm_close();
    if (s_media == POSTERN_MEDIA_STARTED || s_media == POSTERN_MEDIA_STARTING) {
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
        s_media = POSTERN_MEDIA_STOPPING;
        return;
    }
    s_media = POSTERN_MEDIA_IDLE;
}

static bool postern_mcc_matches(uint32_t hz)
{
    if (s_mcc.type == ESP_A2D_MCT_SBC) {
        uint8_t freq = postern_sbc_freq(hz);
        return freq != 0 && (s_mcc.cie.sbc_info.samp_freq & freq) != 0 && postern_sbc_channel(&s_mcc.cie.sbc_info) != 0;
    }
    if (s_mcc.type == ESP_A2D_MCT_M24 && (s_mcc.cie.m24_info.ch & ESP_A2D_M24_CIE_CH_2) != 0) {
        if (hz == POSTERN_RATE_HZ_44K) {
            return (s_mcc.cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_44K) != 0;
        }
        if (hz == POSTERN_RATE_HZ_48K) {
            return (s_mcc.cie.m24_info.samp_freq2 & ESP_A2D_M24_CIE_SF2_48K) != 0;
        }
    }
    return false;
}

static void postern_codec_failed(void)
{
    bool aac = s_pref_kind == POSTERN_PREF_AAC || (s_mcc_valid && s_mcc.type == ESP_A2D_MCT_M24);

    s_mcc_valid = false;
    s_start_acked = false;
    if (aac) {
        s_aac_give_up = true;
        s_start_pending = true;
    } else {
        s_start_pending = false;
    }
    esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
    s_media = POSTERN_MEDIA_STOPPING;
}

static void postern_encode_note(esp_err_t err)
{
    postern_evt_t evt;

    memset(&evt, 0, sizeof(evt));
    evt.kind = POSTERN_EVT_ENCODE;
    evt.encode_err = err;
    postern_send(&evt);
}

static void postern_on_encode(esp_err_t err)
{
    if (s_media != POSTERN_MEDIA_STARTING && s_media != POSTERN_MEDIA_STARTED) {
        return;
    }
    if (err != ESP_OK) {
        if (s_media == POSTERN_MEDIA_STARTED || s_pcm_open) {
            postern_pcm_close();
        }
        postern_codec_failed();
        return;
    }
    if (s_media != POSTERN_MEDIA_STARTING) {
        return;
    }
    postern_set_running(true);
    s_media = POSTERN_MEDIA_STARTED;
    s_start_pending = false;
    s_start_acked = false;
}

static void postern_launch_media(void)
{
    if (s_media != POSTERN_MEDIA_STARTING || !s_start_acked || !s_mcc_valid) {
        return;
    }
    if (!postern_mcc_matches(s_stream_hz)) {
        ESP_LOGW(TAG, "negotiated codec is not %u Hz stereo", (unsigned)s_stream_hz);
        postern_codec_failed();
        return;
    }
    if (postern_pcm_open(s_stream_hz) != ESP_OK) {
        ESP_LOGE(TAG, "I2S slave did not start");
        postern_codec_failed();
        return;
    }
    if (postern_encode_start(s_rx, s_conn, s_audio_mtu, &s_mcc, s_stream_hz, postern_encode_note) != ESP_OK) {
        ESP_LOGE(TAG, "encoder did not start");
        postern_pcm_close();
        postern_codec_failed();
        return;
    }
}

static void postern_on_media_ack(const esp_a2d_cb_param_t *a2d)
{
    bool ok = a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS;

    if (!s_start_pending && a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START && ok) {
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
        s_media = POSTERN_MEDIA_STOPPING;
        return;
    }

    switch (s_media) {
    case POSTERN_MEDIA_CHECK:
        if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY && ok) {
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
            s_media = POSTERN_MEDIA_STARTING;
        } else if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) {
            ESP_LOGW(TAG, "source ready check failed");
            postern_pref_refused();
        }
        break;
    case POSTERN_MEDIA_STARTING:
        if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START && ok) {
            s_start_acked = true;
            postern_launch_media();
        } else if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START) {
            s_media = POSTERN_MEDIA_IDLE;
            s_start_pending = false;
        }
        break;
    case POSTERN_MEDIA_STOPPING:
        if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_SUSPEND) {
            postern_become_idle();
        }
        break;
    default:
        break;
    }
}

static void postern_drop_link(void)
{
    bool was_discovering = s_discovering;

    s_connected = false;
    s_connecting = false;
    s_chosen = false;
    portENTER_CRITICAL(&s_mux);
    s_sbc_valid = false;
    s_aac_valid = false;
    portEXIT_CRITICAL(&s_mux);
    s_aac_give_up = false;
    s_start_pending = false;
    s_start_acked = false;
    s_mcc_valid = false;
    s_audio_mtu = 0;
    s_media = POSTERN_MEDIA_IDLE;
    s_pref_kind = 0;
    postern_pcm_close();
    postern_set_link(false);
    if (was_discovering) {
        postern_cancel_discovery();
        return;
    }
    postern_begin_discovery();
}

static void postern_on_a2d(const postern_evt_t *evt)
{
    const esp_a2d_cb_param_t *a2d = &evt->a2d;

    switch (evt->a2d_event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            s_connected = true;
            s_connecting = false;
            s_conn = a2d->conn_stat.conn_hdl;
            s_audio_mtu = a2d->conn_stat.audio_mtu;
            memcpy(s_peer, a2d->conn_stat.remote_bda, sizeof(s_peer));
            postern_set_link(true);
            postern_save_peer(s_peer);
            postern_cancel_discovery();
            esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
            ESP_LOGI(TAG, "A2DP connected");
            postern_try_stream();
        } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            ESP_LOGI(TAG, "A2DP disconnected");
            postern_drop_link();
        }
        break;
    case ESP_A2D_REPORT_SNK_CODEC_CAPS_EVT:
        postern_store_mcc(&a2d->a2d_report_snk_codec_caps_stat.mcc);
        s_conn = a2d->a2d_report_snk_codec_caps_stat.conn_hdl;
        postern_try_stream();
        break;
    case ESP_A2D_AUDIO_CFG_EVT:
        s_mcc = a2d->audio_cfg.mcc;
        s_mcc_valid = true;
        ESP_LOGI(TAG, "negotiated codec %u", (unsigned)s_mcc.type);
        postern_launch_media();
        break;
    case ESP_A2D_SEP_REG_STATE_EVT:
        if (a2d->a2d_sep_reg_stat.reg_state != ESP_A2D_SEP_REG_SUCCESS) {
            ESP_LOGW(TAG, "SEP %u failed", (unsigned)a2d->a2d_sep_reg_stat.seid);
            if (a2d->a2d_sep_reg_stat.seid == 0) {
                s_aac_sep_ok = false;
            } else if (a2d->a2d_sep_reg_stat.seid == 1) {
                s_sbc_sep_ok = false;
            }
        }
        postern_sep_arrived();
        break;
    case ESP_A2D_SRC_SET_PREF_MCC_EVT:
        if (s_media != POSTERN_MEDIA_PREF) {
            break;
        }
        if (a2d->a2d_set_pref_mcc_stat.set_status == ESP_BT_STATUS_SUCCESS) {
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
            s_media = POSTERN_MEDIA_CHECK;
        } else {
            ESP_LOGW(TAG, "preferred codec was refused");
            postern_pref_refused();
        }
        break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        postern_on_media_ack(a2d);
        break;
    case ESP_A2D_AUDIO_STATE_EVT:
        if (a2d->audio_stat.state != ESP_A2D_AUDIO_STATE_STARTED && s_media == POSTERN_MEDIA_STARTED) {
            postern_pcm_close();
            postern_become_idle();
        }
        break;
    default:
        break;
    }
}

static void postern_on_evt(const postern_evt_t *evt)
{
    switch (evt->kind) {
    case POSTERN_EVT_PROFILE:
        if (evt->profile == ESP_A2D_INIT_SUCCESS) {
            s_profile_tries = 0;
            s_aac_sep_ok = true;
            s_sbc_sep_ok = true;
            ESP_LOGI(TAG, "A2DP source ready");
            postern_register_seps();
        } else if (s_profile_tries < 2) {
            s_profile_tries++;
            ESP_LOGW(TAG, "A2DP source init failed, retry %d", s_profile_tries);
            esp_a2d_source_init();
        } else {
            ESP_LOGE(TAG, "A2DP source init failed");
        }
        break;
    case POSTERN_EVT_FOUND:
        if (!s_connected && !s_connecting && !s_chosen) {
            memcpy(s_peer, evt->bda, sizeof(s_peer));
            s_chosen = true;
            esp_bt_gap_cancel_discovery();
        }
        break;
    case POSTERN_EVT_DISCOVERY_STOPPED:
        s_discovering = false;
        if (s_chosen && !s_connected && !s_connecting) {
            s_connecting = true;
            if (esp_a2d_source_connect(s_peer) != ESP_OK) {
                s_connecting = false;
                s_chosen = false;
                postern_begin_discovery();
            }
        } else if (!s_connected && !s_connecting && !s_chosen) {
            postern_begin_discovery();
        }
        break;
    case POSTERN_EVT_CAPS:
        postern_try_stream();
        break;
    case POSTERN_EVT_START:
        if (s_media == POSTERN_MEDIA_STARTED) {
            break;
        }
        s_start_pending = true;
        if (s_media == POSTERN_MEDIA_IDLE) {
            postern_try_stream();
        }
        break;
    case POSTERN_EVT_STOP:
        postern_stop_stream();
        break;
    case POSTERN_EVT_ENCODE:
        postern_on_encode(evt->encode_err);
        break;
    case POSTERN_EVT_A2D:
        postern_on_a2d(evt);
        break;
    default:
        break;
    }
}

static void postern_task(void *arg)
{
    (void)arg;
    while (true) {
        uint8_t cmd;
        postern_evt_t evt;

        xTaskNotifyWait(0, UINT32_MAX, NULL, portMAX_DELAY);
        while (s_cmd != NULL && xQueueReceive(s_cmd, &cmd, 0) == pdTRUE) {
            memset(&evt, 0, sizeof(evt));
            evt.kind = cmd;
            postern_on_evt(&evt);
        }
        while (s_queue != NULL && xQueueReceive(s_queue, &evt, 0) == pdTRUE) {
            postern_on_evt(&evt);
        }
    }
}

static esp_err_t postern_start_i2c(void)
{
    i2c_slave_config_t conf = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = (gpio_num_t)s_cfg.sda_gpio,
        .scl_io_num = (gpio_num_t)s_cfg.scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .send_buf_depth = 16,
        .receive_buf_depth = 16,
        .slave_addr = s_cfg.i2c_addr,
        .addr_bit_len = I2C_ADDR_BIT_LEN_7,
        .intr_priority = 0,
        .flags.enable_internal_pullup = 1,
    };
    i2c_slave_event_callbacks_t cbs = {
        .on_request = postern_on_request,
        .on_receive = postern_on_receive,
    };
    esp_err_t err = i2c_new_slave_device(&conf, &s_i2c);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_slave_register_event_callbacks(s_i2c, &cbs, NULL);
}

static esp_err_t postern_start_i2s(void)
{
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_SLAVE);
    chan.dma_desc_num = 6;
    chan.dma_frame_num = 240;
    return i2s_new_channel(&chan, NULL, &s_rx);
}

static esp_err_t postern_start_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return err;
}

static void postern_shutdown(bool bluedroid, bool controller)
{
    postern_pcm_close();
    if (s_i2c != NULL) {
        i2c_del_slave_device(s_i2c);
        s_i2c = NULL;
    }
    if (s_task != NULL) {
        vTaskDelete(s_task);
        s_task = NULL;
    }
    if (s_cmd != NULL) {
        vQueueDelete(s_cmd);
        s_cmd = NULL;
    }
    if (s_queue != NULL) {
        vQueueDelete(s_queue);
        s_queue = NULL;
    }
    if (bluedroid) {
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
    }
    if (controller) {
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
    }
    postern_pcm_close();
    if (s_rx != NULL) {
        i2s_del_channel(s_rx);
        s_rx = NULL;
    }
    s_i2s_ready = false;
    s_inited = false;
}

static esp_err_t postern_start_bt(bool *controller_enabled, bool *bluedroid_enabled)
{
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin = {'1', '2', '3', '4'};
    esp_err_t err;
    const uint8_t *addr;

    *controller_enabled = false;
    *bluedroid_enabled = false;
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK) {
        esp_bt_controller_deinit();
        return err;
    }
    *controller_enabled = true;
    err = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_bluedroid_enable();
    if (err != ESP_OK) {
        esp_bluedroid_deinit();
        return err;
    }
    *bluedroid_enabled = true;

    esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(iocap));
    esp_bt_gap_set_pin(pin_type, 4, pin);
    esp_bt_gap_set_device_name(POSTERN_DEVICE_NAME);
    esp_bt_gap_register_callback(postern_gap_cb);
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    err = esp_a2d_register_callback(postern_a2d_cb);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_a2d_source_init();
    if (err != ESP_OK) {
        return err;
    }

    addr = esp_bt_dev_get_address();
    if (addr != NULL) {
        ESP_LOGI(TAG, "address %02x:%02x:%02x:%02x:%02x:%02x",
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    }
    return ESP_OK;
}

esp_err_t postern_init(void)
{
    esp_err_t err;
    bool controller = false;
    bool bluedroid = false;

    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!postern_load_config(&s_cfg)) {
        ESP_LOGE(TAG, "GPIOs or the I2C address are unset");
        return ESP_ERR_INVALID_STATE;
    }

    postern_file_init(&s_file);
    s_connected = false;
    s_connecting = false;
    s_discovering = false;
    s_chosen = false;
    s_sbc_valid = false;
    s_aac_valid = false;
    s_aac_sep_ok = true;
    s_sbc_sep_ok = true;
    s_aac_give_up = false;
    s_start_pending = false;
    s_start_acked = false;
    s_mcc_valid = false;
    s_audio_mtu = 0;
    s_media = POSTERN_MEDIA_IDLE;
    s_pref_kind = 0;
    s_profile_tries = 0;
    s_sep_wait = 0;
    s_sep_gate = false;
    s_pcm_open = false;
    s_i2s_ready = false;
    s_i2s_on = false;

    s_queue = xQueueCreate(12, sizeof(postern_evt_t));
    s_cmd = xQueueCreate(8, sizeof(uint8_t));
    if (s_queue == NULL || s_cmd == NULL) {
        postern_shutdown(false, false);
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(postern_task, "postern", 6144, NULL, 5, &s_task) != pdPASS) {
        postern_shutdown(false, false);
        return ESP_ERR_NO_MEM;
    }
    err = postern_start_i2c();
    if (err != ESP_OK) {
        postern_shutdown(false, false);
        return err;
    }
    err = postern_start_i2s();
    if (err != ESP_OK) {
        postern_shutdown(false, false);
        return err;
    }
    err = postern_start_nvs();
    if (err != ESP_OK) {
        postern_shutdown(false, false);
        return err;
    }

    err = postern_start_bt(&controller, &bluedroid);
    if (err != ESP_OK) {
        postern_shutdown(bluedroid, controller);
        return err;
    }

    s_inited = true;
    ESP_LOGI(TAG, "I2C peripheral at 0x%02x, I2S slave waiting for start", s_cfg.i2c_addr);
    return ESP_OK;
}
