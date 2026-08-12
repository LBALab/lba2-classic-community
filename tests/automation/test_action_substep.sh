#!/usr/bin/env bash
# Hero action input is frame-rate independent under fixed-timestep sub-stepping (issue #456).
#
# The #358 sub-step loop advances the simulation N times for one input sample when a frame runs
# below ~60 fps. Its first cut masked Input down to movement bits (I_JOY) on every sub-step after
# the first, to stop held actions re-firing. But the hero object loop (OBJECT.CPP MOVE_MANUAL) reads
# a *release* off the momentary-action bits (`if (LastInput & I_THROW) InitAnim(GEN_ANIM_RIEN)`, plus
# the LastMyFire idle reset), so dropping I_THROW/I_ACTION_M mid-frame looked like the player let go:
# the throw/attack animation was reset to idle every sub-step and never reached its throw/hit frame.
# Below ~60 fps the magic ball never left Twinsen's hand and the sword combo broke (#456).
#
# The fix holds BOTH Input and LastInput at the frame's sample on sub-steps > 0, so a held action is
# steady state (no phantom press, no phantom release). This test drives a held throw and a held melee
# across the frame-rate range and asserts the action still fires and its animation is not spuriously
# reset. The reference is --fixed-dt 16 (~60 fps, one sub-step, the historical path); the low-fps
# lanes must match it.
#
# Observables (harness-only, no gameplay effect): --dump-state "throws" counts hero projectile spawns
# (ThrowExtra / ThrowMagicBall); "action_resets" counts times an in-progress hero action animation was
# reset to idle by a MOVE_MANUAL release/idle path — during a HELD action with no real release this
# must stay ~0. Both are driven headlessly by the `input`, `behaviour`, `weapon`, `vargame` verbs.
#
# --fixed-timestep 16 is passed explicitly: the throttle only sub-steps when on, and a developer's
# lba2.cfg may have FixedTimestep:0 (throttle off), under which every lane runs one step and the bug
# cannot appear. Local-only (needs retail data + a save); skips cleanly otherwise. Not in CI.
TESTNAME=action_substep
. "$(dirname "$0")/lib.sh"
precheck

# Git-tracked corpus save (reproducible on any machine), same fixture test_melee_throttle uses.
SAVE="$REPO/tests/savegame/corpus/saves/steam_classic_2023/Desert Island.LBA"
[ -f "$SAVE" ] || skip "no Desert Island corpus save (steam_classic_2023 not present?)"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# Isolate the cfg/save dir: each run passes --fixed-timestep 16 explicitly, and a fresh user dir
# keeps that (and any config write) out of the developer's shared lba2.cfg, so this test neither
# depends on nor mutates the cfg FixedTimestep other suites read. The engine creates the folder.
export LBA2_USER_DIR="$tmp/user"

# throws_and_resets <dt> <ticks> <exec> -> echoes "<throws> <action_resets> <gen_anim>"
run_probe() {
    local dt="$1" ticks="$2" ex="$3"
    local out="$tmp/s_${dt}_$RANDOM.json"
    ctl --fixed-timestep 16 --fixed-dt "$dt" --load "$SAVE" \
        --exec "$ex" --tick "$ticks" --dump-state "$out" --exit >/dev/null 2>&1 \
        || fail "run (dt=$dt) returned non-zero exit"
    jget "$out" "'%d %d %d' % (d['throws'], d['action_resets'], d['hero']['gen_anim'])"
}

# Equal game-time (~3200 ms) at each rate: ticks = 3200 / dt.
declare -A TICKS=( [16]=200 [40]=80 [64]=50 [128]=25 )
LOW_LANES="40 64 128"

# ── 1. Held throw: the magic ball must leave Twinsen's hand at every frame rate ──────────────────
# Arm the magic ball explicitly (behaviour + weapon + inventory flag) so the test does not depend on
# what the fixture save had equipped.
ARM="behaviour 0; weapon 1; vargame 1 1"
read -r t_ref r_ref _ < <(run_probe 16 "${TICKS[16]}" "$ARM; input throw 220")
[ "$t_ref" -ge 1 ] || skip "fixture can't throw a magic ball at 60 fps (throws=$t_ref) — needs a save where the hero can act"
[ "$r_ref" -eq 0 ] || fail "60 fps reference already resets the throw anim (action_resets=$r_ref) — unexpected"

for dt in $LOW_LANES; do
    read -r t r _ < <(run_probe "$dt" "${TICKS[$dt]}" "$ARM; input throw 220")
    # fires the same number of times as 60 fps: < ref is the #456 drop, > ref is a sub-step re-fire.
    [ "$t" -eq "$t_ref" ] || fail "held throw at dt=$dt fired $t times, 60fps fired $t_ref (issue #456 sub-step input drop)"
    [ "$r" -eq 0 ] || fail "held throw at dt=$dt reset the throw anim $r times mid-hold (issue #456 phantom release)"
done
echo "  throw: fires ${t_ref}x at 16/40/64/128 ms steps, 0 spurious resets"

# ── 2. Held melee: the sword combo must keep swinging, not collapse to idle ──────────────────────
# Aggressive behaviour, action held. A cancelled combo shows as action_resets climbing and the hero
# settling on GEN_ANIM_RIEN (0) instead of a COUP animation. Allow a small slack: the combo naturally
# cycles COUP -> brief idle -> COUP, and one such boundary can coincide with a sub-step edge.
read -r _ mr_ref mg_ref < <(run_probe 16 "${TICKS[16]}" "behaviour 2; input action 220")
MELEE_SLACK=5
for dt in $LOW_LANES; do
    read -r _ mr _ < <(run_probe "$dt" "${TICKS[$dt]}" "behaviour 2; input action 220")
    [ "$mr" -le "$MELEE_SLACK" ] || fail "held melee at dt=$dt reset the attack anim $mr times (60fps=$mr_ref; issue #456)"
done
echo "  melee: attack anim resets <= $MELEE_SLACK at 40/64/128 ms steps (60fps=$mr_ref)"

# ── 3. 60 fps is untouched: one sub-step must equal the throttle-off historical path ─────────────
# The fix only changes the simStep > 0 branch, which never runs at dt=16 (one step). Guard that the
# throttled 60 fps path is byte-identical to the throttle-off path over the same input.
on="$tmp/on.json"; off="$tmp/off.json"
ctl --fixed-timestep 16 --fixed-dt 16 --load "$SAVE" \
    --exec "$ARM; input throw 60" --tick 40 --dump-state "$on" --exit >/dev/null 2>&1 || fail "throttle-on run failed"
ctl --fixed-timestep 0  --fixed-dt 16 --load "$SAVE" \
    --exec "$ARM; input throw 60" --tick 40 --dump-state "$off" --exit >/dev/null 2>&1 || fail "throttle-off run failed"
if ! python3 -c "
import json,sys
a=json.load(open('$on'))['hero']; b=json.load(open('$off'))['hero']
sys.exit(0 if a==b else 1)"; then
    fail "60 fps throttled path diverges from the historical (throttle-off) path — the fix perturbed the reference"
fi
echo "  60fps: throttled path == throttle-off path (fix is a no-op at one sub-step)"

pass "hero action input frame-rate independent (throw fires ${t_ref}x, melee holds, 60fps unchanged)"
