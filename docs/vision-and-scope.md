# Vision and scope

Status: baseline · 2026-10-28

## Aim

A Stamp Pico that receives I2S from a peer can send that audio to Bluetooth headphones. This repository is that firmware. Porta is the first peer. The component does not assume Porta's GPIO numbers, so another project can compile `components/postern` for its own ESP32 board.

## In scope

- I2S slave receive, one direction.
- A2DP source, SBC first and AAC when the headphones negotiate it.
- A small I2C peripheral interface for sample rate, start, stop, and link status.
- Compile-time GPIO and address selection.
- The Stamp Pico image: `main` calls `postern_init`, and this repository flashes that image.

## Out of scope

- A screen, a button, or a product UI.
- Wi-Fi, Spotify, AirPlay, or DLNA.
- Resampling, and playback of any rate other than 44.1 kHz or 48 kHz.
- A second application beside `main/` ([P-0034](pitfalls.md#p-0034-one-image)).
- Choosing the peer's pins. Those belong to the peer.

## Phases

Phase 1 code receives PCM and transmits AAC-LC when the headphones negotiate it, with SBC as the fallback, and the I2C control path. It has not been run on a board. See [roadmap](roadmap.md).
