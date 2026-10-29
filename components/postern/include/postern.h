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

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int bclk_gpio;
    int ws_gpio;
    int din_gpio;
    int sda_gpio;
    int scl_gpio;
    uint8_t i2c_addr;
} postern_config_t;

/* Reads the compile-time Kconfig values. Returns false when any GPIO is
   unset or the I2C address is 0. Does not start I2S or Bluetooth. */
bool postern_load_config(postern_config_t *config);

/* Starts the I2C peripheral, the I2S slave, and Classic A2DP. AAC-LC is
   offered first. SBC is the fallback, encoded in this component because
   the external-codec switch removes Bluedroid's built-in encoder. PCM stays
   idle until the peer writes a legal rate and start. ESP_ERR_INVALID_STATE
   means a GPIO or the address is unset, or init already succeeded. */
esp_err_t postern_init(void);
