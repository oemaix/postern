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

#include "sdkconfig.h"

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
