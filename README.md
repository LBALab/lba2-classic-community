# Little Big Adventure 2 Classic - Community Engine Source Code

Little Big Adventure 2 (aka Twinsen's Odyssey) is the sequel to Little Big Adventure (aka Relentless: Twinsen's Adventure) in 1997.

This repository is the community fork of the classic source release. We maintain the project with preservation in mind while improving portability and long-term maintainability.

## About this repository

The original LBA2 engine source is the [`lba2-classic`](https://github.com/2point21/lba2-classic) codebase: it is mostly assembly, with C++ for game logic, and is the canonical historical release. `lba2-classic-community` is a community fork for evolving and modernizing the code: ports of assembly to C++, SDL3, libsmacker, and other updates. The goal is to preserve the history and culture of the original while making the codebase easier to build and extend. See [ASM_TO_CPP_REFERENCE.md](docs/ASM_TO_CPP_REFERENCE.md) for which modules have been ported from ASM to C++ in this fork.

For a history of project changes, please see the [CHANGELOG.md](CHANGELOG.md).

## Quick start (running the game)

### Prerequisites

- CMake 3.23+
- Ninja (for `make build` and presets)
- A C/C++ compiler with C++98 support (GCC, Clang)
- SDL3 (shared library)
- GNU Make — only required for the `make` shortcuts; plain CMake works without it
- Optional: UASM — only required for `ENABLE_ASM=ON` workflows

On macOS, install with `brew install ninja sdl3`.

### First clone

1. `make` or `make help` — lists convenience targets (`build`, `run`, `clean`, `test`, …).
2. `make build` — configures `build/` (Ninja, Debug) and compiles `lba2`. Or plain CMake: `cmake -B build && cmake --build build`.
3. **Retail game data** are not in this repo. You need a directory that contains `lba2.hqr`. How you point the engine at it is your choice: `export LBA2_GAME_DIR=/path`, `./data/` (gitignored), `--game-dir`, or bounded automatic discovery — see [docs/GAME_DATA.md](docs/GAME_DATA.md). Nothing is "special-cased" except that marker file.
4. `make run` or `./scripts/dev/build-and-run.sh` — build if needed, then run. `make run` sets `LBA2_GAME_DIR` automatically if `./data` or `../LBA2` contains `lba2.hqr`; otherwise pass `--game-dir /path/to/classic/install` to the binary.
5. `make test` — host-only tests (path resolution, parsers, ABI bounds, version checks); no retail files or Docker required.

**Windows:** Use MSYS2 (recommended; see [docs/WINDOWS.md](docs/WINDOWS.md)). Discovery and the game work the same (`LBA2_GAME_DIR`, `--game-dir`, paths with `\` or `/`). The root `Makefile` and `scripts/dev/*.sh` need a Unix-like shell (MSYS2 UCRT64, Git Bash, or WSL); alternatively run `cmake` and `build/SOURCES/lba2.exe` from cmd.exe / PowerShell and set the env var with `set LBA2_GAME_DIR=...`.

## CMake presets

For platform-specific builds, use the presets in `CMakePresets.json` (all use the `Ninja` generator, so `ninja` must be on `PATH`):

- **Linux:** `cmake --preset linux && cmake --build --preset linux`
- **macOS:** `cmake --preset macos_arm64 && cmake --build --preset macos_arm64` (or `macos_x86_64`)
- **Windows:** `cmake --preset windows_ucrt64 && cmake --build --preset windows_ucrt64` — see [docs/WINDOWS.md](docs/WINDOWS.md)
- **Cross-compile Windows from Linux:** `cmake --preset cross_linux2win && cmake --build --preset cross_linux2win`. To skip the preset, use the toolchain file directly: `cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-i686.cmake`.

## Build options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `SOUND_BACKEND` | `null`, `miles`, `sdl` | `sdl` | Sound backend. Use `sdl` for audio via SDL3. `miles` requires the proprietary Miles Sound System SDK. See [docs/AUDIO.md](docs/AUDIO.md). |
| `MVIDEO_BACKEND` | `null`, `smacker` | `smacker` | Motion video backend. Use `smacker` for FMV playback via the bundled open-source libsmacker. |
| `DEBUG_TOOLS` | `ON`, `OFF` | `OFF` | Enable original Adeline developer debug tools: overlay, FPS counter, screenshots, collision visualization, benchmarks, cheat codes, bug save/load, command-line scene selection. See [docs/DEBUG.md](docs/DEBUG.md). |
| `LBA2_BUILD_TESTS` | `ON`, `OFF` | `OFF` | Build CTest targets (ASM equivalence + host tests such as `test_res_discovery`). |
| `LBA2_BUILD_ASM_EQUIV_TESTS` | `ON`, `OFF` | `ON` | ASM↔CPP equivalence suite (needs `objcopy`). Set `OFF` for host-only tests (e.g. macOS CI, `make test`). |

Minimal build (no audio/video): `-DSOUND_BACKEND=null -DMVIDEO_BACKEND=null`. When `MVIDEO_BACKEND=smacker`, video audio routes through the active sound backend (SDL: real audio; NULL/MILES: silent). See `LIB386/SMACKER/README.md` and `LIB386/AIL/MILES/README.md` for details on the proprietary SDKs and their open-source replacements.

## Debug console

This source port includes a Quake-style drop-down debug console. It is always available (no build flag), designed to be minimally invasive — normal gameplay is unchanged unless you open and use it. See [docs/CONSOLE.md](docs/CONSOLE.md) for commands, usage, and integration details.

## Project structure

```text
lba2-classic-community/
├── CMakeLists.txt            # Root build configuration
├── CMakePresets.json         # Cross-platform preset builds (linux/macos/windows/...)
├── Makefile                  # Convenience targets (build/run/test/format)
├── cmake/                    # Toolchains and CMake helpers
├── scripts/                  # Dev and CI helper scripts
├── SOURCES/                  # Main game logic and app entrypoints
│   ├── CONSOLE/              # Always-on debug console module (core + state)
│   ├── 3DEXT/                # 3D extensions (terrain, sky, rain, decor)
│   ├── CONFIG/               # Input/config UI and bindings
│   └── *.CPP, *.H, *.ASM     # Gameplay systems (AI, physics, save/load, etc.)
├── LIB386/                   # Engine libraries
│   ├── 3D/                   # Projection, rotation, matrices
│   ├── AIL/                  # Audio abstraction (SDL/Miles/null)
│   ├── ANIM/                 # Animation system
│   ├── OBJECT/               # 3D object rendering
│   ├── pol_work/             # Polygon fillers/rasterization
│   ├── SVGA/                 # Text/sprite/dirty-box rendering paths
│   ├── SYSTEM/               # Platform/system/input/timer abstractions
│   ├── H/                    # Shared legacy headers/types
│   └── libsmacker/           # Open-source Smacker decoder (LGPL 2.1)
├── tests/                    # Host tests + ASM↔CPP equivalence test wiring
├── docs/                     # Project documentation index and subsystem docs
└── run_tests_docker.sh       # Docker wrapper for ASM↔CPP test workflows
```

## Documentation

**Engine reference** (terms, lifecycles, scene index): [GLOSSARY](docs/GLOSSARY.md), [LIFECYCLES](docs/LIFECYCLES.md), [SCENES](docs/SCENES.md).

Build, debug, preservation, and porting docs are in [docs/](docs/README.md).

## Preservation notes

This codebase is a window into 1990s game development at Adeline Software International in Lyon, France. Beyond the technical content, the source files contain original developer artifacts worth exploring. The ASCII art and French comments documented below are from the original Adeline / lba2-classic codebase (same files or content preserved when porting ASM to C++ in this fork).

- **ASCII art banners** -- The developers decorated their source files with elaborate text banners in two distinct styles. See [ASCII_ART.md](docs/ASCII_ART.md) for a full catalog.
- **French comments** -- The code is written with French comments throughout, many of which are informal, humorous, or expressive in ways that reflect the personality of the team. See [FRENCH_COMMENTS.md](docs/FRENCH_COMMENTS.md) for a curated selection with English translations.

## License

This source code is licensed under the [GNU General Public License](https://github.com/LBALab/lba2-classic-community/blob/main/LICENSE).

Please note this license only applies to **Little Big Adventure 2** engine source code. **Little Big Adventure 2** game assets (art, models, textures, audio, etc.) are not open-source and therefore aren't redistributable.

## How can I contribute?

Read our [Contribution Guidelines](https://github.com/LBALab/lba2-classic-community/blob/main/CONTRIBUTING.md).

## Links

* **Official Website:** https://twinsenslittlebigadventure.com/
* **Discord:** https://discord.gg/jsTPWYXHsh
* **Docs:** https://lba-classic-doc.readthedocs.io/

## Buy the game

* [GOG](https://www.gog.com/game/little_big_adventure_2)  
* [Steam](https://store.steampowered.com/app/398000/Little_Big_Adventure_2/)

## Original development team

*Direction:* Frédérick Raynal
*Programmers:* Sébastien Viannay / Laurent Salmeron / Cédric Bermond / Frantz Cournil / Marc Bureau du Colombier
*3D Artists & Animations:* Paul-Henri Michaud / Arnaud Lhomme
*Artists:* Yaeël Barroz, Sabine Morlat, Didier Quentin
*Story & Design:* Frédérick Raynal / Didier Chanfray / Yaël Barroz / Laurent Salmeron / Marc Albinet
*Dialogs:* Marc Albinet
*Story coding:* Frantz Cournil / Lionel Chaze / Pascal Dubois
*Video Sequences:* Frédéric Taquet / Benoît Boucher / Ludovic Rubin / Merlin Pardot
*Music & Sound FX:* Philippe Vachey
*Testing:* Bruno Marion / Thomas Ferraz / Alexis Madinier / Christopher Horwood / Bertrand Fillardet
*Quality Control:* Emmanuel Oualid

## Copyright

The intellectual property is currently owned by [2.21]. Copyright [2.21]
Originally developed by Adeline Software International in 1994
