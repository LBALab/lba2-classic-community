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
