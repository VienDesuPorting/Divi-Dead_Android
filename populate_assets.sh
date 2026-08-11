#!/usr/bin/env bash
set -euo pipefail
SRC="${1:-}"
DEST="$(dirname "$0")/app/src/main/assets"
if [ -z "$SRC" ]; then
    echo "Usage: $0 /path/to/dividead-folder"
    exit 1
fi
mkdir -p "$DEST/LANG" "$DEST/OGG"
for f in SG.DL1 WV.DL1; do
    [ -f "$SRC/$f" ] && cp "$SRC/$f" "$DEST/" && echo "  [OK] $f"
done
[ -f "$SRC/LANG/ENGLISH.TXT" ] && cp "$SRC/LANG/ENGLISH.TXT" "$DEST/LANG/" && echo "  [OK] LANG/ENGLISH.TXT"
[ -d "$SRC/OGG" ] && cp "$SRC/OGG/"*.OGG "$DEST/OGG/" 2>/dev/null && echo "  [OK] OGG/"
[ -f "$SRC/CS_ROGO.MPG" ] && cp "$SRC/CS_ROGO.MPG" "$DEST/" && echo "  [OK] CS_ROGO.MPG"
echo "Done. Assets in: $DEST"
