# Game data (retail files)

The engine needs the original Little Big Adventure 2 data files. They are not included in this repository (licensing). You must own a legitimate copy (e.g. [GOG](https://www.gog.com/game/little_big_adventure_2), [Steam](https://store.steampowered.com/app/398000/)).

## What to point the engine at

Use the directory that contains `LBA2.HQR` (and the other `.hqr` files, `music/`, `video/`, `vox/`, etc.). On Steam, classic data may live under a Classic-related install path; remastered or other editions may use a different layout — this fork targets classic data only.

**Validation:** discovery and overrides only succeed when the chosen path is a directory that contains `LBA2.HQR` (that is the required marker; see `IsValidResourceDir` / `FILE_VALID_RES_DIR` in `SOURCES/DIRECTORIES.CPP`). The filename is matched with the same case-folding logic as other asset files. A demo build marks on `RESS.HQR` instead, because no demo install ships `LBA2.HQR`; see [The 1997 playable demo](#the-1997-playable-demo).

**Single asset root:** Once that directory is chosen, `GetResPath`, `GetJinglePath`, `GetMoviePath`, and related APIs resolve files relative to that same `directoriesResDir` (see `InitDirectories` / `GetResPath` in `SOURCES/DIRECTORIES.CPP`). The engine does not look for `music/`, `video/`, `vox/`, or other `.hqr` files on separate paths; finding `LBA2.HQR` establishes the one root for classic data.

**Sibling scan:** If the fixed paths above fail, the engine looks at siblings of the parent of the current working directory (for example the folder that contains both your clone and a retail install). For each immediate subdirectory, it tries that folder and `CommonClassic`, `common`, `Common`, and `Classic` under it. Only bounded scans (entry and directory-count limits); see `TryParentSiblingScan` in `SOURCES/RES_DISCOVERY.CPP`.

## Overrides (highest priority first)

| Mechanism | Example |
|-----------|---------|
| Command line | `./lba2cc --game-dir /path/to/game` or `--data-dir` (alias) |
| Environment | `export LBA2_GAME_DIR=/path/to/game` |
| Persisted choice | `last_game_dir.txt` in the user-prefs folder, written automatically the first time you pick a folder via the launch dialog (see [First-launch folder picker](#first-launch-folder-picker) below) |
| Discovery | See [SOURCES/RES_DISCOVERY.CPP](../SOURCES/RES_DISCOVERY.CPP): SDL binary directory, current working directory, parents of cwd, `./data`, `../LBA2`, `../game`, sibling scan of parent-of-cwd |

Both `--game-dir` and `LBA2_GAME_DIR` also try a `Common/` inside the path you give, so pointing either at an install root works as well as pointing it at the data folder.

**Naming a path that holds no game data stops the run.** Neither override falls through to the probes below it: an override that names the wrong place is a mistake, and continuing would boot whatever discovery turns up next. That is a launch against assets nobody asked for, reported only by the `Assets:` line and still exiting 0. You get the picker instead (or a clean error when headless).

If none of the above resolves a valid directory, the engine falls back to the [First-launch folder picker](#first-launch-folder-picker). On a headless system (no display, CI runner, etc.), the picker can't open and the process exits with a log listing candidates tried and hints.

## Which probe matched

The boot banner names the probe that resolved the assets, after the path:

```
Assets: /home/user/GOG/tlba2-classic/Common/  (--game-dir, Common/)
Assets: /home/user/LBA2-GOG/                  (sibling scan)
```

The probes run silently and in priority order, so without this a run that picks an unexpected folder for a good reason (a forgotten `LBA2_GAME_DIR`, a stale `last_game_dir.txt`) looks the same as one that is simply broken. The label is the difference between reading a bug report and guessing at one. Labels name the mechanism, not the code: `--game-dir`, `LBA2_GAME_DIR`, `last_game_dir.txt`, `next to the binary`, `working directory`, `parent walk`, `sibling scan`, `first-launch picker`, and the Android storage roots.

## First-launch folder picker

When all four override mechanisms above fail and the engine is running in a windowed environment, it shows the platform-native folder dialog (NSOpenPanel on macOS, IFileDialog on Windows, GTK / xdg-desktop-portal on Linux). The user picks the folder containing `LBA2.HQR`; the engine validates it via `IsValidResourceDir`, persists the choice to `last_game_dir.txt`, and continues startup.

Android is the exception: there is no folder picker (SDL's file dialog returns `content://` document URIs, not filesystem paths the engine can probe). When discovery fails there, the engine shows a message telling the user to copy their data to `/sdcard/lba2cc/` and relaunch, instead of opening a dialog. See [docs/ANDROID.md](ANDROID.md) for the full Android data layout.

| Scenario | Behavior |
|---|---|
| User picks a valid folder (contains `LBA2.HQR`) | Engine launches; `last_game_dir.txt` updated; subsequent launches skip the dialog. |
| User picks an invalid folder | Engine shows a "Game data not found" message and re-opens the dialog. |
| User cancels the dialog | Engine exits cleanly (same exit code as the headless no-data case). |
| Persisted path no longer valid (game install moved/deleted) | Engine silently falls through to auto-discovery, then the picker. The next valid pick rewrites `last_game_dir.txt`. |
| Headless environment (no display, CI, dummy video driver) | Picker can't open; engine exits with the existing "no game data" error and the candidate path list. |
| Linux/WSL with no dialog backend installed | The display works (so the engine shows the "Game data folder not found" message), but `SDL_ShowOpenFolderDialog` reports "File dialog driver unsupported" because neither `zenity` nor `xdg-desktop-portal` is available. Engine shows a follow-up message-box with recovery hints and exits. See [Picker backends per environment](#picker-backends-per-environment) below for the right install command, or use `--game-dir` / `LBA2_GAME_DIR` to skip the picker. |

The persisted path lives at `<SDL_GetPrefPath("Twinsen", "LBA2")>/last_game_dir.txt` — typically:

| Platform | Path |
|---|---|
| Linux | `~/.local/share/Twinsen/LBA2/last_game_dir.txt` (honors `XDG_DATA_HOME`) |
| macOS | `~/Library/Application Support/Twinsen/LBA2/last_game_dir.txt` |
| Windows | `%APPDATA%\Twinsen\LBA2\last_game_dir.txt` |

It's a single-line text file (the absolute path to the chosen game-data folder). Safe to delete by hand to force the picker to re-appear on next launch — the engine treats a missing file as "never picked," same as a fresh install.

### Forcing the picker without deleting the file

Pass `--pick-game-dir` on the command line. The engine skips the persisted-LastGameDir probe and the auto-discovery chain, going straight to the picker for this run only. Use cases:

- Test the first-launch UX repeatedly without `rm`-ing the persisted file each time.
- Switch to a different LBA2 install without editing `last_game_dir.txt` by hand.
- Anyone whose persisted or auto-discovered path resolves to the wrong place and wants a UI re-do.

A successful pick rewrites `last_game_dir.txt` with the new path (acts as a "switch install" UX). A cancelled pick changes nothing — your previous setting stays intact.

`--game-dir` and `LBA2_GAME_DIR` still win against `--pick-game-dir` if both are set on the same launch (explicit path beats "show the picker"). Useful for `lba2cc --game-dir /path/to/install` overriding a misbehaving persisted setting one-shot, or `--pick-game-dir` for the "let me browse" UX.

### Picker backends per environment

SDL3 implements the Linux folder picker via one of two backends — `zenity` (GTK-based, simple) or `xdg-desktop-portal` (Freedesktop portal protocol, integrates with the desktop environment). [SDL3's hint documentation](https://wiki.libsdl.org/SDL3/SDL_HINT_FILE_DIALOG_DRIVER) says it tries "all available dialog backends in a reasonable order" without committing to one — install whichever fits your environment.

| Environment | Recommended install | Notes |
|---|---|---|
| **WSL Ubuntu / Debian** | `sudo apt install zenity` | One package, works immediately. Portal would need extra D-Bus session bus setup that is fragile inside WSL. |
| **Arch + KDE Plasma** | `pacman -S xdg-desktop-portal xdg-desktop-portal-kde` | Native Plasma-styled file dialog. |
| **Arch + GNOME** | `pacman -S xdg-desktop-portal xdg-desktop-portal-gnome` (often pre-installed) | Native GNOME dialog. `xdg-desktop-portal-gtk` is an equivalent fallback. |
| **Arch + Hyprland** | `pacman -S xdg-desktop-portal-hyprland xdg-desktop-portal-gtk` | XDPH itself does not provide a file picker; the [Hyprland wiki](https://wiki.hypr.land/Hypr-Ecosystem/xdg-desktop-portal-hyprland/) recommends installing `-gtk` alongside it. **Don't** add `-kde` or `-gnome` — mixing portal implementations on Hyprland causes D-Bus timeouts and breaks every app that uses portals. |
| **sway / minimal Wayland** | `xdg-desktop-portal-gtk` (or `zenity` if you have XWayland) | GTK portal is the cleaner Wayland answer. |
| **i3 / minimal X11** | `zenity` | Simplest universal fallback. |

If you want to force a specific backend even when both are installed, set `SDL_FILE_DIALOG_DRIVER=zenity` (or `=portal`) in the environment before launching. Useful for debugging which backend's misbehaving on a particular setup.

**Explicit path (recommended for anything non-local):** Use `--game-dir` or `LBA2_GAME_DIR` when the install is not next to your working tree (another drive, `~/Games/…`, CI, etc.). That is the stable, portable option.

**What “adjacent” means in discovery:** Heuristics only look at predictable relative locations (cwd, parents of cwd, `../LBA2`, `./data`, SDL binary dir, then siblings of the parent of cwd with a small set of subfolder names). There is no full-disk or deep recursive search. “Adjacent” here means *often the same parent folder as your clone* when you run from the repo root — not “any path on the machine.”

**Safety:** Every candidate is accepted only if `IsValidResourceDir` succeeds (`LBA2.HQR` present). There is no execution of paths from untrusted config beyond what you pass on the command line or in the environment.

## Developer layout

Common choices:

- Symlink or copy retail files into `data/` at the repo root (that directory is gitignored in this repo so retail files are not committed by mistake).
- One directory up: `../LBA2/` (or `../game/`) with `LBA2.HQR` inside.

From any directory in the clone you can use:

```bash
./scripts/dev/build-and-run.sh
```

or `make run` (see [Makefile](../Makefile)). Set `LBA2_BUILD_DIR` if you do not use `build/`.

Windows without Bash: configure CMake as in [WINDOWS.md](WINDOWS.md), then run `lba2cc.exe` with `--game-dir` or set `LBA2_GAME_DIR` in the environment.

## GOG DRM-free "Original Edition" packages

The GOG DRM-free standalone product (and the equivalent content inside the GOG Galaxy "Original Edition" DLC under `Speedrun/Windows/`) ships the 1997 retail CD-ROM as a raw BIN image (`LBA2.GOG`) rather than extracted files. HQRs are duplicated at the install root so gameplay loads, but `VIDEO.HQR`, music WAVs, and `.VOX` voices live only inside the BIN.

**The engine reads the image directly (no extraction needed).** Point it at the install root, the folder holding both `LBA2.HQR` and `LBA2.GOG`, and the disc-image source mounts the BIN and resolves the in-image assets (FMV, voices, music) through the normal file path. The image is detected by content (the ISO9660 "CD001" volume descriptor), not by name, so the `.gog` extension, a raw `.bin`/`.iso`, or a `.cue`/`.dat` pair all work. When a mount happens the boot log adds a `Disc:` line beside `Assets:`; with no image present the engine stays filesystem-only as before. See [DISC_IMAGE_SOURCE.md](DISC_IMAGE_SOURCE.md) for the mechanism.

Discovery accepts a folder whose game data exists only inside the image: the `LBA2.HQR` check falls through to a probe of any image sitting there. Today's GOG packages ship the HQRs extracted alongside the BIN anyway, so for them discovery succeeds on the filesystem and only the media comes from the image.

**Extracting the media instead (optional).** If you would rather have loose files on disk (for modding, inspection, or to run without the image), extract them once:

```
python3 scripts/dev/extract_lba2_gog_media.py /path/to/gog-install/
```

This walks the ISO9660 filesystem inside `LBA2.GOG`, extracts everything under `/LBA2/VIDEO/`, `/LBA2/VOX/`, and `/LBA2/MUSIC/`, and writes them next to the existing HQRs (~522 MB total: 1× `VIDEO.HQR`, 39× `.VOX`, 24× ADPCM `.WAV`). Files extracted by this script are byte-identical to what the GOG Galaxy / Steam Classic SKUs ship in `Common/`, verified by md5.

The script is idempotent (re-runs skip files already at the expected size; pass `--force` to overwrite). Python 3 stdlib only, no dependencies. After extraction, point the engine at the install root as usual.

Not affected by this:

- GOG Galaxy or Steam buyers of *TLBA2 Classic* (with or without the Original Edition DLC): both already ship the assets extracted under `Common/`.
- Anyone with a 1997 LBA2 retail CD can rip it into a BIN/CUE pair the engine mounts directly. See [Using your own CD](#using-your-own-cd) below.

See issue [#119](https://github.com/LBALab/lba2-classic-community/issues/119) for the analysis behind this; the in-engine disc reader it proposed is now implemented (see [DISC_IMAGE_SOURCE.md](DISC_IMAGE_SOURCE.md)).

## Using your own CD

The engine reads a CD rip directly. You do not need to extract anything, and you do not need a
storefront copy.

**Which pressing you have matters less than it used to.** The engine reads MODE1 and MODE2 raw
images and cooked ISOs, and works out for itself which themes a disc pressed and which byte order
they were written in. The European Electronic Arts discs press one theme, the US Activision disc
presses six, the Brazilian Activision disc presses seven and numbers them differently, and one
circulating European rip stores its audio in raw Red Book (big-endian) order. All of them play.
See [DISC_IMAGE_SOURCE.md](DISC_IMAGE_SOURCE.md#the-pressings).

**Rip to BIN/CUE, not ISO.** This matters more than it sounds. A 1997 retail disc is *mixed mode*:
one data track holding the game, then the soundtrack as Red Book audio tracks. ISO 9660 is a
filesystem format with room for one data track and nowhere to record a table of contents, so an ISO
of this disc either drops the music entirely or carries it as bytes nothing can address. A BIN/CUE
pair keeps both: the BIN holds every sector, the CUE says where each track starts.

| Platform | Tool |
| --- | --- |
| Windows | ImgBurn, *Create image file from disc*. Choose BIN/CUE as the output |
| Linux | `cdrdao read-cd --read-raw --datafile disc.bin --device /dev/sr0 disc.toc`, then `toc2cue disc.toc disc.cue` |
| Any, preservation-grade | DiscImageCreator (the tool Redump uses), which also writes a log of what it read |

macOS has no reliable free option for mixed-mode audio extraction; rip on another machine if that is
what you have.

**Then point the engine at it.** Put the pair in a folder of its own and use either:

```
lba2cc --game-dir /path/to/rip        # the folder holding disc.bin and disc.cue
lba2cc --disc /path/to/rip/disc.cue   # names the cue directly; no --game-dir needed
```

Or no flags at all: put the executable in the folder with the `.bin` and `.cue` and run it. The
folder the binary sits in, and the folder you run from, are both checked for a disc image as well
as for extracted files. The folder picker accepts one too, so browsing to a rip on first launch
works. What is *not* checked for an image is the rest of the discovery sweep (parents of the
working directory, sibling folders): those are guesses, there are up to 160 of them, and opening
every file in each to look for an image is the kind of thing that hangs a machine rather than
helping it.

**Check it worked** with the `disc` console command (or `--exec "disc"` from a script). A good rip
reports the image, its cue, and which CD track each theme will come from:

```
Image:      /path/to/rip/disc.bin
Layout:     2352-byte sectors, data at byte 0
Asset root: /TWINSEN   (632 files)
Sectors:    292864 raw, so in-image CD audio can be read
Cue:        /path/to/rip/disc.cue (7 tracks)
Music:      matched by CD track number
  TADPCM2   track 02  in image at sector 207513
  ...
```

If a theme reports `not in this cue`, the rip lost that track or the cue does not describe it.

**A data-only rip still plays.** If all you have is an ISO or a data-track image, the game runs
normally and the jingles play (they are files inside the filesystem); only the Red Book themes are
silent. `disc` will say so, and will tell you if the file has content past the filesystem that it
cannot address, which is the signature of a container holding its audio at a different stride.

**A mounted disc also plays**, with the same limitation. Point `--game-dir` at the folder holding
the game data on the disc, which on the 1997 US disc is `<drive>:\TWINSEN` rather than the disc
root. Assets and jingles come straight off the medium; the themes need a rip, because reading audio
from a drive is not something the engine does (see
[DISC_IMAGE_SOURCE.md](DISC_IMAGE_SOURCE.md#what-is-not-done)).

### Extracting to loose files

You do not need this for a bin/cue rip, which plays as it is. It is for the cases the engine cannot
serve from the medium: a physical disc whose themes you want, a container that mounts data-only
(MDX, NRG, CCD), or simply wanting the files on disk to mod or inspect.

```
python3 scripts/dev/disc_extract.py /path/to/rip ~/lba2-data
```

That writes the game data out of the image and cuts the six Red Book tracks into
`music/TADPCM2.WAV` and friends, about 535 MB from the US disc. Then
`lba2cc --game-dir ~/lba2-data` as usual.

### Straight from the drive

With the disc in a drive, `--from-drive` reads the soundtrack off it. Point the source at the game
data on the disc (`<drive>:\TWINSEN` on the 1997 US disc) and name the drive:

```
python3 scripts/dev/disc_extract.py /media/cdrom/TWINSEN ~/lba2-data --from-drive /dev/sr0
py -3 scripts\dev\disc_extract.py G:\TWINSEN C:\lba2-data --from-drive G
```

It reads the disc's own table of contents, so the track numbering is the disc's rather than a guess,
and every audio track it finds has to be one the naming table knows or it says so. Omit the device
and it uses the first drive it finds.

**Trimming the join.** On a mixed-mode disc the data track's run-out bleeds into the front of the
first audio track, and a drive hands that back as audio: five sectors of full-scale noise ahead of
"The Empire" on the US disc. A cue can say "start later" and ours does; a table of contents cannot,
so the script finds the join itself. What identifies it is not loudness but distribution, since data
read as audio is uniform noise averaging half of full scale where the music that follows averages
0.005. The test only opens on a first sector at or above a quarter of full scale, so a track that
simply starts loud is never trimmed.

Audio sectors carry no error correction, so a scratched disc gives clicks that `cdparanoia` would
re-read and interpolate away and this will not. The script reports its retry count; if it is
non-zero, or a theme sounds wrong, rip with `cdparanoia` and come back through `--tracks-from`.
macOS has no drive backend here, so rip and use `--tracks-from` there.

### From a rip you already made

Rip with whatever your platform has (`cdparanoia`, EAC, ImgBurn) and point the script at the result.
It reads the track number out of each filename, so `track02.cdda.wav` and `Track 2.wav` both work:

```
python3 scripts/dev/disc_extract.py ~/ripped ~/lba2-data                    # audio only
python3 scripts/dev/disc_extract.py disc.mdx ~/lba2-data --tracks-from ~/ripped
```

**The naming is the point.** A ripper gives you `track02.wav`; what it cannot tell you is that CD
track 6 is `JADPCM01` rather than `TADPCM6`, or that the `TADPCM1` theme is not pressed on the US
disc at all. That mapping lives in [CDTRACKS.CPP](../LIB386/SYSTEM/CDTRACKS.CPP), and
`tests/cdtracks` fails if the script's copy of it drifts. Renaming the files by hand is where this
goes wrong: every scene gets a plausible theme, just not its own.

The script is idempotent (`--force` to overwrite, `--dry-run` to see the plan) and stdlib-only. It
declines to guess: a cue that describes a different file than the image, or one with a single
external audio track whose number is not a CD track number, is reported rather than acted on.

## The 1997 playable demo

The demo that shipped on magazine cover discs is a separate SKU, not a mode. Its data set is a
different install and its behaviour lives behind `#ifdef DEMO` in the original sources, so a demo
binary cannot run retail data and a retail binary cannot run demo data. It is a compile-time release
axis rather than a `Version` key, alongside the other build variants in
[VERSIONS.md](VERSIONS.md#build-variants). Build it with:

```bash
cmake -S . -B build-demo -G Ninja -DLBA2_BUILD_DEMO=ON
cmake --build build-demo
```

then point it at a demo install (`--game-dir`) the same way as retail.

What the data holds, against the retail CD:

| | Demo | Retail |
|---|---|---|
| Islands | `CITADEL` only | 17 `.ILE`/`.OBL` pairs |
| `LBA2.HQR` (credits) | absent | present |
| `ANIM3DS.HQR` | absent | present |
| `VIDEO/VIDEO.HQR` | absent | 231 MB |
| `VOX/` | absent | 39 banks across 3 languages |
| Music | `MUSIC/*.WAV`, 9 tracks | 24 tracks |
| `SCRSHOT.HQR` | present | absent |

The HQR container is the same format throughout, same LZSS and LZMIT codecs and the same header
layout, so every bank the demo does ship loads unmodified. The banks are subsets: 743 animations
against 2084, 129 bodies against 470, 1243 background chunks against 18101. Music filenames are a
subset of `ListJingle`, so the normal music path plays them with no special case. `SCRSHOT.HQR` is
the one bank the demo adds, holding the marketing stills its slideshow shows.

Two of those gaps are load-bearing and the demo build accounts for them: `LBA2.HQR` is the marker
that identifies a resource directory, so the demo build looks for `RESS.HQR` instead, and
`VIDEO.HQR` is required at boot, so the demo build warns and runs without cutscenes. Retail's movie
bank alone is larger than the whole 17 MB demo, which is why no demo install has one.

Behaviour that differs from retail, all of it original:

- Three fixed scenarios (`demo0.lba`, `demo1.lba`, `demo2.lba`) instead of free saves. New Game
  loads the first, Load Game is unavailable, and an in-game save writes back to the current slot
  without prompting for a name.
- Leaving certain cubes advances to the next scenario, hardcoded in `GereZoneChangeCube` handling
  in `OBJECT.CPP` and commented in the original as "Grosse Rustine pour la demo".
- The title logo is drawn over gameplay every frame. Retail ships the same sprites but draws them
  only during the attract reel and on Activision/Virgin builds.
- The player starts with all darts, sewer covers are forced hidden, ambient sound is suppressed in
  the phantom cube, and the `LF_DEMO` life-script function reports `1` so scripts can branch on it.
- The end of the demo runs the `SCRSHOT.HQR` slideshow, which the console exposes as
  `ui slideshow`.

## Config file

See [CONFIG.md](CONFIG.md). If `lba2.cfg` is missing from the user config folder, the engine copies from the asset directory when present; if the asset directory has no `lba2.cfg`, an embedded template (from the build) is written instead.

That copy carries more than volumes. Its `Version` key is what tells the engine which release this is, and each distribution states its own: the US retail disc says `1` (Activision) and GOG says `3` (EA). Steam Classic is the exception twice over. It keeps its config in the user's Saved Games folder (`%USERPROFILE%\Saved Games\2point21\tlba2-classic\Settings\lba2.cfg`) rather than beside the assets, so the seeding never sees it, and that file has no `Version` key in any case. Steam therefore resolves to `0` (unknown), which differs from `3` (EA) only in the distributor splash. The value selects the distributor splash, the in-game and new-game logo sprites, the no-CD message and the CD volume label. So an install assembled by hand, or repackaged without its `lba2.cfg`, will look like an unknown release to a fresh profile even though every asset is present. Nothing audible depends on it (see [DISC_IMAGE_SOURCE.md](DISC_IMAGE_SOURCE.md#which-release-the-engine-thinks-it-is)), and an existing profile keeps whatever it was first seeded with, so changing the asset directory later does not change it. `distrib` in the console reports and sets it. [VERSIONS.md](VERSIONS.md) has the full picture: every site the value branches at, and what each shipped disc declares.

## Cross-references

- [CONFIG.md](CONFIG.md) — `lba2.cfg` keys and paths
- [TESTING.md](TESTING.md) — `test_res_discovery` (host tests for path resolution)
- [AUDIO.md](AUDIO.md) — retail audio asset map (Steam vs GOG; what's ADPCM-WAV, what's `.VOX`, the lone `SETUP.MID` artifact)
