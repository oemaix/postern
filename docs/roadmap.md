# Roadmap

Status: baseline · 2026-10-28

**Current phase:** 1. The I2S slave, the I2C register driver, and A2DP with AAC-LC plus an SBC fallback are in the component. They have not been run on a board.

## Phase 0

This repository: the component entry, the Kconfig pins, the docs, and the devshell. Done.

## Phase 1

`postern_init` starts three paths. PCM stays idle until the peer writes a legal rate and start.

- I2S slave receive at 16-bit stereo, 44.1 kHz or 48 kHz.
- A2DP source. AAC-LC is offered first, at 128 kbit/s, and SBC is encoded in the component when AAC is not available. A later boot pages the last connected address. Inquiry connects to the first discoverable rendering device.
- I2C peripheral implementing the register file ([D-0014](decisions.md#d-0014-i2c-map)).

## Still open

- The reference defaults are the Stamp Pico GPIOs and address `0x1D` ([D-0017](decisions.md#d-0017-reference-pins), [D-0016](decisions.md#d-0016-i2c-address)). `postern_init` returns `ESP_ERR_INVALID_STATE` while a GPIO is `-1` or the address is `0x00`.
- `idf.py` is not in this shell ([D-0010](decisions.md#d-0010-no-idf-yet)). This tree has not been linked with ESP-IDF 6.1.
- SBC and AAC have not been measured on the Stamp Pico ([P-0002](pitfalls.md#p-0002-aac-first)).
