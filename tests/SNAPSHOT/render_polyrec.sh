#!/bin/bash
# render_polyrec.sh — Render a polygon recording through both ASM and CPP paths
#
# Usage: render_polyrec.sh <recording.lba2polyrec> [output_dir] [asm_binary] [cpp_binary] [replay args...]
#
# Produces .raw, .ppm, and .png (if ImageMagick available) for visual comparison.
# When passed --render-individually together with --stop-after N, it renders one
# output set per draw call in the requested range.

set -euo pipefail

REC="${1:?Usage: render_polyrec.sh <recording.lba2polyrec> [output_dir] [asm_binary] [cpp_binary]}"
BASENAME=$(basename "${REC}" .lba2polyrec)
OUTPUT_DIR="${2:-.}"
ASM_BIN="${3:-./replay_polyrec_asm}"
CPP_BIN="${4:-./replay_polyrec_cpp}"
if [ $# -gt 4 ]; then
    shift 4
else
    set --
fi

RENDER_INDIVIDUALLY=false
START_AFTER=""
STOP_AFTER=""
COMMON_ARGS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --render-individually)
            RENDER_INDIVIDUALLY=true
            shift
            ;;
        --start-after)
            START_AFTER="${2:?--start-after requires an argument}"
            shift 2
            ;;
        --stop-after)
            STOP_AFTER="${2:?--stop-after requires an argument}"
            shift 2
            ;;
        *)
            COMMON_ARGS+=("$1")
            shift
            ;;
    esac
done

mkdir -p "$OUTPUT_DIR"

echo "=== Polygon recording render ==="
echo "Recording: $REC"
echo "Output dir: $OUTPUT_DIR"

render_pair() {
    local suffix="$1"
    shift

    local asm_raw="$OUTPUT_DIR/${BASENAME}${suffix}_asm.raw"
    local cpp_raw="$OUTPUT_DIR/${BASENAME}${suffix}_cpp.raw"
    local asm_ppm="$OUTPUT_DIR/${BASENAME}${suffix}_asm.ppm"
    local cpp_ppm="$OUTPUT_DIR/${BASENAME}${suffix}_cpp.ppm"
    local ref_ppm="$OUTPUT_DIR/${BASENAME}${suffix}_game_ref.ppm"
    local asm_ok=0
    local cpp_ok=0

    echo "Rendering ASM path${suffix:+ ($suffix)}..."
    if "$ASM_BIN" "$REC" "$asm_raw" --ppm "$asm_ppm" --ref-ppm "$ref_ppm" "$@"; then
        asm_ok=1
    else
        echo "WARNING: ASM replay failed (exit code $?)"
    fi

    echo "Rendering CPP path${suffix:+ ($suffix)}..."
    if "$CPP_BIN" "$REC" "$cpp_raw" --ppm "$cpp_ppm" "$@"; then
        cpp_ok=1
    else
        echo "WARNING: CPP replay failed (exit code $?)"
    fi

    if command -v convert &>/dev/null; then
        local ppm
        for ppm in "$asm_ppm" "$cpp_ppm" "$ref_ppm"; do
            if [ -f "$ppm" ]; then
                local png="${ppm%.ppm}.png"
                convert "$ppm" "$png"
                echo "  Converted: $png"
            fi
        done

        local asm_png="$OUTPUT_DIR/${BASENAME}${suffix}_asm.png"
        local cpp_png="$OUTPUT_DIR/${BASENAME}${suffix}_cpp.png"
        local diff_png="$OUTPUT_DIR/${BASENAME}${suffix}_diff.png"
        if [ -f "$asm_png" ] && [ -f "$cpp_png" ]; then
            convert "$asm_png" "$cpp_png" -compose difference -composite "$diff_png"
            echo "  Diff image: $diff_png"
        fi
    fi

    if [ "$asm_ok" -eq 1 ] && [ "$cpp_ok" -eq 1 ]; then
        if cmp -s "$asm_raw" "$cpp_raw"; then
            echo "PASS: Framebuffers match${suffix:+ ($suffix)}"
        else
            local asm_size
            local cpp_size
            asm_size=$(stat -c%s "$asm_raw" 2>/dev/null || stat -f%z "$asm_raw")
            cpp_size=$(stat -c%s "$cpp_raw" 2>/dev/null || stat -f%z "$cpp_raw")
            if [ "$asm_size" = "$cpp_size" ]; then
                local diff_count
                diff_count=$(cmp -l "$asm_raw" "$cpp_raw" | wc -l)
                echo "FAIL: $diff_count bytes differ${suffix:+ ($suffix)}"
            else
                echo "FAIL: Size mismatch (ASM=$asm_size, CPP=$cpp_size)${suffix:+ ($suffix)}"
            fi
        fi
    else
        echo "SKIP: Cannot compare (ASM_OK=$asm_ok, CPP_OK=$cpp_ok)${suffix:+ ($suffix)}"
    fi
}

if [ "$RENDER_INDIVIDUALLY" = "true" ]; then
    if [ -z "$STOP_AFTER" ]; then
        echo "ERROR: --render-individually requires --stop-after"
        exit 1
    fi

    range_start=1
    if [ -n "$START_AFTER" ] && [ "$START_AFTER" -ge 0 ]; then
        range_start=$((START_AFTER + 1))
    fi
    range_stop=$STOP_AFTER

    if [ "$range_start" -gt "$range_stop" ]; then
        echo "ERROR: empty render range ($range_start > $range_stop)"
        exit 1
    fi

    draw_call=$range_start
    while [ "$draw_call" -le "$range_stop" ]; do
        suffix=$(printf '_draw_%04d' "$draw_call")
        prev_draw=$((draw_call - 1))
        echo "--- rendering draw call $draw_call independently ---"
        render_pair "$suffix" "${COMMON_ARGS[@]}" --start-after "$prev_draw" --stop-after "$draw_call"
        draw_call=$((draw_call + 1))
    done
else
    replay_args=("${COMMON_ARGS[@]}")
    if [ -n "$START_AFTER" ]; then
        replay_args+=(--start-after "$START_AFTER")
    fi
    if [ -n "$STOP_AFTER" ]; then
        replay_args+=(--stop-after "$STOP_AFTER")
    fi
    render_pair "" "${replay_args[@]}"
fi
