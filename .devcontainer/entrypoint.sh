#!/bin/bash
# Point vscode at the workspace owner, then sleep.
#
# Rootless Docker shows the host user as uid 0. Rootful Docker shows the
# real host id. updateRemoteUserUID stays off (P-0023). A safe.directory
# line in ~/.gitconfig does not survive a rebuild and does not fix the
# uid of files this container writes.
#
# When the workspace owner is uid 0, vscode shares that uid with root.
# getpwuid(0) returns root, so the session home is /root. Nix feature
# flags live in /etc/nix/nix.conf and NIX_CONFIG, not in a home file.
#
# init reports the container running before this script finishes, so
# post-create.sh may still hold the image uid. vscode-prev keeps that
# uid, with passwordless sudo, so post-create.sh can re-exec.

set -u

ready=/run/devcontainer-uid-ready
# The checkout directory name is not fixed. Both configs pass the mount
# path in DEVCONTAINER_WORKSPACE (P-0027).
ws=${DEVCONTAINER_WORKSPACE:-}

note() {
  echo "devcontainer: $*" >&2
}

keep_old_uid() {
  local old_uid="$1"
  local old_gid="$2"
  if id -u vscode-prev >/dev/null 2>&1; then
    return 0
  fi
  if ! useradd -o -u "${old_uid}" -g "${old_gid}" -M -N -s /usr/sbin/nologin vscode-prev; then
    note "could not keep uid ${old_uid} in passwd."
    return 0
  fi
  printf '%s\n' 'vscode-prev ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/vscode-prev
  chmod 440 /etc/sudoers.d/vscode-prev
  if ! visudo -cf /etc/sudoers.d/vscode-prev >/dev/null; then
    rm -f /etc/sudoers.d/vscode-prev
    note "rejected sudoers for vscode-prev."
  fi
}

align_vscode() {
  local owner_uid owner_gid old_uid old_gid home_uid store_uid
  if [ -z "${ws}" ]; then
    note "DEVCONTAINER_WORKSPACE is unset; vscode uid unchanged."
    return 0
  fi
  if [ ! -d "${ws}" ]; then
    note "${ws} is not mounted; vscode uid unchanged."
    return 0
  fi
  owner_uid=$(stat -c %u "${ws}") || return 0
  owner_gid=$(stat -c %g "${ws}") || return 0
  old_uid=$(id -u vscode)
  old_gid=$(id -g vscode)
  if [ "${old_uid}" != "${owner_uid}" ] || [ "${old_gid}" != "${owner_gid}" ]; then
    keep_old_uid "${old_uid}" "${old_gid}"
    if [ "${old_gid}" != "${owner_gid}" ]; then
      if ! groupmod -o -g "${owner_gid}" vscode; then
        note "groupmod failed; vscode stays gid $(id -g vscode)."
      fi
    fi
    if [ "$(id -u vscode)" != "${owner_uid}" ] || [ "$(id -g vscode)" != "${owner_gid}" ]; then
      if ! usermod -o -u "${owner_uid}" -g "${owner_gid}" vscode; then
        note "usermod failed; vscode stays uid $(id -u vscode)."
      fi
    fi
  fi
  if [ "$(id -u vscode)" != "${owner_uid}" ]; then
    note "vscode uid $(id -u vscode) does not match ${ws} owner ${owner_uid}."
    return 0
  fi
  # /nix may be the host store. chown would retarget that store (P-0024).
  home_uid=$(stat -c %u /home/vscode)
  if [ "${home_uid}" != "${owner_uid}" ]; then
    if ! chown -R "${owner_uid}:${owner_gid}" /home/vscode; then
      note "chown of /home/vscode failed."
    fi
  fi
  if awk '$2 == "/nix" { found=1 } END { exit !found }' /proc/mounts; then
    note "/nix is the host store; owner unchanged."
  else
    store_uid=$(stat -c %u /nix)
    if [ "${store_uid}" != "${owner_uid}" ]; then
      if ! chown -R "${owner_uid}:${owner_gid}" /nix; then
        note "chown of /nix failed."
      fi
    fi
  fi
  note "vscode is $(id -u vscode):$(id -g vscode); ${ws} is ${owner_uid}:${owner_gid}."
}

# mknod is rejected in the rootless user namespace (P-0033). The host
# devtmpfs is mounted at /host-dev. Scan it for every ttyUSBn and
# ttyACMn, including a cable plugged in after the IDE starts. Do not
# open those nodes here: open toggles modem lines and resets the board.
is_serial_name() {
  case "$1" in
    ttyUSB*[!0-9]*|ttyACM*[!0-9]*) return 1 ;;
    ttyUSB[0-9]*|ttyACM[0-9]*) return 0 ;;
    *) return 1 ;;
  esac
}

sync_serial() {
  local name path host target
  if [ ! -d /host-dev ]; then
    return 1
  fi
  for host in /host-dev/ttyUSB* /host-dev/ttyACM*; do
    [ -c "${host}" ] || continue
    name=${host##*/}
    is_serial_name "${name}" || continue
    path="/dev/${name}"
    if [ -L "${path}" ] && [ "$(readlink "${path}")" = "${host}" ]; then
      continue
    fi
    if [ -e "${path}" ] && [ ! -L "${path}" ]; then
      rm -f "${path}" 2>/dev/null || true
      if [ -e "${path}" ]; then
        if [ ! -e "/run/postern-serial-skip-${name}" ]; then
          note "${path} is a mount and was left in place."
          touch "/run/postern-serial-skip-${name}"
        fi
        continue
      fi
    fi
    if ln -sfn "${host}" "${path}"; then
      note "linked ${path}"
    else
      note "ln ${path} failed."
    fi
  done
  for path in /dev/ttyUSB* /dev/ttyACM*; do
    [ -L "${path}" ] || continue
    target=$(readlink "${path}") || continue
    case "${target}" in
      /host-dev/ttyUSB*|/host-dev/ttyACM*) ;;
      *) continue ;;
    esac
    name=${target##*/}
    is_serial_name "${name}" || continue
    if [ ! -c "${target}" ]; then
      rm -f "${path}" && note "removed ${path}"
    fi
  done
}

watch_serial() {
  while true; do
    sync_serial || true
    sleep 1
  done
}

if ! sync_serial; then
  note "/host-dev is not mounted; serial links stay absent."
fi
watch_serial &

align_vscode || note "alignment returned nonzero."
if ! touch "${ready}"; then
  note "could not write ${ready}."
fi

if [ "$#" -eq 0 ]; then
  exec sleep infinity
fi
exec "$@"
