# Little Big Adventure 2 Classic Community

Little Big Adventure 2 (aka Twinsen's Odyssey) is the sequel to Little Big Adventure (aka Relentless: Twinsen's Adventure) in 1997.

This repository is the community fork of the classic source release — a source port maintained with preservation in mind while improving portability and long-term maintainability. Game assets aren't included; you need a legitimate copy of LBA2 to play.

## Features

- **The complete game** — the original 1997 engine, ported to 64-bit and modern compilers; bring your own copy of LBA2
- **Native builds** — Linux, macOS, Windows, Android / Android TV (7.0+, API 24), and experimental Nintendo Switch homebrew
- **Widescreen and HD** — render resolutions up to 1080p, offered per monitor from the Display menu; the classic 640×480 4:3 mode is still there ([docs/RUNTIME_RESOLUTION.md](docs/RUNTIME_RESOLUTION.md))
- **Reads disc images directly** — retail assets straight out of a raw ISO/BIN or `.cue` pair, so a GOG Original Edition install or a CD rip needs no extraction step
- **Retail-compatible saves** — 1997 saves load directly, and every build of this port writes the same 32-bit save bytes
- **Frame-rate-independent movement** — a fixed 60 Hz simulation step, so walk and jump distances no longer change with your frame rate
- **Gamepad support** — full controller mapping, radial deadzones, and a rebindable D-pad ([docs/CONTROLLER.md](docs/CONTROLLER.md))
- **Third-person camera** — optional follow camera in exterior scenes, orbited with the mouse or the right stick, and zoomed with the mouse wheel or the numpad ([docs/CAMERA.md](docs/CAMERA.md))
- **Render smoothing** — optional texture filtering and dithered shading for the software rasterizer, off by default ([docs/GFX_OPTIONS.md](docs/GFX_OPTIONS.md))
- **Modern audio backend** — SDL3 in place of the original Miles Sound System
- **FMV playback** — via the bundled open-source libsmacker
- **Debug console** — always-on Quake-style console ([docs/CONSOLE.md](docs/CONSOLE.md))

For a history of project changes, please see the [CHANGELOG.md](CHANGELOG.md).

## About this repository

The original LBA2 engine source is the [`lba2-classic`](https://github.com/2point21/lba2-classic) codebase: it is mostly assembly, with C++ for game logic, and is the canonical historical release. `lba2-classic-community` is a community fork for evolving and modernizing the code: ports of assembly to C++, SDL3 for graphics/audio/input, libsmacker for FMV, native Linux/macOS/Windows/Android builds, and player-facing additions such as widescreen support, gamepad play, and an always-on debug console. The goal is to preserve the history and culture of the original while making the codebase easier to build and extend. See [ASM_TO_CPP_REFERENCE.md](docs/ASM_TO_CPP_REFERENCE.md) for which modules have been ported from ASM to C++ in this fork.

## Why this project

We want this to be the way to play LBA2 today, made with the same love and care Adeline put into the original.

Care here starts with the 1997 game as the reference. When the port drifts from the original DOS release without meaning to, that is a bug, and we check the original rather than guess. Where the original has a bug of its own, fixing it is the player's choice (Options → Advanced options → Fix retail bugs), not a silent change. What we learn about the engine goes into [docs/knowledge/](docs/knowledge/index.md) with the evidence behind it, so nobody has to find it out twice.

## Playing

Pre-built binaries for Linux, macOS, Windows, Android, and Nintendo Switch homebrew.

**Stable** — [the latest tagged release](https://github.com/LBALab/lba2-classic-community/releases/latest). A milestone build with less churn than rolling. Pre-1.0 — bugs are still being ironed out, and full-playthrough testing isn't part of the release criteria yet (see the [1.0 bar](docs/RELEASING.md) for what changes at 1.0).

**Bleeding edge** — [the rolling `latest` pre-release](https://github.com/LBALab/lba2-classic-community/releases/tag/latest). Rebuilt from `main` on every push, may contain unreleased changes, and sometimes carries artifacts not yet in the latest stable tag (newly added platforms land here first). Useful for verifying a fix landed or chasing the freshest feature; not play-tested.

All releases: [Releases page](https://github.com/LBALab/lba2-classic-community/releases).

### Game data

You need a legitimate copy of LBA2 ([GOG](https://www.gog.com/game/little_big_adventure_2), [Steam](https://store.steampowered.com/app/398000/Little_Big_Adventure_2/), or your retail CD) — the engine doesn't ship assets.

On desktop, a folder picker opens on first launch; point it at the directory containing `LBA2.HQR` (alongside `music/`, `video/`, `vox/`, …, or under a `Common/` / `CommonClassic/` subfolder for Steam/GOG re-releases). Your choice is remembered.

To skip the picker, pass an explicit path:

```bash
./lba2cc --game-dir /path/to/game
LBA2_GAME_DIR=/path/to/game ./lba2cc
```

To re-pick later: `./lba2cc --pick-game-dir`. See [docs/GAME_DATA.md](docs/GAME_DATA.md) for the full discovery order and override precedence.

### Linux

Grab `lba2cc-<version>-anylinux-<arch>.AppImage`, then:

```bash
chmod +x lba2cc-*-anylinux-*.AppImage
./lba2cc-*-anylinux-*.AppImage
```

> On distros without FUSE, or for packagers wrapping the binary, a static `lba2cc-<version>-linux-<arch>.tar.gz` is published alongside the AppImage — extract and run `lba2cc`.

### macOS

Download `lba2cc-<version>-macos-universal.dmg` (runs on Apple Silicon and Intel Macs, macOS 11 or later), open it, drag `LBA2 Classic Community.app` to Applications. First launch needs right-click → **Open** (macOS Gatekeeper blocks unsigned apps on double-click). The DMG ships its own README covering this — read it before the first launch.

### Windows

Download `lba2cc-<version>-windows-x64.zip`, unzip anywhere, run `lba2cc.exe`. Portable build: no installer, no DLLs needed.

### Android

Download the `lba2cc-<version>-android-arm64-v8a.apk` (or `armeabi-v7a`). Requires **Android 7.0 (API 24)** or later. You must provide your own retail game data (see [Game data](#game-data) and [docs/ANDROID.md](docs/ANDROID.md) for setup).

### Nintendo Switch

Experimental homebrew build: download `lba2cc-switch.zip` from the [rolling pre-release](https://github.com/LBALab/lba2-classic-community/releases/tag/latest) and extract it onto the SD-card root. The executable lands at `sdmc:/switch/lba2cc/lba2cc.nro`.

Copy your retail HQR files, including `LBA2.HQR`, directly into `sdmc:/lba2/`, preserving the installation's music, movie and voice subdirectories. No game assets are included. Launch through Homebrew Menu in application mode/title takeover, not the memory-constrained Album applet.

Saves, settings and logs live under `sdmc:/switch/lba2cc/user/`; back up this directory before upgrading. See [docs/SWITCH.md](docs/SWITCH.md) for the SD-card layout, network launching and hardware-validation limits.

## Building from source quick start

### Prerequisites

- CMake 3.23+
- Ninja (for `make build` and presets)
- A C/C++ compiler with C++98 support (GCC, Clang)
- SDL3 (shared library)
- GNU Make — only required for the `make` shortcuts; plain CMake works without it
- Optional: UASM — only required for `ENABLE_ASM=ON` workflows

On macOS, install with `brew install ninja sdl3`.

Run `./scripts/dev/check-tooling.sh` to see what your machine is missing. That covers the build; the tools needed for testing, linting, packaging, Android, and releasing are indexed by tier in [docs/TOOLING.md](docs/TOOLING.md).

### First clone

1. `make` or `make help` — lists convenience targets (`build`, `run`, `clean`, `test`, …).
2. `make build` — configures `build/` (Ninja, Debug) and compiles `lba2cc`. Or plain CMake: `cmake -B build && cmake --build build`.
3. Point the engine at your game data — `export LBA2_GAME_DIR=/path`, `./data/` (gitignored), `--game-dir`, or bounded automatic discovery (the `LBA2.HQR` marker is the only special-cased file). See [docs/GAME_DATA.md](docs/GAME_DATA.md).
4. `make run` or `./scripts/dev/build-and-run.sh` — build if needed, then run. `make run` sets `LBA2_GAME_DIR` automatically if `./data` or `../LBA2` contains `LBA2.HQR`; otherwise pass `--game-dir /path/to/classic/install` to the binary.
5. `make test` — host-only tests (path resolution, parsers, ABI bounds, version checks); no retail files or Docker required.

**Windows:** Use MSYS2 (recommended; see [docs/WINDOWS.md](docs/WINDOWS.md)). Discovery and the game work the same (`LBA2_GAME_DIR`, `--game-dir`, paths with `\` or `/`). The root `Makefile` and `scripts/dev/*.sh` need a Unix-like shell (MSYS2 UCRT64, Git Bash, or WSL); alternatively run `cmake` and `build/SOURCES/lba2cc.exe` from cmd.exe / PowerShell and set the env var with `set LBA2_GAME_DIR=...`.

## CMake presets

For platform-specific builds, use the presets in `CMakePresets.json` (all use the `Ninja` generator, so `ninja` must be on `PATH`):

- **Linux:** `cmake --preset linux && cmake --build --preset linux`
- **macOS:** `cmake --preset macos_arm64 && cmake --build --preset macos_arm64` (or `macos_x86_64`, or `macos_universal` for both in one binary)
- **Windows:** `cmake --preset windows_ucrt64 && cmake --build --preset windows_ucrt64` — see [docs/WINDOWS.md](docs/WINDOWS.md)
- **Cross-compile Windows from Linux:** `cmake --preset cross_linux2win && cmake --build --preset cross_linux2win`. To skip the preset, use the toolchain file directly: `cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-i686.cmake`.
- **Nintendo Switch homebrew:** `bash scripts/dev/build-switch.sh` — builds an SDL2 `.nro` and SD-card ZIP inside a digest-pinned devkitPro Docker image. No host SDK needed; hardware validation remains pending. See [docs/SWITCH.md](docs/SWITCH.md).

## Build options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `PLATFORM_BACKEND` | `sdl3`, `sdl2`, `null` | `sdl3` | Window, presentation, input, clocks and host services. SDL3 is the default; SDL2 supports desktop and Switch homebrew with audio, but no native folder picker. Android requires SDL3. `null` presents offscreen without an SDK and requires `SOUND_BACKEND=null`. See [docs/PLATFORM_ABSTRACTION.md](docs/PLATFORM_ABSTRACTION.md). |
| `SOUND_BACKEND` | `null`, `miles`, `sdl` | `sdl` | Sound backend. `sdl` uses the selected SDL2/SDL3 platform's SDK. `miles` requires the proprietary Miles Sound System SDK. See [docs/AUDIO.md](docs/AUDIO.md). |
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

Read our [Contribution Guidelines](https://github.com/LBALab/lba2-classic-community/blob/main/CONTRIBUTING.md) and the [Code Style](https://github.com/LBALab/lba2-classic-community/blob/main/CODESTYLE.md) reference.

Use whatever tools help you, AI assistants included. What makes a change good is not who or what typed it, but whether someone cared enough to play it, check it against the original and explain why it is right.

## Links

* **Official Website:** https://twinsenslittlebigadventure.com/
* **Discord:** https://discord.gg/jsTPWYXHsh
* **Docs:** https://lba-classic-doc.readthedocs.io/

## Buy the game

* [GOG](https://www.gog.com/game/little_big_adventure_2)  
* [Steam](https://store.steampowered.com/app/398000/Little_Big_Adventure_2/)

## Original development team

* **Direction:** Frédérick Raynal
* **Programmers:** Sébastien Viannay / Laurent Salmeron / Cédric Bermond / Frantz Cournil / Marc Bureau du Colombier
* **3D Artists & Animations:** Paul-Henri Michaud / Arnaud Lhomme
* **Artists:** Yaeël Barroz, Sabine Morlat, Didier Quentin
* **Story & Design:** Frédérick Raynal / Didier Chanfray / Yaël Barroz / Laurent Salmeron / Marc Albinet
* **Dialogs:** Marc Albinet
* **Story coding:** Frantz Cournil / Lionel Chaze / Pascal Dubois
* **Video Sequences:** Frédéric Taquet / Benoît Boucher / Ludovic Rubin / Merlin Pardot
* **Music & Sound FX:** Philippe Vachey
* **Testing:** Bruno Marion / Thomas Ferraz / Alexis Madinier / Christopher Horwood / Bertrand Fillardet
* **Quality Control:** Emmanuel Oualid

Use the `credits` command in the [console](docs/CONSOLE.md) to see the full original credits.

## Copyright

The intellectual property is currently owned by [2.21]. Copyright [2.21]
Originally developed by Adeline Software International in 1994
