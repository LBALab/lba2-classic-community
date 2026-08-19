#!/usr/bin/env bash
# Contradictory input: what the engine does when two inputs ask for opposite things.
#
# Three resolutions, none of them written down outside the code, and all three are what an input
# change would flip without anyone noticing until a player complained:
#
#   - Left and Right held together. The turn block tests I_LEFT first and only reaches I_RIGHT in
#     its else, so Left wins (OBJECT.CPP, and again in ManualRealAngle).
#   - Up and Down held together. Same shape on the other axis: Up wins.
#   - Two behaviour keys in the same frame. I_NORMAL, I_SPORTIF, I_AGRESSIF and I_DISCRET are an
#     if/else-if chain, so the one listed FIRST wins rather than the one pressed. Pressing
#     Aggressive while Normal is also down gives Normal, whichever order they arrive in.
#
# Each is asserted as an identity rather than as a direction: holding both must land the hero in
# exactly the state that holding the winner alone lands him in. That is a stronger claim than "he
# turned left", and it is the one that catches a change making the loser contribute a little.
#
# Driven with `key`, not `input`: `key` goes through TabKeys and therefore through the binding
# table, which is the layer these resolutions live behind. `input` ORs its bits in after GetInput
# has rebuilt and would not exercise it (docs/plan/INPUT_PLAN.md, increment 1).
#
# Local-only (needs retail data + the tracked corpus save). Not in host_quick CI.
TESTNAME=input_combos
. "$(dirname "$0")/lib.sh"
precheck

FIX="$(dirname "$0")/../savegame/corpus/saves/steam_classic_2023/Wannies fragment.LBA"
[ -f "$FIX" ] || skip "fixture missing: $FIX"

# SDL scancodes, so this does not depend on the `key` verb's short-name table growing.
LEFT=80; RIGHT=79; UP=82; DOWN=81; F5=62; F7=64

out=$(mktemp -d); trap 'rm -rf "$out"' EXIT

# One run: hold whatever $1 says for a fixed number of ticks, then report the hero.
run() {
    local name="$1" execs="$2"
    ctl_headless --load "$FIX" --fixed-dt 16 --exec "$execs" --tick 150 \
        --dump-state "$out/$name.json" --exit >/dev/null 2>&1 \
        || fail "$name run: exit $?"
}

# The fields that matter for a given axis, as one string to compare.
facing()  { python3 -c "import json;h=json.load(open('$out/$1.json'))['hero'];print(h['beta'],h['gen_anim'])"; }
walking() { python3 -c "import json;h=json.load(open('$out/$1.json'))['hero'];print(h['x'],h['z'],h['gen_anim'])"; }
behav()   { python3 -c "import json;print(json.load(open('$out/$1.json'))['hero']['behaviour'])"; }

# --- turning: Left beats Right ----------------------------------------------
run left       "key $LEFT 200"
run right      "key $RIGHT 200"
run leftright  "key $LEFT 200; key $RIGHT 200"

[ "$(facing left)" != "$(facing right)" ] \
    || fail "the two turn directions are indistinguishable, so this proves nothing"
[ "$(facing leftright)" = "$(facing left)" ] \
    || fail "Left+Right should land exactly where Left alone lands: left=$(facing left) both=$(facing leftright)"

# --- walking: Up beats Down --------------------------------------------------
run up       "key $UP 250"
run down     "key $DOWN 250"
run updown   "key $UP 250; key $DOWN 250"

[ "$(walking up)" != "$(walking down)" ] \
    || fail "forward and back are indistinguishable, so this proves nothing"
[ "$(walking updown)" = "$(walking up)" ] \
    || fail "Up+Down should land exactly where Up alone lands: up=$(walking up) both=$(walking updown)"

# --- behaviour: the chain's order decides, not the player's ------------------
run bnormal     "key $F5 40"
run baggressive "key $F7 40"
run bnormfirst  "key $F5 40; key $F7 40"
run baggrfirst  "key $F7 40; key $F5 40"

[ "$(behav bnormal)" != "$(behav baggressive)" ] \
    || fail "the two behaviours are indistinguishable, so this proves nothing"
[ "$(behav bnormfirst)" = "$(behav bnormal)" ] && [ "$(behav baggrfirst)" = "$(behav bnormal)" ] \
    || fail "the chain's first entry should win either way: normal=$(behav bnormal) n+a=$(behav bnormfirst) a+n=$(behav baggrfirst)"

pass "contradictions resolve by code order: Left over Right (beta $(facing left | cut -d' ' -f1)), Up over Down, and behaviour $(behav bnormal) over $(behav baggressive) whichever is pressed first"
