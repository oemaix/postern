# Postern

Postern is an ESP-IDF component. It takes I2S from a peer that owns the clocks and sends that audio to Bluetooth headphones. The first consumer is Porta. Any other ESP-IDF application can build it in and choose its own pins.

**Status:** Phase 1 code is in the component: I2S slave, I2C register driver, and A2DP SBC. It has not been run on a board. AAC is later. The current phase is in [docs/roadmap.md](docs/roadmap.md).

## Documentation

| Document | Contents |
| --- | --- |
| [Overview](docs/overview.md) | What the component is |
| [Vision and scope](docs/vision-and-scope.md) | Aims and boundaries |
| [Glossary](docs/glossary.md) | Terms used in the suite |
| [Use cases](docs/use-cases.md) | What a consuming project can do |
| [Requirements](docs/requirements.md) | Functional and non-functional requirements |
| [Architecture](docs/architecture.md) | Signals, pins, and the build |
| [Operations](docs/operations.md) | Devshell, container, editor, how to consume the component |
| [Roadmap](docs/roadmap.md) | Phases and current status |
| [Decisions](docs/decisions.md) | Decided and undecided items (`D-xxxx`) |
| [Pitfalls](docs/pitfalls.md) | Traps to avoid (`P-xxxx`) |

There is no UI document. This component draws nothing ([D-0011](docs/decisions.md#d-0011-no-ui)).

## Development

The supported entry is the Nix flake:

```sh
nix develop
```

The default terminal profile **Nix Develop** runs the same shell, using zsh when the host has zsh and bash otherwise. An optional Ubuntu 26.04 devcontainer installs Nix and opens the same shell. Details are in [docs/operations.md](docs/operations.md).

## Layout

```text
README.md
CMakeLists.txt         ESP-IDF component entry
Kconfig                compile-time pins and I2C address
include/postern.h
include/postern_regs.h  I2C register file
src/postern.c           I2S slave, I2C peripheral, A2DP SBC
src/postern_file.c      register-file rules
tests/postern_file_test.c  host check of those rules
docs/                  specification
flake.nix              development shell
.devcontainer/         optional Ubuntu LTS + Nix
.vscode/               terminal profile and required extensions
.cursor/               local Cursor rules and hook (not tracked)
```

The license is Apache 2.0.
