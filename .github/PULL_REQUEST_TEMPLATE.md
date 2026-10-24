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

## How it was checked

<!-- What you ran or played, on which platform and game data, and against what control
(main, or the setting off). A number comes with the command or probe that made it. -->

## Not checked

<!-- Optional: what this PR does not cover, so a reviewer knows where to look. -->

## Notes for reviewers

<!-- Optional: tradeoffs considered, follow-ups, anything non-obvious. -->

## Checklist

- [ ] Builds and runs on: Linux / macOS / Windows / Android (keep the ones you tried)
- [ ] If LIB386 / 3DEXT touched: ASM equivalence tests pass (`./run_tests_docker.sh`)
- [ ] If the game plays differently with every setting at its default from before: it restores the original (no switch needed), fixes a bug the original had (its own `fix_` switch under Fix retail bugs), or is an enhancement behind its own setting, off by default
- [ ] Ran the tests and fixtures this change can reach; a recording or golden it replaces was remade against a control
- [ ] Docs updated in the same PR if behavior or workflow changed
- [ ] French comments and ASCII art preserved in any modified files

<!-- The why behind these: docs/PRINCIPLES.md -->
