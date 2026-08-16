#!/usr/bin/env bash
# A flag that says "this run only" leaves nothing behind, and still takes effect.
#
# --fixed-timestep and --language both override a setting that WriteConfigFile
# serialises out of a global at exit, so both used to write themselves into the
# player's lba2.cfg: one run with --fixed-timestep 100 throttled every later
# launch, and one --language Deutsch to check the German text made German the
# setting. Their own help text and their comments said the opposite.
#
# --vsync joins them for the same reason it exists: the Display submenu prints
# the setting, so a UI capture has to be able to name it, and a run that named
# it must not leave the player's own choice replaced.
#
# Both halves are asserted, because either one alone passes on a broken engine.
# "Does not persist" is satisfied by a flag that does nothing at all, and "takes
# effect" is satisfied by one that changes the setting for good. The chosen-value
# beats then prove the keys can move at all, or the first half would pass on an
# engine that had simply stopped writing the config.
#
# Local-only (needs the binary and retail data); skips cleanly otherwise.
TESTNAME=cli_override_persistence
. "$(dirname "$0")/lib.sh"
precheck

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
user="$tmp/user"

run() { # run <extra args...> -> boot output on stdout
    ctl --user-dir "$user" --fixed-dt 16 --tick 3 --exit "$@" 2>&1
}

value_of() { # value_of <key>
    grep -aE "^[[:space:]]*${1}[[:space:]]*:" "$user/lba2.cfg" 2>/dev/null | head -1 |
        sed -E 's/^[^:]*:[[:space:]]*//; s/[[:space:]]*$//' | tr -d '\r'
}

# --- what this machine's config says before anything overrides it -------------
run >/dev/null
[ -f "$user/lba2.cfg" ] || fail "no config was written at all"
base_step=$(value_of FixedTimestep)
base_lang=$(value_of Language)
base_vsync=$(value_of VSync)
[ -n "$base_step" ] && [ -n "$base_lang" ] && [ -n "$base_vsync" ] \
    || fail "config is missing the keys under test (FixedTimestep='$base_step' Language='$base_lang' VSync='$base_vsync')"

# Probe values that differ from the baseline, or the run would override a setting
# to what it already was and every assertion below would hold for free.
probe_step=100; [ "$base_step" = "100" ] && probe_step=50
probe_lang=Deutsch; [ "$base_lang" = "Deutsch" ] && probe_lang=Italiano
probe_vsync=off; want_vsync=OFF
[ "$base_vsync" = "0" ] && { probe_vsync=on; want_vsync=ON; }

# --- --fixed-timestep: in force for the run, absent from the config -----------
out=$(run --fixed-timestep "$probe_step" --exec-at 1 "fixedtimestep")
printf '%s' "$out" | grep -aqE "FixedTimestep: ON \($probe_step ms" \
    || fail "--fixed-timestep $probe_step did not take effect: $(printf '%s' "$out" | grep -aoE 'FixedTimestep: .*' | head -1)"
got=$(value_of FixedTimestep)
[ "$got" = "$base_step" ] \
    || fail "--fixed-timestep $probe_step persisted: config says '$got', was '$base_step'"

# --- --language: same, and the banner names the language it ran in ------------
out=$(run --language "$probe_lang")
printf '%s' "$out" | grep -aqE "Language +$probe_lang" \
    || fail "--language $probe_lang did not take effect: $(printf '%s' "$out" | grep -aoE 'Language +.*' | head -1)"
got=$(value_of Language)
[ "$got" = "$base_lang" ] \
    || fail "--language $probe_lang persisted: config says '$got', was '$base_lang'"

# --- --vsync: same, and the console reports the state it forced ---------------
# Asked for through the console rather than a banner line because the setting has
# no other visible spelling, and `vsync` with no argument prints it without
# writing anything (unlike `vsync on`, which is the player choosing).
out=$(run --vsync "$probe_vsync" --exec-at 1 "vsync")
printf '%s' "$out" | grep -aqE "Vsync: $want_vsync" \
    || fail "--vsync $probe_vsync did not take effect: $(printf '%s' "$out" | grep -aoE 'Vsync: .*' | head -1)"
got=$(value_of VSync)
[ "$got" = "$base_vsync" ] \
    || fail "--vsync $probe_vsync persisted: config says '$got', was '$base_vsync'"

# --- a setting the player chose is a different thing, and does persist --------
# Without this the two assertions above would also pass on an engine that had
# stopped writing the config, or stopped honouring the key.
chosen=40; [ "$base_step" = "40" ] && chosen=24
run --exec "fixedtimestep $chosen" >/dev/null
got=$(value_of FixedTimestep)
[ "$got" = "$chosen" ] \
    || fail "the console's own 'fixedtimestep $chosen' did not persist: config says '$got'"

# --- and a later overridden run leaves that choice alone ----------------------
# The regression this test exists for: the harness runs constantly on developer
# machines, and each run used to overwrite whatever the player had set.
run --fixed-timestep "$probe_step" >/dev/null
got=$(value_of FixedTimestep)
[ "$got" = "$chosen" ] \
    || fail "--fixed-timestep $probe_step overwrote the chosen $chosen: config says '$got'"

out=$(run --exec-at 1 "fixedtimestep")
printf '%s' "$out" | grep -aqE "FixedTimestep: ON \($chosen ms" \
    || fail "the chosen $chosen ms is not in force next launch: $(printf '%s' "$out" | grep -aoE 'FixedTimestep: .*' | head -1)"

pass "--fixed-timestep, --language and --vsync apply without persisting; a chosen $chosen ms survives them"
