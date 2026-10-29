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

Audio on the cable moves only toward Postern. The peer drives BCLK and WS. Postern's I2S peripheral is a slave. I2C uses the register file in `components/postern/include/postern_regs.h` ([D-0014](decisions.md#d-0014-i2c-map)). The 7-bit address is `0x1D` ([D-0016](decisions.md#d-0016-i2c-address)).

## Compile-time pins

These symbols are in `components/postern/Kconfig`. They name GPIOs on the chip that runs Postern.

| Symbol | Direction | Stamp Pico default |
| --- | --- | --- |
| `POSTERN_I2S_BCLK_GPIO` | in | `18` |
| `POSTERN_I2S_WS_GPIO` | in | `19` |
| `POSTERN_I2S_DIN_GPIO` | in | `36` |
| `POSTERN_I2C_SDA_GPIO` | peripheral | `21` |
| `POSTERN_I2C_SCL_GPIO` | peripheral | `22` |
| `POSTERN_I2C_ADDRESS` | 7-bit address | `0x1D` |

`-1` still means a GPIO is unset, and `0x00` still means the address is unset. GPIO26 is reserved for a later data output and is not in this table. GPIO1 and GPIO3 stay the console ([D-0017](decisions.md#d-0017-reference-pins)).

`postern_load_config` copies them into `postern_config_t`. This image keeps the Stamp Pico defaults. A project that reuses the component overrides the values in its sdkconfig or in menuconfig under **Postern**. Changing a wire means building again ([D-0004](decisions.md#d-0004-compile-time-pins)).

The peer's GPIO numbers are not these numbers ([P-0001](pitfalls.md#p-0001-peer-pins)).

## PCM

16-bit stereo, 44.1 kHz or 48 kHz ([D-0005](decisions.md#d-0005-pcm-format)). One clock master, the peer. Postern does not resample ([P-0004](pitfalls.md#p-0004-no-resample)).

## Bluetooth

A2DP source against ESP-IDF 6.1. `sdkconfig.defaults` sets `BT_A2DP_USE_EXTERNAL_CODEC`, enables AAC, and allows two stream endpoints ([P-0018](pitfalls.md#p-0018-external-codec)). A project that reuses the component sets the same symbols. That switch removes Bluedroid's built-in SBC encoder. Postern registers AAC-LC at the higher-priority endpoint and SBC at the second, encodes both with `esp_audio_codec`, and sends the frames itself. AAC is used when the headphones offer 44.1 kHz or 48 kHz stereo AAC-LC. Otherwise the link stays on SBC ([D-0006](decisions.md#d-0006-bluetooth-codecs)). A linked encoder is not a passed session ([P-0002](pitfalls.md#p-0002-aac-first)). The reference module is an M5Stamp Pico, which has no PSRAM ([D-0008](decisions.md#d-0008-target)).

`postern_init` releases the BLE controller memory, enables Classic Bluetooth only, and advertises the name `Postern`. There is no display. Legacy pairing answers PIN `1234`. SSP Just Works is accepted in the callback ([P-0015](pitfalls.md#p-0015-open-pairing)). After a link has connected once, the next boot pages that address. Inquiry is the first attempt and the fallback, and it connects to the first device whose class of device includes rendering ([P-0012](pitfalls.md#p-0012-first-sink)). Inquiry is cancelled when the link comes up. I2C port 0 is the peripheral. The I2S channel is a slave and is enabled only after the stream starts. The control task returns once the encoder task exists. The encoder task closes itself ([P-0019](pitfalls.md#p-0019-encoder-task)). That task's I2S read is bounded; a missing clock becomes silence ([P-0017](pitfalls.md#p-0017-a2dp-callback)).

## Register file

The driver is `components/postern/src/postern_file.c`, behind the I2C slave callbacks. A write's first byte is the offset. One transaction that starts at `0x04` and carries four bytes stores `44100` or `48000`. Any other write that touches those four bytes sets the rate-rejected bit and leaves the accepted rate unchanged ([P-0009](pitfalls.md#p-0009-partial-rate)). A different rate written while the stream is running does the same ([P-0011](pitfalls.md#p-0011-rate-while-running)). On the ESP32 the read callback runs once per transaction, so the reply is every byte from the current offset through the end of the file ([P-0016](pitfalls.md#p-0016-i2c-read)). Start sets the running bit only after the encoder is ready and the sink accepted that rate. The headphones not offering the rate leaves running clear and does not mark the written rate illegal. Stop clears running. Suspend is sent only once a stream start has been requested. A start that arrives while that suspend is in flight is applied when the link returns to idle. The link can stay up.

The I2C callbacks run in an interrupt. This image leaves `CONFIG_I2C_ISR_IRAM_SAFE` unset, and a project that reuses the component does the same ([P-0013](pitfalls.md#p-0013-i2c-iram)).

## Image

The repository root is the ESP-IDF project ([D-0021](decisions.md#d-0021-image)). `main` calls `postern_init`. On `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND` it erases NVS and calls `postern_init` again. The target is ESP32, flash is 4 MB, and `sdkconfig.defaults` carries the Bluetooth symbols. The app partition is the ESP-IDF large single factory app, 1.5 MB. `idf.py` is in the devshell ([D-0022](decisions.md#d-0022-idf-in-the-shell)). The image has not been run on a board.

Another project sets `EXTRA_COMPONENT_DIRS` to `components/postern` and calls `postern_init` from its own `app_main` ([P-0030](pitfalls.md#p-0030-reuse-directory)).
