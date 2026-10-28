# Architecture

Status: baseline · 2026-10-28

```mermaid
flowchart LR
  peer[Peer I2S master]
  postern[Postern]
  phones[Bluetooth headphones]

  peer -->|BCLK WS DIN| postern
  peer -->|I2C| postern
  postern -->|A2DP| phones
```

Audio on the cable moves only toward Postern. The peer drives BCLK and WS. Postern's I2S peripheral is a slave. I2C uses the register file in `include/postern_regs.h` ([D-0014](decisions.md#d-0014-i2c-map)). The 7-bit address is still unset ([D-0016](decisions.md#d-0016-i2c-address)).

## Compile-time pins

These symbols are in `Kconfig`. They name GPIOs on the chip that runs Postern.

| Symbol | Direction | Unset |
| --- | --- | --- |
| `POSTERN_I2S_BCLK_GPIO` | in | `-1` |
| `POSTERN_I2S_WS_GPIO` | in | `-1` |
| `POSTERN_I2S_DIN_GPIO` | in | `-1` |
| `POSTERN_I2C_SDA_GPIO` | peripheral | `-1` |
| `POSTERN_I2C_SCL_GPIO` | peripheral | `-1` |
| `POSTERN_I2C_ADDRESS` | 7-bit address | `0x00` |

`postern_load_config` copies them into `postern_config_t`. A consuming project sets the values in its sdkconfig or in menuconfig under **Postern**. Changing a wire means building again ([D-0004](decisions.md#d-0004-compile-time-pins)).

The peer's GPIO numbers are not these numbers ([P-0001](pitfalls.md#p-0001-peer-pins)).

## PCM

16-bit stereo, 44.1 kHz or 48 kHz ([D-0005](decisions.md#d-0005-pcm-format)). One clock master, the peer. Postern does not resample ([P-0004](pitfalls.md#p-0004-no-resample)).

## Bluetooth

A2DP source. SBC is the session that has to pass. AAC is next ([D-0006](decisions.md#d-0006-bluetooth-codecs), [P-0002](pitfalls.md#p-0002-aac-first)). The reference module is an M5Stamp Pico, which has no PSRAM ([D-0008](decisions.md#d-0008-target)).

## What is not here

No `main`, no partition table, and no sdkconfig. Those belong to the consuming project ([D-0002](decisions.md#d-0002-component)). `idf.py` is not in the devshell yet ([D-0010](decisions.md#d-0010-no-idf-yet)).
