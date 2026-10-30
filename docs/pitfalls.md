# Pitfalls

Status: baseline · 2026-10-30

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
- **Avoid:** Do not `useradd` at uid 1000 while the image account `ubuntu` still holds it. Do not leave the `users` group on gid 100.
- **Why:** `ubuntu:26.04` already has user and group `ubuntu` at 1000:1000. `groupadd --gid 1000` then exits 4 and the image build stops. The empty `users` group occupies gid 100, and `updateRemoteUserUID` refuses a gid that already exists, so the host primary group is never applied. `.devcontainer/Dockerfile` deletes the `ubuntu` account and the `users` group, then creates `dev`.

### P-0023 Container uid

- **Status:** Open
- **Avoid:** Do not turn `updateRemoteUserUID` off. Do not `chown` the workspace. Do not write `safe.directory` into the host gitconfig. Do not pass `$USER` to `postCreateCommand`.
- **Why:** `updateRemoteUserUID` rewrites `dev` to the host uid and primary gid ([D-0023](decisions.md#d-0023-devcontainer-profiles)). Rootless Docker shows that host user as uid 0 inside the mount. `.devcontainer/realize.sh` then runs `usermod -o` so `dev` shares uid 0 with root and the editor can save. The home stays `/home/dev`. `postCreateCommand` is not a login, so `$USER` is empty and sudo does not match. `git-identity.sh` writes a container-local `~/.gitconfig` that includes the host file and sets `safe.directory` to `/workspace`.

### P-0024 Host Nix store

- **Status:** Open
- **Avoid:** Do not `chown` a host store. Do not `chown -R` the private volume. Do not write `sandbox = false` into the host `nix.conf`. Do not prepend `/run/current-system/sw/bin` to the shell `PATH`. Do not take the first profile under `/nix/var/nix/profiles/per-user/`.
- **Why:** NixOS and Nix bind the host store and use its daemon ([D-0023](decisions.md#d-0023-devcontainer-profiles)). The store's objects belong to that daemon. Other chowns only the empty `/nix` directory so the installer can write it. `sandbox = false` is written only into the container home, and only when the daemon socket is absent. The image `/etc/nix/nix.conf` sets `experimental-features = nix-command flakes`. The host file is mounted over it when that file already sets `experimental-features`. `/usr/local/bin/nix` execs the NixOS client, then the default profile, then the per-user profile owned by this uid. It does not put the NixOS userland on `PATH`.

### P-0025 One devcontainer for every host

- **Status:** Open
- **Avoid:** Do not bind-mount the host `/nix` from **postern (Other host)**. Do not give the volume `devcontainer-nix` to **postern (NixOS host)** or **postern (Nix host)**.
- **Why:** A host with no `/nix` makes Docker create an empty directory when that path is bind-mounted, and the shell is gone ([D-0023](decisions.md#d-0023-devcontainer-profiles)). Other uses the named volume and installs a private Nix there. NixOS and Nix bind the host store and do not install a second Nix. `require-host.sh` refuses the wrong profile.

### P-0026 Floating Nix installer

- **Status:** Open
- **Avoid:** Do not install Nix in the image. Do not install Nix on the NixOS or Nix profiles.
- **Why:** Those profiles bind the host store ([D-0023](decisions.md#d-0023-devcontainer-profiles)). A second Nix there fights the host daemon. The Other profile runs `https://nixos.org/nix/install` with `--no-daemon` into the volume `devcontainer-nix`, and only when that volume has no profile for this uid. The image does not pin a Nix version.

### P-0027 Checkout directory name

- **Status:** Open
- **Avoid:** Do not hardcode `/workspaces/postern` or the host directory name. Do not leave the checkout on the default `/workspaces/<name>` mount.
- **Why:** `workspaceMount` bind-mounts the checkout at `/workspace`, which is also the image `WORKDIR` ([D-0023](decisions.md#d-0023-devcontainer-profiles)). `sudo` clears `containerWorkspaceFolder`, so the scripts cannot recover the path from the environment. A clone under another name still lands at `/workspace`.

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
- **Why:** [D-0022](decisions.md#d-0022-idf-in-the-shell). [hnaderi/espidf-nix](https://github.com/hnaderi/espidf-nix) puts `idf.py` on `PATH`. Espressif's installer writes v6.1 under `~/.espressif` the first time that command runs. The store holds the wrapper, not the chip tools. The installer writes absolute paths into the activation script and the virtualenv shebangs. That home is the home of the user who runs the command. In the container it is `/home/dev`, not the host home, and the shell does not bind one tree onto the other.

### P-0032 FHS namespace

- **Status:** Open
- **Avoid:** Do not drop [hnaderi/espidf-nix](https://github.com/hnaderi/espidf-nix) because bubblewrap failed in one container. Do not replace it with an ESP-IDF flake pinned to an older nixpkgs.
- **Why:** `idf.py` runs inside `buildFHSEnv`. That needs a new mount namespace. A container that forbids it stops at `bwrap: Creating new namespace failed`. Each profile adds `SYS_ADMIN` and `seccomp=unconfined` ([D-0022](decisions.md#d-0022-idf-in-the-shell)). A running container does not gain that until it is rebuilt. The wrapper is still the shell's `idf.py`.

### P-0033 Serial device node

- **Status:** Open
- **Avoid:** Do not `mknod` the serial node inside the container. Do not mount the host `/dev` on the container `/dev`. Do not bind one host device inode with `--device`. Do not `--group-add dialout`. Do not make the host nodes world-writable. Do not name a baud rate or a flash offset in `devcontainer.json`. Do not treat a `uaccess` tag on the parent USB device as access to the tty node. Do not link only `ttyUSB0` and `ttyACM0`. Do not open the host tty from the serial scan.
- **Why:** The bench nodes are `ttyUSB*` (major 188) and `ttyACM*` (major 166). Rootless Docker maps container uid 0 to the host user, then drops the host supplementary groups. `mknod` in that user namespace returns EPERM. A `--device` bind is one host inode and disappears when the cable is unplugged. `--group-add dialout` adds a container gid, not the host `dialout` gid. Each profile bind-mounts the host `/dev` at `/host-dev`. `.devcontainer/serial-link.sh` links each `ttyUSB*` and `ttyACM*` node into `/dev` and repeats the scan while the container runs. Opening the node from that scan toggles modem lines and resets the board. `idf.py` bind-mounts `/dev` and `/host-dev` when it starts. The device cgroup allows majors 188 and 166. Mode `0660` is not readable here until the host grants the session user. `.devcontainer/nixos-host/serial.rules` sets that mode, group `dialout`, and `TAG+="uaccess"` on every such node. NixOS imports `.devcontainer/nixos-host/serial-udev.nix`. A tag on the parent USB device does not do that. That script is copied into the image, so a running container stays on the old scan until it is rebuilt.

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

### P-0022 Zsh exit status

- **Status:** Resolved
- **Avoid:** Do not start the interactive devshell zsh with no startup file.
- **Why:** Resolved 2026-10-30. The flake does not set `ZDOTDIR` or `SHELL`, and `shell/.zshrc` is gone. The terminal starts a login `zsh`, then `nix develop`.

### P-0029 esptool in the flake

- **Status:** Resolved
- **Avoid:** Do not add esptool to this flake until a command in this shell flashes or inspects an image.
- **Why:** Resolved 2026-10-30. The image is flashed with `idf.py`, whose wrapper provides `esptool.py` ([D-0022](decisions.md#d-0022-idf-in-the-shell)). The nixpkgs `esptool` package stays out.
