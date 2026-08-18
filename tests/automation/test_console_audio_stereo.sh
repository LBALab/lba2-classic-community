#!/usr/bin/env bash
# What the console does to reverse stereo is what the cfg holds afterwards.
#
# The verb applies the setting to the sample driver. If it does not also record
# it, three things follow and none of them is visible from the console's own
# reply: the choice is not written to the cfg, `audio global reset` reapplies the
# stale value and silently undoes it, and the options menu reads the old one.
#
# This is the shape the surface rule exists to prevent. A generic surface that
# calls a feature's apply function directly, rather than going through the
# feature, leaves the feature's state and the driver's state disagreeing.
#
# Local-only (needs the binary and retail data); skips cleanly otherwise.
TESTNAME=console_audio_stereo
. "$(dirname "$0")/lib.sh"
precheck

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

value_of() { # value_of <file> <key>
    grep -aE "^[[:space:]]*$2[[:space:]]*:" "$1" | head -1 |
        sed -E "s/^[^:]*:[[:space:]]*//; s/[[:space:]]*$//" | tr -d '\r'
}

run() { # run <user-dir> <exec-string>
    ctl --user-dir "$1" --exec "$2" --fixed-dt 16 --tick 3 --exit 2>&1
}

# --- turning it on is remembered ---------------------------------------------
on="$tmp/on"; mkdir -p "$on"
printf 'ReverseStereo: 0\n' > "$on/lba2.cfg"
run "$on" "audio global reverse_stereo 1" >/dev/null || fail "the verb exited non-zero"
got=$(value_of "$on/lba2.cfg" ReverseStereo)
[ "$got" = "1" ] || fail "console set reverse_stereo 1, cfg came out as '$got'"

# --- and turning it off again --------------------------------------------------
off="$tmp/off"; mkdir -p "$off"
printf 'ReverseStereo: 1\n' > "$off/lba2.cfg"
run "$off" "audio global reverse_stereo 0" >/dev/null || fail "the verb exited non-zero"
got=$(value_of "$off/lba2.cfg" ReverseStereo)
[ "$got" = "0" ] || fail "console set reverse_stereo 0, cfg came out as '$got'"

# --- reset reapplies what is set, not what was set before ----------------------
# `audio global reset` reapplies the stored value. Run after a change, it must
# not walk the setting back to what the cfg held at boot.
rst="$tmp/reset"; mkdir -p "$rst"
printf 'ReverseStereo: 0\n' > "$rst/lba2.cfg"
run "$rst" "audio global reverse_stereo 1; audio global reset" >/dev/null || fail "the verbs exited non-zero"
got=$(value_of "$rst/lba2.cfg" ReverseStereo)
[ "$got" = "1" ] || fail "reset after a change walked reverse_stereo back to '$got'"

pass "the console's reverse-stereo choice is recorded, persisted and survives a reset"
