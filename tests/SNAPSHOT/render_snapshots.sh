#!/bin/bash
# render_snapshots.sh — Render ASM and CPP framebuffers for a snapshot
#
# Runs both replay programs and leaves the raw + PPM outputs in the
# fixtures directory next to the snapshot file.
#
# Usage: render_snapshots.sh <snapshot.lba2snap> [asm_binary] [cpp_binary]
#
# Output files (placed alongside the input snapshot):
#   snapshot_0000_asm.raw   — raw 8-bit indexed framebuffer (ASM)
#   snapshot_0000_cpp.raw   — raw 8-bit indexed framebuffer (CPP)
#   snapshot_0000_asm.ppm   — PPM image (ASM) — viewable
#   snapshot_0000_cpp.ppm   — PPM image (CPP) — viewable
#
# If ImageMagick's `convert` or `magick` is available, also produces:
#   snapshot_0000_asm.png   — PNG image (ASM)
#   snapshot_0000_cpp.png   — PNG image (CPP)
#   snapshot_0000_diff.png  — visual diff (highlighted differences)

set -euo pipefail

SNAP="${1:?Usage: render_snapshots.sh <snapshot.lba2snap> [asm_binary] [cpp_binary]}"
ASM_BIN="${2:-./replay_snapshot_asm}"
CPP_BIN="${3:-./replay_snapshot_cpp}"

# Derive output paths from input snapshot path
SNAP_DIR="$(dirname "$SNAP")"
SNAP_BASE="$(basename "$SNAP" .lba2snap)"

ASM_RAW="${SNAP_DIR}/${SNAP_BASE}_asm.raw"
CPP_RAW="${SNAP_DIR}/${SNAP_BASE}_cpp.raw"
ASM_PPM="${SNAP_DIR}/${SNAP_BASE}_asm.ppm"
CPP_PPM="${SNAP_DIR}/${SNAP_BASE}_cpp.ppm"
REF_PPM="${SNAP_DIR}/${SNAP_BASE}_reference_cpp.ppm"
ASM_PNG="${SNAP_DIR}/${SNAP_BASE}_asm.png"
CPP_PNG="${SNAP_DIR}/${SNAP_BASE}_cpp.png"
REF_PNG="${SNAP_DIR}/${SNAP_BASE}_reference_cpp.png"
DIFF_PNG="${SNAP_DIR}/${SNAP_BASE}_diff.png"

echo "=== Snapshot render ==="
echo "Snapshot: $SNAP"
echo "Output:   ${SNAP_DIR}/${SNAP_BASE}_*.{raw,ppm,png}"
echo ""

# Run ASM replay (also extracts the game reference framebuffer if present)
echo "Rendering ASM..."
"$ASM_BIN" "$SNAP" "$ASM_RAW" --ppm "$ASM_PPM" --ref-ppm "$REF_PPM"
echo "  -> $ASM_RAW"
echo "  -> $ASM_PPM"
[ -f "$REF_PPM" ] && echo "  -> $REF_PPM (game reference, CPP-rendered)"

# Run CPP replay
echo "Rendering CPP..."
"$CPP_BIN" "$SNAP" "$CPP_RAW" --ppm "$CPP_PPM"
echo "  -> $CPP_RAW"
echo "  -> $CPP_PPM"

# Convert PPM to PNG if a converter is available
CONVERT=""
if command -v magick &>/dev/null; then
    CONVERT="magick"
elif command -v convert &>/dev/null; then
    CONVERT="convert"
fi

if [ -n "$CONVERT" ]; then
    echo ""
    echo "Converting to PNG..."
    if [ -f "$ASM_PPM" ]; then
        "$CONVERT" "$ASM_PPM" "$ASM_PNG" && echo "  -> $ASM_PNG"
    fi
    if [ -f "$CPP_PPM" ]; then
        "$CONVERT" "$CPP_PPM" "$CPP_PNG" && echo "  -> $CPP_PNG"
    fi
    if [ -f "$REF_PPM" ]; then
        "$CONVERT" "$REF_PPM" "$REF_PNG" && echo "  -> $REF_PNG (game reference)"
    fi

    # Generate visual diff if both PNGs exist
    if [ -f "$ASM_PNG" ] && [ -f "$CPP_PNG" ]; then
        "$CONVERT" "$ASM_PNG" "$CPP_PNG" -compose difference -composite \
                   -auto-level "$DIFF_PNG" 2>/dev/null \
            && echo "  -> $DIFF_PNG (visual diff)" \
            || echo "  (diff image skipped)"
    fi
else
    echo ""
    echo "Note: Install ImageMagick for automatic PNG conversion."
    echo "      PPM files can be viewed with most image viewers."
fi

# Quick comparison
echo ""
if cmp -s "$ASM_RAW" "$CPP_RAW"; then
    echo "RESULT: ASM and CPP framebuffers are IDENTICAL"
else
    ASM_SIZE=$(wc -c < "$ASM_RAW")
    CPP_SIZE=$(wc -c < "$CPP_RAW")
    if [ "$ASM_SIZE" = "$CPP_SIZE" ]; then
        DIFF_COUNT=$(cmp -l "$ASM_RAW" "$CPP_RAW" | wc -l)
        echo "RESULT: DIFFERENT — $DIFF_COUNT bytes differ"
    else
        echo "RESULT: DIFFERENT — size mismatch (ASM: $ASM_SIZE, CPP: $CPP_SIZE)"
    fi
fi
