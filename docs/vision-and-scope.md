# Vision and scope

Status: baseline · 2026-10-28

## Aim

A board that already has an I2S master can gain Bluetooth headphones by compiling this component and wiring five signals. Porta is the first such board. The component does not assume Porta's GPIO numbers.

## In scope

- I2S slave receive, one direction.
- A2DP source, SBC first and AAC when the headphones negotiate it.
- A small I2C peripheral interface for sample rate, start, stop, and link status.
- Compile-time GPIO and address selection.

## Out of scope

- A screen, a button, or a product UI.
- Wi-Fi, Spotify, AirPlay, or DLNA.
- Resampling, and playback of any rate other than 44.1 kHz or 48 kHz.
- An application image in this repository.
- Choosing the peer's pins. Those belong to the peer.

## Phases

Phase 1 code receives PCM and transmits SBC, with the I2C control path. It has not been run on a board. AAC follows once SBC is stable. See [roadmap](roadmap.md).
