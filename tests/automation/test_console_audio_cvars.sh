#!/usr/bin/env bash
# The audio cvars are the same settings the cfg holds, from the other end.
#
# Each volume is declared once in AMBIANCE.CPP and handed to the cfg reader, the
# cfg writer and the console. This drives the console end and reads the cfg end,
# so a declaration that only half-works shows up here: a cvar that reads a stale
# value, a write that never reaches the cfg, or a range that only one of the two
# paths enforces.
#
# Out-of-range is the case worth having. Before the settings table these volumes
# were clamped by hand on the cfg read only, so the console could assign straight
# through the pointer and leave a value the reader would never have allowed.
#
# Local-only (needs the binary and retail data); skips cleanly otherwise.
TESTNAME=console_audio_cvars
. "$(dirname "$0")/lib.sh"
precheck

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

value_of() { # value_of <file> <key>
    grep -aE "^[[:space:]]*$2[[:space:]]*:" "$1" | head -1 |
        sed -E "s/^[^:]*:[[:space:]]*//; s/[[:space:]]*$//" | tr -d '\r'
}

# --- what the cfg holds is what the cvar reads --------------------------------
u="$tmp/read"; mkdir -p "$u"
cat > "$u/lba2.cfg" <<'CFG'
WaveVolume: 33
VoiceVolume: 44
MusicVolume: 55
MasterVolume: 66
CFG
out=$(ctl --user-dir "$u" --exec "snd_wave; snd_voice; snd_music; snd_master" \
          --fixed-dt 16 --tick 3 --exit 2>&1) || fail "the cvar reads exited non-zero"
for pair in "snd_wave 33" "snd_voice 44" "snd_music 55" "snd_master 66"; do
    set -- $pair
    printf '%s\n' "$out" | grep -qa "$1 = $2" || fail "$1 should read $2 from the cfg; got: $(printf '%s\n' "$out" | grep -a "$1 =")"
done

# --- what the console sets is what the cfg keeps ------------------------------
u="$tmp/write"; mkdir -p "$u"
printf 'MusicVolume: 40\n' > "$u/lba2.cfg"
ctl --user-dir "$u" --exec "snd_music 90; snd_voice 12" --fixed-dt 16 --tick 3 --exit >/dev/null 2>&1 ||
    fail "the cvar writes exited non-zero"
got=$(value_of "$u/lba2.cfg" MusicVolume)
[ "$got" = "90" ] || fail "snd_music 90 should persist as 90, cfg holds '$got'"
got=$(value_of "$u/lba2.cfg" VoiceVolume)
[ "$got" = "12" ] || fail "snd_voice 12 should persist as 12, cfg holds '$got'"

# --- the console cannot set a volume the cfg reader would refuse --------------
u="$tmp/range"; mkdir -p "$u"
printf 'MusicVolume: 40\n' > "$u/lba2.cfg"
ctl --user-dir "$u" --exec "snd_music 999; snd_wave -20" --fixed-dt 16 --tick 3 --exit >/dev/null 2>&1 ||
    fail "the out-of-range writes exited non-zero"
got=$(value_of "$u/lba2.cfg" MusicVolume)
[ "$got" = "127" ] || fail "snd_music 999 should clamp to 127, cfg holds '$got'"
got=$(value_of "$u/lba2.cfg" WaveVolume)
[ "$got" = "0" ] || fail "snd_wave -20 should clamp to 0, cfg holds '$got'"

# --- the stereo cvar and the audio verb are the same setting ------------------
# Two ways in, declared once. If they disagree the setting has two homes again,
# which is what this arrangement exists to prevent.
#
# Read through the cfg rather than by naming the cvar with no argument: a cvar
# the console types as a bool toggles when it is named bare (SETTINGS.CPP picks
# the type from the range), so a bare read would change the thing it is asking
# about. The volumes above are integers and print.
u="$tmp/stereo_verb"; mkdir -p "$u"
printf 'ReverseStereo: 0\n' > "$u/lba2.cfg"
ctl --user-dir "$u" --exec "audio global reverse_stereo 1" --fixed-dt 16 --tick 3 --exit >/dev/null 2>&1 ||
    fail "the stereo verb exited non-zero"
got=$(value_of "$u/lba2.cfg" ReverseStereo)
[ "$got" = "1" ] || fail "stereo set through the verb should persist as 1, cfg holds '$got'"

u="$tmp/stereo_cvar"; mkdir -p "$u"
printf 'ReverseStereo: 0\n' > "$u/lba2.cfg"
ctl --user-dir "$u" --exec "snd_reverse_stereo 1" --fixed-dt 16 --tick 3 --exit >/dev/null 2>&1 ||
    fail "the stereo cvar exited non-zero"
got=$(value_of "$u/lba2.cfg" ReverseStereo)
[ "$got" = "1" ] || fail "stereo set through the cvar should persist as 1, cfg holds '$got'"

# and the same in the other direction, so neither route is a one-way latch
u="$tmp/stereo_off"; mkdir -p "$u"
printf 'ReverseStereo: 1\n' > "$u/lba2.cfg"
ctl --user-dir "$u" --exec "snd_reverse_stereo 0" --fixed-dt 16 --tick 3 --exit >/dev/null 2>&1 ||
    fail "the stereo cvar exited non-zero"
got=$(value_of "$u/lba2.cfg" ReverseStereo)
[ "$got" = "0" ] || fail "stereo cleared through the cvar should persist as 0, cfg holds '$got'"

pass "the audio cvars read, write, clamp and agree with the verb"
