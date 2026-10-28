# Roadmap

Status: baseline · 2026-10-28

**Current phase:** 0. The component loads its compile-time configuration and does nothing else.

## Phase 0

This repository: the component entry, the Kconfig pins, the docs, and the devshell.

## Phase 1

- I2S slave receive at 16-bit stereo, 44.1 kHz or 48 kHz.
- A2DP source with SBC.
- I2C peripheral for rate, start, stop, and link status, after [D-0014](decisions.md#d-0014-i2c-map).

## Later

- AAC on the A2DP source, after SBC is measured on the target ([P-0002](pitfalls.md#p-0002-aac-first)).

## Blockers before phase 1 code

- A consuming project's sdkconfig must set the five GPIOs and the I2C address. The defaults are unset on purpose.
- The I2C register map ([D-0014](decisions.md#d-0014-i2c-map)).
- `idf.py` is not in this shell ([D-0010](decisions.md#d-0010-no-idf-yet)).
