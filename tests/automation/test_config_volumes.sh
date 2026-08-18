#!/usr/bin/env bash
# The volume keys survive a boot, and an out-of-range one is brought into range
# rather than passed through.
#
# These settings do not go through the declarative table in CONFIG_FILE.CPP, so
# the clamp coverage in tests/settings does not reach them: ReadVolumeSettings
# (AMBIANCE.CPP) reads and clamps them by hand. Three of the five keys appear in
# test_config_upgrade.sh, which round-trips in-range values only and says the
# clamping "is a different test". This is that test, and it covers the two keys
# that were in no fixture at all.
#
# The key names are not the variable names, which is the other thing worth
# pinning: WaveVolume is SampleVolume, MusicVolume is JingleVolume. A rename on
# either side of that mapping is silent, and costs the player their settings.
#
# CDVolume is asserted only when the build wrote it: it sits behind #ifdef CDROM,
# which the shipped executable defines and another target need not. Skipping the
# assertion is right there; skipping the key would not be, since it shares the
# unsigned-variable-signed-reader shape that made a negative MusicVolume read as
# the loudest setting.
#
# Local-only (needs the binary and retail data); skips cleanly otherwise.
TESTNAME=config_volumes
. "$(dirname "$0")/lib.sh"
precheck

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

run() { # run <user-dir> -> boot, writing the config back on exit
    ctl --user-dir "$1" --fixed-dt 16 --tick 2 --exit 2>&1
}

value_of() { # value_of <file> <key>
    grep -aE "^[[:space:]]*$2[[:space:]]*:" "$1" | head -1 |
        sed -E "s/^[^:]*:[[:space:]]*//; s/[[:space:]]*$//" | tr -d '\r'
}

# --- 1. in-range values come back unchanged ----------------------------------
# Each differs from its compiled default, so a run that ignored the file and
# wrote defaults over it cannot pass.
u1="$tmp/inrange"; mkdir -p "$u1"
cat > "$u1/lba2.cfg" <<'CFG'
WaveVolume: 33
VoiceVolume: 44
MusicVolume: 55
MasterVolume: 66
CDVolume: 77
ReverseStereo: 1
CFG

run "$u1" >/dev/null || fail "a run against a config with volumes exited non-zero"
for pair in "WaveVolume 33" "VoiceVolume 44" "MusicVolume 55" "MasterVolume 66" "CDVolume 77" "ReverseStereo 1"; do
    set -- $pair
    got=$(value_of "$u1/lba2.cfg" "$1")
    [ -n "$got" ] || { [ "$1" = CDVolume ] && continue; fail "$1 is missing from the config after a run"; }
    [ "$got" = "$2" ] || fail "$1 went in as '$2' and came out as '$got'"
done

# --- 2. above the range is brought down to it --------------------------------
u2="$tmp/high"; mkdir -p "$u2"
cat > "$u2/lba2.cfg" <<'CFG'
WaveVolume: 999
VoiceVolume: 999
MusicVolume: 999
MasterVolume: 999
CDVolume: 999
CFG

run "$u2" >/dev/null || fail "a run against over-range volumes exited non-zero"
for key in WaveVolume VoiceVolume MusicVolume MasterVolume CDVolume; do
    got=$(value_of "$u2/lba2.cfg" "$key")
    [ -z "$got" ] && [ "$key" = CDVolume ] && continue
    [ "$got" = "127" ] || fail "$key 999 should clamp to 127, came out as '$got'"
done

# --- 3. below the range is brought up to it ----------------------------------
# 127 is the loudest and 0 is silence, so a negative value that survives the read
# is not a quiet setting, it is a volume the mixer was never meant to be handed.
u3="$tmp/low"; mkdir -p "$u3"
cat > "$u3/lba2.cfg" <<'CFG'
WaveVolume: -5
VoiceVolume: -5
MusicVolume: -5
MasterVolume: -5
CDVolume: -5
CFG

run "$u3" >/dev/null || fail "a run against negative volumes exited non-zero"
for key in WaveVolume VoiceVolume MusicVolume MasterVolume CDVolume; do
    got=$(value_of "$u3/lba2.cfg" "$key")
    [ -z "$got" ] && [ "$key" = CDVolume ] && continue
    [ "$got" = "0" ] || fail "$key -5 should clamp to 0, came out as '$got'"
done

pass "the volume keys round-trip, and out-of-range values are clamped"
