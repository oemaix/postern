# Glossary

Status: baseline · 2026-10-28

| Term | Meaning |
| --- | --- |
| Postern | This component and this repository ([D-0001](decisions.md#d-0001-name)). |
| Peer | The other board. It is the I2S master and the I2C controller. |
| Consuming project | The ESP-IDF application that compiles Postern in and flashes the image. |
| I2S slave | Postern receives bit clock, word select, and data. It does not drive them. |
| A2DP source | The Bluetooth role that sends audio to headphones. |
| Kconfig | The ESP-IDF compile-time options in `Kconfig`. The application sets them in its sdkconfig. |
| SBC / AAC | Codecs on the Bluetooth link. SBC must work. AAC is the later goal ([D-0006](decisions.md#d-0006-bluetooth-codecs)). |
| Decision | A `D-xxxx` entry in [decisions](decisions.md). Ids are not reused. |
| Pitfall | A `P-xxxx` entry in [pitfalls](pitfalls.md). Open entries are binding. |
