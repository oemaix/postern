# Use cases

Status: baseline · 2026-10-28

The actor for UC-01 and UC-02 is this image on the Stamp Pico. UC-03 is another project reusing the component. Playback behaviour is what the listener hears.

## UC-01 Play the I2S stream

- **Phase:** 1
- **Preconditions:** The peer drives 16-bit stereo PCM at 44.1 kHz or 48 kHz. The GPIO and address defaults match the Stamp Pico unless a reusing project overrides them. Postern pages the last address that connected. If there is none, or it does not answer, a sink is discoverable and Postern connects to the first device whose class of device includes rendering.
- **Main flow:** PCM arrives on DIN. Headphones play it using SBC.
- **Extensions:** When the headphones and the encoder both support AAC, the link uses AAC ([D-0006](decisions.md#d-0006-bluetooth-codecs)).

## UC-02 Follow the peer's rate

- **Phase:** 1
- **Preconditions:** UC-01, and the peer may switch between 44.1 kHz and 48 kHz.
- **Main flow:** The peer writes four rate bytes at offset `0x04`, then start at `0x02`. Postern uses that rate and does not resample.

## UC-03 Build for another board

- **Phase:** 0
- **Preconditions:** The reusing project is an ESP-IDF application for an ESP32. `EXTRA_COMPONENT_DIRS` points at `components/postern`.
- **Main flow:** The project changes the five GPIOs and the address when the board is not the Stamp Pico reference, sets the Bluetooth symbols from `sdkconfig.defaults`, then builds. The same component sources are used.
