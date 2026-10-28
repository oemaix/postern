#!/usr/bin/env bash
# Once per create, after entrypoint.sh has moved vscode onto the
# workspace owner. Then fill the Nix store. This does not mark the
# repo safe.directory: that line lives in ~/.gitconfig and a rebuild
# drops it (P-0023).

set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "${root}"

ready=/run/devcontainer-uid-ready

if [ -x /usr/local/bin/devcontainer-entrypoint ]; then
  for _ in $(seq 1 120); do
    if [ -f "${ready}" ]; then
      break
    fi
    sleep 0.5
  done
  if [ ! -f "${ready}" ]; then
    echo "devcontainer: timed out waiting for uid alignment." >&2
    exit 1
  fi
  owner=$(stat -c %u "${root}")
  if [ "$(id -u)" != "${owner}" ]; then
    if [ -n "${POSTERN_DEVCONTAINER_REEXEC:-}" ]; then
      echo "devcontainer: still uid $(id -u) after re-exec; workspace owner is ${owner}." >&2
      exit 1
    fi
    export POSTERN_DEVCONTAINER_REEXEC=1
    # sudo resets the environment. NIX_CONFIG is the durable feature
    # switch for this process; dropping it disables flakes (P-0024).
    exec sudo -n \
      --preserve-env=POSTERN_DEVCONTAINER_REEXEC,NIX_CONFIG,NIX_REMOTE \
      -u vscode \
      bash -c 'cd "$1" && exec bash "$2"' _ "${root}" "$0"
  fi
fi

exec /usr/local/bin/nix develop --command true
