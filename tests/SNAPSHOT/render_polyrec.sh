#!/bin/bash
# render_polyrec.sh — Render a polygon recording through both ASM and CPP paths
#
# Usage: render_polyrec.sh <recording.lba2polyrec> [output_dir] [asm_binary] [cpp_binary]
#
# Produces .raw, .ppm, and .png (if ImageMagick available) for visual comparison.

set -euo pipefail

REC="${1:?Usage: render_polyrec.sh <recording.lba2polyrec> [output_dir] [asm_binary] [cpp_binary]}"
BASENAME=$(basename "${REC}" .lba2polyrec)
OUTPUT_DIR="${2:-.}"
ASM_BIN="${3:-./replay_polyrec_asm}"
CPP_BIN="${4:-./replay_polyrec_cpp}"

mkdir -p "$OUTPUT_DIR"

ASM_RAW="$OUTPUT_DIR/${BASENAME}_asm.raw"
CPP_RAW="$OUTPUT_DIR/${BASENAME}_cpp.raw"
ASM_PPM="$OUTPUT_DIR/${BASENAME}_asm.ppm"
CPP_PPM="$OUTPUT_DIR/${BASENAME}_cpp.ppm"
REF_PPM="$OUTPUT_DIR/${BASENAME}_game_ref.ppm"

echo "=== Polygon recording render ==="
echo "Recording: $REC"
echo "Output dir: $OUTPUT_DIR"

echo "Rendering ASM path..."
"$ASM_BIN" "$REC" "$ASM_RAW" --ppm "$ASM_PPM" --ref-ppm "$REF_PPM"

echo "Rendering CPP path..."
"$CPP_BIN" "$REC" "$CPP_RAW" --ppm "$CPP_PPM"

# Convert PPM to PNG if ImageMagick is available
if command -v convert &>/dev/null; then
    for ppm in "$ASM_PPM" "$CPP_PPM" "$REF_PPM"; do
        if [ -f "$ppm" ]; then
            png="${ppm%.ppm}.png"
            convert "$ppm" "$png"
            echo "  Converted: $png"
        fi
    done

    # Generate diff image if both PNG exist
    ASM_PNG="$OUTPUT_DIR/${BASENAME}_asm.png"
    CPP_PNG="$OUTPUT_DIR/${BASENAME}_cpp.png"
    DIFF_PNG="$OUTPUT_DIR/${BASENAME}_diff.png"
    if [ -f "$ASM_PNG" ] && [ -f "$CPP_PNG" ]; then
        convert "$ASM_PNG" "$CPP_PNG" -compose difference -composite "$DIFF_PNG"
        echo "  Diff image: $DIFF_PNG"
    fi
fi

# Compare raw framebuffers
if cmp -s "$ASM_RAW" "$CPP_RAW"; then
    echo "PASS: Framebuffers match"
else
    ASM_SIZE=$(stat -c%s "$ASM_RAW" 2>/dev/null || stat -f%z "$ASM_RAW")
    CPP_SIZE=$(stat -c%s "$CPP_RAW" 2>/dev/null || stat -f%z "$CPP_RAW")
    if [ "$ASM_SIZE" = "$CPP_SIZE" ]; then
        DIFF_COUNT=$(cmp -l "$ASM_RAW" "$CPP_RAW" | wc -l)
        echo "FAIL: $DIFF_COUNT bytes differ"
    else
        echo "FAIL: Size mismatch (ASM=$ASM_SIZE, CPP=$CPP_SIZE)"
    fi
fi
