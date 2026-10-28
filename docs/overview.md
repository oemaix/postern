# Overview

Status: baseline · 2026-10-28

Postern runs on the Bluetooth board. A peer, such as the Porta Core2, is the I2S master and the I2C controller. Postern is the I2S slave and the I2C peripheral. PCM arrives on one data pin. Postern sends it to headphones as an A2DP source.

The pins and the I2C address are chosen when the consuming application is compiled ([D-0004](decisions.md#d-0004-compile-time-pins)). This repository does not flash itself. The consuming project does ([D-0002](decisions.md#d-0002-component)).

AAC-LC is offered first. SBC is the codec that has to work when AAC is not available ([D-0006](decisions.md#d-0006-bluetooth-codecs)). A linked encoder is not a passed session ([P-0002](pitfalls.md#p-0002-aac-first)). The sample format is 16-bit stereo at 44.1 kHz or 48 kHz. Postern does not resample ([D-0005](decisions.md#d-0005-pcm-format)).
