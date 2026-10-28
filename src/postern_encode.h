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

#pragma once

#include "driver/i2s_std.h"
#include "esp_a2dp_api.h"
#include "esp_err.h"

/* Offered AAC-LC bitrate. Dual-channel 44.1 kHz and 48 kHz both accept it. */
enum { POSTERN_AAC_BITRATE_BPS = 128000 };

/* Called on the encoder task when the encoder is ready, or when it has
   stopped sending because the frame or the link cannot continue. The
   callback only queues work for the Postern task. */
typedef void (*postern_encode_cb_t)(esp_err_t err);

/* Creates the encoder task and returns. ESP_ERR_INVALID_ARG means the
   configuration is not SBC or AAC at 44.1 kHz or 48 kHz. ESP_ERR_NO_MEM
   means the task was not created. The callback reports the open result. */
esp_err_t postern_encode_start(i2s_chan_handle_t rx, esp_a2d_conn_hdl_t conn, uint16_t audio_mtu,
    const esp_a2d_mcc_t *mcc, uint32_t sample_hz, postern_encode_cb_t done);

/* Asks the encoder task to leave the codec and the I2S driver on its own.
   False means that task is still inside a call. The I2S channel then stays enabled. */
bool postern_encode_stop(void);
