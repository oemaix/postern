# Glossary

Status: baseline · 2026-10-28

| Term | Meaning |
| --- | --- |
| Postern | This firmware and this repository ([D-0001](decisions.md#d-0001-name), [D-0021](decisions.md#d-0021-image)). |
| Peer | The other board. It is the I2S master and the I2C controller. |
| Reusing project | An ESP-IDF application that sets `EXTRA_COMPONENT_DIRS` to `components/postern` and flashes its own image ([P-0030](pitfalls.md#p-0030-reuse-directory)). |
| I2S slave | Postern receives bit clock, word select, and data. It does not drive them. |
| A2DP source | The Bluetooth role that sends audio to headphones. |
| Kconfig | The ESP-IDF compile-time options in `components/postern/Kconfig`. This image sets the Bluetooth symbols in `sdkconfig.defaults`. |
| SBC / AAC | Codecs on the Bluetooth link. AAC-LC is offered first. SBC must work when AAC is not available ([D-0006](decisions.md#d-0006-bluetooth-codecs)). |
| Decision | A `D-xxxx` entry in [decisions](decisions.md). Ids are not reused. |
| Pitfall | A `P-xxxx` entry in [pitfalls](pitfalls.md). Open entries are binding. |
