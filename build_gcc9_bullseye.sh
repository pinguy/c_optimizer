#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
IMAGE=${COPT_GCC9_IMAGE:-localhost/c-optimizer-gcc9:bullseye}
CONTAINERFILE="$HERE/toolchains/gcc9-bullseye/Containerfile"

if [ "$#" -gt 0 ]; then
  SRC=$1
else
  SRC=${SRC:-"$HERE/examples/VOIDRUNNER.c"}
fi

if [ ! -f "$SRC" ]; then
  printf '[gcc9] ERROR: source file not found: %s\n' "$SRC" >&2
  exit 1
fi

command -v podman >/dev/null 2>&1 || {
  printf '[gcc9] ERROR: podman is required for the GCC 9 Bullseye toolchain\n' >&2
  exit 1
}

if ! podman image exists "$IMAGE" || [ "${COPT_GCC9_REBUILD:-0}" = 1 ]; then
  printf '[gcc9] building toolchain image: %s\n' "$IMAGE"
  podman build -t "$IMAGE" -f "$CONTAINERFILE" "$HERE/toolchains/gcc9-bullseye"
fi

SRC_ABS=$(realpath "$SRC")
SRC_DIR=$(dirname -- "$SRC_ABS")
SRC_FILE=$(basename -- "$SRC_ABS")
SRC_STEM=${SRC_FILE%.*}
safe_name=$(printf '%s' "$SRC_STEM" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9._-]+/-/g; s/^-+|-+$//g')
[ -n "$safe_name" ] || safe_name=optimized-c

if [ -n "${OUT:-}" ]; then
  case "$OUT" in
    /*) OUT_HOST=$OUT ;;
    *) OUT_HOST=$(realpath -m "$PWD/$OUT") ;;
  esac
else
  OUT_HOST="$SRC_DIR/$safe_name"
fi

OUT_DIR=$(dirname -- "$OUT_HOST")
OUT_FILE=$(basename -- "$OUT_HOST")
mkdir -p "$OUT_DIR"

printf '[gcc9] image:    %s\n' "$IMAGE"
printf '[gcc9] source:   %s\n' "$SRC_ABS"
printf '[gcc9] output:   %s\n' "$OUT_HOST"

podman run --rm --pull=never --userns=keep-id \
  -v "$HERE:/c_optimizer:ro" \
  -v "$SRC_DIR:/input:ro" \
  -v "$OUT_DIR:/output" \
  -w /c_optimizer \
  "$IMAGE" \
  sh -lc '
    set -eu
    mkdir -p /tmp/copt-work
    CC=gcc COPT_OPT=-Os COPT_EXTRA_LDLIBS=-ldl OUT="$1" WORKDIR=/tmp/copt-work ./build_asm_syscall.sh "$2"
  ' sh "/output/$OUT_FILE" "/input/$SRC_FILE"
