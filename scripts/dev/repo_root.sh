#!/usr/bin/env bash
# Print absolute path to repository root (directory containing top-level CMakeLists.txt).
set -euo pipefail

if root="$(git rev-parse --show-toplevel 2>/dev/null)"; then
  printf '%s\n' "$root"
  exit 0
fi

dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
while [[ "$dir" != "/" ]]; do
  if [[ -f "$dir/CMakeLists.txt" ]] && grep -q '^project(' "$dir/CMakeLists.txt" 2>/dev/null; then
    printf '%s\n' "$dir"
    exit 0
  fi
  dir="$(dirname "$dir")"
done

echo "repo_root: could not find project root (git or CMakeLists.txt)" >&2
exit 1
