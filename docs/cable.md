# Cable

Status: baseline · 2026-10-30

Wiring for [D-0003](decisions.md#d-0003-role). Postern is the I2S slave and the I2C peripheral. The peer is the I2S master and the I2C controller. This image has not been run on a board ([roadmap](roadmap.md)).

## Levels and power

Signal wires are 3.3 V. Grove red is 5 V. It does not land on a GPIO, and it is not the pull-up rail. The Stamp Pico takes power on its 5 V pin or its USB connector. The two boards share a ground.

## I2C

SDA and SCL are open-drain. Each line needs one pull-up to 3.3 V.

On the Porta cable those resistors are already on the Core2. The core schematic labels R59 and R60, each 5.1 kΩ, as the external I2C pull-ups from GPIO32 and GPIO33 to MCU_VDD ([Core2 schematic](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/schematic/Core/CORE2_V1.0_SCH.pdf)). Core2 v1.3 keeps PORT.A on those pins. Its revision notes do not move that network ([Core2 v1.3](https://docs.m5stack.com/en/core/Core2_v1.3)). That pair is the bus pull-up. The Stamp Pico header, and a Module Proto carrier, do not add a second pair.

GPIO21 and GPIO22 on the Stamp Pico are plain header pins. `postern_init` enables the ESP32 internal pull-ups. Those are about 45 kΩ and are not this pair ([P-0035](pitfalls.md#p-0035-i2c-pull-ups)).

A peer that is not a Core2 fits its own pair to 3.3 V, near 5 kΩ.

The controller clock is 100 kHz. The ESP32 peripheral cannot stretch SCL. The register reply is queued in the one request callback ([P-0016](pitfalls.md#p-0016-i2c-read)). The 7-bit address is `0x1D` ([D-0016](decisions.md#d-0016-i2c-address)). The reference cable has no other device on this bus.

## I2S

BCLK, WS, and DIN are push-pull from the peer. They take no pull-ups. MCLK is not connected. Postern does not drive a data pin back to the peer.

The format is Philips, 16-bit stereo, 44.1 kHz or 48 kHz ([D-0005](decisions.md#d-0005-pcm-format)). The channel is opened with `I2S_ROLE_SLAVE` and `I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG`. The peer is the only clock master. GPIO26 is DIN. GPIO36 is input-only and is not this wire ([P-0020](pitfalls.md#p-0020-input-only-gpio36)).

Slave-side pins are [D-0017](decisions.md#d-0017-reference-pins). On Porta the peer numbers are Core2 GPIO32 and GPIO33 for I2C, and GPIO14, GPIO13, and GPIO26 for I2S. The data wire is GPIO26 on both chips. The clock and I2C numbers are not ([P-0001](pitfalls.md#p-0001-peer-pins)).
