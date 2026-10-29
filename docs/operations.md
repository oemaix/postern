# Operations

Status: baseline · 2026-10-28

## Devshell

```sh
nix develop
```

The shell provides git, CMake, Ninja, host `gcc`, and `idf.py` for ESP-IDF v6.1 ([D-0022](decisions.md#d-0022-idf-in-the-shell)). It does not provide a project Python, `uv`, or `pip` ([D-0019](decisions.md#d-0019-python-out-of-the-flake)). There is no `.venv`. A later third-party import gets a gitignored `.venv` from `uv`, and `pip` stays unused ([P-0008](pitfalls.md#p-0008-pip), [P-0028](pitfalls.md#p-0028-python-in-the-flake)). Do not add an ESP-IDF flake that is pinned to an older nixpkgs ([P-0006](pitfalls.md#p-0006-idf-flake)). The nixpkgs `esptool` package stays out. `idf.py` provides `esptool.py`.

Nix only sees files Git knows about. Stage `flake.nix` and `flake.lock` before the next `nix develop` if either file changed ([P-0005](pitfalls.md#p-0005-untracked-flake)).

## Image

From this shell, in the repository root ([D-0021](decisions.md#d-0021-image)):

```sh
idf.py set-target esp32
idf.py build
```

The first command installs ESP-IDF v6.1 for `esp32` under `~/.espressif`. Later commands reuse that install. `idf.py` enters that install through bubblewrap and needs a mount namespace ([P-0032](pitfalls.md#p-0032-fhs-namespace)). A devcontainer install records `/root` in that tree. On the host, where that prefix is missing, the shell binds the login user's tree there for the ESP-IDF commands ([P-0031](pitfalls.md#p-0031-install-home)). Flash with `idf.py -p` and the port chosen at that moment. This page does not name a port, a baud rate, or an offset. The container links every host `ttyUSB*` and `ttyACM*` node into `/dev` while it runs ([P-0033](pitfalls.md#p-0033-serial-device-node)).

The register rules can be checked on the host, without the IDF. The binary goes under `tmp/`, which is scratch.

```sh
gcc -std=c11 -Wall -Wextra -Werror \
  -I components/postern/include -I components/postern/src \
  components/postern/src/postern_file.c tests/postern_file_test.c \
  -o tmp/postern-file-test
tmp/postern-file-test
```

## Reusing the component

Another ESP-IDF application sets `EXTRA_COMPONENT_DIRS` to `components/postern` in this checkout ([P-0030](pitfalls.md#p-0030-reuse-directory)). The target is ESP32. Enable Classic Bluetooth and A2DP. Set `BT_A2DP_USE_EXTERNAL_CODEC`, `BT_A2DP_CODEC_AAC_ENABLED`, and `BT_A2DP_SEP_NUM_MAX` to at least 2 ([P-0018](pitfalls.md#p-0018-external-codec)). Leave `CONFIG_I2C_ISR_IRAM_SAFE` unset ([P-0013](pitfalls.md#p-0013-i2c-iram)). The component depends on `espressif/esp_audio_codec` 2.4; that application's build fetches it. Call `postern_init` from `app_main`. It returns `ESP_ERR_INVALID_STATE` while a GPIO is `-1` or the address is `0`. It calls `nvs_flash_init` and does not erase; on `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND` the application erases and calls again. The GPIO defaults are the Stamp Pico reference, and `POSTERN_I2C_ADDRESS` defaults to `0x1D`. Override them when the board is not that module.

## Devcontainer

The container is optional. `.devcontainer/Dockerfile` builds Ubuntu 26.04 and installs single-user Nix 2.35.4 for the `vscode` user ([D-0018](decisions.md#d-0018-devcontainer), [P-0026](pitfalls.md#p-0026-floating-nix-installer)). That user is the image's existing uid 1000 account, renamed ([P-0021](pitfalls.md#p-0021-ubuntu-uid-1000)). On start, `.devcontainer/entrypoint.sh` moves that account onto the workspace owner. The path is `DEVCONTAINER_WORKSPACE` ([P-0027](pitfalls.md#p-0027-checkout-directory-name)). `updateRemoteUserUID` stays off ([P-0023](pitfalls.md#p-0023-container-uid)).

| Config | Who |
| --- | --- |
| **Postern (Nix on Ubuntu)** | A host without NixOS. The Nix store stays in the image. |
| **Postern (NixOS host store)** | A NixOS host. The container bind-mounts that host's `/nix` read-only and uses its daemon ([P-0024](pitfalls.md#p-0024-host-nix-store), [P-0025](pitfalls.md#p-0025-one-devcontainer-for-every-host)). |

Both configs add `SYS_ADMIN` and `seccomp=unconfined` so `idf.py` can create its mount namespace ([P-0032](pitfalls.md#p-0032-fhs-namespace)). Both bind the login user's `~/.espressif` on the host onto `/root/.espressif` ([P-0031](pitfalls.md#p-0031-install-home)). Both bind the host `/dev` at `/host-dev`. The entrypoint links every `ttyUSB*` and `ttyACM*` node there ([P-0033](pitfalls.md#p-0033-serial-device-node)). The host tty rule is `.devcontainer/nixos-host/serial.rules`. NixOS imports `.devcontainer/nixos-host/serial-udev.nix`. A rebuild is required before a running container sees those mounts and the entrypoint link.

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
| `ms-vscode.cpptools` | The firmware sources |
| `ms-vscode.vscode-serial-monitor` | Serial logs from this image |
| `ms-python.python` | The agent hook. The flake does not provide its interpreter ([D-0019](decisions.md#d-0019-python-out-of-the-flake)) |

The Espressif VS Code extension is not required. The shell already provides `idf.py` ([D-0022](decisions.md#d-0022-idf-in-the-shell)). That extension installs a second copy and would fight it.

## Agent hook

`.cursor/hooks.json` is local and gitignored. It rewrites agent shell commands into `nix develop` unless `IN_NIX_SHELL` is already set. The script is `.cursor/hooks/nix-develop.py`. It is standard-library Python. The process that launches it supplies `python3`. That interpreter is not a flake package ([D-0019](decisions.md#d-0019-python-out-of-the-flake)).

## Git

The remote is `https://github.com/oemaix/postern.git` ([D-0012](decisions.md#d-0012-no-remote)). Porta pins this repository as the submodule `postern` on branch `main`.
