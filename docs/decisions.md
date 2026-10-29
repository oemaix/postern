# Decisions

Status: baseline · 2026-10-28

Ids are stable. A new decision takes the next number. Status is Decided or Undecided. A recommendation is not a decision.

| Id | Status | Summary |
| --- | --- | --- |
| [D-0001](#d-0001-name) | Decided | The component is named Postern |
| [D-0002](#d-0002-component) | Decided | Superseded by D-0021 |
| [D-0003](#d-0003-role) | Decided | I2S slave in, A2DP source out, I2C peripheral |
| [D-0004](#d-0004-compile-time-pins) | Decided | GPIOs and the I2C address are Kconfig |
| [D-0005](#d-0005-pcm-format) | Decided | 16-bit stereo, 44.1 or 48 kHz, no resample |
| [D-0006](#d-0006-bluetooth-codecs) | Decided | SBC must work; AAC is the later goal |
| [D-0007](#d-0007-framework) | Decided | ESP-IDF |
| [D-0008](#d-0008-target) | Decided | ESP32, PSRAM not required |
| [D-0009](#d-0009-nixpkgs) | Decided | nixpkgs 26.05 |
| [D-0010](#d-0010-no-idf-yet) | Decided | Superseded by D-0022 |
| [D-0011](#d-0011-no-ui) | Decided | No UI document |
| [D-0012](#d-0012-no-remote) | Decided | Remote is `https://github.com/oemaix/postern.git` |
| [D-0013](#d-0013-license) | Decided | Apache License 2.0 |
| [D-0014](#d-0014-i2c-map) | Decided | Register file, offsets `0x00`–`0x07` |
| [D-0015](#d-0015-python) | Decided | Superseded by D-0019 |
| [D-0016](#d-0016-i2c-address) | Decided | 7-bit address `0x1D` |
| [D-0017](#d-0017-reference-pins) | Decided | Stamp Pico GPIO defaults |
| [D-0018](#d-0018-devcontainer) | Decided | Ubuntu 26.04, Nix 2.35.4, two host configs |
| [D-0019](#d-0019-python-out-of-the-flake) | Decided | No project Python in the flake; a later `.venv` uses `uv` |
| [D-0020](#d-0020-no-esptool) | Decided | Superseded by D-0022 |
| [D-0021](#d-0021-image) | Decided | This repository is the Stamp Pico image |
| [D-0022](#d-0022-idf-in-the-shell) | Decided | `idf.py` is ESP-IDF v6.1 through EIM on this pin |

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
- **Consequence:** Superseded by [D-0021](#d-0021-image). The original consequence was no `main/` and no partition table ([P-0003](pitfalls.md#p-0003-no-application)), with a released binary produced by a consuming project.

### D-0003 Role

- **Status:** Decided
- **Choice:** Postern is an I2S slave and an I2C peripheral. PCM moves only from the peer to Postern, then out over A2DP. The peer is the I2S master and the I2C controller.
- **Source:** The Porta cable contract, 2026-10-28.
- **Consequence:** BCLK, WS, and DIN are inputs. Postern does not drive a data output back to the peer.

### D-0004 Compile-time pins

- **Status:** Decided
- **Choice:** The five GPIOs and the 7-bit I2C address are Kconfig symbols, fixed when the image is built. GPIO `-1` and address `0x00` mean unset. `postern_load_config` returns false in that case. A project that reuses the component overrides them in its own sdkconfig.
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
- **Consequence:** The component registers an AAC-LC endpoint and an SBC endpoint and encodes both. [P-0002](pitfalls.md#p-0002-aac-first) still stands: the path has not been measured on the target, so AAC is not treated as done.

### D-0007 Framework

- **Status:** Decided
- **Choice:** The component is ESP-IDF. Arduino and PlatformIO are not used.
- **Source:** The Porta framework decision, 2026-10-28.
- **Consequence:** `components/postern/CMakeLists.txt` uses `idf_component_register`. The repository root calls `project(postern)` ([D-0021](#d-0021-image)).

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
- **Consequence:** Superseded by [D-0022](#d-0022-idf-in-the-shell). The esptool clause was superseded by [D-0020](#d-0020-no-esptool), which [D-0022](#d-0022-idf-in-the-shell) also supersedes.

### D-0011 No UI

- **Status:** Decided
- **Choice:** There is no `docs/ui-design.md`.
- **Source:** A Bluetooth bridge has no screen.
- **Consequence:** Do not add listener-facing screens here.

### D-0012 No remote

- **Status:** Decided
- **Choice:** The remote is `https://github.com/oemaix/postern.git`. Porta pins the submodule on branch `main`.
- **Source:** Project decision, 2026-10-28, revised 2026-10-30 when that remote existed. The first choice was a local path only.
- **Consequence:** A clone of this repository stands alone. Porta's submodule URL is that remote ([D-0021](#d-0021-image)).

### D-0013 License

- **Status:** Decided
- **Choice:** Apache License 2.0. The text is `LICENSE`. The copyright line is `Copyright 2026 oemaix`.
- **Source:** Same license as Porta, so a consuming project can link the component.
- **Consequence:** New source files carry the Apache 2.0 header when they are added. This id does not grant a Bluetooth license.

### D-0014 I2C map

- **Status:** Decided
- **Choice:** The peer talks to an 8-byte register file. The constants live in `components/postern/include/postern_regs.h`. The 7-bit address is not part of the map ([D-0016](#d-0016-i2c-address)).
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
- **Consequence:** The peripheral driver is `components/postern/src/postern_file.c`, started by `postern_init`. `0x00` remains the unset sentinel, and init fails while it is unset. The reference default is `0x1D` ([D-0016](#d-0016-i2c-address), [P-0007](pitfalls.md#p-0007-unset-config), [P-0009](pitfalls.md#p-0009-partial-rate)).

### D-0017 Reference pins

- **Status:** Decided
- **Choice:** The M5Stamp Pico reference defaults are GPIO21 for SDA, GPIO22 for SCL, GPIO18 for BCLK, GPIO19 for WS, and GPIO36 for DIN. GPIO26 stays free for a later I2S data output and is not a Kconfig symbol. GPIO1 and GPIO3 stay UART0. Another board overrides the five symbols. The I2C address is [D-0016](#d-0016-i2c-address).
- **Source:** Project decision, 2026-10-29. Those GPIOs are on the Stamp Pico header. GPIO36 is input-only, so it can receive DIN and cannot drive the later output.
- **Consequence:** Do not hard-code the numbers in `src/`. A peer's GPIO numbers belong to the other chip ([P-0001](pitfalls.md#p-0001-peer-pins), [P-0020](pitfalls.md#p-0020-input-only-din)). `postern_init` fails while a GPIO is `-1` or the address is `0x00`.

### D-0016 I2C address

- **Status:** Decided
- **Choice:** The 7-bit address is `0x1D`. `POSTERN_I2C_ADDRESS` defaults to that value. `0x00` still means unset, and `postern_load_config` returns false for it.
- **Source:** Project decision, 2026-10-29. The reference cable's Port A has no other slave. `0x1D` is inside the usable range `0x08`–`0x77`.
- **Consequence:** Another board overrides the symbol in its sdkconfig. Do not hard-code `0x1D` in `src/` ([P-0007](pitfalls.md#p-0007-unset-config)).

### D-0018 Devcontainer

- **Status:** Decided
- **Choice:** The optional container is Ubuntu 26.04 with single-user Nix 2.35.4. Reopen in Container offers **Postern (Nix on Ubuntu)** and **Postern (NixOS host store)**. They share `.devcontainer/Dockerfile`. The host-store config bind-mounts `/nix` read-only and uses the host daemon.
- **Source:** The same container shape as Porta, 2026-10-29. Opening this repository alone needs its own image. Porta's container remains the one for the product checkout.
- **Consequence:** The installer URL is pinned ([P-0026](pitfalls.md#p-0026-floating-nix-installer)). `sandbox = false` stays in the image. The entrypoint moves `vscode` onto the workspace owner inside the container ([P-0023](pitfalls.md#p-0023-container-uid)). `DEVCONTAINER_WORKSPACE` is that path ([P-0027](pitfalls.md#p-0027-checkout-directory-name)). Only the NixOS config mounts `/nix` ([P-0025](pitfalls.md#p-0025-one-devcontainer-for-every-host)). `/etc/nix/nix.conf` sets `accept-flake-config = false` ([P-0024](pitfalls.md#p-0024-host-nix-store)). The terminal profile is `.vscode/settings.json`. zsh comes from the devshell ([P-0022](pitfalls.md#p-0022-zsh-exit-status)).

### D-0015 Python

- **Status:** Decided
- **Choice:** The recommendation on this id was the flake's `python3` and the standard library, then `uv` and a gitignored `.venv` on the day a third-party package is imported. `pip` is not used. That `.venv` stays separate from the Python tree ESP-IDF will install for `idf.py`.
- **Source:** Open question, 2026-10-28. The same question Porta recorded that day.
- **Consequence:** Superseded by [D-0019](#d-0019-python-out-of-the-flake). The flake no longer carries `python3` or `uv`. [P-0008](pitfalls.md#p-0008-pip) still forbids `pip`.

### D-0019 Python out of the flake

- **Status:** Decided
- **Choice:** The Postern flake does not carry a Python interpreter, `uv`, or `pip`. This supersedes [D-0015](#d-0015-python). Nothing in this repository imports a third-party Python package, so that ecosystem stays out of `flake.nix` until a Postern script must use it. On that day a gitignored `.venv` is created with `uv`. `pip` is not used. That `.venv` stays separate from the Python tree ESP-IDF will install for `idf.py`.
- **Source:** Project decision, 2026-10-29. `python3` and `uv` left the flake because this repository does not call them. Same policy as Porta D-0044.
- **Consequence:** `nix develop` does not put `python3` on `PATH`. The agent hook remains standard-library Python and is started by the `python3` that launches it. No `.venv` and no `requirements.txt` until a third-party import exists. [P-0008](pitfalls.md#p-0008-pip). [P-0028](pitfalls.md#p-0028-python-in-the-flake).

### D-0020 No esptool

- **Status:** Decided
- **Choice:** The devshell does not install esptool. This supersedes the esptool clause of [D-0010](#d-0010-no-idf-yet). CMake and Ninja stay. `idf.py` stays out.
- **Source:** Project edit, 2026-10-29. This repository has no image to flash ([D-0002](#d-0002-component)).
- **Consequence:** Superseded by [D-0022](#d-0022-idf-in-the-shell). The original consequence was that a consuming project flashes its own application, and that esptool stays out until a command in this shell invokes it.

### D-0021 Image

- **Status:** Decided
- **Choice:** This repository is the flashable M5Stamp Pico image, and it is developed on its own. `main` calls `postern_init`. The bridge stays the component `components/postern`, so another project can set `EXTRA_COMPONENT_DIRS` to that directory and build the same sources with its own sdkconfig. This supersedes [D-0002](#d-0002-component).
- **Source:** Project decision, 2026-10-30. The Stamp Pico firmware is this repository. Porta and any later product reuse the component. They do not carry a second copy of the image.
- **Consequence:** `idf.py build` runs at the repository root and produces `postern.bin` for the ESP32 target. Flash is 4 MB, the module's flash, with no PSRAM ([D-0008](#d-0008-target)). The partition table is the ESP-IDF large single factory app, 1.5 MB, because the linked image does not fit the 1 MB default. `sdkconfig.defaults` enables Classic A2DP, the external codec, AAC, and two stream endpoints. GPIO defaults stay the Stamp Pico reference ([D-0017](#d-0017-reference-pins)). Do not add a second application beside `main/` ([P-0034](pitfalls.md#p-0034-one-image)). Do not point `EXTRA_COMPONENT_DIRS` at the repository root ([P-0030](pitfalls.md#p-0030-reuse-directory)). `postern_init` still calls `nvs_flash_init` and does not erase. On `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`, `main` erases and calls `postern_init` again.

### D-0022 IDF in the shell

- **Status:** Decided
- **Choice:** The devshell provides `idf.py` for ESP-IDF v6.1 from [hnaderi/espidf-nix](https://github.com/hnaderi/espidf-nix), built with this nixpkgs pin, for the `esp32` target. Espressif's Installation Manager writes the chip tools under `~/.espressif` on the first run. `esptool.py` is that wrapper's tool. The nixpkgs `esptool` package stays out. This supersedes [D-0010](#d-0010-no-idf-yet) and [D-0020](#d-0020-no-esptool).
- **Source:** Project decision, 2026-10-30. The same shape as Porta D-0028. The image in [D-0021](#d-0021-image) is what this shell builds and flashes.
- **Consequence:** `nix develop` puts `idf.py` on `PATH`. The chip toolchain is not in the Nix store ([P-0031](pitfalls.md#p-0031-install-home)). The command needs a mount namespace ([P-0032](pitfalls.md#p-0032-fhs-namespace)). A devcontainer install records `/root` in `~/.espressif`. On the host the shell binds the login user's tree there when that prefix is missing. A Nix flake pinned to an older nixpkgs stays out ([P-0006](pitfalls.md#p-0006-idf-flake)). Flash with `idf.py -p` and the port chosen at that moment. The container links the host `ttyUSB*` and `ttyACM*` nodes ([P-0033](pitfalls.md#p-0033-serial-device-node)). This repository does not name a port, a baud rate, or an offset.
