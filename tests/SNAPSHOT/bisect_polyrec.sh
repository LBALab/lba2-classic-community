#!/bin/bash
# bisect_polyrec.sh — Binary-search per-polygon-call bisection
#
# Finds the first polygon draw call that causes ASM vs CPP divergence.
#
# Usage: bisect_polyrec.sh <recording.lba2polyrec> [asm_binary] [cpp_binary]
#
# Uses --stop-after N to replay only the first N draw calls, then binary
# searches for the exact call that introduces the first difference.

set -euo pipefail

REC="${1:?Usage: bisect_polyrec.sh <recording.lba2polyrec> [asm_binary] [cpp_binary]}"
ASM_BIN="${2:-./replay_polyrec_asm}"
CPP_BIN="${3:-./replay_polyrec_cpp}"

ASM_OUT=$(mktemp /tmp/polyrec_bisect_asm_XXXXXX.raw)
CPP_OUT=$(mktemp /tmp/polyrec_bisect_cpp_XXXXXX.raw)
trap "rm -f '$ASM_OUT' '$CPP_OUT'" EXIT

# Helper: replay N draw calls and compare
compare_at() {
    local n=$1
    "$ASM_BIN" "$REC" "$ASM_OUT" --stop-after "$n" 2>/dev/null
    "$CPP_BIN" "$REC" "$CPP_OUT" --stop-after "$n" 2>/dev/null
    cmp -s "$ASM_OUT" "$CPP_OUT"
}

echo "=== Polygon call bisection ==="
echo "Recording: $REC"

# First, run full replay to confirm there IS a difference
echo "Running full replay to confirm divergence..."
"$ASM_BIN" "$REC" "$ASM_OUT" 2>/dev/null
"$CPP_BIN" "$REC" "$CPP_OUT" 2>/dev/null

if cmp -s "$ASM_OUT" "$CPP_OUT"; then
    echo "PASS: No divergence found — framebuffers are identical"
    exit 0
fi

echo "Divergence confirmed. Starting binary search..."

# Binary search: find the first N where outputs diverge
lo=0
hi=100000  # upper bound — will be refined

# First, find a reasonable upper bound by checking powers of 2
n=1
while true; do
    if ! compare_at "$n"; then
        hi=$n
        break
    fi
    if [ "$n" -ge 100000 ]; then
        echo "ERROR: Could not find divergence within 100000 draw calls"
        exit 2
    fi
    lo=$n
    n=$((n * 2))
done

echo "  Divergence occurs between draw call $lo and $hi"

# Binary search for exact call
while [ $((hi - lo)) -gt 1 ]; do
    mid=$(( (lo + hi) / 2 ))
    echo "  Testing --stop-after $mid ..."
    if compare_at "$mid"; then
        lo=$mid
    else
        hi=$mid
    fi
done

echo ""
echo "=== RESULT ==="
echo "First divergent draw call: #$hi"
echo ""
echo "To render up to that call:"
echo "  $ASM_BIN $REC asm_bisect.raw --stop-after $hi --ppm asm_bisect.ppm"
echo "  $CPP_BIN $REC cpp_bisect.raw --stop-after $hi --ppm cpp_bisect.ppm"
echo ""
echo "To render just before (should match):"
echo "  $ASM_BIN $REC asm_before.raw --stop-after $lo --ppm asm_before.ppm"
echo "  $CPP_BIN $REC cpp_before.raw --stop-after $lo --ppm cpp_before.ppm"

# Return a distinct non-zero status so callers can stop on the first mismatch.
exit 1
