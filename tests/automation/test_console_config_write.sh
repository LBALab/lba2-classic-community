#!/usr/bin/env bash
# The console verbs that persist a setting actually persist it.
#
# Three of them write one key each rather than going through WriteConfigFile,
# and a bare write is refused while the game data's config is stacked under the
# player's. Every install that ships an lba2.cfg attaches that layer, so those
# verbs stored nothing on a retail disc while two of the three reported success
# anyway. Nothing exercised a console write, so nothing said so.
#
# The install is built here rather than borrowed: a tree of symlinks to the real
# game data with a config of its own, which guarantees the layer is attached
# whatever the developer's own data folder happens to look like. Borrowing an
# install would make the test pass vacuously on a tree that ships no config,
# which is exactly the case that was never broken.
#
# Local-only (needs the binary and retail data); skips cleanly otherwise.
TESTNAME=console_config_write
. "$(dirname "$0")/lib.sh"
precheck

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# --- an install that ships its own config -------------------------------------
install="$tmp/install"
mkdir -p "$install"
for f in "$LBA2_GAME_DIR"/*; do
    ln -s "$f" "$install/$(basename "$f")" 2>/dev/null || true
done
[ -e "$install/LBA2.HQR" ] || [ -e "$install/lba2.hqr" ] ||
    skip "cannot symlink the game data (needs privileges on Windows)"

# Declares a release, so a layer is attached, and one nobody will write, so the
# console's own value is distinguishable from it. LanguageInstall is the probe
# for the other half: an installer key that must stay in the install.
cat > "$install/lba2.cfg" <<'CFG'
Version: 1
LanguageInstall: English
CFG
install_cfg_before=$(cat "$install/lba2.cfg")

user="$tmp/user"
mkdir -p "$user"

value_of() { # value_of <file> <key>
    grep -aE "^[[:space:]]*${1}[[:space:]]*:" "$user/lba2.cfg" 2>/dev/null | head -1 |
        sed -E 's/^[^:]*:[[:space:]]*//; s/[[:space:]]*$//' | tr -d '\r'
}

# --- one session, four settings -----------------------------------------------
out=$(ctl --user-dir "$user" --game-dir "$install" --fixed-dt 16 \
      --exec "distrib virgin; distrib logo off; vsync off; fixedtimestep 20" \
      --tick 3 --exit 2>&1)

# Both shapes of the failure: the verb that says so, and the two that used to
# report the setting applied while storing nothing.
printf '%s' "$out" | grep -aq 'Failed to write cfg' &&
    fail "a console verb could not write to the config"
printf '%s' "$out" | grep -aq 'this run only' &&
    fail "a console verb applied a setting without storing it"

[ -f "$user/lba2.cfg" ] || fail "no config was written at all"

while read -r key want; do
    got=$(value_of "$key")
    [ "$got" = "$want" ] || fail "$key should be '$want' after the console wrote it, got '$got'"
done <<'EOF'
Version 4
ShowDistribLogo 0
VSync 0
FixedTimestep 20
EOF

# --- the layer is written around, not written out -----------------------------
# The naive way to make a write succeed is to drop the layer and serialise the
# buffer, which folds the install's keys into the player's file where they
# outlive the install they came from.
[ -z "$(value_of LanguageInstall)" ] ||
    fail "an installer-only key was folded into the profile"

# And the install is input, on read-only media as much as anywhere.
[ "$install_cfg_before" = "$(cat "$install/lba2.cfg")" ] ||
    fail "the console wrote into the install's own config"

# --- and the values are in force next launch ----------------------------------
# What persistence is for. The release line reports the written value rather
# than the 1 the install declares, and the splash reports off.
second=$(ctl --user-dir "$user" --game-dir "$install" --fixed-dt 16 \
         --exec "distrib logo" --tick 3 --exit 2>&1)

printf '%s' "$second" | grep -aqE 'Release +virgin \(4\) from the config' ||
    fail "the written release is not in force: $(printf '%s' "$second" | grep -aoE 'Release +.*' | head -1)"
printf '%s' "$second" | grep -aq 'Splash: OFF' ||
    fail "the written splash setting is not in force"

pass "distrib, distrib logo, vsync and fixedtimestep all persist through an attached layer"
