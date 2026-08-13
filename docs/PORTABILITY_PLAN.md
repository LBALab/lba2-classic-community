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
compiled defaults  <  <resDir>/lba2.cfg  <  <profile>/lba2.cfg  <  CLI
```

Writes only ever reach the profile config.

The bottom is the default each read site already names, not the built-in template. The template
stays a seed, written once into a profile whose install ships no config of its own. As a layer its
values would also reach every install that ships a config but leaves a key out, changing settled
defaults (`FullScreen` among them) on machines that never saw it. The split this needs already exists in the code:
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

`--profile <name>`, or `LBA2_PROFILE`, resolving to `<userDir>/profiles/<name>/`, with its own `save/`, `lba2.cfg`,
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

Built, with `profile:` as the one key the file carries.

**The rule.** A file named `portable.txt` beside the binary makes the tree self-contained: the user
directory becomes `User/<build>/` next to the binary instead of the per-user path SDL picks, where
`<build>` is the same token the per-user path uses, so `User/LBA2/` mirrors `Twinsen/LBA2/`. An empty
file is the whole switch; a `profile:` line names the profile that tree uses by default. An empty file
is the whole feature. Dolphin uses exactly this name and shape, and ScummVM does the same thing by
looking for its ini beside the binary, so it is a convention players already recognise.

Its name is matched whatever the case. A player types this file by hand on a filesystem that ignores
case and then carries the tree to one that does not, and the four case variations the engine probes
for shipped assets do not cover `portable.TXT`. A miss is silent, so the tree simply stops being
portable with nothing said.

Beside the **binary**, not the working directory. A shortcut, a file manager or a launcher each set
a different working directory, and the one thing this file has to be is stable. Discovery already
distinguishes the two, with `next to the binary` as its own probe label.

**Where it sits in the precedence.** Slotted in just above the per-user default:

```
--user-dir  >  LBA2_USER_DIR  >  portable.txt  >  per-user default
```

Every explicit input still wins, so the file changes only the fallback and cannot surprise someone
who set the environment variable deliberately. The alternative, letting the marker beat the
environment, argues that a stick should keep its data with it whatever the host machine says. That
reading is defensible and it is a worse default: it makes a file the player may have forgotten about
override something they typed.

`--profile` composes as it does with `--user-dir`, giving `User/<build>/profiles/<name>/`.

**What happens when the tree is read only.** A marker on a CD, a read-only mount or a system install
directory names a folder that cannot be created. The rule already set for `--user-dir` applies: stop
the boot naming the path, rather than quietly falling back to the per-user directory. A run that
wrote somewhere other than where it was told is the failure this whole layer exists to prevent.

**Not shipped by default.** A release bundle that carried `portable.txt` would make every install
portable, including one unzipped into a system directory. It is something the player creates, or
something a deliberately packaged portable edition includes.

#### The open question: what, if anything, goes inside

Empty means default portable behaviour. Whether a non-empty file means anything is undecided. The
candidates, and what each is actually worth:

| Key | What it would do | Worth it? |
|---|---|---|
| `profile:` | the default profile for this tree | **Strongest candidate.** It is the one thing no flag can express: "this tree is my GOG setup", travelling with the tree rather than with a shell alias |
| `gameDir:` | where the data is, relative to the binary | Weak. Discovery already probes next to the binary and a `Common/` under it, which is the layout a bundle would use anyway |
| `disc:` | which image in the bundle to mount | Weak. Same reasoning, and `--disc` covers the case of several |
| `userDir:` | somewhere other than `User/` | Argues against itself. A portable tree that writes elsewhere is not portable, and `--user-dir` already says that better |

`profile:` is in. The rest of that list is not, and stays out: each duplicates a flag, and a file
whose keys are all flag aliases is a second way to say the same thing that then has to be kept in
step.

It sits below both the flag and the environment, because the marker belongs to the tree while those
belong to a run. A name the marker cannot use stops the boot rather than landing quietly in the
default profile. Anything else in the file is still reserved and still unread.

If keys do arrive, they should read through the same layered mechanism as everything else rather
than growing a private parser.

#### To settle before building

**The demo SKU gets its own folder.** Settled, and generalised: rather than a sibling `User-Demo/`,
the portable root holds one directory per build, `User/LBA2/` beside `User/LBA2-Demo/`. Same token,
same shape as the per-user path, and a second game hosted on this engine is another value of it
rather than a third naming scheme. See what a second game does to this, below.

**The sweep gets no row for it.** `ISOLATION` is per-install and portability is per-tree, so the two
do not share a shape. `tests/discovery` covers the predicate instead: unmarked, marked, a directory
wearing the marker's name, and a base path with and without its separator.

### Saves the install ships

`<gameDir>/SAVE/` is where the DOS engine wrote, so every install has the folder. Retail ones leave
it empty. The 1997 demo fills it with `DEMO0`, `DEMO1` and `DEMO2`, the three sections its fixed save
slots are built to load, plus a `CURRENT`.

Nothing looked there. `SetDemoSaveGame` builds its path through `GetSavePath`, which resolves under
the user directory, so on any profile the three slots pointed at files that did not exist and the
demo's sections were unreachable. True in every mode, not only a portable one.

A profile is now handed those saves the first time it is used. Copied rather than read in place,
because they are slots the player saves back into and the install may be a mounted image or
read-only media. Keyed on the save folder not existing yet, which is the only moment a profile has
never been used: seeding on a missing file instead would raise saves the player had deleted, on
every launch.

The rule is general and the demo is simply the case with something to copy. A retail install ships
an empty folder, so it is a silent no-op there.

Not covered: an install whose saves exist only inside a disc image. The seeding enumerates a
directory, which it does through the filesystem.

Measured before leaving it out. None of the four retail images to hand carries a single `.lba`, two
Activision pressings and two EA, and there is a structural reason: a pressed disc is the shipped
product, and saves are made by the player on the hard disk. The `SAVE/` folder on an extracted
install is put there by the installer, which is why it is empty.

The one distribution that ships saves is the demo, and it went out on covermount discs, so a rip of
one is the case that could turn up. Covering it would not need the enumeration: `Copy` opens through
`OpenRead`, which already falls through to a mounted image, so trying the three slot names the demo
build already knows would find them on either medium in about ten lines. Worth doing when a demo
image appears, and not before, since nothing attests one.

### What a second game does to this

LBA1 is the north star for the engine/game split, and it changes one of the two roots. Written down
now because the cheap moment is before anyone has a portable tree.

**The per-user path already handles it; the portable path did not.** `SDL_GetPrefPath("Twinsen",
"LBA2")` puts the build in the path. `portable.txt` resolved to a flat `User/`, so a tree holding two
builds had them share it. The portable root now mirrors the per-user one:

```
Twinsen/LBA2/          Twinsen/LBA2-Demo/          per-user
<tree>/User/LBA2/      <tree>/User/LBA2-Demo/      portable
```

One token, `ADELINE_PREF_APP`, keys both. A second game is another value of it and needs no second
scheme.

**The ordering is the real constraint.** The user directory is resolved at
[PERSO.CPP:3018](../SOURCES/PERSO.CPP#L3018), the log opens at 3032, and the game data is not
discovered until 3070. So the token has to be known before anything has looked at a single asset.
That is free while it is compile-time, and it is the fork worth deciding deliberately:

- **One binary per game**, as the demo SKU already is. The constant extends, the ordering holds,
  nothing here moves.
- **One binary hosting either, chosen from the data.** Then the user directory cannot be resolved
  until after discovery, and the boot log has nowhere to live in the meantime. That log was moved
  *earlier* on purpose, so the "we cannot find your game data" diagnostics reach a file, which is
  exactly when a persisted log matters most. Reversing that is the cost, and it is not small.

**Markers do not discriminate on their own.** Comparing the banks a 1994 LBA1 install ships against
an LBA2 one:

| | Banks |
|---|---|
| LBA1 only | `FILE3D`, `INVOBJ`, `LBA_BLL`, `LBA_BRK`, `LBA_GRI`, `MIDI_MI`, `MIDI_SB` |
| Shared | `ANIM`, `BODY`, **`RESS`**, `SAMPLES`, `SCENE`, `SPRITES`, `TEXT` |
| LBA2 only | `ANIM3DS`, `HOLOMAP`, `LBA2`, `LBA_BKG`, `OBJFIX`, `SCREEN`, `SPRIRAW` |

`RESS.HQR` is shared, and it is what the demo build validates on. A demo binary already accepts a
retail LBA2 folder for that reason; an LBA1 install would be a third folder it accepts. Whatever
marker LBA1 takes should come from its own column, `FILE3D.HQR` being the obvious one, and the demo's
marker deserves revisiting at the same time.

**LBA1 has no release identity to report.** It ships `LBA.CFG` in the same DefFile format with the
same `Language` and `LanguageCD` keys, so the layered read carries straight over. It has no
`DistribVersion`: the mechanism and its six constants are LBA2 additions, and `lba1-classic` carries
only the same dead `Version_US` extern. A US and a EU pressing exist commercially with nothing in
the config to tell them apart, so the banner's `Release` line has no answer for LBA1 and should be
absent rather than say `unknown`, which would claim a mechanism exists.

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

| Line | Before | Now |
|---|---|---|
| `Assets:` | path plus winning probe | unchanged |
| `Disc:` | image name, when mounted | unchanged |
| `Saves:`, `Config:`, `Log:` | bare paths | a `Writes:` line under them naming the profile and how the user directory was chosen |
| `Config` (the later block) | path only | which layers supplied the values |
| `Release` | absent | the declared `DistribVersion`, and whether anything declared it |

`Writes:` is silent when neither a profile nor an override is in play, so an install that names
neither keeps the banner it always had.

The data fingerprint is **undecided**, not rejected. It is out of the first change because what it is
for has not been settled, not because it would not work.

Measured across the six installs to hand, folding the sizes of a few banks does discriminate:

| Banks | Behaviour across the six |
|---|---|
| `RESS`, `TEXT`, `SCENE` | GOG, Steam and both EA discs identical; each Activision pressing different, and different from each other |
| `SPRITES`, `BODY`, `ANIM`, `LBA2` | byte-identical on all six, so they carry no signal |

So the four releases that are the same data fold to one value, and the two that are not fold to two
others. Sizes are a `stat` each and reach inside a mounted image as readily as the filesystem, since
`FileSize` goes through `OpenRead`.

What that leaves is the question underneath, and the two answers want opposite designs:

- **Change detection**, "is this the same data set as last time?", wants every cheap input it can
  get, so a mod, a patch or a half-finished copy shows up.
- **Release detection**, "which pressing is this?", wants exactly the three discriminating banks,
  and is a larger step than a banner line: nothing in this engine has ever detected a release.
  VERSIONS.md's finding is that identity is declared and believed. A fingerprint that could tell an
  Activision disc from an EA one without reading the config would make "your config says EA and this
  data is not" a thing the engine could notice, which is a capability, not a report.

Two things follow. `dist_check.sh` used to run the `distrib` console command to learn the release
identity; with it in the banner, one boot and one grep covers the whole row, and any run in that
file can be asked rather than only the one built to ask. And the matrix's isolation checks
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
diffable summary. Extend it rather than building a second harness. Five checks:

Each install gets a profile inside one throwaway user directory, rather than a directory each. That
is the arrangement profiles exist for, and it puts every run through profile path composition and the
game-dir binding, which nothing else exercises.

| Check | Asserts | Reported as |
|---|---|---|
| `IDENTITY` | the release the engine reports matches the `Version` the install's own config declares | a column, `-` for a disc image |
| `WROTE` | nothing under the game directory is written | a column |
| `BOUND` | naming the profile and nothing else finds the folder that profile was given | a column |
| `ISOLATION` | the developer's own user directory is byte-identical before and after the sweep | one line after the table |
| `REBIND` | a profile seeded from one install, re-run against another, reports the second one's `Version` | one line after the table |

`REBIND` is what motivated layer 2, and it was measured rather than inferred. A profile seeded from
an install declaring 3, pointed at a medium declaring 1, kept reporting `3 (ea)` while booting data
that declares 1, because the config was copied on first run and never revisited. The layered read
closes it: the same profile now reports 1 there and 3 back on the first install.

`WROTE` found something nobody was looking for, on its first run. An install whose voices sit on the
filesystem rather than inside a disc image had their mtime stamped by playing them: the 1997 CD
cache keeping a last-used mark so the oldest copy could be evicted when the disk filled, still
firing on voices that were never copied and cannot be evicted. Content untouched, only the
timestamp, which is why nothing had noticed. Fixed in
[MESSAGE.CPP](../SOURCES/MESSAGE.CPP), and the row is green.

That is the case for the check rather than an aside. It cost a directory hash before and after each
install and found a live defect the same day, in code nobody was reading.

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

**What is the fingerprint for?** The pass over the matrix is done and the input set is answered:
`RESS`, `TEXT` and `SCENE` carry all the discriminating signal across the six installs on hand, and
`SPRITES`, `BODY`, `ANIM` and `LBA2` are byte-identical on every one. What is not answered is
whether the job is change detection or release detection, and that decides the input set rather than
the other way round. Not in the first change for that reason alone.

**How much of the banner is load-bearing?** The disc-image line is deliberately silent when no image
is mounted, so that a filesystem-only install produces a byte-identical banner. Adding identity lines
unconditionally gives up that property. Either accept it, or keep the new lines silent when they
carry nothing surprising, which is harder to reason about and easier to get wrong.
