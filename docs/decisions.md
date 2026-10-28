# Decisions

Status: baseline · 2026-10-28

Ids are stable. A new decision takes the next number. Status is Decided or Undecided. A recommendation is not a decision.

| Id | Status | Summary |
| --- | --- | --- |
| [D-0001](#d-0001-name) | Decided | The component is named Postern |
| [D-0002](#d-0002-component) | Decided | This repository is a component, not an application |
| [D-0003](#d-0003-role) | Decided | I2S slave in, A2DP source out, I2C peripheral |
| [D-0004](#d-0004-compile-time-pins) | Decided | GPIOs and the I2C address are Kconfig |
| [D-0005](#d-0005-pcm-format) | Decided | 16-bit stereo, 44.1 or 48 kHz, no resample |
| [D-0006](#d-0006-bluetooth-codecs) | Decided | SBC must work; AAC is the later goal |
| [D-0007](#d-0007-framework) | Decided | ESP-IDF |
| [D-0008](#d-0008-target) | Decided | ESP32, PSRAM not required |
| [D-0009](#d-0009-nixpkgs) | Decided | nixpkgs 26.05 |
| [D-0010](#d-0010-no-idf-yet) | Decided | The shell has no `idf.py` yet |
| [D-0011](#d-0011-no-ui) | Decided | No UI document |
| [D-0012](#d-0012-no-remote) | Decided | No network remote; Porta uses a local path |
| [D-0013](#d-0013-license) | Decided | Apache License 2.0 |
| [D-0014](#d-0014-i2c-map) | Decided | Register file, offsets `0x00`–`0x07` |
| [D-0015](#d-0015-python) | Undecided | The flake's Python is enough until a package is needed |
| [D-0016](#d-0016-i2c-address) | Undecided | 7-bit I2C address |

## Decided

### D-0001 Name

- **Status:** Decided
- **Choice:** The component and the repository are named Postern.
- **Source:** Project decision, 2026-10-28. Porta is the gate. A postern is the small side gate.
- **Consequence:** The Kconfig menu, the C symbols, and these docs use that name.

### D-0002 Component

- **Status:** Decided
- **Choice:** This repository is an ESP-IDF component. The consuming project's application is the image that gets flashed. Other projects reuse it by building the component with their own sdkconfig.
- **Source:** Project decision, 2026-10-28.
- **Consequence:** There is no `main/` and no partition table ([P-0003](pitfalls.md#p-0003-no-application)). A released binary, when one exists, is produced by a consuming project.

### D-0003 Role

- **Status:** Decided
- **Choice:** Postern is an I2S slave and an I2C peripheral. PCM moves only from the peer to Postern, then out over A2DP. The peer is the I2S master and the I2C controller.
- **Source:** The Porta cable contract, 2026-10-28.
- **Consequence:** BCLK, WS, and DIN are inputs. Postern does not drive a data output back to the peer.

### D-0004 Compile-time pins

- **Status:** Decided
- **Choice:** The five GPIOs and the 7-bit I2C address are Kconfig symbols, fixed when the consuming project is built. GPIO `-1` and address `0x00` mean unset. `postern_load_config` returns false in that case.
- **Source:** Project decision, 2026-10-28. Board wiring does not change while the firmware runs, so the application sdkconfig is the configuration site.
- **Consequence:** Adapting Postern to another board is a rebuild, not a runtime protocol. Do not hard-code pins in `src/` ([P-0001](pitfalls.md#p-0001-peer-pins), [P-0007](pitfalls.md#p-0007-unset-config)).

### D-0005 PCM format

- **Status:** Decided
- **Choice:** The I2S stream is 16-bit stereo at 44.1 kHz or 48 kHz. Postern does not resample. Any other rate is the peer's job to convert, or it is refused.
- **Source:** The Porta I2S contract, 2026-10-28.
- **Consequence:** [P-0004](pitfalls.md#p-0004-no-resample).

### D-0006 Bluetooth codecs

- **Status:** Decided
- **Choice:** SBC is the A2DP codec that must work. AAC is the goal after SBC is stable on the target.
- **Source:** The Porta transmitter contract, 2026-10-28.
- **Consequence:** [P-0002](pitfalls.md#p-0002-aac-first).

### D-0007 Framework

- **Status:** Decided
- **Choice:** The component is ESP-IDF. Arduino and PlatformIO are not used.
- **Source:** The Porta framework decision, 2026-10-28.
- **Consequence:** `CMakeLists.txt` uses `idf_component_register`.

### D-0008 Target

- **Status:** Decided
- **Choice:** The reference module is the M5Stamp Pico (ESP32-PICO-D4). The component must build for the ESP32 target without PSRAM. Another ESP32 board may set different GPIOs.
- **Source:** Project decision, 2026-10-28.
- **Consequence:** Do not require PSRAM. Do not assume ESP32-S3.

### D-0009 Nixpkgs

- **Status:** Decided
- **Choice:** The flake pins `nixos-26.05`.
- **Source:** Same pin as Porta, so the two shells match.
- **Consequence:** [P-0006](pitfalls.md#p-0006-idf-flake).

### D-0010 No IDF yet

- **Status:** Decided
- **Choice:** The devshell installs esptool, CMake, and Ninja. It does not install `idf.py`.
- **Source:** nixpkgs 26.05 has no ESP-IDF package that this pin can import cleanly.
- **Consequence:** The component sources are not built in phase 0.

### D-0011 No UI

- **Status:** Decided
- **Choice:** There is no `docs/ui-design.md`.
- **Source:** A Bluetooth bridge has no screen.
- **Consequence:** Do not add listener-facing screens here.

### D-0012 No remote

- **Status:** Decided
- **Choice:** The repository has no network remote. Porta pins it with the local URL `../postern`.
- **Source:** Project decision, 2026-10-28.
- **Consequence:** `git submodule update` from a different machine needs that sibling directory, or a remote added later. This id is the place to record the URL when one exists.

### D-0013 License

- **Status:** Decided
- **Choice:** Apache License 2.0. The text is `LICENSE`. The copyright line is `Copyright 2026 oemaix`.
- **Source:** Same license as Porta, so a consuming project can link the component.
- **Consequence:** New source files carry the Apache 2.0 header when they are added. This id does not grant a Bluetooth license.

### D-0014 I2C map

- **Status:** Decided
- **Choice:** The peer talks to an 8-byte register file. The constants live in `include/postern_regs.h`. The 7-bit address is not part of the map ([D-0016](#d-0016-i2c-address)).
- **Access:** The first byte of a write is the register offset. Further bytes auto-increment. A read is a write of the offset, a repeated start, then the bytes. Multi-byte values are little-endian. Offsets past the file read as zero.

  | Offset | Name | Access | Value |
  | --- | --- | --- | --- |
  | `0x00` | WHOAMI | R | `0x50` |
  | `0x01` | VERSION | R | `0x01` |
  | `0x02` | CONTROL | W | `0x01` start, `0x02` stop. Any other byte is ignored. |
  | `0x03` | STATUS | R | bit 0 link up, bit 1 running, bit 2 rate rejected |
  | `0x04`–`0x07` | RATE | RW | Hertz, four bytes. `44100` or `48000`. |

- **Rate write:** One transaction starting at `0x04` carries all four bytes. Anything else leaves the accepted rate unchanged and sets the rate-rejected bit. A start while the accepted rate is unset or rejected does not set running.
- **Source:** Project decision, 2026-10-28, accepting the register-file recommendation. The bits are the ones [D-0003](#d-0003-role) already named: rate, start, stop, and link status.
- **Consequence:** The peripheral driver is `src/postern_file.c`, started by `postern_init`. `0x00` remains the unset address in Kconfig, and init fails while it is unset ([P-0007](pitfalls.md#p-0007-unset-config), [P-0009](pitfalls.md#p-0009-partial-rate)).

## Undecided

### D-0015 Python

- **Status:** Undecided
- **Options:** The flake's `python3` only. `uv` plus a gitignored `.venv` as soon as the shell exists. `pip` into that environment.
- **Recommendation:** The flake is enough today. The hook and the shell scripts use the standard library and nothing from PyPI. Add `uv` to `flake.nix`, and create a gitignored `.venv` with it, on the day a third-party package is imported. Do not use `pip`. Keep that environment separate from the Python tree ESP-IDF will install for `idf.py`.
- **Source:** Same choice as Porta, 2026-10-28.
- **Consequence:** No `.venv`, no `uv`, and no `requirements.txt` until this id closes. [P-0008](pitfalls.md#p-0008-pip).

### D-0016 I2C address

- **Status:** Undecided
- **Options:** A free 7-bit address chosen after the consuming board is scanned. A fixed address written into Kconfig once that scan exists.
- **Recommendation:** Leave `POSTERN_I2C_ADDRESS` at `0x00` until the first board lists the addresses already on its bus.
- **Source:** [D-0014](#d-0014-i2c-map) closed the register file and left the address open.
- **Consequence:** `postern_load_config` still returns false when the address is `0`. Do not put a guessed address in the sources ([P-0007](pitfalls.md#p-0007-unset-config)).
