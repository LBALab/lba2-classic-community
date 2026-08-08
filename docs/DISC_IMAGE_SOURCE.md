# Disc-image resource source (implementation guide)

**Status:** Approved plan, implemented as a commit sequence on `feat/disc-image-source` (one PR).
Makes installs that ship retail assets inside a raw ISO/BIN disc image playable without
extraction. The concrete near-term target is the GOG "Original Edition" ([issue #119](https://github.com/LBALab/lba2-classic-community/issues/119)),
tested against `../LBA2-GOG`; the plumbing scales to LBA1 later.

**Naming.** The format is a **raw ISO/BIN disc image** (MODE1/2352, ISO9660 inside). GOG's `.gog`
is merely that distributor's extension for the BIN, so nothing here is named "gog": the image is
detected by **content** (the ISO9660 "CD001" PVD at logical sector 16), accepting any extension or
a `.cue`/`.dat`.

## Architecture
- **Data:** a resource-source fallback at the `LIB386/SYSTEM/FILES.CPP` `OpenRead/Read/Seek/Close`
  seam. POSIX-open first; on miss, read from a mounted disc image. The whole HQR pipeline
  (`HQFILE.CPP HQF_Init` to `OpenRead`) and `ASSET_PREFLIGHT` inherit it. Order: extracted files,
  then disc image, then missing.
- **Music:** all music in the SDL build funnels through `STREAM.CPP PlayStream` (jingles via
  `MUSIC.CPP PlayJingle`; "CD" tracks via `CD.CPP PlayCD`, which just `PlayStream`s a
  `music/TrackNN.wav`). `PlayStream` already resolves WAV first, then `.ogg`/`.OGG`/`track<N>.ogg`
  fallbacks (full installs ship `.ogg`). The gap for a disc image is that `PlayStream` loads via
  `SDL_LoadWAV` / `stb_vorbis_open_filename` / `fopen`, which bypass the `OpenRead` seam, so in-BIN
  music is unreachable. The fix is to bridge the stream loader to the image (below), not to add
  format handling.
- **Readers:** an `ISO9660` reader (data reads). For music, a cue parser + audio-track locator is
  needed only for a true in-BIN CD-DA source (below); both SDL-free engine modules under
  `LIB386/SYSTEM/`.

## Music: bridge the stream loader to the image (do not extract)
LBA2-GOG's music lives as container files inside the BIN data track (`/LBA2/MUSIC/JADPCM*.WAV`
jingles and `TADPCM1-5.WAV` tracks), plus one external `LBA2.OGG`. Because the engine already
requests `*.WAV` and falls back to `*.ogg`, the disc-image music path is just: when both the
filesystem WAV and the filesystem OGG fallback miss, read the file's bytes via
`DiscImage_OpenRead`/`Read` and decode from memory (`SDL_LoadWAV_IO` for WAV,
`stb_vorbis_open_memory` for OGG). This reuses the data seam and the existing resolution order; no
extraction, no temp files, no new format handling.

**External cue audio (redbook track shipped as a standalone file).** GOG keeps music tracks 1-5 in
the data image (`TADPCM1-5`) but ships track 6 as the external `LBA2.OGG`, listed in the cue
(`LBA2.DAT`) as its one `TRACK AUDIO` entry. The engine requests `music/TADPCM6.WAV`, which is absent
from both the filesystem and the image. So the stream loader has a final source: for a redbook
"track" request (`TADPCM<N>`/`Track<N>`, never a jingle) that missed everywhere, it parses the
install's cue for the first external (non-BINARY) audio file and plays it (`LBA2.OGG` is OGG, decoded
by the existing OGG path; a WAV cue track would load via `SDL_LoadWAV`). The cue reader is a small
SDL-free module (`LIB386/SYSTEM/CUE.CPP`, `Cue_ParseExternalAudio` / `Cue_ResolveInDir`), reused
later by the CD-DA source. On GOG only track 6 is missing and the cue has exactly one external audio
file, so the mapping is unambiguous.

A separate **in-BIN CD-DA passthrough** source (raw 16-bit LE stereo 44.1 kHz PCM in 2352-byte audio
sectors, located via the cue `INDEX`, streamed straight into the audio sink, cueplay-style) is only
needed for a true rip whose cue lists AUDIO tracks *inside* the BIN (a pure CD rip, and likely the
LBA1 disc). The cue reader deliberately skips those (AUDIO under a BINARY file), since they are this
separate source's job, not an external file.

## Commit sequence (one branch, one PR)
1. **docs:** this guide.
2. **reader + host test:** the `ISO9660` reader as a first-class engine module
   (`LIB386/H/SYSTEM/ISO9660.H` + `LIB386/SYSTEM/ISO9660.CPP`, compiled into the `sys` library). A
   `host_quick` test (`tests/iso9660`) against a synthetic in-test ISO9660 fixture: `iso_open`
   detects 2352-raw vs 2048-cooked, `iso_read` returns a known file by path, `iso_walk` enumerates
   it, a non-ISO file is rejected. (Mirrors the `VOC_HEADER` pure-unit + `tests/voc_header` pattern.)
3. **ISO9660 streaming reads (`iso_stat` / `iso_pread`) + host test.** Lazy extent access so a file
   is streamed off the image rather than slurped: `iso_stat` resolves a path to its (LBA, size) with
   no data read, `iso_pread` reads an arbitrary byte range. Needed because `VIDEO.HQR` is ~233 MB, so
   opening it (and answering `FileSize`) must not pull the whole file into RAM. `tests/iso9660` grows
   a multi-sector fixture covering cross-sector reads and EOF clamping.
4. **mount + `OpenRead` intercept (data) + preflight + boot-log `Disc:` line.** A `DiscImage` manager
   (`LIB386/SYSTEM/DISCIMG.CPP`) content-probes the install dir for an ISO9660 image, locates the
   asset-root subtree by finding the resource marker (`lba2.hqr`) inside it (CD masters nest under a
   volume dir, e.g. `/LBA2`), and resolves any path under the mount base by stripping the base and
   resolving the remainder beneath that root, so it serves *all* assets (the marker HQR included),
   not just media. `OpenRead` falls back to it (filesystem first); tagged virtual handles stream via
   `iso_pread` and dispatch in `Read`/`Seek`/`Close`; `FileSize` (via Seek-to-end, no slurp) and
   `ExistsFileOrDir` (incl. the `music/` and `vox/` dir checks) become image-aware. `host_quick`
   (`tests/disc_image`) against a synthetic nested image; verified by headless smoke against
   `../LBA2` (no image: no `Disc:` line, banner unchanged) and `../LBA2-GOG` (mounts `LBA2.GOG`,
   630 files, `Assets all present` with FMV/voices resolved from the image).
5. **music from the image:** bridge `STREAM.CPP`'s loader to the disc image. When both the
   filesystem WAV and the filesystem OGG fallback miss, `PlayWavFromImage` reads the file's bytes via
   `DiscImage_OpenRead`/`Read` and decodes the WAV from memory (`SDL_LoadWAV_IO` over
   `SDL_IOFromConstMem`). The WAV/OGG resolution order already exists; this only adds the image as a
   final source. Covers LBA2-GOG's in-BIN `JADPCM*`/`TADPCM*` music, which are IMA-ADPCM WAV that
   SDL decodes. In-BIN OGG is not needed for LBA2-GOG (its `LBA2.OGG` is an external file) and the
   trimmed `stb_vorbis.h` exposes no memory entry point, so OGG-from-image is a deferred extension
   (declare `stb_vorbis_open_memory` and feed the decode thread). The chosen WAV source is logged via
   the existing `[MUSIC]` trace (gated by the audio log toggle).
6. **external cue audio:** a redbook track shipped as a standalone file (GOG's `LBA2.OGG` = track 6,
   absent from the data image). Adds an SDL-free cue reader (`LIB386/SYSTEM/CUE.CPP`, host test
   `tests/cue`) and a final stream-loader source: a redbook "track" request that missed the
   filesystem and the image is satisfied by the cue's first external (non-BINARY) audio file. Verified
   on `../LBA2-GOG`: `playmusic 6` resolves `music/TADPCM6.WAV` to the install's `LBA2.OGG`. (The
   in-BIN CD-DA passthrough source is still separate and only for a true rip / LBA1; see above.)
7. **(later) LBA1:** markers / game-id; LBA1's combined music+data image is already covered by the
   PCM-passthrough source. Orthogonal to the `GameProfile`.

A remaining increment for a pure rip (only the disc image present, nothing extracted): discovery
validates the install dir by finding `lba2.hqr` *before* the mount, so a dir with only the image
does not yet pass `IsValidResourceDir`. Making discovery probe a candidate dir's image closes that
gap; today's target (`../LBA2-GOG`) ships the HQRs extracted, so discovery succeeds and only the
media comes from the image.

## Boot log
Today's happy-path banner is a header block (`Assets:/Saves:/Config:/Log:`) then an aligned status
block (`Events/Joystick/Audio/Display/Assets/Language`) and a `Ready in ...` line. The mount is
**visible only when it happens, silent otherwise**:

- On mount, one header line beside `Assets:`, naming the image by basename (it lives in the assets
  dir already stated above) and reporting that it is an ISO9660 image rather than a plain file:
  ```
  Assets: /path/to/install/
  Disc:   mounted LBA2.GOG  (ISO9660, 630 files)
  ```
- No image found: no `Disc:` line, banner unchanged (no "none" noise).
- The full image path and the in-image asset root are logged at `DEBUG` from the mount
  (`DISCIMG: mounted <path> (asset root '<root>', <N> files)`). Debug is off by default, so this
  stays out of the banner and normal output; run with `--log-level debug` (or `loglevel debug`)
  to surface it in `adeline.log`, the terminal, and the console at once.
- The `Assets   all present` status may note disc-sourced assets only when relevant; per-resource
  source (FS vs disc) stays at debug level (same gating).
- CD-music source is logged at info/debug, not in the concise banner.
- Additive only: existing discovery/preflight lines (which the `Control_*` harness and tooling may
  parse) are left byte-identical.

## Testing
- **`host_quick` (CI, no retail data):** the iso9660 reader vs a synthetic fixture, including
  malformed and truncated images (a record whose name runs off its extent, a root that points past
  EOF) that must stop cleanly rather than read past their buffers (clean under the sanitizer preset);
  the cue parser vs sample cue text; a resolution unit (FS-hit / image-fallback / miss) against a
  fake source. Each commit ships its test.
- **Retail smoke (local, opt-in, like the Docker tests):** `Control_*` headless boot of `../LBA2`
  (regression baseline, FS-only) and `../LBA2-GOG` (the new path): assert assets load and the boot
  is clean. Dev helper: extend `scripts/dev/iso_bin.py` to `iso_walk` an image and diff against
  expected.

## Robustness (the throughline)
No image, filesystem-only (today's behaviour, no regression). Missing asset, the existing preflight
path. Missing music, silent (CD audio was always optional). Any mount or cue-parse failure degrades,
never crashes.

# Retail disc support (assessment and plan)

The shipped source above targets the GOG "Original Edition" layout: HQRs extracted on the
filesystem, media inside one BIN, music as in-image WAV plus one external OGG. This section
assesses a genuine *retail CD rip* against it, and plans the remaining work.

Reference dump: `../TWINSEN`, the US "Twinsen's Odyssey" (Activision) CD, dumped three ways.
Baseline for no-regression: `../LBA2-GOG` (GOG Original Edition) and `../LBA2` (Steam Classic).

## What the retail dump actually is

| File | Format | Layout | Mounts today |
| --- | --- | --- | --- |
| `TWINSEN.mdx` | DAEMON Tools `MEDIA DESCRIPTOR` 2.1 | 64-byte header, data track cooked 2048 at +64, audio raw 2352 at +424679488, 816-byte **encrypted** descriptor at +625425040 | **no** |
| `TWINSEN.bin` | raw `MODE1/2352` | 292714 sectors: data track 0..207362, then 85351 audio sectors (18:58.01) | yes |
| `TWINSEN.iso` | cooked 2048 | 207363 sectors, the data track only, no audio | yes |
| `TWINSEN.cue` | cue sheet | one `MODE1/2352` data track plus **one merged** `TRACK 02 AUDIO` at 46:04:63 | n/a |

**Provenance.** The `.mdx` is the original rip; the `.bin`/`.iso` were derived from it offline
(notes kept with the dump). The `.iso` is a byte slice of the data track past the 64-byte header.
The `.bin` regenerated the per-sector sync, header, EDC and ECC that a cooked image discards, which
are deterministic functions of the sector address and its 2048 payload, and copied the audio region
untouched. That layout was re-derived independently here from the raw bytes and matches. The
practical consequence: **the `.mdx` is not a recovery path for anything the `.bin` lacks.** Its
audio region is the same 85351 sectors, and the only thing it holds that the conversion could not
carry over is the track table, which is inside the encrypted footer.

Disc identity: volume label `TWINSEN`, assets nested under `/TWINSEN` (not `/LBA2`), `TWINSEN.EXE`
rather than `LBA2.EXE`, and the disc's own `LBA2.CFG` carries `Version: 1`, which is
`ACTIVISION_VERSION` in `DEFINES.H`. English only: 13 `.VOX` files (`EN_*`) against GOG's 39, and
no per-language subdirectories.

Data payload versus GOG, compared by md5 inside both images:

- **Byte-identical:** `LBA2.HQR`, `BODY.HQR`, `ANIM.HQR`, `ANIM3DS.HQR`, `SPRITES.HQR`,
  `SPRIRAW.HQR`, `OBJFIX.HQR`, `SCREEN.HQR`, `HOLOMAP.HQR`, `LBA_BKG.HQR`, and the `.ILE`/`.OBL`
  island pairs.
- **Differs:** `RESS.HQR`, `SCENE.HQR`, `TEXT.HQR`, `SAMPLES.HQR` (same size, different content),
  `VIDEO.HQR` (231403412 vs 233966456 bytes).

Entry counts match on every differing HQR (`TEXT` 181, `RESS` 50, `SCENE` 224, `SAMPLES` 896,
`VIDEO` 35), so the differences are localisation and master revision, not a different container
layout. Nothing here challenges the frozen `TEXT.HQR` contract in [TEXT.md](TEXT.md).

## What the retail rip already worked with

Before any of the work below, pointing the engine at a directory holding `TWINSEN.bin` plus an
extracted `LBA2.HQR` (needed only to get past discovery, gap 1) mounted and played:

```
Disc:   mounted TWINSEN.bin  (ISO9660, 632 files)
[INFO] Assets     all present
```

Every HQR, `.ILE`/`.OBL`, `VIDEO.HQR` and the `.VOX` files resolved through the `OpenRead` seam out
of the raw BIN, the asset root was found from the marker (`/TWINSEN/LBA2.HQR` gives root `TWINSEN`),
and the game rendered normally at 1920x1080. In-image jingles played. `TWINSEN.iso` behaved the same
minus the audio track. So the ISO9660 reader, the asset-root heuristic and the data seam already
handled a real retail master; the nesting difference (`/TWINSEN` versus `/LBA2`) cost nothing,
because the root is found by marker rather than by name.

## Gaps the rip exposed

All five are now closed. Listed in the order they bite a user, with the commit that fixes each.

1. **Discovery rejected a pure dump.** `IsValidResourceDir` required `lba2.hqr` on the filesystem
   and ran before `DiscImage_Mount`, so a directory holding only the image was refused with
   "does not hold the game data". It now falls through to `DiscImage_DirHoldsMarker`, which probes
   the folder for an image and looks for the marker inside it: the same question the mount answers
   moments later, asked one step earlier. A directory holding nothing but `TWINSEN.bin` and
   `TWINSEN.cue` boots and renders.
2. **No CD-DA source, so every theme was silent.** On the retail disc the themes are Red Book
   audio, not files: `/TWINSEN/MUSIC` ships `JADPCM02-18` and `LOGADPCM` only, and requests for
   `music/TADPCM<N>.WAV` ended at `Could not find`. `PlayStreamInternal` gained a source that looks
   up the music's CD track number, reads the cue as a table of contents, and plays that track:
   either a standalone file the cue names, or audio sectors read straight out of the image. Audio
   sectors are already 16-bit LE stereo 44.1 kHz PCM, so they go to the same stream setup a WAV
   uses and inherit its volume, pause and resume behaviour.
3. **`.mdx` was not recognised.** `iso_open` probed sector 16 at `16*2352+16` and `16*2048`, both
   assuming the image starts at byte 0; the MDX's descriptor sits at byte 32833 behind its 64-byte
   header, and the failure mode was a wall of `missing required asset` naming no cause.
   `iso_open_ex` adds an opt-in scan of the file's head for a primary volume descriptor and derives
   the base offset from it, validating the descriptor before accepting. That recognises the layout
   rather than the container, so nothing in the reader knows what an MDX is.
4. **The cue reader collapsed a multi-track rip.** `Cue_ParseExternalAudio` returned the *first*
   external audio file, enough for GOG (which has exactly one) but wrong for a rip: given a cue
   with `track02.wav`, `track03.wav`, `track04.wav`, every redbook request resolved to
   `track02.wav`, so the whole soundtrack played as one song. `Cue_ParseToc` now reports every
   track with its number, and matching is by number.
5. **Image selection depended on directory order.** `DiscImage_Mount` took the first file `readdir`
   handed back that `iso_open` accepted, so with `TWINSEN.bin` and `TWINSEN.iso` side by side the
   cooked ISO could win and silently lose the audio track, differently on different machines.
   Candidates are ranked now: raw over cooked, then larger, then lower name, with the passed-over
   ones logged at debug.

## The soundtrack layout, and what this dump is missing

The US disc carries its themes as Red Book audio in CD tracks 2 through 8, exactly the seven
non-`JINGLE` entries in `MUSIC.CPP`'s `TrackCDUS`. Track N there is `ListJingle[N-1]`, which pins
the name for each. An independent CD-DA rip of the same release
([Internet Archive](https://archive.org/details/lba2-cd-soundtrack)) gives the titles and durations,
and they line up one to one with the ADPCM masters GOG ships:

| CD track | Title | CD-DA rip | GOG asset | In this dump |
| --- | --- | --- | --- | --- |
| 2 | Song for Gabriel | 237.6 s | `TADPCM1.WAV` (234.2 s) | **absent** |
| 3 | The Empire | 219.7 s | `TADPCM2.WAV` (225.5 s) | 0:00 |
| 4 | Honey B. | 195.3 s | `TADPCM3.WAV` (194.1 s) | 3:45 |
| 5 | Emerald Moon | 211.0 s | `TADPCM4.WAV` (209.6 s) | 6:59 |
| 6 | Zeelich | 221.1 s | `TADPCM5.WAV` (220.3 s) | 10:30 |
| 7 | Purple | 52.0 s | `JADPCM01.WAV` (49.9 s) | 14:15 |
| 8 | LBA's Theme (1997) | 231.0 s | `LBA2.OGG` (231.3 s) | 15:06 |

Each segment was identified by envelope cross-correlation of the audio region against the decoded
GOG masters; the five long themes plus the distinctive 52-second `Purple` land in `ListJingle`
order, which is what makes the mapping safe rather than a guess. Tracks 3 through 8 sum to
1130 s against the 1138 s region, the remainder being pregaps.

So two separate things are true, and only one of them is a dead end:

- **The TOC is reconstructible.** The cue's single merged audio track is a limitation of the
  conversion (with the real table encrypted, inventing boundaries would have been worse than
  declaring one track), not of the data. The boundaries are measurable, so a correct cue can be
  regenerated for the tracks that are present:

  ```
  TRACK 03 AUDIO  INDEX 01 46:04:68   ; LBA 207368  The Empire          TADPCM2
  TRACK 04 AUDIO  INDEX 01 49:50:59   ; LBA 224309  Honey B.            TADPCM3
  TRACK 05 AUDIO  INDEX 01 53:04:59   ; LBA 238859  Emerald Moon        TADPCM4
  TRACK 06 AUDIO  INDEX 01 56:35:22   ; LBA 254647  Zeelich             TADPCM5
  TRACK 07 AUDIO  INDEX 01 60:19:66   ; LBA 271491  Purple              JADPCM01
  TRACK 08 AUDIO  INDEX 01 61:11:39   ; LBA 275364  LBA's Theme (1997)  TADPCM6
  ```

  Note the numbering starts at 3. Because the engine keys on the CD track *number*, renumbering
  these 2 through 7 would shift every theme by one scene. A TOC parser must therefore trust the
  declared track number and tolerate a gap in the sequence, rather than inferring the number from
  position in the file. Standard burning tools will reject a cue with a hole, which is fine: this
  cue is for the engine, not for a burner.

- **`TADPCM1` is gone.** CD track 2 is not in the `.mdx`, so it is not in the `.bin` either. The
  best correlation for `TADPCM1` anywhere in the region is 0.48, against 0.73 to 0.86 for every
  track that is genuinely there, and the region is short by about one `TADPCM1`. A re-rip from the
  physical disc is the only way to recover it. Failing that, GOG's `TADPCM1.WAV` dropped into
  `music/` is a working substitute: the filesystem is checked before the image, so it wins without
  any special case.


## How resolution works now

The rule the whole feature is built on: the disc image is a fallback *below* the filesystem, and the
resolution order is only ever extended at the tail. An install with no image, or with the assets
already extracted, takes exactly the path it took before.

Music, in `STREAM.CPP PlayStreamInternal`, in order:

1. Filesystem WAV.
2. Filesystem OGG (`.ogg` / `.OGG` / `track<N>.ogg`, the Steam and GOG transcodes).
3. The mounted image, as a file: covers GOG's in-BIN `JADPCM*` and `TADPCM*`.
4. **By CD track number**, when the install's cue describes two or more audio tracks, i.e. a real
   disc table of contents. The music name maps to a track number through `CDTRACKS`, and that track
   is played from wherever the cue says it lives: a standalone file, or raw sectors in the image.
5. **The single external audio file**, unchanged. This is what keeps GOG working: its `LBA2.DAT`
   lists exactly one external track, and matching by number would fail, because that file is
   numbered 2 in the cue while the engine wants music track 8.

Rule 4 sits above rule 5 but only engages on cues that rule 5 handles badly (or not at all), so it
is purely additive. GOG and Steam Classic both resolve their music at steps 1 to 3 and never reach
either.

`CDTRACKS` holds the disc's track order (CD track N is `ListJingle[N-1]`, tracks 2 to 8). It lives
beside the disc sources rather than in `MUSIC.CPP` because it describes a disc layout, not the
engine's music indirection, the same reason `CD.CPP` keeps its first-track constants. Its test reads
`MUSIC.CPP` and asserts the order still matches, so the two cannot drift apart quietly.

## Diagnosability

The `disc` console command (so also `--exec "disc"` headless) reports the mounted image, its sector
layout and base offset, the asset root and file count, the cue with every track in it, and then the
resolution the music path will make for each track the disc might carry. On an install with the
files extracted it says nothing is mounted, which is the answer, not an error.

That last section is the one that pays for itself. On the reference dump it prints:

```
Music:      matched by CD track number
  TADPCM1   track 02  not in this cue
  TADPCM2   track 03  in image at sector 207368
  ...
```

which states the dump's actual defect in one line, instead of leaving "the music is silent" to be
chased through sector arithmetic.

## What is not done

- **Release identity from the disc.** The disc's own `LBA2.CFG` says `Version: 1`, and the engine
  already has the concept (`DistribVersion`, the `distrib` console command). Seeding it from a
  mounted image would spare a US-disc user running `distrib activision`. Left out because the
  config read happens before the mount, so it is an ordering change rather than a small addition,
  and nothing in the music path needs it: the CD-track map keys off the cue, not the release.
- **CD-DA out of an MDX.** Its audio region uses a different stride from its data region and its
  track table is encrypted, so there is nothing to address it with. MDX support is data only, and
  the conversion to `bin`+`cue` is the answer for anyone who wants the soundtrack.
- **An extraction helper.** A script that writes `music/Track<NN>.wav` out of a `bin`+`cue` would
  make any container playable with no engine risk. Worth having as the documented answer for NRG,
  CCD and friends, but nothing needs it now that the common shapes resolve directly.

## Coverage

`host_quick`, no retail data needed:

- `tests/iso9660`: both layouts, a container-prefixed image that the plain probe must still reject
  and the scan must find, a signature look-alike that neither may accept, plus the existing
  malformed and truncated cases. Clean under `linux_sanitize`.
- `tests/cue`: the TOC parser over a retail in-image shape (with a hole in the track numbering, and
  an `INDEX 00` pregap that must not be mistaken for the start), a multi-file external rip, GOG's
  two-track shape, and a malformed `INDEX`. The original external-audio cases are unchanged.
- `tests/cdtracks`: the mapping both ways, its bounds, path and case handling, and the check that
  reads `MUSIC.CPP` to confirm the order still agrees.
- `tests/disc_image`: the marker probe discovery uses, and that a raw image beats a cooked one
  regardless of name or directory order.

Retail smoke, local:

| Install | Expected |
| --- | --- |
| `../LBA2` (Steam Classic) | no `Disc:` line, music from `Music/*.ogg`, unchanged |
| `../LBA2-GOG` | `Disc: mounted LBA2.GOG (ISO9660, 630 files)`, in-image WAV music, `LBA2.OGG` for track 6, unchanged |
| `../TWINSEN` (bin + cue only) | mounts, `Assets all present`, renders, six of seven themes play from the image |
