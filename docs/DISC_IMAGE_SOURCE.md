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
- **Music:** all music in the SDL build funnels through `STREAM.CPP PlayStream` via
  `MUSIC.CPP PlayJingle`. (When this was written, entries the retail disc pressed as CD audio went
  to `CD.CPP PlayCD` instead; that path is gone, see "Which release the engine thinks it is".) `PlayStream` already resolves WAV first, then `.ogg`/`.OGG`/`track<N>.ogg`
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
   asset-root subtree by finding the resource marker (`LBA2.HQR`) inside it (CD masters nest under a
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
validates the install dir by finding `LBA2.HQR` *before* the mount, so a dir with only the image
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

1. **Discovery rejected a pure dump.** `IsValidResourceDir` required `LBA2.HQR` on the filesystem
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

## The soundtrack layout

The US disc carries six of its themes as Red Book audio. The rule is that the CD track number is
the music index: **track N holds `MUSIC.CPP`'s `ListJingle[N]`**. Track 1 is the data track, so the
`ListJingle[1]` slot (`TADPCM1`) has no track of its own, and that piece of music is simply not on
this disc.

This is read off the disc, not inferred. Mounting the `.mdx` in DAEMON Tools (which is the only
software that can decrypt its descriptor) exposes the real table of contents to the OS, and
`IOCTL_CDROM_READ_TOC` against the virtual drive gives:

| CD track | Disc LBA | Length | Music | Confirmed by |
| --- | --- | --- | --- | --- |
| 1 | 0 | | data | ISO9660 filesystem |
| 2 | 207513 | 226.0 s | `TADPCM2` | length and audio content |
| 3 | 224463 | 191.5 s | `TADPCM3` | length and audio content |
| 4 | 238826 | 213.0 s | `TADPCM4` | length and audio content |
| 5 | 254801 | 222.0 s | `TADPCM5` | length and audio content |
| 6 | 271451 | 54.0 s | `JADPCM01` | length and audio content |
| 7 | 275501 | 231.5 s | `TADPCM6` | length and audio content |

Each track was matched against the decoded GOG masters by envelope cross-correlation (allowing for
the 0 to 2.4 s of pregap the tracks carry), and independently by duration. Both agree on every
track, and both rule out `TADPCM1`, which fits nothing.

**Two earlier conclusions here were wrong, and the TOC is what corrected them.**

The first: this dump is *not* missing a track. Comparing the disc's lead-out (LBA 292864) with the
`.bin`'s length (292714 sectors) shows the image is the disc shifted by exactly 150 sectors, which
is the data track's run-out that the conversion dropped when it took the ISO volume size as the
track length. The audio is 85351 sectors on both. Nothing was lost, and there is nothing to
re-rip.

The second: the mapping is not `ListJingle[N-1]`. That reading came from lining
`TrackCDUS` up against `ListJingle` by hand and it shifts every theme by one, which is the kind of
error that plays perfectly and is simply the wrong song. The disc says otherwise.

## The corrected cue

Cue `INDEX` values are offsets into the file, so these are the disc LBAs less the 150-sector shift.
The lead-out lands exactly on the `.bin`'s length, which is the arithmetic check that the whole
table is right:

```
FILE "TWINSEN.bin" BINARY
  TRACK 01 MODE1/2352
    INDEX 01 00:00:00
  TRACK 02 AUDIO
    INDEX 01 46:04:68        ; LBA 207368  TADPCM2  (see below)
  TRACK 03 AUDIO
    INDEX 01 49:50:63        ; LBA 224313  TADPCM3
  TRACK 04 AUDIO
    INDEX 01 53:02:26        ; LBA 238676  TADPCM4
  TRACK 05 AUDIO
    INDEX 01 56:35:26        ; LBA 254651  TADPCM5
  TRACK 06 AUDIO
    INDEX 01 60:17:26        ; LBA 271301  JADPCM01
  TRACK 07 AUDIO
    INDEX 01 61:11:26        ; LBA 275351  TADPCM6
```

Track 2 is the one exception to "disc LBA less 150". The image keeps five sectors of data-track
run-out (LBA 207363 to 207367) ahead of the audio, which play as a short burst of full-scale noise;
they carry a Mode-1 payload but no detectable sync header, so nothing upstream treats them as data.
Track 2 therefore starts at the first silent sector, 207368. The skipped sectors hold no audio: the
music's own fade-in begins around LBA 207371, after three sectors of digital silence. The other
five tracks start on the pregap noise floor and need no adjustment, and a five-sector difference at
their starts (0.067 s, inside the pregap) is inaudible either way.

This was audible before it was measurable, and it is worth saying how it was found: the first pass
put track 2 at 46:04:63 purely from the arithmetic, and a listening test caught the pop. The
sector-level RMS profile then showed exactly where the run-out ended.

`TADPCM1` has no CD track, so a request for it misses. Dropping GOG's `TADPCM1.WAV` into `music/`
supplies it: the filesystem is checked before the image, so it wins with no special case.

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

## Choosing the image: `--disc`

The ranking is right for a normal install, which makes two things unreachable: the cooked `.iso`
sitting beside a raw `.bin`, and any container the deeper probe finds, since that pass only runs
when the cheap one comes up empty. `--disc <file>` names the image instead.

It takes an image or a cue, because pointing tools at the cue is the convention and the sheet names
the image beside it (`Cue_ImageForSheet`, which returns "not a cue" rather than guessing, so no
extension sniffing). It never falls back to the scan: someone who says which disc to use is owed
that disc or an error, not a different one chosen quietly. And it is read before discovery, not just
before the mount, because discovery also probes for an image when a folder holds no extracted data,
and both should be looking at the same disc.

```
--disc TWINSEN.iso   mounted TWINSEN.iso  (ISO9660, 632 files)   the ranking would never pick this
--disc TWINSEN.mdx   mounted TWINSEN.mdx  (ISO9660, 632 files)   only the container probe finds it
--disc TWINSEN.cue   mounted TWINSEN.bin  (ISO9660, 632 files)   resolved from the sheet
--disc /etc/hostname not a disc image we can read                no silent fallback
```

`--disc` on its own is enough; `--game-dir` is not required alongside it. Two things make that
true. The cue is read from the *image's* directory first, because the sheet that describes a disc
sits beside that disc, and external audio the cue names is resolved against the sheet rather than
the install. And when no game folder is given explicitly, the disc's folder becomes the install.
That last part is not cosmetic: the override makes every candidate folder look valid to discovery,
so without it the mount base would be whichever candidate happened to be tried first, a stale file
there would shadow the disc, and the boot banner would name a folder holding none of the assets.
An explicit `--game-dir` still wins, for the case where extracted files and the image live apart.

## Diagnosability

When a cooked image holds bytes past its filesystem, `disc` says so and how many. That is the
difference between an `.iso`, whose audio the conversion genuinely dropped, and a container like an
MDX that still has the whole soundtrack in it at a stride this reader does not follow:

```
TWINSEN.iso   Sectors:  cooked image, no CD audio in it
TWINSEN.mdx   Sectors:  cooked image, and 191 MB past the filesystem this reader
                        cannot address. If that is CD audio, convert to bin/cue.
```

The `disc` console command (so also `--exec "disc"` headless) reports the mounted image, its sector
layout and base offset, the asset root and file count, the cue with every track in it, and then the
resolution the music path will make for each track the disc might carry. On an install with the
files extracted it says nothing is mounted, which is the answer, not an error.

That last section is the one that pays for itself. On the reference dump it prints:

```
Music:      matched by CD track number
  TADPCM2   track 02  in image at sector 207368
  TADPCM3   track 03  in image at sector 224313
  ...
```

which states the dump's actual defect in one line, instead of leaving "the music is silent" to be
chased through sector arithmetic.

## Containers other than bin/cue: kept, not extended

Decided rather than drifted into, so it is not re-litigated. The container probe stays. It costs
about 93 lines plus 68 of tests, and it buys exactly one format: MDS/MDX. Everything else is raw
from byte 0 and needed nothing (MDF, CCD/IMG, ISO, BIN), and NRG keeps its metadata in a footer so
the probe does not help it either.

The case for keeping it is that the code is written and tested, and it means the game *runs* on a
container it would otherwise refuse, with `disc` saying plainly what is missing. Data plus jingles
minus themes is a playable game; a refusal to mount is not.

What is deliberately not done is extending it. Serving the audio inside such a container is a real
possibility rather than a fantasy: the offset is derivable from fields the image already carries,

    audio_byte(lba) = base + volumeSectors x dataStride + (lba - volumeSectors) x 2352

which is a generalisation rather than a special case, since for a raw image the terms collapse to
today's `base + lba x 2352`. Verified against all three containers of the reference dump: same
audio, one formula. It is not implemented because the cue's `FILE` would have to name the container
(we refuse to apply a cue's sector addresses to a different file, and "probably the same disc" is
not good enough when the failure mode is full-scale noise), and editing a cue to say so is about as
much work as converting the image properly. If a second sample ever turns up, this is a small
change with the arithmetic already checked.

## Extraction

`scripts/dev/disc_extract.py` writes a rip out to loose files: the asset root's subtree, plus the
Red Book tracks cut into `music/TADPCM2.WAV` and friends. It exists for the shapes the engine
deliberately does not serve, not as an alternative to mounting. A `bin`+`cue` already plays whole,
and extracting it costs 535 MB to gain nothing.

What it is for is a physical disc, whose themes the engine will not read (below), and a container
that mounts data-only. Rip the audio with any tool and the script names the files; it takes the
track number from the filename, so `track02.cdda.wav` and `Track 2.wav` both land right.

The naming is the whole contribution. A ripper knows track numbers and nothing else; that track 6
holds `JADPCM01` rather than `TADPCM6` is this repo's finding, from the disc's own table of
contents. So the script carries a copy of the `CDTRACKS.CPP` table, and `tests/cdtracks` reads both
and fails if they disagree. A drift there would be silent in a way the engine's cannot be: nothing
errors, the themes simply come out under each other's names.

It refuses the same guesses the engine refuses. A cue whose `FILE` names something other than the
image being read is reported, not applied, for the reason given above. A cue with a single external
audio track is reported too: GOG's lists its one theme as `TRACK 02` when the music is `TADPCM6`, so
the number there is not a CD track number, and the engine's own rule 4 draws the line in the same
place.

## What is not done

- **Drive-backed CD audio, a deliberate non-goal.** A player can point the engine at a mounted disc
  (`G:\TWINSEN`, or a real drive) and the data side works: assets all present, jingles straight off
  the medium. The themes are Red Book audio on the physical disc, and the CD-DA source reads
  *images*, not drives.

  SDL is no help here. SDL 1.2 had an `SDL_CDROM` API; SDL 2 removed it and SDL 3 never brought it
  back (checked: zero cdrom symbols across SDL 3.5's headers). It would not have served anyway,
  because it commanded the *drive* to play a track through its own output rather than handing back
  sectors. That path needs an analogue cable from the drive to the sound card, which stopped being
  fitted decades ago, and it bypasses the mixer entirely, so the volume slider, the pause/resume
  parking and the fades would all be outside it. That is the two-worlds split `PlayCD` represented
  and this work removed.

  What is actually needed is digital audio extraction: read 2352-byte audio sectors and feed the
  same stream the image path uses. That means either a `libcdio` dependency or three platform
  backends (`IOCTL_CDROM_RAW_READ`, `CDROMREADAUDIO`, and something for macOS). Both are a lot of
  surface for the one player who has a physical disc and will not rip it, when a single pass through
  any imaging tool puts them on a path that already works end to end. That player now has a second
  route as well: keep playing off the mounted disc and rip only the audio, which
  `disc_extract.py --tracks-from` names into place (above).

  If it is ever wanted, the shape is carved out: a fourth source at the tail of
  `PlayStreamInternal`, using the same `CDTRACKS` mapping, with the table of contents read from the
  drive instead of a cue. The work is the extraction, not the plumbing.
- **CD-DA out of an MDX.** Its audio region uses a different stride from its data region and its
  track table is encrypted, so there is nothing to address it with. MDX support is data only, and
  the conversion to `bin`+`cue` is the answer for anyone who wants the soundtrack.

## Coverage

`host_quick`, no retail data needed:

- `tests/iso9660`: both layouts, a container-prefixed image that the plain probe must still reject
  and the scan must find, a signature look-alike that neither may accept, plus the existing
  malformed and truncated cases. Clean under `linux_sanitize`.
- `tests/cue`: the TOC parser over a retail in-image shape (with a hole in the track numbering, and
  an `INDEX 00` pregap that must not be mistaken for the start), a multi-file external rip, GOG's
  two-track shape, and a malformed `INDEX`. The original external-audio cases are unchanged.
- `tests/cdtracks`: the mapping both ways, its bounds, path and case handling, the check that
  reads `MUSIC.CPP` to confirm the order still agrees, and the one that reads `disc_extract.py` so
  the extractor cannot name a theme differently from the engine.
- `tests/disc_image`: the marker probe discovery uses, and that a raw image beats a cooked one
  regardless of name or directory order.

Retail smoke, local:

| Install | Expected |
| --- | --- |
| `../LBA2` (Steam Classic) | no `Disc:` line, music from `Music/*.ogg`, unchanged |
| `../LBA2-GOG` | `Disc: mounted LBA2.GOG (ISO9660, 630 files)`, in-image WAV music, `LBA2.OGG` for track 6, unchanged |
| `../TWINSEN` (bin + cue only) | mounts, `Assets all present`, renders, all six CD tracks play from the image |

## Which release the engine thinks it is

Worth recording, because it took a while to see and it is load-bearing for the music path.

`DistribVersion` gates the music track table, the CD volume label, two sprites and the distributor
splash. It is not detected: it comes from the `Version` key in `lba2.cfg`. What makes it work
anyway is that a profile with no config is seeded from the *game directory's* `LBA2.CFG`, and with
a disc image mounted that resolves through the disc seam to the config inside the image. So the
medium tells the engine which release it is:

| Install | `Version` in its config | Fresh profile reports |
| --- | --- | --- |
| Retail disc (US) | 1 | `activision` |
| GOG | 3 | `ea` |
| Steam Classic | none *beside the assets* | `unknown` |

Steam is worth a footnote: it does ship a config, but in the user's Saved Games
folder (`%USERPROFILE%\Saved Games\2point21\tlba2-classic\Settings`) rather than beside the
assets, so the seeding never reaches it. That file carries no `Version` key either, so `unknown` is
the answer whichever way you look, and it differs from `ea` only in the distributor splash.

Two things follow. First, a retail disc correctly lands on `TrackCDUS`, which is *why* the routing
below had to be fixed rather than worked around: the disc is not misidentified, it is identified
correctly and the path it selects was broken. Second, an existing profile keeps whatever it was
first seeded with, so a profile predating a config-bearing game directory reports `unknown` and
takes the other table. That asymmetry is why the same disc could play music on one machine and not
another.

`PlayMusic` now routes both tables through `PlayJingle`. The two tables hold identical music
numbers and differ only in the `JINGLE` flag, which says "this one is CD audio on the original
medium" rather than "this is different music", so the choice of table no longer changes what
plays, only where the bytes come from. That is decided below, in the resolution order above.
`tests/cdtracks` pins the identical-numbers invariant by reading `MUSIC.CPP`, since the routing is
only correct while it holds.
