# Game data (retail files)

The engine needs the **original Little Big Adventure 2** data files. They are **not** included in this repository (licensing). You must own a legitimate copy (e.g. [GOG](https://www.gog.com/game/little_big_adventure_2), [Steam](https://store.steampowered.com/app/398000/)).

## What to point the engine at

Use the directory that contains **`lba2.hqr`** (and the other `.hqr` files, `music/`, `video/`, `vox/`, etc.). On Steam, classic data may live under a **Classic**-related install path; remastered or other editions may use a different layout—this fork targets **classic** data only.

## Overrides (highest priority first)

| Mechanism | Example |
|-----------|---------|
| Command line | `./lba2 --game-dir /path/to/game` or `--data-dir` (alias) |
| Environment | `export LBA2_GAME_DIR=/path/to/game` |
| Discovery | See [SOURCES/RES_DISCOVERY.CPP](../SOURCES/RES_DISCOVERY.CPP): SDL binary directory, current working directory, parents of cwd, `./data`, `../LBA2`, `../game` |

If no valid directory is found, the process exits with a log listing **candidates tried** and hints.

## Developer layout

Common choices:

- Symlink or copy retail files into **`data/`** at the repo root.
- One directory up: **`../LBA2/`** (or `../game/`) with `lba2.hqr` inside.

From any directory in the clone you can use:

```bash
./scripts/dev/build-and-run.sh
```

or **`make run`** (see [Makefile](../Makefile)). Set **`LBA2_BUILD_DIR`** if you do not use `build/`.

Windows without Bash: configure CMake as in [WINDOWS.md](WINDOWS.md), then run `lba2.exe` with `--game-dir` or set `LBA2_GAME_DIR` in the environment.

## Config file

See [CONFIG.md](CONFIG.md). If `lba2.cfg` is missing from the **user** config folder, the engine copies from the asset directory when present; if the asset directory has no `lba2.cfg`, an **embedded** template (from the build) is written instead.

## Cross-references

- [CONFIG.md](CONFIG.md) — `lba2.cfg` keys and paths
- [TESTING.md](TESTING.md) — `test_res_discovery` (host tests for path resolution)
