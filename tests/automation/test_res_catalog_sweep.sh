#!/usr/bin/env bash
# Resolution catalog sweep: boot the engine at every resolution the catalog can
# offer and confirm each one boots, renders the resolution sub-page, and writes
# a frame at exactly that size. Catches a catalog entry that lists a mode the
# engine cannot actually run (the class of bug the 1024->1080 Y-ceiling fixed,
# where the console listed 1920x1080 but Res_Switch rejected it).
#
# Two sources, deduped:
#   1. The recommended tier, swept explicitly via --resolution. Deterministic,
#      and covers HD (1280x720, 1920x1080) even under SDL_VIDEODRIVER=dummy,
#      whose reported display size would otherwise filter the taller modes out
#      of the listing. Keep in sync with kRecommended in SOURCES/RES_CATALOG.CPP.
#   2. Whatever `resolution --all` actually lists on this machine (advanced +
#      display-native), so the sweep follows the live catalog too.
#
# Local-only: needs the built binary and LBA2_GAME_DIR. No save fixture needed
# (the resolution sub-page renders at the boot menu). Skips cleanly otherwise.
TESTNAME=res_catalog_sweep
. "$(dirname "$0")/lib.sh"
precheck

# PNG IHDR width/height (big-endian u32 at offsets 16/20), stdlib only.
png_dims() {
    python3 - "$1" <<'PY' 2>/dev/null
import struct, sys
with open(sys.argv[1], "rb") as f:
    d = f.read(24)
print(*struct.unpack(">II", d[16:24]))
PY
}
have_py=1
command -v python3 >/dev/null 2>&1 || have_py=0

# 1. Recommended tier (always swept, HD included).
modes="640x480 768x480 1024x480 1280x720 1920x1080"

# 2. Append the live catalog listing, deduped.
listing="$(ctl_headless --exec "resolution --all" --tick 2 --exit 2>/dev/null |
    grep -oE '[0-9]{3,4}x[0-9]{3,4}' | sort -u)"
for m in $listing; do
    case " $modes " in
    *" $m "*) ;;
    *) modes="$modes $m" ;;
    esac
done

tmp="$(mktemp -t "${TESTNAME}.XXXXXX.png")"
trap 'rm -f "$tmp"' EXIT

n=0
bad=""
for m in $modes; do
    w="${m%x*}"
    h="${m#*x}"
    rm -f "$tmp"

    if ! ctl_headless_at "$m" --exec "ui resolution $tmp" --tick 6 --exit \
        >/dev/null 2>&1; then
        bad="$bad $m(exit)"
        continue
    fi
    [ -s "$tmp" ] || {
        bad="$bad $m(no-png)"
        continue
    }
    # A real menu frame compresses to tens of KB; an all-black/blank one is tiny.
    sz=$(wc -c <"$tmp")
    [ "$sz" -gt 2000 ] || bad="$bad $m(blank:${sz}b)"
    if [ "$have_py" = 1 ]; then
        dims="$(png_dims "$tmp")"
        [ "$dims" = "$w $h" ] || bad="$bad $m(dims:${dims// /x})"
    fi
    n=$((n + 1))
done

# Leave lba2.cfg's persisted ResolutionX/Y back at the 640x480 default rather
# than whatever sorted last, so a later interactive launch isn't surprised.
ctl_headless_at 640x480 --tick 1 --exit >/dev/null 2>&1 || true

[ -z "$bad" ] || fail "resolutions that did not boot+render correctly:$bad"
pass "$n catalog resolutions boot and render the sub-page at the right size"
