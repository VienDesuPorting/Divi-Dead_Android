#!/usr/bin/env bash
# Copy Divi-Dead PC assets into app/src/main/assets/ and convert
# OPEN.AVI to MPEG-1 .MPG if ffmpeg is available.
set -euo pipefail

SRC="${1:-}"
DEST="$(dirname "$0")/app/src/main/assets"

if [ -z "$SRC" ]; then
    echo "Usage: $0 /path/to/dividead-folder"
    exit 1
fi

mkdir -p "$DEST/LANG" "$DEST/OGG"

# --- Archives ---
for f in SG.DL1 WV.DL1; do
    [ -f "$SRC/$f" ] && cp "$SRC/$f" "$DEST/" && echo "  [OK] $f"
done

# --- UI strings (optional) ---
if [ -f "$SRC/LANG/ENGLISH.TXT" ]; then
    cp "$SRC/LANG/ENGLISH.TXT" "$DEST/LANG/" && echo "  [OK] LANG/ENGLISH.TXT"
fi

# --- Music (pre-converted OGG from PSP, or see README for MIDI→OGG) ---
if [ -d "$SRC/OGG" ]; then
    cp "$SRC/OGG/"*.OGG "$DEST/OGG/" 2>/dev/null && echo "  [OK] OGG/"
fi

# --- Videos ---
# CS_ROGO.MPG — already MPEG-1 in PC version, copy as-is
if [ -f "$SRC/CS_ROGO.MPG" ]; then
    cp "$SRC/CS_ROGO.MPG" "$DEST/" && echo "  [OK] CS_ROGO.MPG"
fi

# OPEN.MPG — if PC version shipped OPEN.MPG, copy directly.
# Else convert OPEN.AVI to MPEG-1 (pl_mpeg only supports MPEG-PS).
#
# The original OPEN.AVI is 480x264 @ 15 fps with mono 22050 Hz PCM audio.
# Two things to fix during conversion:
#  - MPEG-1 doesn't support 15 fps → force 30 fps (-r 30, duplicates frames)
#  - MP2 at 22050 Hz mono doesn't allow 192 kbps → upsample to 44100 Hz stereo
if [ -f "$SRC/OPEN.MPG" ]; then
    cp "$SRC/OPEN.MPG" "$DEST/" && echo "  [OK] OPEN.MPG"
elif [ -f "$SRC/OPEN.AVI" ]; then
    if command -v ffmpeg >/dev/null 2>&1; then
        echo "  Converting OPEN.AVI → OPEN.MPG (MPEG-1 + MP2)..."
        ffmpeg -y -i "$SRC/OPEN.AVI" \
            -r 30 \
            -c:v mpeg1video -q:v 4 \
            -c:a mp2 -b:a 192k -ar 44100 -ac 2 \
            "$DEST/OPEN.MPG" < /dev/null
        echo "  [OK] OPEN.MPG (converted from AVI)"
    else
        echo "  [WARN] ffmpeg not found — OPEN.AVI not converted."
        echo "         Install ffmpeg and re-run, or convert manually:"
        echo "         ffmpeg -i OPEN.AVI -r 30 -c:v mpeg1video -q:v 4 \\"
        echo "                -c:a mp2 -b:a 192k -ar 44100 -ac 2 OPEN.MPG"
        cp "$SRC/OPEN.AVI" "$DEST/" 2>/dev/null || true
    fi
fi

echo "Done. Assets in: $DEST"
