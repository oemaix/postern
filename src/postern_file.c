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

#include "postern_file.h"

void postern_file_init(postern_file_t *file)
{
    if (file == NULL) {
        return;
    }
    file->rate_hz = 0;
    file->rate_rejected = false;
    file->running = false;
    file->link_up = false;
    file->offset = 0;
}

static bool postern_span_overlaps_rate(uint8_t offset, size_t data_len)
{
    size_t begin = offset;
    size_t end = begin + data_len;
    size_t rate_begin = POSTERN_REG_RATE;
    size_t rate_end = POSTERN_REG_RATE + POSTERN_REG_RATE_LEN;
    return begin < rate_end && end > rate_begin;
}

static void postern_apply_rate(postern_file_t *file, const uint8_t data[POSTERN_REG_RATE_LEN])
{
    uint32_t hz = postern_reg_rate_load(data);
    if (!postern_reg_rate_ok(hz)) {
        file->rate_rejected = true;
        return;
    }
    if (file->running && hz != file->rate_hz) {
        file->rate_rejected = true;
        return;
    }
    if (!file->running) {
        file->rate_hz = hz;
    }
    file->rate_rejected = false;
}

postern_file_action_t postern_file_write(postern_file_t *file, const uint8_t *bytes, size_t len)
{
    if (file == NULL || bytes == NULL || len == 0) {
        return POSTERN_FILE_NONE;
    }

    uint8_t offset = bytes[0];
    file->offset = offset;
    if (len == 1) {
        return POSTERN_FILE_NONE;
    }

    const uint8_t *data = bytes + 1;
    size_t data_len = len - 1;
    bool rate_transaction = offset == POSTERN_REG_RATE && data_len == POSTERN_REG_RATE_LEN;

    if (rate_transaction) {
        postern_apply_rate(file, data);
    } else if (postern_span_overlaps_rate(offset, data_len)) {
        file->rate_rejected = true;
    }

    postern_file_action_t action = POSTERN_FILE_NONE;
    for (size_t i = 0; i < data_len; i++) {
        size_t reg = (size_t)offset + i;
        if (reg != POSTERN_REG_CONTROL || !postern_reg_control_ok(data[i])) {
            continue;
        }
        if (data[i] == POSTERN_CONTROL_STOP) {
            file->running = false;
            action = POSTERN_FILE_STOP;
            continue;
        }
        if (file->rate_hz != 0 && !file->rate_rejected && !file->running) {
            action = POSTERN_FILE_START;
        }
    }

    {
        size_t next = (size_t)offset + data_len;
        file->offset = next > POSTERN_REG_FILE_LEN ? POSTERN_REG_FILE_LEN : (uint8_t)next;
    }
    return action;
}

static uint8_t postern_file_at(const postern_file_t *file, size_t offset)
{
    uint8_t value = 0;

    switch (offset) {
    case POSTERN_REG_WHOAMI:
        value = POSTERN_WHOAMI;
        break;
    case POSTERN_REG_VERSION:
        value = POSTERN_MAP_VERSION;
        break;
    case POSTERN_REG_STATUS:
        if (file->link_up) {
            value |= POSTERN_STATUS_LINK_UP;
        }
        if (file->running) {
            value |= POSTERN_STATUS_RUNNING;
        }
        if (file->rate_rejected) {
            value |= POSTERN_STATUS_RATE_REJECTED;
        }
        break;
    case POSTERN_REG_RATE + 0:
    case POSTERN_REG_RATE + 1:
    case POSTERN_REG_RATE + 2:
    case POSTERN_REG_RATE + 3: {
        uint8_t stored[POSTERN_REG_RATE_LEN];
        postern_reg_rate_store(stored, file->rate_hz);
        value = stored[offset - POSTERN_REG_RATE];
        break;
    }
    default:
        break;
    }
    return value;
}

void postern_file_read_byte(postern_file_t *file, uint8_t *out)
{
    if (file == NULL || out == NULL) {
        return;
    }
    *out = postern_file_at(file, file->offset);
    if (file->offset < POSTERN_REG_FILE_LEN) {
        file->offset++;
    }
}

size_t postern_file_copy(const postern_file_t *file, uint8_t *out, size_t max)
{
    size_t n = 0;

    if (file == NULL || out == NULL) {
        return 0;
    }
    while (n < max && (size_t)file->offset + n < POSTERN_REG_FILE_LEN) {
        out[n] = postern_file_at(file, (size_t)file->offset + n);
        n++;
    }
    return n;
}

void postern_file_commit(postern_file_t *file, size_t n)
{
    size_t next;

    if (file == NULL || n == 0) {
        return;
    }
    next = (size_t)file->offset + n;
    file->offset = next > POSTERN_REG_FILE_LEN ? POSTERN_REG_FILE_LEN : (uint8_t)next;
}

void postern_file_set_link(postern_file_t *file, bool up)
{
    if (file != NULL) {
        file->link_up = up;
    }
}

void postern_file_set_running(postern_file_t *file, bool running)
{
    if (file != NULL) {
        file->running = running;
    }
}
