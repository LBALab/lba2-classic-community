# shellcheck shell=bash
#
# Single source of truth for the clang-format version used by CI
# (check-format.sh), local formatting (apply-format.sh), and the pre-commit
# hook. Bump CLANG_FORMAT_MAJOR here to move the whole project to a new
# clang-format in one place.
#
# Source this fragment (do not execute it). It sets:
#   CLANG_FORMAT_MAJOR - the required major version
#   clang_format       - path/name of a clang-format whose major version
#                        matches, or "" if none is installed.
#
# It never falls back to a different major version. Silently formatting with a
# clang-format that disagrees with CI is worse than not formatting at all, so
# callers decide what an empty result means: CI and apply-format fail; the
# pre-commit hook skips with a warning.

CLANG_FORMAT_MAJOR=18

clang_format=""
for _cf_cand in \
    "clang-format-${CLANG_FORMAT_MAJOR}" \
    "clang-format" \
    "$( (command -v xcrun >/dev/null 2>&1 && xcrun --find clang-format 2>/dev/null) || true)"; do
    [ -n "${_cf_cand}" ] || continue
    command -v "${_cf_cand}" >/dev/null 2>&1 || continue
    _cf_ver="$({ "${_cf_cand}" --version 2>/dev/null || true; } | grep -oE '[0-9]+' | head -n1 || true)"
    if [ "${_cf_ver}" = "${CLANG_FORMAT_MAJOR}" ]; then
        clang_format="${_cf_cand}"
        break
    fi
done
unset _cf_cand _cf_ver
