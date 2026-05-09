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
| Discovery | See [SOURCES/RES_DISCOVERY.CPP](../SOURCES/RES_DISCOVERY.CPP): SDL binary directory, current working directory, parents of cwd, `./data`, `../LBA2`, `../game`, sibling scan of parent-of-cwd |

If no valid directory is found, the process exits with a log listing candidates tried and hints.

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
