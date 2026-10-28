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

/* ESP-IDF 6.1 still exports this for the built-in SBC encoder, and the
   a2dp_source example calls it. The public header on that line no longer
   carries the prototype. A matching declaration from the header is compatible. */
extern esp_err_t esp_a2d_source_register_data_callback(int32_t (*callback)(uint8_t *buf, int32_t len));

static const char *POSTERN_DEVICE_NAME = "Postern";

enum {
    POSTERN_EVT_START = 1,
    POSTERN_EVT_STOP,
    POSTERN_EVT_A2D,
    POSTERN_EVT_FOUND,
    POSTERN_EVT_DISCOVERY_STOPPED,
    POSTERN_EVT_PROFILE,
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
static bool s_caps_valid;
static bool s_start_pending;
static esp_bd_addr_t s_peer;
static esp_a2d_conn_hdl_t s_conn;
static esp_a2d_cie_sbc_t s_caps;
static uint32_t s_stream_hz;
static int s_media;
static int s_profile_tries;

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
    postern_set_running(true);
    return ESP_OK;
}

static int32_t postern_data_cb(uint8_t *data, int32_t len)
{
    size_t got = 0;

    /* len -1 asks the source to flush. Nothing is copied. */
    if (data == NULL || len <= 0) {
        return 0;
    }
    if (!s_pcm_open || s_rx == NULL) {
        memset(data, 0, (size_t)len);
        return len;
    }
    if (i2s_channel_read(s_rx, data, (size_t)len, &got, 0) != ESP_OK || got < (size_t)len) {
        if (got < (size_t)len) {
            memset(data + got, 0, (size_t)len - got);
        }
    }
    return len;
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

static void postern_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    postern_evt_t evt;

    memset(&evt, 0, sizeof(evt));
    evt.kind = POSTERN_EVT_A2D;
    evt.a2d_event = event;
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

static void postern_try_stream(void)
{
    esp_a2d_mcc_t pref;
    uint8_t freq;
    uint8_t channel;
    uint8_t blocks;
    uint8_t subbands;
    uint8_t alloc;
    uint8_t min_pool;
    uint8_t max_pool;
    uint32_t hz = 0;

    if (!s_start_pending || !s_connected || !s_caps_valid || s_media != POSTERN_MEDIA_IDLE) {
        return;
    }
    if (!postern_take_rate(&hz)) {
        return;
    }
    freq = postern_sbc_freq(hz);
    channel = postern_sbc_channel(&s_caps);
    blocks = postern_sbc_pick(s_caps.block_len, ESP_A2D_SBC_CIE_BLOCK_LEN_16, ESP_A2D_SBC_CIE_BLOCK_LEN_12,
        ESP_A2D_SBC_CIE_BLOCK_LEN_8, ESP_A2D_SBC_CIE_BLOCK_LEN_4);
    subbands = postern_sbc_pick(s_caps.num_subbands, ESP_A2D_SBC_CIE_NUM_SUBBANDS_8, ESP_A2D_SBC_CIE_NUM_SUBBANDS_4, 0, 0);
    alloc = postern_sbc_pick(s_caps.alloc_mthd, ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS, ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR, 0, 0);
    if (freq == 0 || (s_caps.samp_freq & freq) == 0 || channel == 0 || blocks == 0 || subbands == 0 || alloc == 0) {
        ESP_LOGW(TAG, "sink cannot take %u Hz stereo SBC", (unsigned)hz);
        s_start_pending = false;
        return;
    }

    min_pool = s_caps.min_bitpool < 2 ? 2 : s_caps.min_bitpool;
    max_pool = s_caps.max_bitpool > 53 ? 53 : s_caps.max_bitpool;
    if (min_pool > max_pool) {
        ESP_LOGW(TAG, "sink SBC bitpool is empty");
        s_start_pending = false;
        return;
    }

    memset(&pref, 0, sizeof(pref));
    pref.type = ESP_A2D_MCT_SBC;
    pref.cie.sbc_info.samp_freq = freq;
    pref.cie.sbc_info.ch_mode = channel;
    pref.cie.sbc_info.block_len = blocks;
    pref.cie.sbc_info.num_subbands = subbands;
    pref.cie.sbc_info.alloc_mthd = alloc;
    pref.cie.sbc_info.min_bitpool = min_pool;
    pref.cie.sbc_info.max_bitpool = max_pool;

    s_stream_hz = hz;
    s_media = POSTERN_MEDIA_PREF;
    if (esp_a2d_source_set_pref_mcc(s_conn, &pref) != ESP_OK) {
        s_media = POSTERN_MEDIA_IDLE;
        ESP_LOGW(TAG, "set preferred SBC failed");
        s_start_pending = false;
        return;
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
    postern_pcm_close();
    if (s_media == POSTERN_MEDIA_STARTED || s_media == POSTERN_MEDIA_STARTING) {
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
        s_media = POSTERN_MEDIA_STOPPING;
        return;
    }
    s_media = POSTERN_MEDIA_IDLE;
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
            s_media = POSTERN_MEDIA_IDLE;
            s_start_pending = false;
        }
        break;
    case POSTERN_MEDIA_STARTING:
        if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START && ok) {
            if (postern_pcm_open(s_stream_hz) == ESP_OK) {
                s_media = POSTERN_MEDIA_STARTED;
                s_start_pending = false;
            } else {
                ESP_LOGE(TAG, "I2S slave did not start");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
                s_media = POSTERN_MEDIA_STOPPING;
                s_start_pending = false;
            }
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
    s_caps_valid = false;
    s_start_pending = false;
    s_media = POSTERN_MEDIA_IDLE;
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
        if (a2d->a2d_report_snk_codec_caps_stat.mcc.type == ESP_A2D_MCT_SBC) {
            s_caps = a2d->a2d_report_snk_codec_caps_stat.mcc.cie.sbc_info;
            s_caps_valid = true;
            s_conn = a2d->a2d_report_snk_codec_caps_stat.conn_hdl;
            postern_try_stream();
        }
        break;
    case ESP_A2D_SRC_SET_PREF_MCC_EVT:
        if (s_media != POSTERN_MEDIA_PREF) {
            break;
        }
        if (a2d->a2d_set_pref_mcc_stat.set_status == ESP_BT_STATUS_SUCCESS) {
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
            s_media = POSTERN_MEDIA_CHECK;
        } else {
            ESP_LOGW(TAG, "preferred SBC was refused");
            s_media = POSTERN_MEDIA_IDLE;
            s_start_pending = false;
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
            ESP_LOGI(TAG, "A2DP source ready");
            postern_page_or_discover();
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
    err = esp_a2d_source_register_data_callback(postern_data_cb);
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
    s_caps_valid = false;
    s_start_pending = false;
    s_media = POSTERN_MEDIA_IDLE;
    s_profile_tries = 0;
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
