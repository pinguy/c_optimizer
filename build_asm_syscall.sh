#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if [ "$#" -gt 0 ]; then
  SRC=$1
else
  SRC=${SRC:-VOIDRUNNER.c}
fi

if [ ! -f "$SRC" ]; then
  printf '[build] ERROR: source file not found: %s\n' "$SRC" >&2
  exit 1
fi

SRC_ABS=$(realpath "$SRC")
SRC_DIR=$(dirname -- "$SRC_ABS")
SRC_FILE=$(basename -- "$SRC_ABS")
SRC_STEM=${SRC_FILE%.*}
case "$SRC_FILE" in
  *.c|*.C) ;;
  *) printf '[build] ERROR: expected a single C source file (*.c): %s\n' "$SRC_ABS" >&2; exit 1 ;;
esac

safe_name=$(printf '%s' "$SRC_STEM" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9._-]+/-/g; s/^-+|-+$//g')
[ -n "$safe_name" ] || safe_name=optimized-c

OUT=${OUT:-"$SRC_DIR/$safe_name"}
case "$OUT" in
  /*) ;;
  *) OUT="$PWD/$OUT" ;;
esac
OUT_DIR=$(dirname -- "$OUT")
mkdir -p "$OUT_DIR"

if [ -n "${WORKDIR:-}" ]; then
  mkdir -p "$WORKDIR"
  cleanup_workdir=0
else
  WORKDIR=$(mktemp -d "${TMPDIR:-/tmp}/c-optimizer.XXXXXX")
  cleanup_workdir=1
fi
cleanup() {
  if [ "$cleanup_workdir" -eq 1 ]; then
    rm -rf "$WORKDIR"
  fi
}
trap cleanup EXIT INT TERM

RAW=${RAW:-"$WORKDIR/${safe_name}_raw.raw"}
SST=${SST:-"$WORKDIR/${safe_name}_raw.sstrip"}
START_OBJ="$WORKDIR/start_asm.o"
RUNNER_TMP="$WORKDIR/runner"

CFLAGS="-Oz -Wall -flto -fwhole-program -fno-plt -fno-pie -no-pie -fno-toplevel-reorder -fno-reorder-functions -fno-schedule-insns -fno-schedule-insns2 -fno-ipa-cp -fno-ipa-sra -fno-tree-sra -fno-expensive-optimizations -fno-asynchronous-unwind-tables -fno-unwind-tables -ffunction-sections -fdata-sections -fno-stack-protector -fomit-frame-pointer -fmerge-all-constants -fno-math-errno -fno-ident -fno-lto -fno-inline -fno-jump-tables -fno-code-hoisting -fno-tree-dominator-opts -fno-tree-fre -fno-tree-sink -fno-tree-slsr -fno-tree-forwprop"
LDFLAGS="-fno-lto -no-pie -nostartfiles -Wl,-e,_start -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,norelro -Wl,-z,noseparate-code -Wl,--as-needed -Wl,--hash-style=sysv"
SDL_CFLAGS="-I$SRC_DIR -I$HERE/compat"
printf '[build] start:    syscall _start\n'
printf '[build] source:   %s\n' "$SRC_ABS"
printf '[build] output:   %s\n' "$OUT"
gcc -c -Os -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident "$HERE/start_syscall.S" -o "$START_OBJ"
gcc $CFLAGS $SDL_CFLAGS "$SRC_ABS" "$START_OBJ" -o "$RAW" $LDFLAGS
strip -s "$RAW"
printf '[build] strip:    %s bytes\n' "$(stat -c%s "$RAW")"
cp "$RAW" "$SST"
if command -v sstrip >/dev/null 2>&1; then sstrip "$SST"; else python3 "$HERE/tiny_tools/sstrip64.py" "$SST"; fi
printf '[build] sstrip:   %s bytes\n' "$(stat -c%s "$SST")"
best=; best_sz=999999999; best_lc=0; best_pb=0; best_dict=96KiB
for lc in 0 1 2 3 4; do
  for pb in 0 1 2; do
    for dict in 64KiB 96KiB 128KiB 192KiB 256KiB 384KiB 512KiB 1MiB 2MiB 4MiB; do
      tmp="$SST.bcj.lc${lc}.pb${pb}.d${dict}"
      if xz --format=raw --x86 --lzma1=preset=9e,lc=$lc,lp=0,pb=$pb,dict=$dict -c "$SST" > "$tmp" 2>/dev/null; then
        if xz --format=raw --x86 --lzma1=preset=9e,lc=$lc,lp=0,pb=$pb,dict=$dict -dc "$tmp" > "$tmp.dec" 2>/dev/null && cmp -s "$SST" "$tmp.dec"; then
          sz=$(stat -c%s "$tmp")
          if [ "$sz" -lt "$best_sz" ]; then best_sz=$sz; best="$tmp"; best_lc=$lc; best_pb=$pb; best_dict=$dict; fi
        fi
      fi
      rm -f "$tmp.dec"
    done
  done
done
[ -n "$best" ] || { echo '[build] ERROR: BCJ pack failed' >&2; exit 1; }
mv "$best" "$SST.bcj"
rm -f "$SST".bcj.lc*.pb*.d* 2>/dev/null || true
printf '[build] bcj/lzma:%s bytes (raw x86+lzma lc=%s pb=%s dict=%s)\n' "$(stat -c%s "$SST.bcj")" "$best_lc" "$best_pb" "$best_dict"
cat > "$RUNNER_TMP" <<STUB
#!/bin/sh
a=/tmp/v\$\$;trap 'rm -f "\$a"' 0;tail -n+3 "\$0"|xz -Fraw --x86 --lzma1=lc=$best_lc,pb=$best_pb,dict=$best_dict -d>"\$a";chmod +x "\$a";"\$a" "\$@";r=\$?;exit "\$r"
STUB
cat "$SST.bcj" >> "$RUNNER_TMP"
mv "$RUNNER_TMP" "$OUT"
chmod +x "$OUT"
sz=$(stat -c%s "$OUT")
printf '[build] runner:   %s bytes\n' "$sz"
if [ "$sz" -lt 32768 ]; then printf '[build] budget:   UNDER 32 KiB (%s bytes headroom)\n' "$((32768-sz))"; else printf '[build] budget:   OVER 32 KiB by %s bytes\n' "$((sz-32768))"; fi
printf '[build] DT_NEEDED:\n'
readelf -d "$RAW" | grep NEEDED || true
