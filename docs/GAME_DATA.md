# Game data (retail files)

The engine needs the original Little Big Adventure 2 data files. They are not included in this repository (licensing). You must own a legitimate copy (e.g. [GOG](https://www.gog.com/game/little_big_adventure_2), [Steam](https://store.steampowered.com/app/398000/)).

## What to point the engine at

Use the directory that contains `lba2.hqr` (and the other `.hqr` files, `music/`, `video/`, `vox/`, etc.). On Steam, classic data may live under a Classic-related install path; remastered or other editions may use a different layout — this fork targets classic data only.

**Validation:** discovery and overrides only succeed when the chosen path is a directory that contains `lba2.hqr` (that is the required marker; see `IsValidResourceDir` / `FILE_VALID_RES_DIR` in `SOURCES/DIRECTORIES.CPP`). The filename is matched with the same case-folding logic as other asset files.

**Single asset root:** Once that directory is chosen, `GetResPath`, `GetJinglePath`, `GetMoviePath`, and related APIs resolve files relative to that same `directoriesResDir` (see `InitDirectories` / `GetResPath` in `SOURCES/DIRECTORIES.CPP`). The engine does not look for `music/`, `video/`, `vox/`, or other `.hqr` files on separate paths; finding `lba2.hqr` establishes the one root for classic data.

**Sibling scan:** If the fixed paths above fail, the engine looks at siblings of the parent of the current working directory (for example the folder that contains both your clone and a retail install). For each immediate subdirectory, it tries that folder and `CommonClassic`, `common`, `Common`, and `Classic` under it. Only bounded scans (entry and directory-count limits); see `TryParentSiblingScan` in `SOURCES/RES_DISCOVERY.CPP`.

## Overrides (highest priority first)

| Mechanism | Example |
|-----------|---------|
| Command line | `./lba2cc --game-dir /path/to/game` or `--data-dir` (alias) |
| Environment | `export LBA2_GAME_DIR=/path/to/game` |
| Persisted choice | `last_game_dir.txt` in the user-prefs folder, written automatically the first time you pick a folder via the launch dialog (see [First-launch folder picker](#first-launch-folder-picker) below) |
| Discovery | See [SOURCES/RES_DISCOVERY.CPP](../SOURCES/RES_DISCOVERY.CPP): SDL binary directory, current working directory, parents of cwd, `./data`, `../LBA2`, `../game`, sibling scan of parent-of-cwd |

If none of the above resolves a valid directory, the engine falls back to the [First-launch folder picker](#first-launch-folder-picker). On a headless system (no display, CI runner, etc.), the picker can't open and the process exits with a log listing candidates tried and hints.

## First-launch folder picker

When all four override mechanisms above fail and the engine is running in a windowed environment, it shows the platform-native folder dialog (NSOpenPanel on macOS, IFileDialog on Windows, GTK / xdg-desktop-portal on Linux). The user picks the folder containing `lba2.hqr`; the engine validates it via `IsValidResourceDir`, persists the choice to `last_game_dir.txt`, and continues startup.

| Scenario | Behavior |
|---|---|
| User picks a valid folder (contains `lba2.hqr`) | Engine launches; `last_game_dir.txt` updated; subsequent launches skip the dialog. |
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

**Safety:** Every candidate is accepted only if `IsValidResourceDir` succeeds (`lba2.hqr` present). There is no execution of paths from untrusted config beyond what you pass on the command line or in the environment.

## Developer layout

Common choices:

- Symlink or copy retail files into `data/` at the repo root (that directory is gitignored in this repo so retail files are not committed by mistake).
- One directory up: `../LBA2/` (or `../game/`) with `lba2.hqr` inside.

From any directory in the clone you can use:

```bash
./scripts/dev/build-and-run.sh
```

or `make run` (see [Makefile](../Makefile)). Set `LBA2_BUILD_DIR` if you do not use `build/`.

Windows without Bash: configure CMake as in [WINDOWS.md](WINDOWS.md), then run `lba2cc.exe` with `--game-dir` or set `LBA2_GAME_DIR` in the environment.

## Config file

See [CONFIG.md](CONFIG.md). If `lba2.cfg` is missing from the user config folder, the engine copies from the asset directory when present; if the asset directory has no `lba2.cfg`, an embedded template (from the build) is written instead.

## Cross-references

- [CONFIG.md](CONFIG.md) — `lba2.cfg` keys and paths
- [TESTING.md](TESTING.md) — `test_res_discovery` (host tests for path resolution)
