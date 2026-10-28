# Use cases

Status: baseline · 2026-10-28

The actor is a consuming project, not a listener. Playback behaviour is what that project's user hears.

## UC-01 Play the I2S stream

- **Phase:** 1
- **Preconditions:** The peer drives 16-bit stereo PCM at 44.1 kHz or 48 kHz. The build has set every GPIO and the I2C address. Headphones are paired as an A2DP sink.
- **Main flow:** PCM arrives on DIN. Headphones play it using SBC.
- **Extensions:** When the headphones and the encoder both support AAC, the link uses AAC ([D-0006](decisions.md#d-0006-bluetooth-codecs)).

## UC-02 Follow the peer's rate

- **Phase:** 1
- **Preconditions:** UC-01, and the peer may switch between 44.1 kHz and 48 kHz.
- **Main flow:** The peer writes the rate over I2C before the clocks change. Postern uses that rate and does not resample.

## UC-03 Build for another board

- **Phase:** 0
- **Preconditions:** The consuming project is an ESP-IDF application for an ESP32.
- **Main flow:** The project sets the five GPIOs and the I2C address in menuconfig, then builds. The same component sources are used.
