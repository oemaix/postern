# Postern

Postern is the firmware for the M5Stamp Pico. It takes I2S from a peer that owns the clocks and sends that audio to Bluetooth headphones. This repository is that image, and it is developed on its own. The bridge logic is the component `components/postern`. Another ESP-IDF project reuses that directory and chooses its own pins.

**Status:** Phase 1 code is in the component: I2S slave, I2C register driver, and A2DP with AAC-LC and an SBC fallback. `main` calls `postern_init`. It has not been run on a board. The current phase is in [docs/roadmap.md](docs/roadmap.md).

## Documentation

| Document | Contents |
| --- | --- |
| [Overview](docs/overview.md) | What the firmware is |
| [Vision and scope](docs/vision-and-scope.md) | Aims and boundaries |
| [Glossary](docs/glossary.md) | Terms used in the suite |
| [Use cases](docs/use-cases.md) | What this image does, and how another project reuses it |
| [Requirements](docs/requirements.md) | Functional and non-functional requirements |
| [Architecture](docs/architecture.md) | Signals, pins, and the build |
| [Operations](docs/operations.md) | Devshell, container, editor, how to build and reuse |
| [Roadmap](docs/roadmap.md) | Phases and current status |
| [Decisions](docs/decisions.md) | Decided and undecided items (`D-xxxx`) |
| [Pitfalls](docs/pitfalls.md) | Traps to avoid (`P-xxxx`) |

There is no UI document. This firmware draws nothing ([D-0011](docs/decisions.md#d-0011-no-ui)).

## Development

The supported entry is the Nix flake:

```sh
nix develop
```

From that shell, in this directory:

```sh
idf.py set-target esp32
idf.py build
```

The default terminal profile **Nix Develop** runs the same shell. The container is optional. The flake is still the environment, and the host shell is unchanged. Details are in [docs/operations.md](docs/operations.md).

## Layout

```text
README.md
CMakeLists.txt              ESP-IDF project entry
sdkconfig.defaults          Stamp Pico target and A2DP options
main/                       app_main calls postern_init
components/postern/         reusable bridge component
  CMakeLists.txt
  Kconfig                   compile-time pins and I2C address
  include/postern.h
  include/postern_regs.h    I2C register file
  src/postern.c             I2S slave, I2C peripheral, A2DP
  src/postern_encode.c      AAC-LC and SBC encode task
  src/postern_file.c        register-file rules
  idf_component.yml         esp_audio_codec dependency
tests/postern_file_test.c   host check of those rules
docs/                       specification
flake.nix                   development shell
.devcontainer/              optional Ubuntu 26.04 session on the host Nix
.vscode/                    terminal profile and required extensions
.cursor/                    local Cursor rules and hook (not tracked)
```

The license is Apache 2.0.
