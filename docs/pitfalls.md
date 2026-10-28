# Pitfalls

Status: baseline · 2026-10-28

Open entries are binding. Do not delete an entry. Mark it resolved, with the reason, when it stops applying. New entries take the next id and are added in the same change that discovered them.

Agents read this file before design or implementation work, and they add to it when a new trap shows up. The Cursor rule is only a reminder. This file is the copy that stays in git.

## Open

### P-0001 Peer pins

- **Status:** Open
- **Avoid:** Do not copy the peer's GPIO numbers into Postern's Kconfig as if they were the same pins.
- **Why:** The numbers in a product cable table name the peer. Postern's symbols name the chip this component runs on. On Porta those peer numbers are Core2 GPIOs. The Stamp Pico assignment is still chosen by the consuming project.

### P-0002 AAC first

- **Status:** Open
- **Avoid:** Do not treat AAC as the first Bluetooth codec to bring up.
- **Why:** The reference module has no PSRAM. SBC is the session that has to pass ([D-0006](decisions.md#d-0006-bluetooth-codecs)). Checked 2026-10-28: ESP-IDF 6.1 can register an AAC source endpoint on an ESP32, using an external AAC-LC encoder of about 51 KB of heap for 48 kHz stereo. The component now encodes with that library. That has not been measured on the Stamp Pico after the Bluetooth stack is up. An encoder in the tree is not a passed session.

### P-0003 No application

- **Status:** Open
- **Avoid:** Do not add `main/`, a partition table, or a flashable example in this repository.
- **Why:** [D-0002](decisions.md#d-0002-component) keeps the image in the consuming project. An example here would become a second product.

### P-0004 No resample

- **Status:** Open
- **Avoid:** Do not resample on this chip.
- **Why:** [D-0005](decisions.md#d-0005-pcm-format) leaves 44.1 kHz and 48 kHz as the only rates. The peer converts anything else before the cable.

### P-0005 Untracked flake

- **Status:** Open
- **Avoid:** Do not run `nix flake lock` or `nix develop` while `flake.nix` is untracked.
- **Why:** In a Git checkout Nix copies only the files Git knows. Stage the flake first.

### P-0006 IDF flake

- **Status:** Open
- **Avoid:** Do not import a third-party ESP-IDF Nix flake that is pinned to an older nixpkgs.
- **Why:** This shell is nixpkgs 26.05 ([D-0009](decisions.md#d-0009-nixpkgs)). Those flakes currently break on that pin. `idf.py` arrives in a later change that can build against it ([D-0010](decisions.md#d-0010-no-idf-yet)).

### P-0007 Unset config

- **Status:** Open
- **Avoid:** Do not treat GPIO `-1` or I2C address `0x00` as a real wiring, and do not pick a default address in code.
- **Why:** Those values mean the consuming project has not chosen ([D-0004](decisions.md#d-0004-compile-time-pins)). `postern_load_config` returns false for them. The address is [D-0016](decisions.md#d-0016-i2c-address). The register offsets are already fixed ([D-0014](decisions.md#d-0014-i2c-map)).

### P-0008 pip

- **Status:** Open
- **Avoid:** Do not `pip install` into the devshell, a user site, or a project `.venv`.
- **Why:** The interpreter is the flake's `python3` until [D-0015](decisions.md#d-0015-python) closes. A virtualenv with no packages is a second interpreter. ESP-IDF will bring its own Python later, and that tree stays separate.

### P-0009 Partial rate

- **Status:** Open
- **Avoid:** Do not apply a sample rate unless one write starts at `0x04` and contains all four bytes, and the value is `44100` or `48000`.
- **Why:** [D-0014](decisions.md#d-0014-i2c-map). A shorter write would store a torn Hertz value. Other rates are refused ([D-0005](decisions.md#d-0005-pcm-format)).

### P-0010 Not an S3

- **Status:** Open
- **Avoid:** Do not set the Postern target to ESP32-S3 or to a C or H series chip.
- **Why:** Those chips have no Bluetooth Classic, so they cannot open an A2DP source. The target stays ESP32 ([D-0008](decisions.md#d-0008-target)).

### P-0011 Rate while running

- **Status:** Open
- **Avoid:** Do not apply a new sample rate to a stream that is already running.
- **Why:** A2DP has already negotiated the codec, and this chip does not resample ([P-0004](#p-0004-no-resample)). A different legal rate written while running sets the rate-rejected bit and keeps the rate the stream started with. Stop, write the four rate bytes, then start.

### P-0012 First sink

- **Status:** Open
- **Avoid:** Do not treat inquiry as a device picker.
- **Why:** The register file has no Bluetooth address ([D-0014](decisions.md#d-0014-i2c-map)). The first session, and any session whose saved address does not answer, connects to the first discoverable device whose class of device includes rendering. A later boot pages the last address that reached A2DP connected, then falls back to that inquiry. Two sinks in the room can still attach the wrong one on the first session.

### P-0013 I2C IRAM

- **Status:** Open
- **Avoid:** Do not set `CONFIG_I2C_ISR_IRAM_SAFE` in the consuming project.
- **Why:** The register callbacks run from the I2C interrupt and call `i2c_slave_write`. They are not placed in IRAM.

### P-0015 Open pairing

- **Status:** Open
- **Avoid:** Do not treat pairing as confirmed by a person.
- **Why:** There is no display ([D-0011](decisions.md#d-0011-no-ui)). Legacy pairing answers PIN `1234`. SSP Just Works is accepted in the callback. Inquiry can bond the first rendering device that answers ([P-0012](#p-0012-first-sink)).

### P-0016 I2C read

- **Status:** Open
- **Avoid:** Do not answer an I2C read one byte at a time, and do not leave unread bytes in the slave TX ring buffer.
- **Why:** On the ESP32 the request callback runs once per read. Bytes that spill into the ring buffer are sent ahead of the next reply. The register file is eight bytes and fits in the hardware FIFO.

### P-0017 A2DP callback

- **Status:** Open
- **Avoid:** Do not block inside the A2DP source data callback.
- **Why:** That callback runs on the Bluetooth task. A wait for I2S, while the peer has not started the bit clock, stalls the controller. Encoding runs on its own task. That task's I2S read is bounded. Missed samples are silence.

### P-0018 External codec

- **Status:** Open
- **Avoid:** Do not leave `BT_A2DP_USE_EXTERNAL_CODEC` unset, and do not register AAC without an SBC endpoint.
- **Why:** The flag removes Bluedroid's built-in SBC encoder. The PCM data callback then has nothing to feed. Postern registers AAC-LC at SEID 0 and SBC at SEID 1, encodes with `esp_audio_codec`, and sends frames with `esp_a2d_source_audio_data_send`. AAC frames are raw, without ADTS; the stack adds LATM, so a frame has to fit in the MTU minus 20 bytes. The AAC encoder is opened only after that codec is negotiated, and a failure falls back to SBC. A negotiated AAC bitrate the encoder cannot send inside the MTU ends that AAC attempt and falls back to SBC. The control task does not wait for the encoder to open. The codec table's 40 KB task stack is what this task requests. This path has not been measured on the Stamp Pico ([P-0002](#p-0002-aac-first)).

### P-0019 Encoder task

- **Status:** Open
- **Avoid:** Do not delete the encoder task, and do not close its encoder from another task.
- **Why:** `esp_audio_enc_open` and `i2s_channel_read` cannot be aborted. `vTaskDelete` during those calls can corrupt the heap or the I2S driver. The task checks the stop flag after each call and frees the encoder itself. The I2S channel stays enabled if that task has not left the driver.

## Resolved

### P-0014 External codec

- **Status:** Resolved
- **Avoid:** Do not set `BT_A2DP_USE_EXTERNAL_CODEC` for the built-in SBC phase.
- **Why:** Resolved 2026-10-28. AAC needs that flag, so the built-in encoder is no longer the path. The binding rule is [P-0018](#p-0018-external-codec).
