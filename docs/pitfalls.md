# Pitfalls

Status: baseline · 2026-10-28

Open entries are binding. Do not delete an entry. Mark it resolved, with the reason, when it stops applying. New entries take the next id and are added in the same change that discovered them.

Agents read this file before design or implementation work, and they add to it when a new trap shows up. The Cursor rule is only a reminder. This file is the copy that stays in git.

## Open

### P-0001 Peer pins

- **Status:** Open
- **Avoid:** Do not copy the peer's GPIO numbers into Postern's Kconfig as if they were the same pins.
- **Why:** The numbers in a product cable table name the peer. Postern's symbols name the chip this component runs on. On Porta those peer numbers are Core2 GPIOs. The Stamp Pico assignment is still chosen by the consuming project.

### P-0002 AAC first

- **Status:** Open
- **Avoid:** Do not treat AAC as the first Bluetooth codec to bring up.
- **Why:** The reference module has no PSRAM. SBC is the session that has to pass ([D-0006](decisions.md#d-0006-bluetooth-codecs)). An AAC encoder in the tree is not proof that an A2DP source session negotiates AAC.

### P-0003 No application

- **Status:** Open
- **Avoid:** Do not add `main/`, a partition table, or a flashable example in this repository.
- **Why:** [D-0002](decisions.md#d-0002-component) keeps the image in the consuming project. An example here would become a second product.

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
- **Why:** This shell is nixpkgs 26.05 ([D-0009](decisions.md#d-0009-nixpkgs)). Those flakes currently break on that pin. `idf.py` arrives in a later change that can build against it ([D-0010](decisions.md#d-0010-no-idf-yet)).

### P-0007 Unset config

- **Status:** Open
- **Avoid:** Do not treat GPIO `-1` or I2C address `0x00` as a real wiring, and do not pick a default address in code.
- **Why:** Those values mean the consuming project has not chosen ([D-0004](decisions.md#d-0004-compile-time-pins)). `postern_load_config` returns false for them. The address itself is [D-0014](decisions.md#d-0014-i2c-map).

## Resolved

None yet.
