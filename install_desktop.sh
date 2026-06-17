#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
DESKTOP_FILE="$APP_DIR/c-optimizer.desktop"

mkdir -p "$APP_DIR"

tmp=$(mktemp "${TMPDIR:-/tmp}/c-optimizer-desktop.XXXXXX")
trap 'rm -f "$tmp"' EXIT

cat > "$tmp" <<EOF
[Desktop Entry]
Type=Application
Name=C Optimizer
Comment=Pick a single C file and build a tiny runnable
Exec=$HERE/run_c_optimizer.sh
Terminal=false
Categories=Development;
MimeType=text/x-csrc;
EOF

mv "$tmp" "$DESKTOP_FILE"
chmod 644 "$DESKTOP_FILE"

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$APP_DIR" >/dev/null 2>&1 || true
fi

printf 'Installed desktop launcher: %s\n' "$DESKTOP_FILE"

