# Portability plan: user directories, profiles and provenance

Goal: let one machine hold several LBA2 installs without their saves, configs and remembered paths
colliding, and let a headless run execute in a directory it created and can throw away, affecting
nothing the player owns.

Two mechanisms, and they are not alternatives. Directory separation stops the collisions. Boot
provenance makes the arrangement legible: every path the engine resolved, and how it resolved it,
stated once at boot, so a wrong pairing is visible in a pasted log instead of showing up later as a
wrong splash or a save that will not load.

Related docs: [GAME_DATA.md](GAME_DATA.md) (what an install holds),
[VERSIONS.md](VERSIONS.md) (`DistribVersion` is declared, never detected),
[SAVEGAME.md](SAVEGAME.md) (wire format, version 36),
[BIT_EXACTNESS.md](BIT_EXACTNESS.md) (the default-keep rule this plan obeys),
[CONFIG.md](CONFIG.md), [TESTING.md](TESTING.md).

## What the tree does today

`InitDirectories` ([DIRECTORIES.CPP](../SOURCES/DIRECTORIES.CPP)) takes three directories. Only one
of them is genuinely variable.

| Directory | Resolved by | Overridable |
|---|---|---|
| `resDir` | `ResolveGameDataDir` | yes: `--game-dir`, `--data-dir`, `LBA2_GAME_DIR`, `last_game_dir.txt`, probes |
| `userDir` | `SDL_GetPrefPath(ADELINE_PREF_ORG, ADELINE_PREF_APP)` | no |
| `cfgDir` | passed the same string as `userDir` | no |

So the data side is already pluggable and the writable side is a singleton. Everything written lands
in one folder shared by every install on the machine: `save/` (plus `save/shoot/`, `save/bugs/`),
`lba2.cfg`, `adeline.log`, `last_game_dir.txt`.

Four consequences follow, and each one is a phase below.

**Installs share a save folder.** A save carries `NUM_VERSION` and nothing that says which data set
wrote it, so nothing rejects a mismatch.

**`last_game_dir.txt` is a single global pointer.** Alternating between two installs rewrites the
same file each launch. The demo SKU took its own pref path for exactly this reason, which is a
per-build fix for what is really a per-install problem.

**The config is copied once and never revisited.** On first run, if the profile has no `lba2.cfg`,
[INITADEL.C](../SOURCES/INITADEL.C) copies the one sitting beside the game data, or falls back to the
built-in template. Whichever install you first ran against therefore configures you permanently, and
re-pointing `--game-dir` at a different release leaves the old release's `Version` in place. The
splash, the new-game panel sprite and the CD voice folder then disagree with the medium.

**Harness isolation is a platform-specific hack.** `dist_check.sh` and the automation tests set
`XDG_DATA_HOME`, which works only on Linux, and `SDL_GetPrefPath` caches its result per process so
the override has to win a race. `tests/discovery/` skips a case when it loses that race.

## Prior art

Five patterns, drawn from engines that solved this, and they compose rather than compete.

| Pattern | Exemplar | What it buys |
|---|---|---|
| Writable root as a command-line input | ioquake3 `fs_homepath` vs `fs_basepath` | isolation, on every platform |
| Portable marker beside the binary | Dolphin `portable.txt` | a relocatable, self-contained tree |
| Named targets with per-target overrides | ScummVM `scummvm.ini` sections | several releases of one game coexisting |
| Priority chain with an explicit replace | OpenMW config chain | layering without snapshot-copying |
| Identity from the content | ScummVM MD5 detection | knowing which release you are on |

The last one is where this engine deliberately differs. `DistribVersion` is declared by the config
and never detected, and that stays true here. What this plan adds is change detection, not release
identification. Those are different questions and conflating them is the trap.

## Design

### Layer 1: the user directory is an input

`--user-dir <dir>` and `LBA2_USER_DIR`, precedence CLI > env > `SDL_GetPrefPath`, matching the
CLI > cfg > default rule the resolution path already uses.

Resolved before the first `GetDefaultUserDir` call, which happens early in `main` before the log is
up. Two call sites take it: `GetDefaultUserDir` and `BuildPersistedGameDirPath`
([RES_DISCOVERY.CPP](../SOURCES/RES_DISCOVERY.CPP)), the latter of which reaches `SDL_GetPrefPath`
directly rather than through `directoriesUserDir` and so is a second source of truth today.

This alone gives the harness a clean room on all three platforms and retires the `XDG_DATA_HOME`
dependency.

### Layer 2: the config is a chain, not a copy

Replace the first-run copy with an ordered read:

```
built-in template  <  <resDir>/lba2.cfg  <  <profile>/lba2.cfg  <  CLI
```

Writes only ever reach the profile config. The split this needs already exists in the code:
`WriteConfigFile` does a load-modify-rewrite through `DefFileBufferInit`, preserving keys it does not
know, and it never writes `Version`. So the keys partition by owner with no new bookkeeping.

| Owner | Keys | How you can tell |
|---|---|---|
| User | `Language`, `LanguageCD`, volumes, key bindings, `Shadow`, `FollowCamera`, `ResolutionX/Y`, `LastSave` | `WriteConfigFile` writes them |
| Data | `Version`, `Version_US`, `LanguageInstall`, `Demo`, `PathInstall` | `WriteConfigFile` never touches them |

What this fixes: an empty profile is genuinely empty, so behaviour is a function of the game data and
the command line with no history; re-pointing a profile at a different release picks up that
release's declared `Version` while the player's own settings stay theirs; and the game-data config is
never written, so a read-only install or a mounted image works. Today's copy can fail the boot.

Risks to carry: the corruption strip and the CP850 decode now run over two files, and the early boot
read uses the smaller transient buffer.

### Layer 3: profiles

`--profile <name>` resolving to `<userDir>/profiles/<name>/`, with its own `save/`, `lba2.cfg`,
`adeline.log` and `last_game_dir.txt`. The default profile stays at the root of `userDir` so no
existing install moves and no player's saves appear to vanish. That asymmetry is deliberate and is
what ScummVM does with its global section.

The payoff is a per-profile manifest recording `gameDir`, `disc`, `distrib` and `language`, after
which `--profile ea-cd` is a complete launch: one word selects data, saves and config together.

Two constraints. `ADELINE_MAX_PATH` is 256, and `profiles/<name>/save/` costs around twenty
characters on top of a user profile path that is already long on Windows, so profile resolution needs
a length check with a clear error rather than a silent truncation. And every `Get*Path` caches into a
function-local static, so profile selection is boot-time only, the same rule `DistribVersion` already
follows.

### Layer 4: portable marker

A marker file beside the binary pins `userDir` to a local `User/` directory. Small once layer 1
exists, and it makes the release bundles self-contained.

### Layer 5: provenance at boot

Scope: the boot banner, and nothing else. No stamps written into saves or user files, no gates on
load. The question this answers is "which install, which profile, and how did the engine decide
that", asked of a pasted log.

The banner already does half of it. `Res_GetDiscoverySource` names which probe won the game
directory, for the stated reason that "the engine booted the wrong install" and "the engine ignored
what I set" otherwise look identical in a bug report ([INITADEL.C](../SOURCES/INITADEL.C)). That
reasoning applies just as well to the writable side, which today prints a bare path.

So: **every resolved path says how it was resolved, and the identity the run operates under is
stated once.**

| Line | Today | Adds |
|---|---|---|
| `Assets:` | path plus winning probe | unchanged |
| `Disc:` | image name, when mounted | unchanged |
| `Saves:`, `Log:` | bare paths | the profile they belong to, and how the user directory was chosen: default, `--user-dir`, env, or portable marker |
| `Config:` | bare path | the same, plus which layer supplied the values once the chain exists |
| identity | absent | SKU, declared `DistribVersion`, and the data fingerprint |

The fingerprint is **change detection, not identification**: sizes of a small fixed set of banks
folded into one value, a `stat` each rather than a read of the install. It answers "same data set as
last time?" and says nothing about which commercial release this is. Identity stays with the
declaration, per VERSIONS.md.

Two things follow. `dist_check.sh` runs the `distrib` console command to learn the release identity;
with it in the banner, one boot and one grep covers the whole row. And the matrix's isolation checks
get an independent witness: the log states which directory the run claimed, and the file-level check
confirms nothing outside it moved.

**Why it stops at the log.** Marking the files themselves was considered and dropped. A save cannot
carry an appendix: the compressed load path derives its payload size from `FileSize(GamePathname)`
minus the header ([SAVEGAME.CPP:624](../SOURCES/SAVEGAME.CPP#L624)) and copies that many bytes into a
buffer sized from the header's own `sizefile`, so trailing bytes inflate the copy. Changing the
payload instead is excluded by the default-keep rule in BIT_EXACTNESS.md. That leaves sidecar files
and a mismatch policy, which is a much larger surface than the problem currently justifies. If
mispaired saves turn out to bite in practice, revisit it then, with the boot banner already in place
to diagnose the first reports.

## Testing

### The distribution matrix

`scripts/dev/dist_check.sh` already sweeps the installs, gives each a throwaway profile, and writes a
diffable summary. Extend it rather than building a second harness. Four checks:

| Check | Asserts | Reported as |
|---|---|---|
| `IDENTITY` | the release the engine reports matches the `Version` the install's own config declares | a column, `-` for a disc image |
| `WROTE` | nothing under the game directory is written | a column |
| `ISOLATION` | the developer's own user directory is byte-identical before and after the sweep | one line after the table |
| `REBIND` | a profile seeded from one install, re-run against another, reports the second one's `Version` | one line after the table |

`REBIND` is what motivated layer 2, and it was measured rather than inferred. A profile seeded from
an install declaring 3, pointed at a medium declaring 1, kept reporting `3 (ea)` while booting data
that declares 1, because the config was copied on first run and never revisited. The layered read
closes it: the same profile now reports 1 there and 3 back on the first install.

`WROTE` found something nobody was looking for. On an install whose voices sit on the filesystem
rather than inside a disc image, playing one updates its mtime:
[MESSAGE.CPP:857](../SOURCES/MESSAGE.CPP#L857) touches a voice file already on the hard disk, under
`ONE_GAME_DIRECTORY`. That is the 1997 CD cache keeping an LRU stamp so the oldest copy could be
evicted when the disk filled, and with the voices installed there is nothing to evict. Content is
untouched, only the timestamp. Unfixed, and it keeps that row red.

### What stays out of the repo

The installs a developer holds are their own business, and the repo must not record them. The line:

- **Fine, and already public:** publisher names and their declared `Version` values. VERSIONS.md
  tabulates these because they are engine behaviour.
- **Never committed:** absolute paths, drive letters, mount points, image filenames, or any statement
  of how many installs exist or where. Row names stay functional, never descriptive of someone's
  shelf.
- **Supplied at run time:** the install list arrives through `LBA2_DIST_LIST` or a gitignored local
  file. The committed default stays a set of relative sibling paths that resolve on no machine in
  particular.

Make it enforceable rather than a habit: a guard over tracked files for drive letters, mount-point
prefixes and disc-image extensions, run where the other content guards run.

### What CI can carry

None of the matrix runs on a runner, because there is no game data there and there will not be. It
stays a local gate. The plan doc says so plainly so nobody later reads a green pipeline as coverage.

What CI can carry is the half that needs no assets: user-directory resolution and precedence, the
config chain's layering rules against synthetic config files, profile path construction including the
`ADELINE_MAX_PATH` ceiling, fingerprint stability, and the sidecar read/write round trip including
the absent-sidecar path. Each phase below lands both halves, so the CI half is not skipped merely
because the interesting half cannot run there.

## Sequencing

One PR. The commits are the review unit, in this order:

1. `--user-dir` and `LBA2_USER_DIR`, both call sites.
2. Move `dist_check.sh` and the automation tests off `XDG_DATA_HOME`.
3. `ISOLATION`, `NOPOLLUTE` and `REBIND` in the sweep. `REBIND` red.
4. Read the game-data config as a layer instead of copying it. `REBIND` green.
5. `--profile`.
6. Per-profile game-data pointer and manifest; the sweep switches from `--game-dir` to `--profile`.
7. Portable marker.
8. Boot provenance: how each path was resolved, plus SKU, declared `DistribVersion` and fingerprint.
9. The sweep reads identity from the banner instead of the `distrib` console command.
10. Docs.

Commits 1 to 4 are the load-bearing ones. Everything after is additive and can be dropped from the
PR without stranding what came before.

## Open decisions

**Default profile at the root, or `profiles/default/`?** Root means no migration and no chance of
saves appearing to vanish, at the cost of a permanently asymmetric layout. Moving everything is
cleaner and needs a one-time migration with a fallback. Leaning root. This is the one choice that is
expensive to revisit.

**Which banks feed the fingerprint?** It has to stay stable across a reinstall of the same release
and change when the data set changes. Needs a pass over the matrix before fixing the input set.

**How much of the banner is load-bearing?** The disc-image line is deliberately silent when no image
is mounted, so that a filesystem-only install produces a byte-identical banner. Adding identity lines
unconditionally gives up that property. Either accept it, or keep the new lines silent when they
carry nothing surprising, which is harder to reason about and easier to get wrong.
