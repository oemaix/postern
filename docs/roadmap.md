# Roadmap

Status: baseline · 2026-10-28

**Current phase:** 1. The I2S slave, the I2C register driver, and A2DP SBC are in the component. They have not been run on a board.

## Phase 0

This repository: the component entry, the Kconfig pins, the docs, and the devshell. Done.

## Phase 1

`postern_init` starts three paths. PCM stays idle until the peer writes a legal rate and start.

- I2S slave receive at 16-bit stereo, 44.1 kHz or 48 kHz.
- A2DP source with the built-in SBC encoder. A later boot pages the last connected address. Inquiry connects to the first discoverable rendering device.
- I2C peripheral implementing the register file ([D-0014](decisions.md#d-0014-i2c-map)).

## Later

- AAC on the A2DP source, after SBC is measured on the target ([P-0002](pitfalls.md#p-0002-aac-first)).

## Still open

- A consuming project's sdkconfig must set the five GPIOs and the I2C address. The defaults are unset on purpose, and `postern_init` then returns `ESP_ERR_INVALID_STATE`.
- The 7-bit I2C address ([D-0016](decisions.md#d-0016-i2c-address)).
- `idf.py` is not in this shell ([D-0010](decisions.md#d-0010-no-idf-yet)). This tree has not been linked with ESP-IDF 6.1.
- SBC has not been measured on the Stamp Pico.
