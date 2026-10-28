#!/bin/sh
# Nix client for this container. The NixOS config bind-mounts the host
# /nix read-only, which hides the image profile's store path. Prefer the
# host system profile, then the default profile, then this user's profile.
# Do not take the first per-user profile on the machine (P-0024).
# NIX_CLIENT_ROOT and NIX_CLIENT_USER exist so .devcontainer/nix-client-test.sh
# can point this script at a fake tree.
# containerEnv keeps a newline as the two characters \ and n. Rewrite
# that so flakes stay enabled (P-0024).
set -u

if [ -n "${NIX_CONFIG:-}" ]; then
  NIX_CONFIG=$(printf '%s' "$NIX_CONFIG" | sed 's/\\n/\n/g')
  export NIX_CONFIG
fi

root=${NIX_CLIENT_ROOT:-}
user_name=${NIX_CLIENT_USER:-$(id -un 2>/dev/null || true)}
case "${user_name}" in
  ""|.|..|*/*|*[!a-zA-Z0-9._-]*) user_name="" ;;
esac

socket="${root}/nix/var/nix/daemon-socket/socket"
if [ -S "${socket}" ] && [ -z "${NIX_REMOTE+x}" ]; then
  NIX_REMOTE=daemon
  export NIX_REMOTE
fi

use() {
  bin=$1
  shift
  if [ -n "${bin}" ] && [ -x "${bin}" ]; then
    exec "${bin}" "$@"
  fi
}

use "${root}/nix/var/nix/profiles/system/sw/bin/nix" "$@"
use "${root}/nix/var/nix/profiles/default/bin/nix" "$@"
if [ -n "${user_name}" ]; then
  use "${root}/nix/var/nix/profiles/per-user/${user_name}/profile/bin/nix" "$@"
fi
use "${root}/home/vscode/.nix-profile/bin/nix" "$@"

echo "devcontainer: no Nix binary on the host store or in the image profile." >&2
exit 127
