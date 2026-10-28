#!/bin/sh
# Checks the Nix client selection and the read-only host-store mount.
# Run from the repo: sh .devcontainer/nix-client-test.sh
set -eu

here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
client=${here}/nix.sh
fail=0

bad() {
  printf 'FAIL %s\n' "$*" >&2
  fail=1
}

if ! grep -q 'source=/nix,target=/nix,type=bind,readonly' "${here}/nixos-host/devcontainer.json"; then
  bad "nixos-host/devcontainer.json does not mount /nix read-only"
fi
if grep -q 'source=/nix' "${here}/devcontainer.json"; then
  bad "the Ubuntu config bind-mounts /nix"
fi
if ! grep -q 'Postern (Nix on Ubuntu)' "${here}/devcontainer.json"; then
  bad "the Ubuntu config has no picker name"
fi
if ! grep -q 'Postern (NixOS host store)' "${here}/nixos-host/devcontainer.json"; then
  bad "the NixOS config has no picker name"
fi

if printf '%s\n' 'dev /nix/store btrfs ro 0 0' \
  | awk '$2 == "/nix" { found=1 } END { exit !found }'
then
  bad "mount check treated /nix/store as /nix"
fi
if ! printf '%s\n' 'dev /nix btrfs rw 0 0' \
  | awk '$2 == "/nix" { found=1 } END { exit !found }'
then
  bad "mount check missed /nix"
fi

if ! grep -q 'preserve-env=POSTERN_DEVCONTAINER_REEXEC,NIX_CONFIG,NIX_REMOTE' \
  "${here}/post-create.sh"
then
  bad "post-create.sh does not keep NIX_CONFIG"
fi
if ! grep -q 'releases.nixos.org/nix/nix-' "${here}/Dockerfile"; then
  bad "Dockerfile does not pin the Nix installer"
fi
if grep -q 'https://nixos.org/nix/install' "${here}/Dockerfile"; then
  bad "Dockerfile uses the floating Nix installer"
fi
if ! grep -q 'accept-flake-config = false' "${here}/Dockerfile"; then
  bad "image nix.conf does not set accept-flake-config false"
fi
if grep -E -q '/workspaces/(porta|postern)' "${here}/entrypoint.sh" "${here}/Dockerfile"; then
  bad "workspace path is hardcoded"
fi
if ! grep -q 'DEVCONTAINER_WORKSPACE' "${here}/entrypoint.sh"; then
  bad "entrypoint does not read DEVCONTAINER_WORKSPACE"
fi
for cfg in "${here}/devcontainer.json" "${here}/nixos-host/devcontainer.json"; do
  if ! grep -q '"DEVCONTAINER_WORKSPACE": "${containerWorkspaceFolder}"' "${cfg}"; then
    bad "${cfg} does not pass the container workspace path"
  fi
  if grep -q 'terminal.integrated.profiles' "${cfg}"; then
    bad "${cfg} repeats terminal profiles; they belong in .vscode/settings.json"
  fi
done
if ! grep -q -F 's/\\n/\n/g' "${client}"; then
  bad "wrapper does not rewrite a literal backslash-n in NIX_CONFIG"
fi

list_ids() {
  file=$1
  key=$2
  awk -v key="${key}" '
    index($0, "\"" key "\"") { grab = 1; next }
    grab && index($0, "]") { exit }
    grab {
      if (match($0, /"[^"]+"/)) print substr($0, RSTART + 1, RLENGTH - 2)
    }
  ' "${file}" | sort
}
ext_recommended=$(list_ids "${here}/../.vscode/extensions.json" recommendations)
ext_ubuntu=$(list_ids "${here}/devcontainer.json" extensions)
ext_nixos=$(list_ids "${here}/nixos-host/devcontainer.json" extensions)
if [ -z "${ext_recommended}" ] || [ "${ext_recommended}" != "${ext_ubuntu}" ] \
  || [ "${ext_recommended}" != "${ext_nixos}" ]
then
  bad "extension lists differ from .vscode/extensions.json"
fi
if ! grep -q '"Nix Develop"' "${here}/../.vscode/settings.json"; then
  bad "settings.json has no Nix Develop profile"
fi

tmp=$(mktemp -d)
trap 'rm -rf "${tmp}"' EXIT

make_nix() {
  path=$1
  mkdir -p "$(dirname "${path}")"
  cat > "${path}" << EOF
#!/bin/sh
printf '%s\n' "\$0" "\${NIX_REMOTE-unset}" > "${tmp}/out"
EOF
  chmod 755 "${path}"
}

run_client() {
  rm -f "${tmp}/out"
  # shellcheck disable=SC2086
  env "$@" "${client}" --version
  if [ ! -f "${tmp}/out" ]; then
    bad "client produced no result for: $*"
    printf '%s\n' "" ""
    return 0
  fi
  cat "${tmp}/out"
}

expect() {
  label=$1
  want_bin=$2
  want_remote=$3
  shift 3
  result=$(run_client "$@")
  got_bin=$(printf '%s\n' "${result}" | sed -n '1p')
  got_remote=$(printf '%s\n' "${result}" | sed -n '2p')
  if [ "${got_bin}" != "${want_bin}" ] || [ "${got_remote}" != "${want_remote}" ]; then
    bad "${label}: got ${got_bin} remote=${got_remote}"
  fi
}

base=${tmp}/root
system=${base}/nix/var/nix/profiles/system/sw/bin/nix
default=${base}/nix/var/nix/profiles/default/bin/nix
alice=${base}/nix/var/nix/profiles/per-user/alice/profile/bin/nix
bob=${base}/nix/var/nix/profiles/per-user/bob/profile/bin/nix
image=${base}/home/vscode/.nix-profile/bin/nix

make_nix "${system}"
make_nix "${bob}"
expect "system profile wins over another user" "${system}" unset \
  -u NIX_REMOTE NIX_CLIENT_ROOT="${base}" NIX_CLIENT_USER=alice

rm -rf "${base}"
make_nix "${alice}"
make_nix "${bob}"
expect "current user wins over another per-user profile" "${alice}" unset \
  -u NIX_REMOTE NIX_CLIENT_ROOT="${base}" NIX_CLIENT_USER=alice

rm -rf "${base}"
make_nix "${bob}"
make_nix "${image}"
expect "foreign per-user profile is not used" "${image}" unset \
  -u NIX_REMOTE NIX_CLIENT_ROOT="${base}" NIX_CLIENT_USER=alice

rm -rf "${base}"
make_nix "${default}"
make_nix "${alice}"
expect "default profile wins over the current user" "${default}" unset \
  -u NIX_REMOTE NIX_CLIENT_ROOT="${base}" NIX_CLIENT_USER=alice

rm -rf "${base}"
make_nix "${system}"
mkdir -p "$(dirname "${base}/nix/var/nix/daemon-socket/socket")"
if ! command -v python3 >/dev/null 2>&1; then
  bad "python3 is required to create the daemon socket"
else
  python3 -c 'import socket,sys; s=socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); s.bind(sys.argv[1]); s.listen(1); s.close()' \
    "${base}/nix/var/nix/daemon-socket/socket"
  expect "daemon socket selects NIX_REMOTE" "${system}" daemon \
    -u NIX_REMOTE NIX_CLIENT_ROOT="${base}" NIX_CLIENT_USER=alice
  expect "an explicit NIX_REMOTE is kept" "${system}" local \
    NIX_CLIENT_ROOT="${base}" NIX_CLIENT_USER=alice NIX_REMOTE=local
fi

rewrite=${base}/nix/var/nix/profiles/system/sw/bin/nix
mkdir -p "$(dirname "${rewrite}")"
cat > "${rewrite}" << EOF
#!/bin/sh
printf '%s' "\${NIX_CONFIG-}" > "${tmp}/cfg"
EOF
chmod 755 "${rewrite}"
rm -f "${tmp}/cfg"
env -u NIX_REMOTE NIX_CLIENT_ROOT="${base}" NIX_CLIENT_USER=alice \
  NIX_CONFIG='experimental-features = nix-command\nflakes' \
  "${client}" --version >/dev/null
want=$(printf 'experimental-features = nix-command\nflakes')
got=$(cat "${tmp}/cfg" 2>/dev/null || true)
if [ "${got}" != "${want}" ]; then
  bad "literal backslash-n in NIX_CONFIG was not rewritten"
fi

if [ "${fail}" -ne 0 ]; then
  exit 1
fi
printf '%s\n' "ok"
