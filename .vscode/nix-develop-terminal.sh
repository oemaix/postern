#!/bin/sh
# Installed path: .vscode/nix-develop-terminal.sh
# Interactive terminal inside the flake devshell.
# Prefer zsh when the host has it; otherwise bash.
# Both shells are provided by the devshell, so `nix develop -c` can find them.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$root"
if command -v zsh >/dev/null 2>&1 || [ -x /usr/bin/zsh ] || [ -x /bin/zsh ]; then
  exec nix develop -c zsh
fi
exec nix develop -c bash
