#!/usr/bin/env bash
# Create branch and commit community splash asset tooling (session work).
# Run from repo root: ./scripts/create-community-assets-branch.sh

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BRANCH="${BRANCH:-feat/community-startup-assets-tooling}"

if git rev-parse --verify "$BRANCH" >/dev/null 2>&1; then
  echo "Branch $BRANCH already exists; checking it out."
  git checkout "$BRANCH"
else
  git checkout -b "$BRANCH"
fi

git add \
  tools/probe_community_assets.py \
  tools/requirements-community-assets.txt \
  .gitignore

if git diff --cached --quiet; then
  echo "Nothing staged (already committed or no changes?). Status:"
  git status -sb
  exit 0
fi

git commit -m "feat(tools): PNG probe and LIM/PAL export for community splash assets"

echo "Done. Branch: $(git branch --show-current)"
git log -1 --oneline
git status -sb
