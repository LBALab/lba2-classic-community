#!/bin/bash
# bisect_objects.sh — Find which object causes ASM-vs-CPP divergence
#
# Renders each object individually and diffs the output.
# Usage: bisect_objects.sh <snapshot> <asm_bin> <cpp_bin> <num_objects>

set -euo pipefail

SNAP="$1"
ASM_BIN="$2"
CPP_BIN="$3"
NUM="${4:-20}"

for i in $(seq 0 $((NUM-1))); do
    ASM_OUT="/tmp/bisect_asm_${i}.raw"
    CPP_OUT="/tmp/bisect_cpp_${i}.raw"
    "$ASM_BIN" "$SNAP" "$ASM_OUT" --only-object "$i" 2>/tmp/bisect_asm_${i}.err || true
    "$CPP_BIN" "$SNAP" "$CPP_OUT" --only-object "$i" 2>/tmp/bisect_cpp_${i}.err || true
    if [ ! -f "$ASM_OUT" ] || [ ! -f "$CPP_OUT" ]; then
        echo "obj[$i]: ERROR (missing output file)"
        [ -f "/tmp/bisect_asm_${i}.err" ] && tail -3 "/tmp/bisect_asm_${i}.err"
    elif cmp -s "$ASM_OUT" "$CPP_OUT"; then
        echo "obj[$i]: MATCH"
    else
        DIFF=$(cmp -l "$ASM_OUT" "$CPP_OUT" | wc -l)
        echo "obj[$i]: DIFF ($DIFF bytes)"
        cmp -l "$ASM_OUT" "$CPP_OUT" | head -3
    fi
    rm -f "$ASM_OUT" "$CPP_OUT" "/tmp/bisect_asm_${i}.err" "/tmp/bisect_cpp_${i}.err"
done
