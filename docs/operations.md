# Operations

Status: baseline · 2026-10-28

## Devshell

```sh
nix develop
```

The shell provides git, Python 3, CMake, Ninja, and esptool. It does not provide `idf.py` or an Xtensa toolchain ([D-0010](decisions.md#d-0010-no-idf-yet)). Python is the flake's `python3` and the standard library. There is no `.venv` yet ([D-0015](decisions.md#d-0015-python), [P-0008](pitfalls.md#p-0008-pip)). Do not add an ESP-IDF flake that is pinned to an older nixpkgs ([P-0006](pitfalls.md#p-0006-idf-flake)).

Nix only sees files Git knows about. Stage `flake.nix` before `nix flake lock` ([P-0005](pitfalls.md#p-0005-untracked-flake)).

## Consuming the component

Point the application's `EXTRA_COMPONENT_DIRS` at this repository, or place the checkout where the application already collects components. The GPIO defaults are the Stamp Pico reference, and `POSTERN_I2C_ADDRESS` defaults to `0x1D`. The target is ESP32. Enable Classic Bluetooth and A2DP. Set `BT_A2DP_USE_EXTERNAL_CODEC`, `BT_A2DP_CODEC_AAC_ENABLED`, and `BT_A2DP_SEP_NUM_MAX` to at least 2 ([P-0018](pitfalls.md#p-0018-external-codec)). Leave `CONFIG_I2C_ISR_IRAM_SAFE` unset ([P-0013](pitfalls.md#p-0013-i2c-iram)). The component depends on `espressif/esp_audio_codec` 2.4; the consuming project's ESP-IDF build fetches it. Call `postern_init` from `app_main`. It returns `ESP_ERR_INVALID_STATE` while a GPIO is `-1` or the address is `0`. It calls `nvs_flash_init` and does not erase; on `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND` the application erases and calls again. Build and flash that application with ESP-IDF 6.1. This repository has no flashable image ([D-0002](decisions.md#d-0002-component)).

The register rules can be checked on the host, without the IDF:

```sh
gcc -std=c11 -Wall -Wextra -Werror -I include -I src \
  src/postern_file.c tests/postern_file_test.c -o /tmp/postern_file_test
/tmp/postern_file_test
```

## Devcontainer

The container is optional. `.devcontainer/Dockerfile` builds Ubuntu 26.04 and installs single-user Nix 2.35.4 for the `vscode` user ([D-0018](decisions.md#d-0018-devcontainer), [P-0026](pitfalls.md#p-0026-floating-nix-installer)). That user is the image's existing uid 1000 account, renamed ([P-0021](pitfalls.md#p-0021-ubuntu-uid-1000)). On start, `.devcontainer/entrypoint.sh` moves that account onto the workspace owner. The path is `DEVCONTAINER_WORKSPACE` ([P-0027](pitfalls.md#p-0027-checkout-directory-name)). `updateRemoteUserUID` stays off ([P-0023](pitfalls.md#p-0023-container-uid)).

| Config | Who |
| --- | --- |
| **Postern (Nix on Ubuntu)** | A host without NixOS. The Nix store stays in the image. |
| **Postern (NixOS host store)** | A NixOS host. The container bind-mounts that host's `/nix` read-only and uses its daemon ([P-0024](pitfalls.md#p-0024-host-nix-store), [P-0025](pitfalls.md#p-0025-one-devcontainer-for-every-host)). |

`sandbox = false` and `accept-flake-config = false` are image settings. A rebuild is required after an entrypoint or client change. Those two files are copied into the image.

## Editor

`.vscode/settings.json` defines **Nix Develop** as `nix develop` with `--command zsh`. zsh is a devshell package ([P-0022](pitfalls.md#p-0022-zsh-exit-status)). The image puts `/usr/local/bin` first on `PATH`, so `nix` in the container is the copied client.

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
