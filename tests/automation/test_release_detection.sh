#!/usr/bin/env bash
# The release line reports what the config declared and what the data measures as.
#
# tests/distrib/test_distrib_resolve.cpp pins the rules between the two inputs
# and needs no assets. This runs the other half against a real install: that the
# measurement happens at all, that it reaches the banks wherever they live, and
# that a declaration and a measurement are compared rather than one silently
# standing in for the other.
#
# The declaration side is driven from the profile, never from the install. A
# config beside the game data is input: writing one to set up a test would be
# the same defect `dist_check.sh`'s WROTE assertion exists to catch, and would
# not work at all on an install mounted from a disc image.
#
# Local-only (needs the binary and retail data); skips cleanly otherwise.
TESTNAME=release_detection
. "$(dirname "$0")/lib.sh"
precheck

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

release_line() { # release_line <profile dir> [config body]
    local u="$tmp/$1"; shift
    rm -rf "$u"; mkdir -p "$u"
    [ $# -gt 0 ] && printf '%s\n' "$1" > "$u/lba2.cfg"
    ctl --user-dir "$u" --language English --fixed-dt 16 --tick 3 --exit 2>&1 |
        grep -aoE 'Release[[:space:]]+.*' | head -1 | sed -E 's/[[:space:]]+/ /g; s/ $//'
}

# --- what the data says, with nothing declared over it ------------------------
plain=$(release_line plain)
[ -n "$plain" ] || fail "no Release line in the boot banner"

# Which master this install's banks measure as. An install that ships its own
# config declares over the profile, so the line may report agreement rather than
# name the data, and then the declared value names the master instead.
case "$plain" in
    *"data is LBA2")               data_name="LBA2" ;;
    *"data is Twinsen's Odyssey")  data_name="Twinsen's Odyssey" ;;
    *"from the data")              data_name="Twinsen's Odyssey" ;;
    *"data agrees")
        case "$plain" in
            *"(0)"*|*"(3)"*) data_name="LBA2" ;;
            *)               data_name="Twinsen's Odyssey" ;;
        esac ;;
    *)
        # A tree the size table has no entry for. Ordinary, not a failure: the
        # sample behind the table is small and says so.
        skip "data unrecognised, nothing to compare a declaration against ($plain)" ;;
esac

if [ "$data_name" = "LBA2" ]; then
    agree_ver=3; agree_name="ea";         opposite_ver=1
else
    agree_ver=1; agree_name="activision"; opposite_ver=3
fi

# --- a declaration that agrees with the data ----------------------------------
agreed=$(release_line agreed "Version: $agree_ver")
case "$agreed" in
    *"$agree_name ($agree_ver) from the config, data agrees") ;;
    *) fail "declaring $agree_ver over its own master reported '$agreed'" ;;
esac

# --- a declaration that does not ----------------------------------------------
# The case nothing catches otherwise: one publisher's splash and panel sprite
# drawn over another's data, with the run exiting 0 the whole way.
conflicted=$(release_line conflicted "Version: $opposite_ver")
case "$conflicted" in
    *"($opposite_ver) from the config, data is $data_name") ;;
    *) fail "declaring $opposite_ver over $data_name data reported '$conflicted'" ;;
esac

# --- the declaration still wins -----------------------------------------------
# Detection reports; it does not overrule. The value in force has to be the one
# the config named, not the one the assets measure as.
printf '%s' "$conflicted" | grep -qE "\($opposite_ver\) from the config" ||
    fail "the declared release is not the one in force: '$conflicted'"

# --- neither key is written into the profile ----------------------------------
# Both belong to whoever assembled the install. Copied into a profile they would
# outlive the tree they came from, which is the snapshot the layered read exists
# to avoid.
for key in Version ShowDistribLogo; do
    grep -aqE "^[[:space:]]*${key}[[:space:]]*:" "$tmp/plain/lba2.cfg" &&
        fail "$key was written into a profile that never declared it"
done

pass "$plain"
