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
- **Music:** a cue-aware CD-track resolver in `CD.CPP PlayCD`. Order: extracted `TrackNN.wav`, then
  an external cue AUDIO file (`LBA2.OGG`, via `PlayStream`), then an in-BIN CD-DA track (PCM
  passthrough, below), then silent.
- **Readers:** vendor flade's `iso9660` (data reads) and cueplay's cue parse + audio-track locate
  (music); both SDL-free and GPLv2 (compatible with this repo).

## CD music: passthrough, do not extract
Redbook CD-DA tracks are raw 16-bit LE stereo 44.1 kHz PCM stored as 2352-byte audio sectors in the
BIN. The in-BIN music path **streams those PCM sectors straight into the AIL audio sink on demand**
(a raw-PCM stream source that reads the track's sectors, located via the cue `INDEX`); no `.wav`
extraction, no temp files. The external-file case (`LBA2.OGG`) is already a stream via `PlayStream`.
So neither music path extracts anything.

## Commit sequence (one branch, one PR)
1. **docs:** this guide.
2. **vendor reader + host test:** flade `iso9660.{c,h}` + `util.h` into `LIB386/SYSTEM/` (GPLv2
   attribution). A `host_quick` test against a synthetic in-test ISO9660 fixture: `iso_open` detects
   2352-raw vs 2048-cooked, `iso_read` returns a known file. (Mirrors the `VOC_HEADER` pure-unit +
   `tests/voc_header` pattern.)
3. **mount + `OpenRead` intercept (data) + preflight + boot-log mount line.** Mount by content-probe
   of the data dir; `iso_walk` once to build a case-insensitive basename + relative-path index;
   tagged virtual handles dispatched in `Read/Seek/Close`; `FileSize`/`ExistsFileOrDir` (incl. the
   `vox/` dir check) become image-aware. Smoke against `../LBA2` (FS-only regression) and
   `../LBA2-GOG` (media resolves from the image).
4. **CD-music resolver:** refactor `PlayCD` into the ordered resolver; wire the external OGG (parse
   the cue); add the in-BIN PCM-passthrough source. Boot log notes the chosen source at info level.
5. **(later) LBA1:** markers / game-id; LBA1's combined music+data image is already covered by the
   PCM-passthrough source. Orthogonal to the `GameProfile`.

## Boot log
Today's happy-path banner is a header block (`Assets:/Saves:/Config:/Log:`) then an aligned status
block (`Events/Joystick/Audio/Display/Assets/Language`) and a `Ready in ...` line. The mount is
**visible only when it happens, silent otherwise**:

- On mount, one header line beside `Assets:`:
  ```
  Assets: /path/to/install/
  Disc:   /path/to/install/LBA2.GOG  (ISO9660, 412 files)
  ```
- No image found: no `Disc:` line, banner unchanged (no "none" noise).
- The `Assets   all present` status may note disc-sourced assets only when relevant; per-resource
  source (FS vs disc) stays at debug level.
- CD-music source is logged at info/debug, not in the concise banner.
- Additive only: existing discovery/preflight lines (which the `Control_*` harness and tooling may
  parse) are left byte-identical.

## Testing
- **`host_quick` (CI, no retail data):** the iso9660 reader vs a synthetic fixture; the cue parser
  vs sample cue text; a resolution unit (FS-hit / image-fallback / miss) against a fake source. Each
  commit ships its test.
- **Retail smoke (local, opt-in, like the Docker tests):** `Control_*` headless boot of `../LBA2`
  (regression baseline, FS-only) and `../LBA2-GOG` (the new path): assert assets load and the boot
  is clean. Dev helper: extend `scripts/dev/iso_bin.py` to `iso_walk` an image and diff against
  expected.

## Robustness (the throughline)
No image, filesystem-only (today's behaviour, no regression). Missing asset, the existing preflight
path. Missing music, silent (CD audio was always optional). Any mount or cue-parse failure degrades,
never crashes.
