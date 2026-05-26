#!/usr/bin/env bash
# Persistence: lba2.cfg's ResolutionX / ResolutionY are read at boot.
#
# Three sub-cases:
#   1. Valid cfg dims  -> engine boots at the cfg resolution.
#   2. Invalid cfg     -> engine falls back to compile-time default,
#                          stderr carries a warning naming the bad value.
#   3. No CLI override -> cfg wins over default.
#   4. With CLI override -> CLI wins over cfg (precedence check).
#
# Tests isolate the user environment by setting XDG_DATA_HOME to a temp
# dir; the engine's SDL_GetPrefPath resolves to "$tmpdir/Twinsen/LBA2/",
# so the real user cfg at ~/.local/share/Twinsen/LBA2/lba2.cfg is never
# touched.
TESTNAME=res_switch_cfg
. "$(dirname "$0")/lib.sh"
precheck
need_save

# Each sub-run gets its own XDG_DATA_HOME so cfg writes don't bleed.
make_cfg() {
    local dir="$1" rx="$2" ry="$3"
    mkdir -p "$dir/Twinsen/LBA2"
    cat > "$dir/Twinsen/LBA2/lba2.cfg" <<EOF
ResolutionX: $rx
ResolutionY: $ry
EOF
}

dump_resolution() {
    # Run with given cfg, dump state on tick 2, print "WxH" from the JSON.
    # Uses ctl_headless_cfg_driven (does NOT pin --resolution) so cfg's
    # ResolutionX/Y is what drives the boot resolution — exactly what this
    # test exists to verify. The normal ctl_headless pins --resolution
    # 640x480 to keep the UI goldens stable against developers whose own
    # cfg may have a non-default ResolutionX/Y.
    local xdg="$1" extra="${2:-}"
    local out
    out="$(mktemp -t res_cfg.XXXXXX.json)"
    XDG_DATA_HOME="$xdg" ctl_headless_cfg_driven --load "$LBA2_TEST_SAVE" \
        --fixed-dt 16 --tick 2 --dump-state "$out" --exit $extra \
        >/dev/null 2>&1 \
        || { rm -f "$out"; fail "engine non-zero exit ($?)"; }
    local w h
    w=$(jget "$out" "d['scene']['render_w']" 2>/dev/null || echo "?")
    h=$(jget "$out" "d['scene']['render_h']" 2>/dev/null || echo "?")
    rm -f "$out"
    echo "${w}x${h}"
}

# Pre-declare temp dirs as empty so the EXIT trap doesn't trip set -u
# (from lib.sh) on a sub-case that hasn't run yet.
tmp1="" tmp2="" tmp3="" tmp4=""
trap 'rm -rf "$tmp1" "$tmp2" "$tmp3" "$tmp4"' EXIT

# Sub-case 1: valid cfg dims -> engine boots at those dims.
tmp1="$(mktemp -d -t res_cfg.XXXXXX)"
make_cfg "$tmp1" 768 480
got=$(dump_resolution "$tmp1")
[ "$got" = "768x480" ] || fail "valid cfg (768x480): engine reported $got"

# Sub-case 2: invalid cfg (W not %8==0) -> fall back to default 640x480.
tmp2="$(mktemp -d -t res_cfg.XXXXXX)"
make_cfg "$tmp2" 645 480
got=$(dump_resolution "$tmp2")
[ "$got" = "640x480" ] || fail "invalid cfg (645x480): engine reported $got (expected 640x480 fallback)"

# Sub-case 3: out-of-range cfg -> fall back to default.
tmp3="$(mktemp -d -t res_cfg.XXXXXX)"
make_cfg "$tmp3" 9999 9999
got=$(dump_resolution "$tmp3")
[ "$got" = "640x480" ] || fail "out-of-range cfg (9999x9999): engine reported $got (expected 640x480 fallback)"

# Sub-case 4: CLI override beats valid cfg.
tmp4="$(mktemp -d -t res_cfg.XXXXXX)"
make_cfg "$tmp4" 768 480
got=$(dump_resolution "$tmp4" "--resolution 1024x480")
[ "$got" = "1024x480" ] || fail "CLI override (1024x480 vs cfg 768x480): engine reported $got"

pass "valid cfg / invalid cfg / out-of-range cfg / CLI-beats-cfg all behave correctly"
