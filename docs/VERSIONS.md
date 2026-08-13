# Version fields

The word *version* names six unrelated things in this codebase, and only one of them is about which
commercial release you are running. This document separates them, records exhaustively what each one
changes, and pins the release-identity story against what the pressed discs actually contain.

Nothing here is a proposal. It is a record of the original mechanism, plus the places the port has
touched it.

| Field | Storage | Identifies | Status |
|-------|---------|------------|--------|
| `DistribVersion` | `Version` key in `lba2.cfg` | Which publisher's edition this is | Live, 8 consumers |
| `Version_US` | `Version_US` key in `lba2.cfg` | Nothing | Read, never used |
| `NumVersion` / `NUM_VERSION` | First byte of a `.lba` save | Save layout revision | Live, see [SAVEGAME.md](SAVEGAME.md) |
| `SAVE_VERSION` | Nothing | Save routine lineage | Declared, never referenced |
| `XPL_HEADER.Version` | First field of a `.XPL` header | Palette/shading file format | Present on disk, never read |
| `Version` (the string) | Binary `.rodata` | The build | Port-added, see [RELEASING.md](RELEASING.md) |

## DistribVersion

### Where the value comes from

One read, in `ReadConfigFile`
([PERSO.CPP:2447](../SOURCES/PERSO.CPP#L2447)):

```c
// version distributeur
DistribVersion = DefFileBufferReadValueDefault("Version", UNKNOWN_VERSION);
```

Two things about that line carry most of the story.

The config key is `Version`, the global is `DistribVersion`. The key name says nothing about what it
holds, which is why an install assembled by hand tends to lose it silently.

Nothing detects the release. There is no asset fingerprint, no executable check, no probe of the
CD volume label. The engine believes whatever the config file says, and if the key is absent it
believes `UNKNOWN_VERSION`. The value flows in one direction only: installer writes it, engine reads
it. `WriteConfigFile` never writes `Version` back, so in the original the game cannot change its own
release identity.

The compiled-in default at [GLOBAL.CPP:21](../SOURCES/GLOBAL.CPP#L21) is `ACTIVISION_VERSION`, but
`ReadConfigFile` always overwrites it, so it only applies before the config is read.

### The six values

[DEFINES.H:200-206](../SOURCES/DEFINES.H#L200):

| Value | Constant | Publisher |
|-------|----------|-----------|
| 0 | `UNKNOWN_VERSION` | none declared |
| 1 | `ACTIVISION_VERSION` | Activision |
| 2 | `ACTIVISION_SUD_VERSION` | Activision, *sud* (French for south) |
| 3 | `EA_VERSION` | Electronic Arts |
| 4 | `VIRGIN_VERSION` | Virgin |
| 5 | `VIRGIN_ASIA_VERSION` | Virgin, Asia |

Three publishers, two of them with a territory split. The source never says which territories
`SUD` and `ASIA` cover, and it does not need to: as the next section shows, no branch in the shipped
code distinguishes 2 from 1 or 5 from 4.

### Every place it branches

Eight sites, and this is the complete list.

| Site | Branch | `{0, 3}` | `{1, 2, 4, 5}` |
|------|--------|----------|----------------|
| [PERSO.CPP:3250](../SOURCES/PERSO.CPP#L3250) | CD volume label to look for | `LBA2` | `TWINSEN` |
| [PERSO.CPP:3250](../SOURCES/PERSO.CPP#L3250) | `MessageNoCD` | `MESSAGE_NO_CD` | `MESSAGE_NO_CD_US` |
| [CONFIG.CPP:1463](../SOURCES/CONFIG.CPP#L1463) `AskForCD` | Insert-disc prompt | text id 7 | text id 6 |
| [DIRECTORIES.CPP:406](../SOURCES/DIRECTORIES.CPP#L406) | Voice folder on the CD | `lba2/vox/` | `twinsen/vox/` |
| [MUSIC.CPP:229](../SOURCES/MUSIC.CPP#L229) `InitTabTracks` | Track table | `TrackCD`, first CD track 6 | `TrackCDUS`, first CD track 0 |
| [GAMEMENU.CPP:1108](../SOURCES/GAMEMENU.CPP#L1108) `DrawCadreNewGame` | New-game panel sprite | 11 | 16 |
| [OBJECT.CPP:6122](../SOURCES/OBJECT.CPP#L6122) | In-game corner logo sprite | 11 at x-103 | 16 at x-110 |
| [GAMEMENU.CPP:4870](../SOURCES/GAMEMENU.CPP#L4870) `DistribLogo` | Boot splash | see below | see below |

The two messages are in [DEFINES.H:24-25](../SOURCES/DEFINES.H#L24): *"You need LBA2 CD, sorry!"* and
*"You need Twinsen's Odyssey CD, sorry!"*.

Seven of the eight ask the same yes/no question: is this `UNKNOWN` or `EA`, or is it anything else?
That is the European identity, where the game is *Little Big Adventure 2* and the disc is labelled
`LBA2`, against the Activision/Virgin identity, where it is *Twinsen's Odyssey* and the disc is
labelled `TWINSEN`. Every consumer is written as a `switch` with `UNKNOWN_VERSION` and `EA_VERSION`
cased together and everything else falling to `default`.

Only `DistribLogo` needs more than two answers, and it needs exactly three:

| Value | Splash | Resource |
|-------|--------|----------|
| 1, 2 | Activision | `PCR_ACTIVISION` (72) |
| 3 | Electronic Arts | `PCR_EA` (74) |
| 4, 5 | Virgin | `PCR_VIRGIN` (76) |
| 0 | none | no splash at all |

Resource indices come in pairs, image then palette, which is why they step by two
([COMMON.H:162-164](../SOURCES/COMMON.H#L162), and `ShowLogo` loading `numscr` and `numscr + 1`).

So the practical effect of the six constants is:

- 2 behaves identically to 1, and 5 identically to 4, everywhere.
- 0 behaves identically to 3 everywhere except the splash, which 0 skips.

The finer constants record a distinction the game was never asked to make. Since the value only ever
arrives from an installer-written config, the likeliest reading is that the granularity was for the
installers rather than the game.

### Which of those you can actually see

Two of the eight, on this port.

The boot splash is one, and the new-game panel sprite is the other.

The four CD-related branches are computed and then discarded. `InitCD` passes the volume name to
`OpenCD`, and the SDL backend at [CD.CPP:50](../LIB386/AIL/SDL/CD.CPP#L50) ignores the argument
entirely and returns a fixed drive letter, so the check always succeeds. The no-CD message, the
`AskForCD` prompt and the retry loop are unreachable, and the CD voice folder is only consulted when
a CD prefix is configured. Under the original MILES backend this was real: `OpenCD` walked drives A
to Z, opened each as a Red Book device, and compared the volume label against the name it was given
([MILES/CD.CPP:39](../LIB386/AIL/MILES/CD.CPP#L39)). That comparison is the one place the engine
ever cross-checked `DistribVersion` against the physical medium.

The music table selection no longer changes what plays: both tables hold the same music numbers and
differ only in a `JINGLE` flag that nothing reads now that `PlayMusic` routes everything through
`PlayJingle`. The header comment at [MUSIC.CPP:218](../SOURCES/MUSIC.CPP#L218) explains why the
tables were kept anyway, which is that they are original data recording which entries the US disc
pressed as Red Book audio. See [MUSIC.md](MUSIC.md) and
[DISC_IMAGE_SOURCE.md](DISC_IMAGE_SOURCE.md).

The in-game corner logo is gated on `DemoSlide`, so in a normal session it never draws. That gate is
what makes demo mode the only capture surface where `DistribVersion` visibly differs, which
[TESTING.md](TESTING.md) relies on.

## What the shipped releases declare

Verified against retail media, not inferred: config bytes dumped from the disc, volume labels and
directory trees read from the ISO 9660 descriptors.

| Release | Config header comment | `Version` | Volume label | Game directory |
|---------|----------------------|-----------|--------------|----------------|
| Retail CD, Activision | `(Activision)` | 1 | `TWINSEN` | `/TWINSEN/` |
| Retail CD, Electronic Arts | `(Electronic Arts)` | 3 | `LBA2` | `/LBA2/` |
| GOG | `(Electronic Arts)` | 3 | n/a | n/a |
| Steam Classic | no header comment | key absent | n/a | n/a |
| Demo | no header comment | key absent | n/a | n/a |
| [SOURCES/LBA2.CFG](../SOURCES/LBA2.CFG) in this repo | no header comment | key absent | n/a | n/a |

Two things follow.

The disc layout matches the branch it selects, on every axis the code touches. An Activision config
saying 1 sits on a disc stamped `TWINSEN`, whose game directory is `/TWINSEN/`, whose voice files
are therefore at `/TWINSEN/VOX/`, which is exactly the path
[DIRECTORIES.CPP:406](../SOURCES/DIRECTORIES.CPP#L406) builds for values 1, 2, 4 and 5. An EA config
saying 3 sits on a disc stamped `LBA2` with `/LBA2/` and `/LBA2/VOX/`, which is what the same line
builds for 0 and 3. The engine verifies none of this. The mastering was simply self-consistent,
which is why a mechanism that is pure declaration held up in 1997.

Both pressings keep `LBA2.CFG` inside the game directory next to `LBA2.HQR`, and both set `Version`.
The Activision one ships it fully populated, with language, volumes and key bindings already filled
in; the EA one ships a template with those fields blank.

Nothing tested carries 2, 4 or 5. `ACTIVISION_SUD_VERSION`, `VIRGIN_VERSION` and
`VIRGIN_ASIA_VERSION` are attested only in the source.

### Why modern installs report `unknown`

A fresh profile with no config is seeded from the config sitting beside the game assets, so on GOG
or a mounted retail image the medium tells the engine what it is. Steam Classic breaks that in two
independent ways: it keeps its config under
`%USERPROFILE%\Saved Games\2point21\tlba2-classic\Settings\lba2.cfg` rather than beside the assets,
so the seeding never reaches it, and that file has no `Version` key in any case. It has a
`SteamLanguage` key instead. Either way the answer is 0, which differs from 3 only in the splash.

The same happens to any install assembled by copying assets without the config. See
[GAME_DATA.md](GAME_DATA.md) and
[DISC_IMAGE_SOURCE.md](DISC_IMAGE_SOURCE.md#which-release-the-engine-thinks-it-is).

### Changing it

The `distrib` console command reports the current value and writes a new one
([CONSOLE_CMD.CPP:889](../SOURCES/CONSOLE/CONSOLE_CMD.CPP#L889)). It accepts either the number or
the name (`unknown`, `activision`, `activision_sud`, `ea`, `virgin`, `virgin_asia`), and it is the
only thing in the tree that writes the `Version` key.

It takes effect on the next launch, deliberately. `DistribVersion` is latched at boot into derived
state: `MessageNoCD` is copied, `PtrTrackCD` and `FirstCDTrack` are set by `InitTabTracks`, and the
CD voice path is resolved once and cached in a function-local static. Mutating the global at runtime
would change some of those and not others.

Editing the `Version` line in `lba2.cfg` by hand does the same thing.

## Version_US

A vestige. Declared at [C_EXTERN.H:139](../SOURCES/C_EXTERN.H#L139), defined as `TRUE` at
[GLOBAL.CPP:112](../SOURCES/GLOBAL.CPP#L112), and read from the config at
[PERSO.CPP:2351](../SOURCES/PERSO.CPP#L2351):

```c
Version_US = DefFileBufferReadValue("Version_US");
```

Nothing reads it after that. Not one branch in `SOURCES/` or `LIB386/`.

It is worse than merely unused. `DefFileBufferReadValue` is the no-default variant, which returns
-1 when the key is missing, and no shipped config sets `Version_US`. So the read reliably replaces
the initial `TRUE` with -1 on every launch, and nothing notices.

It predates LBA2. `lba1-classic` carries the same `extern LONG Version_US;` in its `C_EXTERN.H` with
no definition and no use anywhere, so the declaration was already dead when it was copied forward.
LBA1 has no distributor mechanism at all; `DistribVersion` and the six constants are LBA2 additions.

The name invites the assumption that it is the US/EU switch. It is not; that job belongs entirely to
`DistribVersion`.

## Other installer-written keys

Three more keys are written by the DOS installer and never read by the engine. They travel with
`Version` and are worth recognising when reading a shipped config.

| Key | Written by | Read by |
|-----|-----------|---------|
| `LanguageInstall` | installer | nothing |
| `Demo` | installer | nothing |
| `PathInstall` | installer | nothing |

The in-repo template leaves `Version` out on purpose. It is written into a profile only when the
game data ships no config of its own, and a value seeded that way would shadow the install
underneath it for good, which is exactly the snapshot the layered read exists to avoid. Absent
reads as `UNKNOWN_VERSION`, which is what it declared before.

`Demo: 1` in the demo's config is the clearest case. It looks like the flag that selects demo
behaviour, but the demo is a compile-time build (`-DDEMO`) and the key is inert. The demo config
also has no `Version` key, which is consistent: `DistribLogo` under `DEMO` skips the switch entirely
and shows resource index 6, which its comment calls the three-distributor bumper.

## Version numbers that are not release identity

### NUM_VERSION, the save layout revision

The first byte of a `.lba` save. Low 7 bits are `NUM_VERSION` (36), the high bit is `SAVE_COMPRESS`.
Constants at [COMMON.H:168-170](../SOURCES/COMMON.H#L168). It versions the serialisation layout and
has nothing to do with which release wrote the file. Fully covered in
[SAVEGAME.md](SAVEGAME.md); the engine-version-versus-`NUM_VERSION` distinction is in
[RELEASING.md](RELEASING.md#engine-version-vs-num_version).

The two collide by name and by habitat. `NumVersion` and `DistribVersion` are both globals with
*version* in the name, and both are partly config-driven from the same `ReadConfigFile`, since
`CompressSave` folds into `NumVersion` a hundred lines above where `Version` is read. They are
unrelated.

### SAVE_VERSION

`#define SAVE_VERSION 4` at [DEFINES.H:209](../SOURCES/DEFINES.H#L209). Nothing references it. The
matching comment at [SAVEGAME.CPP:516](../SOURCES/SAVEGAME.CPP#L516), *"Version 4 : LBA II only !!"*,
suggests it records which generation of Adeline's save routine this file implements rather than
anything the code decides on.

### XPL_HEADER.Version

First field of the `.XPL` palette and shading header
([COMMON.H:76](../SOURCES/COMMON.H#L76)). The header is mapped over the loaded file at
[AMBIANCE.CPP:457](../SOURCES/AMBIANCE.CPP#L457) and its offsets are used, but the `Version` field is
never read. A format version the loader chose not to check.

### The engine build version

`Version` the string, in [VERSION.CPP](../SOURCES/VERSION.CPP), is the boot banner and the
`--version` output. It is a port addition: the string is composed from `LBA2_PRODUCT_NAME` and
`LBA2_VERSION_STRING`, both generated by CMake from git describe. See
[RELEASING.md](RELEASING.md).

The name collides with the `Version` config key and with `DistribVersion`'s comment header, which is
worth knowing when grepping.

## Build variants

For completeness, the release axes that are compile-time rather than config-driven.

| Macro | Set by | Effect |
|-------|--------|--------|
| `CDROM` | [SOURCES/CMakeLists.txt:149](../SOURCES/CMakeLists.txt#L149) | Compiles the CD paths, so the volume-label and voice-folder branches are live |
| `DEMO` | `LBA2_BUILD_DEMO` CMake option (default OFF) | The 1997 playable demo: replaces `DistribLogo`'s switch, adds the demo bumper, forces the corner logo on, and swaps free saves for three fixed slots. Needs demo game data; see [GAME_DATA.md](GAME_DATA.md#the-1997-playable-demo) |
| `DEBUG_TOOLS` | CMake option | Skips the whole boot logo sequence, so no splash regardless of `DistribVersion` |
| `DEBUG_TOOLS`, `TEST_TOOLS` | CMake options | Enable the stale-save fallback to `LoadGameOldVersion` at [OBJECT.CPP:1373](../SOURCES/OBJECT.CPP#L1373) |
| `LBA_EDITOR`, `EDITLBA2` | editor build | Compiles the editor paths out of the game build |

`DEFINES.H` still carries the 1997 comments for these as commented-out `#define`s
([DEFINES.H:7-17](../SOURCES/DEFINES.H#L7)), including *"Cdrom Version si pas define version disk"*.
The build now defines `CDROM` externally, so the commented-out line in the header is misleading if
read on its own.
