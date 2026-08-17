# Release detection plan

`DistribVersion` is declared by the `Version` key in `lba2.cfg` and believed. Nothing detects it.
[docs/VERSIONS.md](../VERSIONS.md) records that as one of the six things called a version, and
[#485](https://github.com/LBALab/lba2-classic-community/issues/485) proposes closing the gap by
fingerprinting the data.

This plan starts one step earlier, from what the value decides, because that is what bounds how far
detection should be allowed to act on it.

## What the value decides

Six sites branch on `DistribVersion`. Five of them put `UNKNOWN` and `EA` on the same arm:

| Site | at `0` unknown | at `3` ea | at `1` activision |
|---|---|---|---|
| [GAMEMENU.CPP:1114](../../SOURCES/GAMEMENU.CPP#L1114) new-game panel | sprite 11 | sprite 11 | sprite 16 |
| [OBJECT.CPP:6131](../../SOURCES/OBJECT.CPP#L6131) attract-mode corner logo | sprite 11 | sprite 11 | sprite 16 |
| [CONFIG.CPP:1474](../../SOURCES/CONFIG.CPP#L1474) `AskForCD` wording | text 7 | text 7 | text 6 |
| [PERSO.CPP:3383](../../SOURCES/PERSO.CPP#L3383) CD volume label | `LBA2` | `LBA2` | `TWINSEN` |
| [MUSIC.CPP:230](../../SOURCES/MUSIC.CPP#L230) CD track table | EU | EU | US |
| [GAMEMENU.CPP:4876](../../SOURCES/GAMEMENU.CPP#L4876) `DistribLogo` | draws nothing | EA splash | Activision splash |

So the six-value enum is doing two unrelated jobs. Five sites ask one bit, European master or
American one, and the sixth picks a publisher splash. `ACTIVISION_SUD`, `VIRGIN` and `VIRGIN_ASIA`
exist only to land on the American arm and to name a splash; no site distinguishes them from
`ACTIVISION` in any other way.

`DistribLogo` matching no case at `0` is not an oversight to be repaired. `AdelineLogo` is a separate
call at [PERSO.CPP:3466](../../SOURCES/PERSO.CPP#L3466) and runs either way, so `0` means the Adeline
bumper and no publisher splash.

## What no declaration means today

`0` is therefore not an absence. Operatively it is European behaviour with no splash, which is the
right answer twice over for the re-release majority: those trees are byte for byte the EA master, and
a re-release has no publisher splash to show.

The case that is wrong is American data with no config. Measured on a US pressing extracted to a
folder, captured with a pinned clock so the frames are comparable:

| Tree | Config | Boot banner | Attract-mode frame |
|---|---|---|---|
| US pressing | its own, `Version: 1` | `activision (1) from the config` | American logo |
| US pressing | removed | `default (0)` | European logo |
| Re-release | ships none | `default (0)` | European logo |

The second and third frames are byte-identical. American data with no config renders as European, and
not approximately: the same pixels as a European tree. The corner logo is only the visible half. The
same branch also picks the European CD volume label and the European `AskForCD` wording, which
surface when a disc is asked for.

That is the whole problem worth solving. Everything else about `0` is already correct.

## The signal

Three banks separate the releases by byte size, but folded together they hide which axis each one
measures. `scripts/dev/fingerprint_distro.py` reports them separately and
[VERSIONS.md](../VERSIONS.md#reading-it-off-the-data-instead) records the corpus. In short:

| Bank | Values across the corpus | What it measures |
|---|---|---|
| `RESS.HQR` | 582445 on both Activision pressings, 582473 on the EA disc and every re-release | the master the copy was built from |
| `SCENE.HQR`, `TEXT.HQR` | three values each | the pressing, including locale |

`RESS` is invariant within a publisher. The European and Brazilian Activision pressings ship it byte
for byte while differing in both other banks, so a locale nobody has sampled still answers the master
question correctly. Three of its fifty entries carry the difference from the EA master, and none is
branding: 1 `RESS_FONT_GPM`, 44 `RESS_FILE3D`, 47 `RESS_IMPACT`. A master-build difference, which is
why a locale change does not move it.

That is exactly the bit the five behavioural sites ask for, and it is the only question the data can
answer. Which publisher's logo a release was licensed to show is not a fact about the assets.

Cost is one `stat`. [FILES.CPP:118](../../LIB386/SYSTEM/FILES.CPP#L118) routes `FileSize` through
`OpenRead`, so a mounted disc image is sized from its directory record with no data read, and
detection reaches inside an image as readily as a folder.

## Design

### Split the two jobs

Keep `DistribVersion` as the release identity. Add the detected master beside it as its own value,
with three states: European, American, unrecognised. The five behavioural sites are then asking a
question that has a name.

### The splash gets its own switch

The splash is the only thing that makes adopting a detected value unsafe, so take it out of the way
rather than working around it. A separate switch, defaulting to whether a human declared the release:

| Config | Release identity | Splash |
|---|---|---|
| declares a `Version` | as declared | shown |
| declares nothing, data reads European | default `0` | none |
| declares nothing, data reads American | detected American | none |
| declares nothing, data unrecognised | default `0` | none |

This reproduces today's behaviour exactly for every install on hand, changes nothing for the
re-release majority, and fixes the American tree that has no config. A config key overrides the
default in either direction for anyone who wants the splash back or gone.

Defaulting the switch to on for a detected value would be the wrong choice: it would put a publisher
splash in front of players whose release deliberately ships none, on the strength of a size table.
Defaulting it to off for a declared one would delete something an installer explicitly asked for. The
declaration is what separates the two cases, so it should be what sets the default.

### What detection may and may not do

It may answer the master question when nothing declared one. It may never override a declaration.
Three reasons: the original contract is that the installer states the release; `DistribVersion` is
latched at boot into derived state (`MessageNoCD`, `PtrTrackCD`, `FirstCDTrack`), so a value that
arrived later would reach some of those and not others; and a table built from a handful of samples
should not outrank something a human wrote down.

Unrecognised has to be an ordinary outcome. Sizes are a weak hash, the sample is small, and
`ACTIVISION_SUD`, `VIRGIN` and `VIRGIN_ASIA` are attested only in the source. A tree that matches
nothing keeps today's behaviour rather than being guessed at.

### Reporting

One line, operative value first and observation second, so the wanted state does not read as a fault:

```
Release    default (0), data is ea
Release    activision (1) from the config, data agrees
Release    ea (3) from the config, data is activision
Release    default (0), data unrecognised
```

The third line is the case nothing catches today: one publisher's splash and panel sprite drawn over
another's data, exiting 0 the whole time.

## While in here: a dead key

`Version_US` is read from the config into a global that nothing consumes, and
[VERSIONS.md](../VERSIONS.md#version_us) has the full account: it was already dead in `lba1-classic`
before being copied forward, and because the read uses the no-default variant it reliably replaces
its `TRUE` initialiser with -1 on every launch.

It matters here only because of the name. It reads as the US/EU switch this plan is about and is not
one, so a second concept landing beside it needs to be named in a way that cannot be confused with
it, or the dead key should go at the same time.

## Testing

The half that needs no assets, and therefore runs in CI: the size-to-master table, including the
unrecognised path; the precedence rules as a truth table over declared and detected; and the splash
default deriving from declaredness.

The half that needs installs stays local, in `scripts/dev/dist_check.sh`, which already gives each
install a throwaway profile and captures the attract-mode frame where the difference shows. Two rows
to add: the detected master beside the declared release, and a config-less copy of an American tree,
which is the case with no coverage today and the one the plan exists for.

The measurement in this document was made that way and is reproducible: extract a pressing to a
folder, capture the attract frame with a pinned clock, remove the config, capture again.

## What is built

All of the design above, in [DISTRIB.CPP](../../SOURCES/DISTRIB.CPP), with the boot-time measurement in
`ReadConfigFile` and the splash gate in `DistribLogo`. The rules are pinned by
[tests/distrib](../../tests/distrib/test_distrib_resolve.cpp), which needs no assets and runs in CI, and
by [test_release_detection.sh](../../tests/automation/test_release_detection.sh), which needs an install
and checks that the measurement happens at all, reaches inside a mounted disc image, and is compared
against a declaration rather than standing in for one.

`scripts/dev/dist_check.sh` gains a `DATA` column beside `DISTRIB` and a sixth assertion, `MASTER`,
so an install whose assets contradict its own config fails the sweep instead of being a line in a
log. [VERSIONS.md](../VERSIONS.md#reading-it-off-the-data-instead) is the reference; this document is
the reasoning behind it.

## Left out

`Version_US`, which is dead rather than wrong, and removing a config read is its own change with its
own blast radius.

## Open decisions

- Whether the detected master should also be allowed to answer for a tree whose declaration is
  present but unparseable, which is neither declared nor undeclared.
- Whether the splash switch wants a CLI flag beside the config key. A key is what an installer or a
  packager sets; a flag is what a player tries once.
- Whether an unrecognised tree is worth saying anything about on the release line at all, or whether
  it should read the same as no data at all so the line stays quiet in the ordinary case.
