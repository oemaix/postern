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

#include "postern_encode.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_aac_enc.h"
#include "esp_audio_enc.h"
#include "esp_log.h"
#include "esp_sbc_enc.h"

static const char *TAG = "postern";

enum {
    POSTERN_AAC_MTU_OVERHEAD = 20,
    POSTERN_AAC_STACK = 40960,
    POSTERN_SBC_STACK = 8192,
    POSTERN_SEND_TRIES = 10,
    POSTERN_STOP_WAIT_MS = 3000,
};

static i2s_chan_handle_t s_rx;
static esp_a2d_conn_hdl_t s_conn;
static uint16_t s_mtu;
static uint32_t s_hz;
static esp_a2d_mcc_t s_mcc;
static esp_audio_enc_handle_t s_enc;
static TaskHandle_t s_task;
static postern_encode_cb_t s_done;
static volatile bool s_run;
static bool s_aac_registered;
static bool s_sbc_registered;

static void postern_encode_report(esp_err_t err)
{
    postern_encode_cb_t done = s_done;
    if (done != NULL) {
        done(err);
    }
}

static bool postern_register_codec(esp_audio_type_t type)
{
    if (type == ESP_AUDIO_TYPE_AAC) {
        if (s_aac_registered) {
            return true;
        }
        if (esp_aac_enc_register() != ESP_AUDIO_ERR_OK) {
            return false;
        }
        s_aac_registered = true;
        return true;
    }
    if (s_sbc_registered) {
        return true;
    }
    if (esp_sbc_enc_register() != ESP_AUDIO_ERR_OK) {
        return false;
    }
    s_sbc_registered = true;
    return true;
}

static bool postern_aac_bitrate(const esp_a2d_cie_m24_t *m24, int hz, int *out)
{
    uint32_t negotiated = ((uint32_t)(m24->br1 & ESP_A2D_M24_CIE_BR1_MSK) << 16)
        | ((uint32_t)(m24->br2 & ESP_A2D_M24_CIE_BR2_MSK) << 8)
        | (uint32_t)(m24->br3 & ESP_A2D_M24_CIE_BR3_MSK);
    int min_br = hz >= 48000 ? 118000 : 114000;
    int br = negotiated == 0 ? POSTERN_AAC_BITRATE_BPS : (int)negotiated;
    int fit = 320000;

    if (s_mtu > POSTERN_AAC_MTU_OVERHEAD && hz > 0) {
        fit = (int)(((uint64_t)(s_mtu - POSTERN_AAC_MTU_OVERHEAD) * 8ull * (uint64_t)hz) / 1024ull);
    }
    if (br < min_br || br > 320000 || br > fit) {
        return false;
    }
    *out = br;
    return true;
}

static uint8_t postern_sbc_blocks(uint8_t bits)
{
    if ((bits & ESP_A2D_SBC_CIE_BLOCK_LEN_16) != 0) {
        return 16;
    }
    if ((bits & ESP_A2D_SBC_CIE_BLOCK_LEN_12) != 0) {
        return 12;
    }
    if ((bits & ESP_A2D_SBC_CIE_BLOCK_LEN_8) != 0) {
        return 8;
    }
    if ((bits & ESP_A2D_SBC_CIE_BLOCK_LEN_4) != 0) {
        return 4;
    }
    return 0;
}

static bool postern_open_aac(int hz)
{
    int bitrate = 0;
    esp_aac_enc_config_t aac;

    if (!postern_aac_bitrate(&s_mcc.cie.m24_info, hz, &bitrate)) {
        ESP_LOGW(TAG, "AAC bitrate does not fit this link");
        return false;
    }
    aac = (esp_aac_enc_config_t) {
        .sample_rate = hz,
        .channel = ESP_AUDIO_DUAL,
        .bits_per_sample = ESP_AUDIO_BIT16,
        .bitrate = bitrate,
        .adts_used = false,
    };
    esp_audio_enc_config_t cfg = {
        .type = ESP_AUDIO_TYPE_AAC,
        .cfg = &aac,
        .cfg_sz = sizeof(aac),
    };

    if ((s_mcc.cie.m24_info.ch & ESP_A2D_M24_CIE_CH_2) == 0) {
        return false;
    }
    if (!postern_register_codec(ESP_AUDIO_TYPE_AAC)) {
        return false;
    }
    if (esp_audio_enc_open(&cfg, &s_enc) != ESP_AUDIO_ERR_OK) {
        s_enc = NULL;
        return false;
    }
    ESP_LOGI(TAG, "AAC-LC %d Hz, %d bit/s", hz, bitrate);
    return true;
}

static bool postern_open_sbc(int hz)
{
    const esp_a2d_cie_sbc_t *sbc = &s_mcc.cie.sbc_info;
    esp_sbc_ch_mode_t mode;
    uint8_t blocks = postern_sbc_blocks(sbc->block_len);
    uint8_t subbands = (sbc->num_subbands & ESP_A2D_SBC_CIE_NUM_SUBBANDS_8) != 0 ? 8 : 4;
    uint16_t bitpool = sbc->max_bitpool > 53 ? 53 : sbc->max_bitpool;
    esp_sbc_enc_config_t raw;
    esp_audio_enc_config_t cfg;

    if ((sbc->ch_mode & ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO) != 0) {
        mode = ESP_SBC_CH_MODE_JOINT_STEREO;
    } else if ((sbc->ch_mode & ESP_A2D_SBC_CIE_CH_MODE_STEREO) != 0) {
        mode = ESP_SBC_CH_MODE_STEREO;
    } else if ((sbc->ch_mode & ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL) != 0) {
        mode = ESP_SBC_CH_MODE_DUAL;
    } else {
        return false;
    }
    if (blocks == 0 || bitpool < 2) {
        return false;
    }
    if ((sbc->num_subbands & (ESP_A2D_SBC_CIE_NUM_SUBBANDS_8 | ESP_A2D_SBC_CIE_NUM_SUBBANDS_4)) == 0) {
        return false;
    }

    raw = (esp_sbc_enc_config_t) {
        .sbc_mode = ESP_SBC_MODE_STD,
        .allocation_method = (sbc->alloc_mthd & ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS) != 0
            ? ESP_SBC_ALLOC_LOUDNESS : ESP_SBC_ALLOC_SNR,
        .ch_mode = mode,
        .sample_rate = (uint32_t)hz,
        .bits_per_sample = 16,
        .bitpool = bitpool,
        .block_length = blocks,
        .sub_bands_num = subbands,
    };
    cfg = (esp_audio_enc_config_t) {
        .type = ESP_AUDIO_TYPE_SBC,
        .cfg = &raw,
        .cfg_sz = sizeof(raw),
    };
    if (!postern_register_codec(ESP_AUDIO_TYPE_SBC)) {
        return false;
    }
    if (esp_audio_enc_open(&cfg, &s_enc) != ESP_AUDIO_ERR_OK) {
        s_enc = NULL;
        return false;
    }
    ESP_LOGI(TAG, "SBC %d Hz, bitpool %u", hz, (unsigned)bitpool);
    return true;
}

static esp_err_t postern_send_frame(esp_a2d_audio_buff_t *frame)
{
    esp_err_t err = ESP_FAIL;

    for (int i = 0; i < POSTERN_SEND_TRIES && s_run; i++) {
        err = esp_a2d_source_audio_data_send(s_conn, frame);
        if (err == ESP_OK || err == ESP_ERR_INVALID_ARG || err == ESP_ERR_INVALID_SIZE || err == ESP_ERR_INVALID_STATE) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return err;
}

static void postern_encode_finish(uint8_t *pcm, uint8_t *encoded)
{
    free(pcm);
    free(encoded);
    if (s_enc != NULL) {
        esp_audio_enc_close(s_enc);
        s_enc = NULL;
    }
    s_task = NULL;
    s_run = false;
    vTaskDelete(NULL);
}

static void postern_encode_task(void *arg)
{
    int hz = (int)s_hz;
    int in_sz = 0;
    int out_sz = 0;
    uint8_t *pcm = NULL;
    uint8_t *encoded = NULL;
    uint32_t timestamp = 0;
    uint32_t samples = 0;
    uint16_t limit;

    (void)arg;
    if (!s_run) {
        postern_encode_finish(NULL, NULL);
        return;
    }
    if (s_mcc.type == ESP_A2D_MCT_M24 && s_mtu <= POSTERN_AAC_MTU_OVERHEAD) {
        postern_encode_report(ESP_ERR_INVALID_SIZE);
        postern_encode_finish(NULL, NULL);
        return;
    }
    if (hz == 0 || (s_mcc.type == ESP_A2D_MCT_M24 ? !postern_open_aac(hz) : !postern_open_sbc(hz))) {
        postern_encode_report(ESP_ERR_INVALID_STATE);
        postern_encode_finish(NULL, NULL);
        return;
    }
    if (!s_run) {
        postern_encode_finish(NULL, NULL);
        return;
    }
    if (esp_audio_enc_get_frame_size(s_enc, &in_sz, &out_sz) != ESP_AUDIO_ERR_OK || in_sz <= 0 || out_sz <= 0) {
        postern_encode_report(ESP_ERR_INVALID_SIZE);
        postern_encode_finish(NULL, NULL);
        return;
    }
    samples = (uint32_t)in_sz / (uint32_t)(sizeof(int16_t) * 2);
    if (samples == 0) {
        postern_encode_report(ESP_ERR_INVALID_SIZE);
        postern_encode_finish(NULL, NULL);
        return;
    }
    pcm = malloc((size_t)in_sz);
    encoded = malloc((size_t)out_sz);
    if (pcm == NULL || encoded == NULL) {
        postern_encode_report(ESP_ERR_NO_MEM);
        postern_encode_finish(pcm, encoded);
        return;
    }
    postern_encode_report(ESP_OK);

    limit = s_mcc.type == ESP_A2D_MCT_M24 && s_mtu > POSTERN_AAC_MTU_OVERHEAD
        ? (uint16_t)(s_mtu - POSTERN_AAC_MTU_OVERHEAD) : s_mtu;

    while (s_run) {
        size_t got = 0;
        uint32_t timeout_ms = (uint32_t)(((uint64_t)samples * 1000u) / (uint32_t)hz) + 15u;
        esp_audio_enc_in_frame_t in_frame;
        esp_audio_enc_out_frame_t out_frame;
        esp_a2d_audio_buff_t *frame;
        esp_err_t sent;

        memset(&in_frame, 0, sizeof(in_frame));
        memset(&out_frame, 0, sizeof(out_frame));

        if (timeout_ms < 20) {
            timeout_ms = 20;
        }
        if (s_rx == NULL || i2s_channel_read(s_rx, pcm, (size_t)in_sz, &got, timeout_ms) != ESP_OK) {
            got = 0;
        }
        if (!s_run) {
            break;
        }
        if (got < (size_t)in_sz) {
            memset(pcm + got, 0, (size_t)in_sz - got);
        }

        in_frame.buffer = pcm;
        in_frame.len = (uint32_t)in_sz;
        out_frame.buffer = encoded;
        out_frame.len = (uint32_t)out_sz;
        if (esp_audio_enc_process(s_enc, &in_frame, &out_frame) != ESP_AUDIO_ERR_OK || out_frame.encoded_bytes <= 0) {
            continue;
        }
        if (limit != 0 && out_frame.encoded_bytes > limit) {
            ESP_LOGW(TAG, "frame %u does not fit MTU %u", (unsigned)out_frame.encoded_bytes, (unsigned)s_mtu);
            if (s_run) {
                postern_encode_report(ESP_ERR_INVALID_SIZE);
            }
            break;
        }

        frame = esp_a2d_audio_buff_alloc((uint16_t)out_frame.encoded_bytes);
        if (frame == NULL) {
            continue;
        }
        frame->data_len = (uint16_t)out_frame.encoded_bytes;
        frame->number_frame = 1;
        frame->timestamp = timestamp;
        memcpy(frame->data, encoded, (size_t)out_frame.encoded_bytes);
        sent = postern_send_frame(frame);
        if (sent != ESP_OK) {
            esp_a2d_audio_buff_free(frame);
            if (!s_run || sent != ESP_FAIL) {
                if (s_run) {
                    postern_encode_report(sent);
                }
                break;
            }
        } else {
            timestamp += samples;
        }
    }

    postern_encode_finish(pcm, encoded);
}

esp_err_t postern_encode_start(i2s_chan_handle_t rx, esp_a2d_conn_hdl_t conn, uint16_t audio_mtu,
    const esp_a2d_mcc_t *mcc, uint32_t sample_hz, postern_encode_cb_t done)
{
    uint32_t stack = POSTERN_SBC_STACK;

    if (rx == NULL || mcc == NULL || done == NULL || audio_mtu == 0 || (sample_hz != 44100 && sample_hz != 48000)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (mcc->type != ESP_A2D_MCT_M24 && mcc->type != ESP_A2D_MCT_SBC) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_rx = rx;
    s_conn = conn;
    s_mtu = audio_mtu;
    s_hz = sample_hz;
    s_mcc = *mcc;
    s_enc = NULL;
    s_done = done;
    s_run = true;
    if (mcc->type == ESP_A2D_MCT_M24) {
        stack = POSTERN_AAC_STACK;
    }
    if (xTaskCreate(postern_encode_task, "postern_enc", stack, NULL, 10, &s_task) != pdPASS) {
        s_task = NULL;
        s_run = false;
        s_done = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool postern_encode_stop(void)
{
    if (s_task == NULL) {
        s_done = NULL;
        s_rx = NULL;
        return true;
    }
    s_run = false;
    for (int waited = 0; waited < POSTERN_STOP_WAIT_MS && s_task != NULL; waited += 10) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (s_task != NULL) {
        ESP_LOGE(TAG, "encoder task still inside the codec");
        return false;
    }
    s_done = NULL;
    s_rx = NULL;
    return true;
}
