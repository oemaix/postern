# Operations

Status: baseline · 2026-10-28

## Devshell

```sh
nix develop
```

The shell provides git, Python 3, CMake, Ninja, and esptool. It does not provide `idf.py` or an Xtensa toolchain ([D-0010](decisions.md#d-0010-no-idf-yet)). Python is the flake's `python3` and the standard library. There is no `.venv` yet ([D-0015](decisions.md#d-0015-python), [P-0008](pitfalls.md#p-0008-pip)). Do not add an ESP-IDF flake that is pinned to an older nixpkgs ([P-0006](pitfalls.md#p-0006-idf-flake)).

Nix only sees files Git knows about. Stage `flake.nix` before `nix flake lock` ([P-0005](pitfalls.md#p-0005-untracked-flake)).

## Consuming the component

Point the application's `EXTRA_COMPONENT_DIRS` at this repository, or place the checkout where the application already collects components. Set the six **Postern** options in that application's sdkconfig. Build and flash that application. This repository has no flashable image ([D-0002](decisions.md#d-0002-component)).

## Devcontainer

`.devcontainer/` builds Ubuntu 26.04 and installs Nix for the `vscode` user. The image is a skeleton and has not been built here. `sandbox = false` is for the container only.

## Editor

The default terminal profile runs `.vscode/nix-develop-terminal.sh`, which prefers zsh and otherwise uses bash.

Required extensions:

| Extension | Why |
| --- | --- |
| `jnoortheen.nix-ide` | Nix flake |
| `editorconfig.editorconfig` | Shared indentation |
| `DavidAnson.vscode-markdownlint` | Markdown |
| `yzhang.markdown-all-in-one` | Markdown editing |
| `ms-vscode.cpptools` | The component sources |
| `ms-vscode.vscode-serial-monitor` | Serial logs from a consuming project |
| `ms-python.python` | The agent hook |

The Espressif VS Code extension is not required. It installs its own IDF copy.

## Agent hook

`.cursor/hooks.json` is local and gitignored. It rewrites agent shell commands into `nix develop` unless `IN_NIX_SHELL` is already set. The script is `.cursor/hooks/nix-develop.py`.

## Git

There is no network remote ([D-0012](decisions.md#d-0012-no-remote)). Porta records the local path `../postern`. That directory has to stay where the relative URL says it is until a remote exists.
