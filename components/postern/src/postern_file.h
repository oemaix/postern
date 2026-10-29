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

#include "postern_regs.h"

#include <stddef.h>

typedef enum {
    POSTERN_FILE_NONE = 0,
    POSTERN_FILE_START,
    POSTERN_FILE_STOP,
} postern_file_action_t;

typedef struct {
    uint32_t rate_hz;
    bool rate_rejected;
    bool running;
    bool link_up;
    uint8_t offset;
} postern_file_t;

void postern_file_init(postern_file_t *file);

/* One master write. The first byte is the register offset. */
postern_file_action_t postern_file_write(postern_file_t *file, const uint8_t *bytes, size_t len);

/* One byte from the current offset, then auto-increment. Past the file reads as zero. */
void postern_file_read_byte(postern_file_t *file, uint8_t *out);

/* Copies up to max bytes from the current offset. Does not move the offset. */
size_t postern_file_copy(const postern_file_t *file, uint8_t *out, size_t max);

/* Moves the offset forward by n bytes, stopping at the end of the file. */
void postern_file_commit(postern_file_t *file, size_t n);

void postern_file_set_link(postern_file_t *file, bool up);
void postern_file_set_running(postern_file_t *file, bool running);
