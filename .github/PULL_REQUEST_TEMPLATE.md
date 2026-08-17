<!--
PR title format: `<type>(<scope>): <summary>`
Types: feat | fix | port | perf | refactor | docs | test | build | ci | chore
Scope is optional. Example: `fix(credits): preserve gameplay state across console-invoked credits`
The PR title drives the changelog whatever the merge mode. `main` takes merge commits by
default, so your own commit messages land on it too and are worth writing.
See AGENTS.md "Commit & PR conventions".
-->

## What & why

<!-- One or two sentences. What does this change and why does it matter? -->

## Notes for reviewers

<!-- Optional: tradeoffs considered, follow-ups, anything non-obvious. -->

## Checklist

- [ ] Build passes on my platform (Linux / macOS / Windows — pick one or more)
- [ ] If LIB386 / 3DEXT touched: ASM equivalence tests pass (`./run_tests_docker.sh`)
- [ ] If behavior changes: opt-in via flag/cvar/config (preserves default game feel)
- [ ] Docs updated in the same PR if behavior or workflow changed
- [ ] French comments and ASCII art preserved in any modified files
