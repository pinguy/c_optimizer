#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

pick_file() {
  if command -v kdialog >/dev/null 2>&1 && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
    kdialog --title "C Optimizer" --getopenfilename "${HOME:-$PWD}" "*.c|C source files (*.c)" 2>/dev/null || true
    return
  fi
  if command -v zenity >/dev/null 2>&1 && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
    zenity --file-selection --title="Choose a single C file" --file-filter="C source files | *.c" 2>/dev/null || true
    return
  fi
  printf 'Path to single C file: ' >&2
  IFS= read -r path
  printf '%s\n' "$path"
}

info_box() {
  title=$1
  message=$2
  if command -v kdialog >/dev/null 2>&1 && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
    kdialog --title "$title" --msgbox "$message" 2>/dev/null || true
  elif command -v zenity >/dev/null 2>&1 && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
    zenity --info --title="$title" --text="$message" 2>/dev/null || true
  else
    printf '%s\n%s\n' "$title" "$message"
  fi
}

error_box() {
  title=$1
  log=$2
  if command -v kdialog >/dev/null 2>&1 && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
    kdialog --title "$title" --textbox "$log" 900 520 2>/dev/null || true
  elif command -v zenity >/dev/null 2>&1 && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
    zenity --text-info --title="$title" --filename="$log" --width=900 --height=520 2>/dev/null || true
  else
    cat "$log" >&2
  fi
}

if [ "$#" -gt 0 ]; then
  if [ "${COPT_HOST_BUILD:-0}" = 1 ]; then
    exec "$HERE/build_asm_syscall.sh" "$@"
  fi
  exec "$HERE/build_gcc9_bullseye.sh" "$@"
fi

src=$(pick_file)
[ -n "$src" ] || exit 0

src_abs=$(realpath "$src")
stem=$(basename -- "$src_abs")
stem=${stem%.*}
safe_name=$(printf '%s' "$stem" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9._-]+/-/g; s/^-+|-+$//g')
[ -n "$safe_name" ] || safe_name=optimized-c
out="$(dirname -- "$src_abs")/$safe_name"
log=$(mktemp "${TMPDIR:-/tmp}/c-optimizer-log.XXXXXX")

if [ "${COPT_HOST_BUILD:-0}" = 1 ]; then
  builder="$HERE/build_asm_syscall.sh"
else
  builder="$HERE/build_gcc9_bullseye.sh"
fi

if "$builder" "$src_abs" >"$log" 2>&1; then
  if command -v notify-send >/dev/null 2>&1 && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
    notify-send "C Optimizer" "Built $(basename -- "$out")" 2>/dev/null || true
  fi
  info_box "C Optimizer" "Built runnable:\n$out\n\nBuild log:\n$log"
else
  error_box "C Optimizer build failed" "$log"
  exit 1
fi
