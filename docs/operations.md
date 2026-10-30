# Operations

Status: baseline · 2026-10-30

## Devshell

```sh
nix develop
```

The shell provides git and `idf.py` for ESP-IDF v6.1 ([D-0022](decisions.md#d-0022-idf-in-the-shell)). CMake and Ninja for the firmware build are inside that `idf.py` sandbox. The host register-file check uses the compiler Nix puts on `PATH`. The shell does not provide a project Python, `uv`, or `pip` ([D-0019](decisions.md#d-0019-python-out-of-the-flake)). There is no `.venv`. A later third-party import gets a gitignored `.venv` from `uv`, and `pip` stays unused ([P-0008](pitfalls.md#p-0008-pip), [P-0028](pitfalls.md#p-0028-python-in-the-flake)). Do not add an ESP-IDF flake that is pinned to an older nixpkgs ([P-0006](pitfalls.md#p-0006-idf-flake)). The nixpkgs `esptool` package stays out. `idf.py` provides `esptool.py`.

Nix only sees files Git knows about. Stage `flake.nix` and `flake.lock` before the next `nix develop` if either file changed ([P-0005](pitfalls.md#p-0005-untracked-flake)).

## Image

From this shell, in the repository root ([D-0021](decisions.md#d-0021-image)):

```sh
idf.py set-target esp32
idf.py build
```

The first command installs ESP-IDF v6.1 for `esp32` under `~/.espressif` of the user who runs it. Later commands reuse that install. `idf.py` enters that install through bubblewrap and needs a mount namespace ([P-0032](pitfalls.md#p-0032-fhs-namespace)). The shell does not retarget that home ([P-0031](pitfalls.md#p-0031-install-home)). Flash with `idf.py -p` and the port chosen at that moment. This page does not name a port, a baud rate, or an offset. The container links every host `ttyUSB*` and `ttyACM*` node into `/dev` while it runs ([P-0033](pitfalls.md#p-0033-serial-device-node)).

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

The container is optional. The flake is still the environment, and the host shell is unchanged when Docker is never opened. Ubuntu 26.04 is the user session. Opening the folder asks which profile: **postern (NixOS host)**, **postern (Nix host)**, or **postern (Other host)**. NixOS and Nix bind the host store and do not install a second Nix. Other keeps a private Nix in the volume `devcontainer-nix`. Each profile adds `SYS_ADMIN` and `seccomp=unconfined` so `idf.py` can create its mount namespace. The host `/dev` is mounted at `/host-dev`. While the container runs, `serial-link.sh` links every `ttyUSB*` and `ttyACM*` node into `/dev` and does not open it. Majors 188 and 166 are allowed. On NixOS, import `.devcontainer/nixos-host/serial-udev.nix` so the session user can open the node.

## Editor

`.vscode/settings.json` defines **Nix Develop** as a login `zsh` that runs `nix develop`. The image installs that `zsh`. `/usr/local/bin/nix` runs the host client and does not put the NixOS userland on `PATH`.

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
