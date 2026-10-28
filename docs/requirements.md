# Requirements

Status: baseline · 2026-10-28

Phase 1 requirements are the acceptance bar for that phase. The phase 0 tree only loads the compile-time configuration.

## Functional

| Id | Phase | Requirement | Use case |
| --- | --- | --- | --- |
| FR-01 | 0 | `postern_load_config` reports the Kconfig GPIOs and address, and fails when a GPIO is `-1` or the address is `0`. | UC-03 |
| FR-02 | 1 | Postern receives 16-bit stereo I2S at 44.1 kHz or 48 kHz as a slave. | UC-01 |
| FR-03 | 1 | Postern transmits that PCM as an A2DP source using SBC, and uses AAC when the session supports it. | UC-01 |
| FR-04 | 1 | The I2C peripheral implements the register file: rate, start, stop, and link status ([D-0014](decisions.md#d-0014-i2c-map)). | UC-02 |
| FR-05 | 1 | Postern does not resample. A rate other than 44.1 kHz or 48 kHz is refused. | UC-02 |

## Non-functional

| Id | Requirement |
| --- | --- |
| NFR-01 | Pin numbers and the I2C address come from Kconfig ([D-0004](decisions.md#d-0004-compile-time-pins)). |
| NFR-02 | The component does not require PSRAM ([D-0008](decisions.md#d-0008-target)). |
| NFR-03 | A clean Linux host with Nix can enter the devshell from `flake.nix`. |
| NFR-04 | Decisions and pitfalls keep stable ids. |

FR-01 through FR-05 are in the component for the SBC path. AAC in FR-03 is still later. None of this has been run on a board. `postern_init` still fails until the consuming project sets the GPIOs and the I2C address.
