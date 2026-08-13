#!/usr/bin/env bash
# An lba2.cfg that already exists keeps working across an engine update.
#
# Every other config test starts from a profile that does not exist yet, which
# is the easy direction: a fresh profile has nothing to preserve. This one
# starts from a player who already has settings, which is the case the layered
# read changed the mechanics of.
#
# The invariant is not "byte-identical to some older build", which would need a
# second binary and could only ever be measured once. It is that a config
# holding these keys resolves to these values, which is what a later change to
# the layering would break, and would break quietly: settings resolving
# differently after an update is not something players report as a bug.
#
# Four sub-cases, one per way it can go wrong:
#
#   1. the keys the player owns come back out unchanged, and none are dropped
#   2. a key the game data owns is answered, and is not copied into the profile
#   3. a config that names one itself still wins over the data underneath it
#   4. an empty config is a config to fill in, not a fatal
#
# Local-only (needs the binary and retail data); skips cleanly otherwise.
TESTNAME=config_upgrade
. "$(dirname "$0")/lib.sh"
precheck

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# Every value differs from the compiled default for that key, so "unchanged"
# cannot pass on a run that ignored the file and wrote defaults over it. All are
# in range: out-of-range values are clamped on read, which is a different test.
#
# WinMode is what makes the two Input lines meaningful. Without it ReadInputConfig
# restores the default layout and never looks at them, which is also why a
# hand-written fixture is not the same thing as one the engine wrote.
#
# Shadow is deliberately absent: SetDetailLevel derives it from DetailLevel on
# every boot, so it is a written key that is not a kept one. Asserted separately
# below, because a Shadow that "changed by itself" otherwise reads as a config
# bug to whoever looks next.
cfg_in="$tmp/in.cfg"
cat > "$cfg_in" <<'CFG'
WinMode: 1
Language: Deutsch
MasterVolume: 91
MusicVolume: 42
DetailLevel: 1
AllCameras: 0
FollowCamera: 1
ReverseStereo: 1
GamepadDeadzone: 7000
Input0_1: 82
Input3_2: 94
CFG

# The engine writes its config at exit (atexit -> TheEndInfo -> WriteConfigFile),
# so any completed run is a full read-then-write round trip of the file.
run() { # run <user-dir> [extra args...] -> boot output on stdout
    local u="$1"; shift
    ctl --user-dir "$u" --fixed-dt 16 --tick 2 --exit "$@" 2>&1
}

value_of() { # value_of <file> <key>
    grep -aE "^[[:space:]]*$2[[:space:]]*:" "$1" | head -1 |
        sed -E "s/^[^:]*:[[:space:]]*//; s/[[:space:]]*$//" | tr -d '\r'
}

release_line() { # release_line <boot output>
    printf '%s\n' "$1" | grep -aoE 'Release[[:space:]]+.*' | head -1 |
        sed -E 's/[[:space:]]+$//'
}

# --- 1. what the player set is what comes back -------------------------------
owned="$tmp/owned"
mkdir -p "$owned"
cp "$cfg_in" "$owned/lba2.cfg"

out_txt=$(run "$owned") || fail "a run against an existing config exited non-zero"
out="$owned/lba2.cfg"
[ -f "$out" ] || fail "the config is gone after a run"

while IFS= read -r line; do
    key=${line%%:*}
    want=${line#*: }
    got=$(value_of "$out" "$key")
    [ "$got" = "$want" ] || fail "$key went in as '$want' and came out as '$got'"
done < "$cfg_in"

# Keys may be added (a write persists everything the engine owns, including keys
# a hand-written config never had) but never taken away.
in_keys=$(grep -acE '^[[:space:]]*[A-Za-z]' "$cfg_in")
out_keys=$(grep -acE '^[[:space:]]*[A-Za-z]' "$out")
[ "$out_keys" -ge "$in_keys" ] || fail "config shrank from $in_keys keys to $out_keys"

# The one written key that is not a kept one: DetailLevel 1 means shadows on
# characters but not impacts, and SetDetailLevel re-derives that on every boot
# whatever the file said.
got=$(value_of "$out" Shadow)
[ "$got" = "2" ] || fail "DetailLevel 1 should derive Shadow 2, got '$got'"

# --- 2. a key the game data owns is answered, and stays there -----------------
# Version decides the distributor splash, two logo sprites and the CD voice
# folder. Written into a profile it would outlive the install it was read from,
# which is the snapshot the layered read exists to avoid.
[ -z "$(value_of "$out" Version)" ] ||
    fail "Version was copied into the profile; it belongs to the game data"

# An existing config must not shadow or suppress what the data declares: the
# same install has to report the same release to an upgrading player as to a new
# one. Comparing the two runs asserts that without needing to know where the
# data's own config sits or what it says.
fresh="$tmp/fresh"
mkdir -p "$fresh"
fresh_rel=$(release_line "$(run "$fresh")")
owned_rel=$(release_line "$out_txt")
[ -n "$fresh_rel" ] || fail "no Release line in the boot banner"
[ "$owned_rel" = "$fresh_rel" ] ||
    fail "existing config reports '$owned_rel', a fresh profile on the same install reports '$fresh_rel'"

# --- 3. a config that names a Version itself still wins -----------------------
# virgin, because no install here declares it: if the layer underneath were
# winning, this would come back as that install's value instead.
declares="$tmp/declares"
mkdir -p "$declares"
{ cat "$cfg_in"; echo "Version: 4"; } > "$declares/lba2.cfg"
rel=$(release_line "$(run "$declares")")
case "$rel" in
    *"virgin (4) from the config"*) ;;
    *) fail "a config declaring Version 4 reported '$rel'" ;;
esac

# --- 4. an empty config is not a fatal ---------------------------------------
# A truncated write or a half-copied profile leaves one of these behind, and it
# is also the shape a config takes when the file exists but nothing has been
# saved into it yet.
empty="$tmp/empty"
mkdir -p "$empty"
: > "$empty/lba2.cfg"
run "$empty" >/dev/null || fail "a run against an empty config exited non-zero"
[ -s "$empty/lba2.cfg" ] || fail "an empty config was still empty after a run"

pass "$in_keys keys survived a round trip; $owned_rel"
