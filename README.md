# Little Big Adventure 2 Classic - Community Engine Source Code

Little Big Adventure 2 (aka Twinsen's Odyssey) is the sequel to Little Big Adventure (aka Relentless: Twinsen's Adventure) in 1997.

This repository is the community fork of the classic source release. We maintain the project with preservation in mind while improving portability and long-term maintainability.

## About this repository

The original LBA2 engine source is the [`lba2-classic`](https://github.com/2point21/lba2-classic) codebase: it is mostly assembly, with C++ for game logic, and is the canonical historical release. `lba2-classic-community` is a community fork for evolving and modernizing the code: ports of assembly to C++, SDL3, libsmacker, and other updates. The goal is to preserve the history and culture of the original while making the codebase easier to build and extend. See [ASM_TO_CPP_REFERENCE.md](docs/ASM_TO_CPP_REFERENCE.md) for which modules have been ported from ASM to C++ in this fork.

For a history of project changes, please see the [CHANGELOG.md](CHANGELOG.md).

## Quick start (running the game)

### First clone

1. **Dependencies:** CMake (3.23+), a C/C++ compiler, SDL3. The repo `Makefile` also expects Ninja on `PATH`. You can skip the Makefile and use plain CMake with your preferred generator instead.
2. `make` or `make help` — lists convenience targets (`build`, `run`, `clean`, `test`, …).
3. `make build` — configures `build/` (Ninja, Debug) and compiles `lba2`. Or: `cmake -B build && cmake --build build` (default generator is fine if you do not use `make build`).
4. **Retail game data** are not in this repo. You need a directory that contains `lba2.hqr`. How you point the engine at it is your choice: `export LBA2_GAME_DIR=/path`, `./data/` (gitignored), `--game-dir`, or bounded automatic discovery — see [docs/GAME_DATA.md](docs/GAME_DATA.md). Nothing is “special-cased” except that marker file.
5. `make run` or `./scripts/dev/build-and-run.sh` — build if needed, then run (exits with a clear message if no valid data directory was found).
6. `make test` — host-only tests (path resolution, embedded default config, console parser, credits parse); no retail files or Docker required.

**Windows:** Use MSYS2 (recommended; see [docs/WINDOWS.md](docs/WINDOWS.md)). Discovery and the game work the same (`LBA2_GAME_DIR`, `--game-dir`, paths with `\` or `/`). The root `Makefile` and `scripts/dev/*.sh` need a Unix-like shell (MSYS2 UCRT64, Git Bash, or WSL); alternatively run `cmake` and `build/SOURCES/lba2.exe` from cmd.exe / PowerShell and set the env var with `set LBA2_GAME_DIR=...`.

### Build and run (reference)

```bash
cmake -B build && cmake --build build
./build/SOURCES/lba2
```

With optional game data path: `./build/SOURCES/lba2 --game-dir /path/to/classic/install` or set `LBA2_GAME_DIR` first.

`make run` sets `LBA2_GAME_DIR` automatically if `./data` or `../LBA2` contains `lba2.hqr`.

Run `make` for convenience targets: `make build`, `make clean` (removes the default `build/` tree; override with `BUILD_DIR`), `make test`, `make test-docker`, etc.

## Prerequisites

- **CMake** 3.23 or later
- **Ninja** build system for CMake preset-based builds
- **UASM** assembler (optional; only required for `ENABLE_ASM=ON` / ASM-specific workflows)
- **SDL3** (shared library)
- A C/C++ compiler with C++98 support (GCC, Clang)

On macOS, install Ninja with:

```bash
brew install ninja
```

## Building

```bash
cmake -B build
cmake --build build
```

By default, the build uses SDL for audio and libsmacker for FMV playback. Override with `-DSOUND_BACKEND=null -DMVIDEO_BACKEND=null` for a minimal (silent, no video) build.

### Using CMake presets

This repository provides CMake presets for common configurations in `CMakePresets.json`. You can use them instead of specifying the build directory and toolchain flags manually:

All presets use the `Ninja` generator, so `ninja` must be installed and
available on `PATH`.

- **Linux (native build)**:

  ```bash
  cmake --preset linux
  cmake --build --preset linux
  ```

- **macOS (native build)**:

  ```bash
  cmake --preset macos_arm64   # or macos_x86_64
  cmake --build --preset macos_arm64
  ```

- **Windows (native build with MSYS2)** - See [docs/WINDOWS.md](docs/WINDOWS.md) for detailed instructions:

  ```bash
  cmake --preset windows_ucrt64    # recommended for Windows 10+
  cmake --build --preset windows_ucrt64
  ```

- **Cross-compiling Windows from Linux**:

  ```bash
  cmake --preset cross_linux2win
  cmake --build --preset cross_linux2win
  ```

### Build Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `SOUND_BACKEND` | `null`, `miles`, `sdl` | `sdl` | Sound backend. Use `sdl` for audio via SDL3. `miles` requires the proprietary Miles Sound System SDK. See [docs/AUDIO.md](docs/AUDIO.md). |
| `MVIDEO_BACKEND` | `null`, `smacker` | `smacker` | Motion video backend. Use `smacker` for FMV playback via the bundled open-source libsmacker. |
| `DEBUG_TOOLS` | `ON`, `OFF` | `OFF` | Enable original Adeline developer debug tools. See [docs/DEBUG.md](docs/DEBUG.md). |
| `LBA2_BUILD_TESTS` | `ON`, `OFF` | `OFF` | Build CTest targets (ASM equivalence + host tests such as `test_res_discovery`). |
| `LBA2_BUILD_ASM_EQUIV_TESTS` | `ON`, `OFF` | `ON` | ASM↔CPP equivalence suite (needs `objcopy`). Set `OFF` for host-only tests (e.g. macOS CI, `make test`). |

When `MVIDEO_BACKEND` is set to `smacker`, the build links in `libsmacker` and the FMV player. Video audio routes through the active sound backend (SDL: real audio; NULL/MILES: silent). See `LIB386/SMACKER/README.md` and `LIB386/AIL/MILES/README.md` for details on the proprietary SDKs and their open-source replacements.

### Debug Console

This source port includes a Quake-style drop-down debug console. It lets you inspect engine/game state and run developer commands during gameplay, which is useful for both debugging and interactive exploration of engine behavior.

It is designed to be minimally invasive: normal gameplay is unchanged unless you open and use it.

See [docs/CONSOLE.md](docs/CONSOLE.md) for commands, usage, and integration details.

### Debug Tools

To build with the original Adeline developer debug tools enabled:

```bash
cmake -B build -DDEBUG_TOOLS=ON
cmake --build build
```

This enables keyboard shortcuts for debugging (debug overlay, FPS counter, screenshots, collision visualization, benchmarks), cheat codes, bug save/load system, and command-line scene selection. See [docs/DEBUG.md](docs/DEBUG.md) for full documentation.

### Cross-compiling for Windows (from Linux)

A MinGW toolchain file is provided for cross-compiling a 32-bit Windows build from Linux:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-i686.cmake
cmake --build build
```

## Project Structure

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

## Preservation Notes

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

## Original Development Team

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
