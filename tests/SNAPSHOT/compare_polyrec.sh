#!/bin/bash
# compare_polyrec.sh — Compare ASM vs CPP framebuffer output for a polygon recording
#
# Usage: compare_polyrec.sh <recording.lba2polyrec> [asm_binary] [cpp_binary]
#
# Exit code: 0 = identical, 1 = different, 2 = error

set -euo pipefail

REC="${1:?Usage: compare_polyrec.sh <recording.lba2polyrec> [asm_binary] [cpp_binary]}"
ASM_BIN="${2:-./replay_polyrec_asm}"
CPP_BIN="${3:-./replay_polyrec_cpp}"

ASM_OUT=$(mktemp /tmp/polyrec_asm_XXXXXX.raw)
CPP_OUT=$(mktemp /tmp/polyrec_cpp_XXXXXX.raw)
trap "rm -f '$ASM_OUT' '$CPP_OUT'" EXIT

echo "=== Polygon recording replay comparison ==="
echo "Recording: $REC"
echo "ASM binary: $ASM_BIN"
echo "CPP binary: $CPP_BIN"

# Run ASM replay
echo "Running ASM replay..."
if ! "$ASM_BIN" "$REC" "$ASM_OUT"; then
    echo "ERROR: ASM replay failed"
    exit 2
fi

# Run CPP replay
echo "Running CPP replay..."
if ! "$CPP_BIN" "$REC" "$CPP_OUT"; then
    echo "ERROR: CPP replay failed"
    exit 2
fi

# Compare outputs
if cmp -s "$ASM_OUT" "$CPP_OUT"; then
    ASM_SIZE=$(stat -c%s "$ASM_OUT" 2>/dev/null || stat -f%z "$ASM_OUT")
    echo "PASS: Framebuffers match ($ASM_SIZE bytes)"
    exit 0
else
    ASM_SIZE=$(stat -c%s "$ASM_OUT" 2>/dev/null || stat -f%z "$ASM_OUT")
    CPP_SIZE=$(stat -c%s "$CPP_OUT" 2>/dev/null || stat -f%z "$CPP_OUT")
    echo "FAIL: Framebuffers differ"
    echo "  ASM output: $ASM_SIZE bytes"
    echo "  CPP output: $CPP_SIZE bytes"
    if [ "$ASM_SIZE" = "$CPP_SIZE" ]; then
        DIFF_COUNT=$(cmp -l "$ASM_OUT" "$CPP_OUT" | wc -l)
        echo "  Differing bytes: $DIFF_COUNT"
        echo "  First 20 differences (offset ASM CPP):"
        cmp -l "$ASM_OUT" "$CPP_OUT" | head -20
    else
        echo "  Size mismatch!"
    fi
    exit 1
fi
