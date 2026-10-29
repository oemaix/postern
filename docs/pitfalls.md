# Pitfalls

Status: baseline · 2026-10-28

Open entries are binding. Do not delete an entry. Mark it resolved, with the reason, when it stops applying. New entries take the next id and are added in the same change that discovered them.

Agents read this file before design or implementation work, and they add to it when a new trap shows up. The Cursor rule is only a reminder. This file is the copy that stays in git.

## Open

### P-0001 Peer pins

- **Status:** Open
- **Avoid:** Do not copy the peer's GPIO numbers into Postern's Kconfig as if they were the same pins.
- **Why:** The numbers in a product cable table name the peer. Postern's symbols name the chip this component runs on. On Porta the peer numbers are Core2 GPIOs: GPIO32 and GPIO33 for I2C, GPIO14, GPIO13, and GPIO26 for I2S. The Stamp Pico defaults are GPIO21, GPIO22, GPIO18, GPIO19, and GPIO36 ([D-0017](decisions.md#d-0017-reference-pins)). Core2 GPIO26 and Stamp Pico GPIO26 are not the same net.

### P-0002 AAC first

- **Status:** Open
- **Avoid:** Do not treat AAC as the first Bluetooth codec to bring up.
- **Why:** The reference module has no PSRAM. SBC is the session that has to pass ([D-0006](decisions.md#d-0006-bluetooth-codecs)). Checked 2026-10-28: ESP-IDF 6.1 can register an AAC source endpoint on an ESP32, using an external AAC-LC encoder of about 51 KB of heap for 48 kHz stereo. The component now encodes with that library. That has not been measured on the Stamp Pico after the Bluetooth stack is up. An encoder in the tree is not a passed session.

### P-0004 No resample

- **Status:** Open
- **Avoid:** Do not resample on this chip.
- **Why:** [D-0005](decisions.md#d-0005-pcm-format) leaves 44.1 kHz and 48 kHz as the only rates. The peer converts anything else before the cable.

### P-0005 Untracked flake

- **Status:** Open
- **Avoid:** Do not run `nix flake lock` or `nix develop` while `flake.nix` is untracked.
- **Why:** In a Git checkout Nix copies only the files Git knows. Stage the flake first.

### P-0006 IDF flake

- **Status:** Open
- **Avoid:** Do not import a third-party ESP-IDF Nix flake that is pinned to an older nixpkgs.
- **Why:** This shell is nixpkgs 26.05 ([D-0009](decisions.md#d-0009-nixpkgs)). `idf.py` comes from [hnaderi/espidf-nix](https://github.com/hnaderi/espidf-nix) built with this pin ([D-0022](decisions.md#d-0022-idf-in-the-shell)). A flake pinned to an older nixpkgs breaks on this pin.

### P-0007 Unset config

- **Status:** Open
- **Avoid:** Do not treat GPIO `-1` or I2C address `0x00` as a real wiring, and do not hard-code the address in `src/`.
- **Why:** Those values mean unset ([D-0004](decisions.md#d-0004-compile-time-pins)). `postern_load_config` returns false for them. The reference address is `0x1D` in Kconfig ([D-0016](decisions.md#d-0016-i2c-address)). The register offsets are already fixed ([D-0014](decisions.md#d-0014-i2c-map)).

### P-0008 pip

- **Status:** Open
- **Avoid:** Do not `pip install` into the devshell, a user site, or a project `.venv`.
- **Why:** [D-0019](decisions.md#d-0019-python-out-of-the-flake). A `.venv`, when one is required, is created with `uv`. That directory must not be the Python tree ESP-IDF installs for `idf.py`. The flake does not carry `uv` until that day ([P-0028](#p-0028-python-in-the-flake)).

### P-0009 Partial rate

- **Status:** Open
- **Avoid:** Do not apply a sample rate unless one write starts at `0x04` and contains all four bytes, and the value is `44100` or `48000`.
- **Why:** [D-0014](decisions.md#d-0014-i2c-map). A shorter write would store a torn Hertz value. Other rates are refused ([D-0005](decisions.md#d-0005-pcm-format)).

### P-0010 Not an S3

- **Status:** Open
- **Avoid:** Do not set the Postern target to ESP32-S3 or to a C or H series chip.
- **Why:** Those chips have no Bluetooth Classic, so they cannot open an A2DP source. The target stays ESP32 ([D-0008](decisions.md#d-0008-target)).

### P-0011 Rate while running

- **Status:** Open
- **Avoid:** Do not apply a new sample rate to a stream that is already running.
- **Why:** A2DP has already negotiated the codec, and this chip does not resample ([P-0004](#p-0004-no-resample)). A different legal rate written while running sets the rate-rejected bit and keeps the rate the stream started with. Stop, write the four rate bytes, then start.

### P-0012 First sink

- **Status:** Open
- **Avoid:** Do not treat inquiry as a device picker.
- **Why:** The register file has no Bluetooth address ([D-0014](decisions.md#d-0014-i2c-map)). The first session, and any session whose saved address does not answer, connects to the first discoverable device whose class of device includes rendering. A later boot pages the last address that reached A2DP connected, then falls back to that inquiry. Two sinks in the room can still attach the wrong one on the first session.

### P-0013 I2C IRAM

- **Status:** Open
- **Avoid:** Do not set `CONFIG_I2C_ISR_IRAM_SAFE` in this image or in a project that reuses the component.
- **Why:** The register callbacks run from the I2C interrupt and call `i2c_slave_write`. They are not placed in IRAM.

### P-0015 Open pairing

- **Status:** Open
- **Avoid:** Do not treat pairing as confirmed by a person.
- **Why:** There is no display ([D-0011](decisions.md#d-0011-no-ui)). Legacy pairing answers PIN `1234`. SSP Just Works is accepted in the callback. Inquiry can bond the first rendering device that answers ([P-0012](#p-0012-first-sink)).

### P-0016 I2C read

- **Status:** Open
- **Avoid:** Do not answer an I2C read one byte at a time, and do not leave unread bytes in the slave TX ring buffer.
- **Why:** On the ESP32 the request callback runs once per read. Bytes that spill into the ring buffer are sent ahead of the next reply. The register file is eight bytes and fits in the hardware FIFO.

### P-0017 A2DP callback

- **Status:** Open
- **Avoid:** Do not block inside the A2DP source data callback.
- **Why:** That callback runs on the Bluetooth task. A wait for I2S, while the peer has not started the bit clock, stalls the controller. Encoding runs on its own task. That task's I2S read is bounded. Missed samples are silence.

### P-0018 External codec

- **Status:** Open
- **Avoid:** Do not leave `BT_A2DP_USE_EXTERNAL_CODEC` unset, and do not register AAC without an SBC endpoint.
- **Why:** The flag removes Bluedroid's built-in SBC encoder. The PCM data callback then has nothing to feed. Postern registers AAC-LC at SEID 0 and SBC at SEID 1, encodes with `esp_audio_codec`, and sends frames with `esp_a2d_source_audio_data_send`. AAC frames are raw, without ADTS; the stack adds LATM, so a frame has to fit in the MTU minus 20 bytes. The AAC encoder is opened only after that codec is negotiated, and a failure falls back to SBC. A negotiated AAC bitrate the encoder cannot send inside the MTU ends that AAC attempt and falls back to SBC. The control task does not wait for the encoder to open. The codec table's 40 KB task stack is what this task requests. This path has not been measured on the Stamp Pico ([P-0002](#p-0002-aac-first)).

### P-0019 Encoder task

- **Status:** Open
- **Avoid:** Do not delete the encoder task, and do not close its encoder from another task.
- **Why:** `esp_audio_enc_open` and `i2s_channel_read` cannot be aborted. `vTaskDelete` during those calls can corrupt the heap or the I2S driver. The task checks the stop flag after each call and frees the encoder itself. The I2S channel stays enabled if that task has not left the driver.

### P-0020 Input-only DIN

- **Status:** Open
- **Avoid:** Do not use GPIO36 as an I2S data output, and do not treat reserved GPIO26 as a pin this component already drives.
- **Why:** On the ESP32, GPIO36 is input-only. The Stamp Pico reference uses it for DIN ([D-0017](decisions.md#d-0017-reference-pins)). GPIO26 can drive a later headset-microphone output. That path is not in the component, so nothing reads or drives the pin.

### P-0021 Ubuntu uid 1000

- **Status:** Open
- **Avoid:** Do not create the `vscode` user with `groupadd` or `useradd` at uid 1000.
- **Why:** `ubuntu:26.04` already has user and group `ubuntu` at 1000:1000. `groupadd --gid 1000` exits 4, and the devcontainer image build stops before Nix is installed. `.devcontainer/Dockerfile` renames that account to `vscode` and keeps its groups, including `dialout`.

### P-0022 Zsh exit status

- **Status:** Open
- **Avoid:** Do not start the interactive devshell zsh with no startup file.
- **Why:** Nix's zsh loads `zsh/newuser` when `.zshrc` is missing. As uid 0 that installer returns 1, so a following `exit` closes the terminal with status 1. The devshell sets `ZDOTDIR` to `shell/`, whose `.zshrc` shows the working directory and ends successfully. A NixOS host still sources `/etc/zshrc` after that. That file installs `command_not_found_handler`, and the handler returns 127. The devcontainer image does not have this rc. `shell/.zshenv` turns global rc files off, so a following `exit` is not that status. `/etc/zshenv` still runs. The devshell also sets `SHELL` to its own zsh. Nix's setup would otherwise leave `SHELL` as the non-interactive bash from the build environment.

### P-0023 Container uid

- **Status:** Open
- **Avoid:** Do not set `safe.directory` so `nix develop` will open this checkout. Do not turn `updateRemoteUserUID` on. Do not leave the running `vscode` user at the image uid when the workspace owner inside the container is different.
- **Why:** Rootless Docker maps the host user to container uid 0. `updateRemoteUserUID` copies the host number into the image, so `vscode` does not own the workspace, and `postCreateCommand` stops with libgit2 error 7. `.devcontainer/entrypoint.sh` reads the owner inside the container and moves `vscode` onto it. When that owner is uid 0, the session home is `/root`. A `safe.directory` line in `~/.gitconfig` is lost on rebuild and does not fix the written uid.

### P-0024 Host Nix store

- **Status:** Open
- **Avoid:** Do not `chown` `/nix` when it is a bind mount of the host store. Do not mount the host store read-write. Do not put a newline in `NIX_CONFIG`. Do not take the first profile under `/nix/var/nix/profiles/per-user/`. Do not leave `accept-flake-config` on.
- **Why:** After the uid move the session home can be `/root`, so a home `nix.conf` is not the file Nix reads. The NixOS config bind-mounts the host `/nix` read-only. `NIX_CONFIG` is one line. A newline in `containerEnv` arrives as `\` and `n`, and `.devcontainer/nix.sh` rewrites that sequence. `/etc/nix/nix.conf` sets `accept-flake-config = false` and `sandbox = false`. The wrapper uses the host daemon when its socket is present, and selects the system profile, then the default profile, then the current user's profile.

### P-0025 One devcontainer for every host

- **Status:** Open
- **Avoid:** Do not bind-mount the host `/nix` from **Postern (Nix on Ubuntu)**. Do not send a host that has no `/nix` through **Postern (NixOS host store)**.
- **Why:** The mount hides the Nix store installed in the image. On a host with no `/nix`, Docker creates an empty directory and the image `nix` is gone ([D-0018](decisions.md#d-0018-devcontainer)).

### P-0026 Floating Nix installer

- **Status:** Open
- **Avoid:** Do not install Nix from `https://nixos.org/nix/install`.
- **Why:** That URL is the current installer. A later image rebuild can change the Nix version. `.devcontainer/Dockerfile` pins Nix 2.35.4 ([D-0018](decisions.md#d-0018-devcontainer)).

### P-0027 Checkout directory name

- **Status:** Open
- **Avoid:** Do not hardcode `/workspaces/postern` in the entrypoint or the image `WORKDIR`.
- **Why:** Dev Containers mounts the checkout at `/workspaces/` plus the directory name. A clone under another name leaves `vscode` on the image uid ([P-0023](#p-0023-container-uid)). Both configs set `DEVCONTAINER_WORKSPACE` to `${containerWorkspaceFolder}`.

### P-0028 Python in the flake

- **Status:** Open
- **Avoid:** Do not add `python3`, `uv`, or `pip` to the Postern flake until a Postern script must import a third-party package. Do not treat the agent hook or `idf.py` as that need.
- **Why:** [D-0019](decisions.md#d-0019-python-out-of-the-flake). `python3` and `uv` left the flake because this repository does not import them. `idf.py` keeps Python 3.13 inside `~/.espressif` ([D-0022](decisions.md#d-0022-idf-in-the-shell)). The hook is a standard-library script started by the host `python3`. When a third-party import arrives, add `uv` and create a gitignored `.venv` with it. `pip` stays out ([P-0008](#p-0008-pip)).

### P-0030 Reuse directory

- **Status:** Open
- **Avoid:** Do not point `EXTRA_COMPONENT_DIRS` at the repository root. Do not run `idf.py` inside `components/postern`.
- **Why:** The root `CMakeLists.txt` is the application ([D-0021](decisions.md#d-0021-image)). `idf.py build` runs there. The reusable component is `components/postern`. A reusing project sets `EXTRA_COMPONENT_DIRS` to that directory. The root file is the project, so a directory that contains it is one component and the search does not walk into `components/postern`.

### P-0031 Install home

- **Status:** Open
- **Avoid:** Do not expect one `~/.espressif` tree to work under a different home. Do not commit `~/.espressif` or `~/.cache/espidf-nix`. Do not rewrite the activation script to one absolute path. Do not point `IDF_VERSION` at an unpinned tag.
- **Why:** [D-0022](decisions.md#d-0022-idf-in-the-shell). [hnaderi/espidf-nix](https://github.com/hnaderi/espidf-nix) puts `idf.py` on `PATH`. Espressif's installer writes v6.1 under `~/.espressif` the first time that command runs. The store holds the wrapper, not the chip tools. The installer writes absolute paths into the activation script and the virtualenv shebangs. In the devcontainer that home is `/root` ([P-0023](#p-0023-container-uid)). The bind mount stores those files in the login user's `~/.espressif`. On the host the `/root` paths are not that tree. The devshell puts wrappers ahead of `idf.py`. When the user's tree exists and `/root/.espressif` does not, the wrapper bind-mounts the tree at `/root/.espressif` for that command. The container already has `/root/.espressif`, so the wrapper runs the real command there.

### P-0032 FHS namespace

- **Status:** Open
- **Avoid:** Do not drop [hnaderi/espidf-nix](https://github.com/hnaderi/espidf-nix) because bubblewrap failed in one container. Do not replace it with an ESP-IDF flake pinned to an older nixpkgs.
- **Why:** `idf.py` runs inside `buildFHSEnv`. That needs a new mount namespace. A container that forbids it stops at `bwrap: Creating new namespace failed`. Both devcontainers add `SYS_ADMIN` and `seccomp=unconfined` ([D-0022](decisions.md#d-0022-idf-in-the-shell)). A running container does not gain that until it is rebuilt. The wrapper is still the shell's `idf.py`.

### P-0033 Serial device node

- **Status:** Open
- **Avoid:** Do not `mknod` the serial node inside the container. Do not mount the host `/dev` on the container `/dev`. Do not bind one host device inode with `--device`. Do not make the host nodes world-writable. Do not name a baud rate or a flash offset in `devcontainer.json`. Do not treat a `uaccess` tag on the parent USB device as access to the tty node. Do not link only `ttyUSB0` and `ttyACM0`. Do not open the host tty from the entrypoint scan.
- **Why:** The bench nodes are `ttyUSB*` (major 188) and `ttyACM*` (major 166). Rootless Docker maps container uid 0 to the host user. `mknod` in that user namespace returns EPERM. A `--device` bind is one host inode and disappears when the cable is unplugged. Both configs bind the host `/dev` at `/host-dev`. The entrypoint links each `ttyUSB*` and `ttyACM*` node into `/dev` and repeats the scan while the container runs. Opening the node from that scan toggles modem lines and resets the board. `idf.py` bind-mounts `/dev` and `/host-dev` when it starts. The device cgroup allows majors 188 and 166. Host group `dialout` is not a gid in this map, and mode `0660` is not readable here until the host grants the session user. `.devcontainer/nixos-host/serial.rules` sets that mode, group `dialout`, and `TAG+="uaccess"` on every such node. NixOS imports `.devcontainer/nixos-host/serial-udev.nix`. A tag on the parent USB device does not do that. The entrypoint is copied into the image, so a running container stays on the old scan until it is rebuilt.

### P-0034 One image

- **Status:** Open
- **Avoid:** Do not add a second application, an example image, or another `main/` beside the repository root.
- **Why:** This repository is the Stamp Pico image ([D-0021](decisions.md#d-0021-image)). Another `main` becomes a second product. Another project reuses `components/postern` ([P-0030](#p-0030-reuse-directory)).

## Resolved

### P-0014 External codec

- **Status:** Resolved
- **Avoid:** Do not set `BT_A2DP_USE_EXTERNAL_CODEC` for the built-in SBC phase.
- **Why:** Resolved 2026-10-28. AAC needs that flag, so the built-in encoder is no longer the path. The binding rule is [P-0018](#p-0018-external-codec).

### P-0003 No application

- **Status:** Resolved
- **Avoid:** Do not add `main/`, a partition table, or a flashable example in this repository.
- **Why:** Resolved 2026-10-30. [D-0021](decisions.md#d-0021-image) makes this repository the Stamp Pico image, so `main/` is the image entry. The binding rule is [P-0034](#p-0034-one-image).

### P-0029 esptool in the flake

- **Status:** Resolved
- **Avoid:** Do not add esptool to this flake until a command in this shell flashes or inspects an image.
- **Why:** Resolved 2026-10-30. The image is flashed with `idf.py`, whose wrapper provides `esptool.py` ([D-0022](decisions.md#d-0022-idf-in-the-shell)). The nixpkgs `esptool` package stays out.
