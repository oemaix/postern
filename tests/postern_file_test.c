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

#include <stdio.h>

static int g_failed;

static void expect(int cond, const char *name)
{
    if (!cond) {
        fprintf(stderr, "fail: %s\n", name);
        g_failed++;
    }
}

static void write_rate(postern_file_t *file, uint32_t hz)
{
    uint8_t frame[1 + POSTERN_REG_RATE_LEN];
    frame[0] = POSTERN_REG_RATE;
    postern_reg_rate_store(frame + 1, hz);
    postern_file_write(file, frame, sizeof(frame));
}

static uint8_t read_at(postern_file_t *file, uint8_t offset)
{
    uint8_t set = offset;
    uint8_t value = 0xff;
    postern_file_write(file, &set, 1);
    postern_file_read_byte(file, &value);
    return value;
}

int main(void)
{
    postern_file_t file;
    uint8_t start[2] = {POSTERN_REG_CONTROL, POSTERN_CONTROL_START};
    uint8_t stop[2] = {POSTERN_REG_CONTROL, POSTERN_CONTROL_STOP};
    uint8_t ignored[2] = {POSTERN_REG_CONTROL, 0x03};
    uint8_t partial[3] = {POSTERN_REG_RATE, 0x44, 0xac};
    uint8_t spill[6] = {POSTERN_REG_CONTROL, POSTERN_CONTROL_START, 0, 1, 2, 3};

    postern_file_init(&file);
    expect(read_at(&file, POSTERN_REG_WHOAMI) == POSTERN_WHOAMI, "whoami");
    expect(read_at(&file, POSTERN_REG_VERSION) == POSTERN_MAP_VERSION, "version");
    expect(postern_file_write(&file, start, sizeof(start)) == POSTERN_FILE_NONE, "start without rate");
    expect(!file.running, "not running yet");

    write_rate(&file, POSTERN_RATE_HZ_44K);
    expect(file.rate_hz == POSTERN_RATE_HZ_44K, "rate 44100");
    expect(!file.rate_rejected, "44100 accepted");
    expect(postern_file_write(&file, start, sizeof(start)) == POSTERN_FILE_START, "start");
    expect(!file.running, "start waits for the stream");

    postern_file_set_running(&file, true);
    write_rate(&file, POSTERN_RATE_HZ_48K);
    expect(file.rate_hz == POSTERN_RATE_HZ_44K, "rate held while running");
    expect(file.rate_rejected, "different rate rejected while running");
    expect(postern_file_write(&file, start, sizeof(start)) == POSTERN_FILE_NONE, "start blocked");

    write_rate(&file, POSTERN_RATE_HZ_44K);
    expect(!file.rate_rejected, "same rate while running is not a rejection");

    expect(postern_file_write(&file, stop, sizeof(stop)) == POSTERN_FILE_STOP, "stop");
    expect(!file.running, "stopped");

    write_rate(&file, 12345);
    expect(file.rate_hz == POSTERN_RATE_HZ_44K, "bad rate keeps the old value");
    expect(file.rate_rejected, "bad rate sets rejected");
    expect(postern_file_write(&file, start, sizeof(start)) == POSTERN_FILE_NONE, "start while rejected");

    write_rate(&file, POSTERN_RATE_HZ_48K);
    expect(file.rate_hz == POSTERN_RATE_HZ_48K, "48000 stored");
    expect(!file.rate_rejected, "valid rate clears rejected");
    expect(read_at(&file, POSTERN_REG_RATE) == 0x80, "rate le 0");
    expect(read_at(&file, POSTERN_REG_RATE + 1) == 0xbb, "rate le 1");
    expect(read_at(&file, POSTERN_REG_RATE + 2) == 0x00, "rate le 2");
    expect(read_at(&file, POSTERN_REG_RATE + 3) == 0x00, "rate le 3");

    file.rate_hz = POSTERN_RATE_HZ_44K;
    file.rate_rejected = false;
    expect(postern_file_write(&file, partial, sizeof(partial)) == POSTERN_FILE_NONE, "partial rate");
    expect(file.rate_hz == POSTERN_RATE_HZ_44K, "partial keeps rate");
    expect(file.rate_rejected, "partial sets rejected");

    file.rate_rejected = false;
    expect(postern_file_write(&file, spill, sizeof(spill)) == POSTERN_FILE_NONE, "spill is not a start");
    expect(file.rate_rejected, "spill sets rejected");

    file.rate_rejected = false;
    file.rate_hz = POSTERN_RATE_HZ_44K;
    expect(postern_file_write(&file, ignored, sizeof(ignored)) == POSTERN_FILE_NONE, "ignored control");
    expect(!file.rate_rejected, "ignored control leaves the rate");

    postern_file_init(&file);
    write_rate(&file, POSTERN_RATE_HZ_44K);
    expect(file.offset == POSTERN_REG_FILE_LEN, "rate write leaves the offset at the end");
    postern_file_set_running(&file, true);
    write_rate(&file, POSTERN_RATE_HZ_48K);
    expect(postern_file_write(&file, stop, sizeof(stop)) == POSTERN_FILE_STOP, "stop while rejected");
    expect(file.rate_rejected, "rejected survives stop");
    expect(postern_file_write(&file, start, sizeof(start)) == POSTERN_FILE_NONE, "start stays blocked");

    {
        uint8_t both[3] = {POSTERN_REG_CONTROL, POSTERN_CONTROL_STOP, POSTERN_CONTROL_START};
        uint8_t snap[4];
        uint8_t set = POSTERN_REG_RATE;
        postern_file_init(&file);
        write_rate(&file, POSTERN_RATE_HZ_44K);
        expect(postern_file_write(&file, both, sizeof(both)) == POSTERN_FILE_STOP, "only the control byte is a command");
        expect(!file.running, "stop clears running");
        expect(!file.rate_rejected, "the following byte is not the rate");
        postern_file_write(&file, &set, 1);
        expect(postern_file_copy(&file, snap, 4) == 4, "copy four rate bytes");
        expect(snap[0] == 0x44 && snap[1] == 0xac, "44100 bytes");
        expect(file.offset == POSTERN_REG_RATE, "copy keeps the offset");
        postern_file_commit(&file, 4);
        expect(file.offset == POSTERN_REG_RATE + 4, "commit advances");
    }

    postern_file_set_link(&file, true);
    expect(read_at(&file, POSTERN_REG_STATUS) == POSTERN_STATUS_LINK_UP, "link bit");
    file.offset = POSTERN_REG_FILE_LEN;
    {
        uint8_t past = 0xff;
        postern_file_read_byte(&file, &past);
        expect(past == 0, "past end");
        expect(file.offset == POSTERN_REG_FILE_LEN, "offset stays past the file");
    }

    if (g_failed != 0) {
        fprintf(stderr, "%d failed\n", g_failed);
        return 1;
    }
    return 0;
}
