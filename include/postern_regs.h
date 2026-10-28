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

#include <stdbool.h>
#include <stdint.h>

/* I2C register file. The 7-bit address is not part of this map. */

#define POSTERN_REG_WHOAMI 0x00u
#define POSTERN_REG_VERSION 0x01u
#define POSTERN_REG_CONTROL 0x02u
#define POSTERN_REG_STATUS 0x03u
#define POSTERN_REG_RATE 0x04u
#define POSTERN_REG_RATE_LEN 4u
#define POSTERN_REG_FILE_LEN 8u

#define POSTERN_WHOAMI 0x50u
#define POSTERN_MAP_VERSION 0x01u

#define POSTERN_CONTROL_START 0x01u
#define POSTERN_CONTROL_STOP 0x02u

#define POSTERN_STATUS_LINK_UP 0x01u
#define POSTERN_STATUS_RUNNING 0x02u
#define POSTERN_STATUS_RATE_REJECTED 0x04u

#define POSTERN_RATE_HZ_44K 44100u
#define POSTERN_RATE_HZ_48K 48000u

static inline bool postern_reg_control_ok(uint8_t command)
{
    return command == POSTERN_CONTROL_START || command == POSTERN_CONTROL_STOP;
}

static inline bool postern_reg_rate_ok(uint32_t hz)
{
    return hz == POSTERN_RATE_HZ_44K || hz == POSTERN_RATE_HZ_48K;
}

static inline void postern_reg_rate_store(uint8_t out[POSTERN_REG_RATE_LEN], uint32_t hz)
{
    out[0] = (uint8_t)hz;
    out[1] = (uint8_t)(hz >> 8);
    out[2] = (uint8_t)(hz >> 16);
    out[3] = (uint8_t)(hz >> 24);
}

static inline uint32_t postern_reg_rate_load(const uint8_t in[POSTERN_REG_RATE_LEN])
{
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}
