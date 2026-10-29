# Overview

Status: baseline · 2026-10-28

Postern runs on the M5Stamp Pico. A peer, such as the Porta Core2, is the I2S master and the I2C controller. Postern is the I2S slave and the I2C peripheral. PCM arrives on one data pin. Postern sends it to headphones as an A2DP source.

This repository is that image ([D-0021](decisions.md#d-0021-image)). The pins and the I2C address are chosen when it is compiled ([D-0004](decisions.md#d-0004-compile-time-pins)). Another project reuses `components/postern` and flashes its own image.

AAC-LC is offered first. SBC is the codec that has to work when AAC is not available ([D-0006](decisions.md#d-0006-bluetooth-codecs)). A linked encoder is not a passed session ([P-0002](pitfalls.md#p-0002-aac-first)). The sample format is 16-bit stereo at 44.1 kHz or 48 kHz. Postern does not resample ([D-0005](decisions.md#d-0005-pcm-format)).
