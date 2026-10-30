#!/bin/sh
# Link host ttyUSB* and ttyACM* into /dev. Do not open them: open toggles
# modem lines and resets the board. Do not mknod. The host devtmpfs is
# mounted at /host-dev.
set -eu

host_dev=${SERIAL_HOST_DEV:-/host-dev}
dev_dir=${SERIAL_DEV_DIR:-/dev}

note() {
  echo "serial-link: $*" >&2
}

is_serial_name() {
  case "$1" in
    ttyUSB*[!0-9]*|ttyACM*[!0-9]*) return 1 ;;
    ttyUSB[0-9]*|ttyACM[0-9]*) return 0 ;;
    *) return 1 ;;
  esac
}

sync_serial() {
  if [ ! -d "$host_dev" ]; then
    return 1
  fi
  for host in "$host_dev"/ttyUSB* "$host_dev"/ttyACM*; do
    [ -c "$host" ] || continue
    name=${host##*/}
    is_serial_name "$name" || continue
    path="$dev_dir/$name"
    if [ -L "$path" ] && [ "$(readlink "$path")" = "$host" ]; then
      continue
    fi
    if [ -e "$path" ] && [ ! -L "$path" ]; then
      rm -f "$path" 2>/dev/null || true
      if [ -e "$path" ]; then
        note "$path is a mount and was left in place."
        continue
      fi
    fi
    if ln -sfn "$host" "$path"; then
      note "linked $path"
    else
      note "ln $path failed."
    fi
  done
  for path in "$dev_dir"/ttyUSB* "$dev_dir"/ttyACM*; do
    [ -L "$path" ] || continue
    target=$(readlink "$path") || continue
    case "$target" in
      "$host_dev"/ttyUSB*|"$host_dev"/ttyACM*) ;;
      *) continue ;;
    esac
    name=${target##*/}
    is_serial_name "$name" || continue
    if [ ! -c "$target" ]; then
      rm -f "$path"
      note "removed $path"
    fi
  done
}

loop() {
  while true; do
    sync_serial || true
    sleep 1
  done
}

start() {
  if [ "$(id -u)" -ne 0 ]; then
    echo "serial-link: start must run as root" >&2
    exit 1
  fi
  if [ -f /run/serial-link.pid ]; then
    pid=$(cat /run/serial-link.pid)
    if kill -0 "$pid" 2>/dev/null; then
      exit 0
    fi
  fi
  setsid "$0" loop >/run/serial-link.log 2>&1 </dev/null &
  echo $! > /run/serial-link.pid
}

case "${1:-}" in
  start) start ;;
  once) sync_serial ;;
  loop) loop ;;
  *)
    echo "usage: serial-link.sh start|once|loop" >&2
    exit 1
    ;;
esac
