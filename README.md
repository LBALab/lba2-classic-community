# Little Big Adventure 2 — Meta Quest port (AI experiment)

This repository is a fork of [LBALab/lba2-classic-community](https://github.com/LBALab/lba2-classic-community).

The purpose of this fork is a **native Meta Quest port of Twinsen's Odyssey** (Little Big Adventure 2). The port is an experiment developed entirely with AI coding agents; it is not an official LBALab release, and its changes should be treated as experimental.

On Quest the game runs with native **stereo 3D**, full **6DoF** head and controller tracking, a **first-person IK body and camera**, and an **interior 3D parallax diorama** effect. The port adds various graphical improvements while keeping the original gameplay and assets intact.

The project targets mainly Quest, but desktop builds should also work; the desktop build has been tested on Linux.

The OpenGL/GLES renderer is the one contributed in [PR #457 by Link4Electronics](https://github.com/LBALab/lba2-classic-community/pull/457). Everything else builds on the community fork: SDL3, libsmacker, the OpenXR/Quest layer, the VR options, movie upscaling and the reworked menus.

Target performance on Quest is **72 fps**, and the game is mostly stable at that rate through a playthrough.

<p align="center">
  <img src="docs/images/quest-port.jpg" alt="Twinsen and Zoé wearing Meta Quest headsets in front of the LBA2 logo" width="420">
</p>

## What this fork adds

| Area | Improvement / option | Notes |
|---|---|---|
| Graphics | OpenGL/GLES renderer | From [PR #457](https://github.com/LBALab/lba2-classic-community/pull/457) by Link4Electronics; the software renderer remains a runtime fallback |
| Graphics | Runtime resolution and widescreen | Selectable from the Display menu; 320×200 up to 1920×1080 |
| Graphics | Texture filters | Nearest, Bilinear, Scale2x, xBR, xBR Lite; applied per texture on world geometry |
| Graphics | Antialiasing | Off, FXAA, MSAA×2, MSAA×4; Quest caps at MSAA×2 |
| Graphics | Render quality (SSAA) | Native, 2×, 4×; Quest is clamped to Native |
| Graphics | Image adjustments | Brightness, contrast and saturation at composite time |
| Graphics | Post effects | Dithering, sharpening, colour grade, soft shadows and sub-pixel interior pan |
| Graphics | Perspective-correct textures | Always on for opaque decor/object polygons; chroma-keyed polys stay affine |
| Graphics | Movie upscaling | Nearest, Scale2x or xBR; default xBR and independent of the game texture filter |
| Quest | Native OpenXR | Android arm64 build, immersive session, native stereo 3D, full 6DoF tracking and single-pass stereo |
| Quest | VR presentation | VR zoom, supersampling, interior hover, sub-pixel pan, interior passthrough, a 3D parallax diorama and a 70% movie screen |
| Quest | First-person body | First-person IK body/camera, full-body hand IK and world-scale control |
| Quest | Controller mapping | A confirm/use, B camera/back, X inventory, Y holomap, long-menu in-game menu, right-thumb cheat chord (Full / Gold) |
| Quest | Quality defaults | xBR Lite, MSAA×2, soft shadows on, vsync off, stereo 3D on, 1.5× supersampling, interior hover/sub-pixel/full body on |
| Quest | Performance target | 72 fps; mostly stable through the game |

## About this repository

This is a fork of [`lba2-classic-community`](https://github.com/LBALab/lba2-classic-community), which is itself a community fork of the original [`lba2-classic`](https://github.com/2point21/lba2-classic) source release: mostly assembly, with C++ for game logic, and the canonical historical release. The upstream community fork ports assembly to C++, adds SDL3 for graphics/audio/input, libsmacker for FMV, native Linux/macOS/Windows/Android builds, widescreen support, gamepad play and an always-on debug console. This fork keeps that preservation work and adds the Quest/OpenXR port; see [ASM_TO_CPP_REFERENCE.md](docs/ASM_TO_CPP_REFERENCE.md) for which modules have been ported from ASM to C++.

Game assets aren't included; you need a legitimate copy of LBA2 to play.

For a history of project changes, please see the [CHANGELOG.md](CHANGELOG.md).

## Playing

This fork is built from source. Quest builds are also published on this fork's [Releases page](https://github.com/s-gelazevicius/lba2-quest/releases); the upstream links below are the community project's releases.

**Upstream stable** — [the latest tagged release](https://github.com/LBALab/lba2-classic-community/releases/latest). A milestone build with less churn than rolling. Pre-1.0 — bugs are still being ironed out, and full-playthrough testing isn't part of the release criteria yet (see the [1.0 bar](docs/RELEASING.md) for what changes at 1.0).

**Upstream bleeding edge** — [the rolling `latest` pre-release](https://github.com/LBALab/lba2-classic-community/releases/tag/latest). Rebuilt from `main` on every push, may contain unreleased changes, and sometimes carries artifacts not yet in the latest stable tag (newly added platforms land here first). Useful for verifying a fix landed or chasing the freshest feature; not play-tested.

All upstream releases: [Releases page](https://github.com/LBALab/lba2-classic-community/releases).

### Game data

You need a legitimate copy of LBA2 ([GOG](https://www.gog.com/game/little_big_adventure_2), [Steam](https://store.steampowered.com/app/398000/Little_Big_Adventure_2/), or your retail CD) — the engine doesn't ship assets.

On first launch a folder picker opens; point it at the directory containing `lba2.hqr` (alongside `music/`, `video/`, `vox/`, …, or under a `Common/` / `CommonClassic/` subfolder for Steam/GOG re-releases). Your choice is remembered.

To skip the picker, pass an explicit path:

```bash
./lba2cc --game-dir /path/to/game
LBA2_GAME_DIR=/path/to/game ./lba2cc
```

To re-pick later: `./lba2cc --pick-game-dir`. See [docs/GAME_DATA.md](docs/GAME_DATA.md) for the full discovery order and override precedence.

### Quest (experimental)

The Quest build is a native Android/OpenXR APK. Set the Android SDK, NDK and an SDL3 Android install, connect the headset with developer mode enabled, then run the wizard:

```bash
export ANDROID_HOME=/path/to/android-sdk
export ANDROID_NDK=/path/to/android-ndk
export SDL3_ANDROID_DIR=/path/to/sdl3-android-install

bash scripts/dev/build-and-install-quest.sh
```

The wizard builds `out/build/quest`, pushes the retail HQR files to `/sdcard/lba2cc/`, and installs the `org.lbalab.lba2cc` APK. See [docs/ANDROID.md](docs/ANDROID.md) for the OpenXR setup and [docs/QUEST_CONTROLS.md](docs/QUEST_CONTROLS.md) for the controller map.

### Install on Quest (prebuilt APK)

The installers download Android platform-tools into a temporary folder, fetch
the latest Quest APK from [this fork's releases](https://github.com/s-gelazevicius/lba2-quest/releases),
ask for the folder containing `lba2.hqr`, copy the game data to
`/sdcard/lba2cc/`, and install the APK.

Requirements:

- Meta Quest 2/3/Pro with Developer Mode and USB debugging enabled
- A data-capable USB cable
- Your own LBA2 game data; no assets are bundled

Linux:

```bash
bash scripts/quest/install-quest-linux.sh
```

macOS:

```bash
bash scripts/quest/install-quest-macos.sh
```

Windows:

```bat
scripts\quest\install-quest-windows.cmd
```

Or run PowerShell directly:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\quest\install-quest-windows.ps1
```

The scripts accept `--assets <path>`, `--apk <path>`, `--tag <tag|latest>`,
`--clean`, `--no-launch`, `--keep-tools`, and `--dry-run` (Windows uses
`-Assets`, `-Apk`, etc.). They install adb temporarily and do not require
Docker or root.

If the app was previously installed with a different debug key, the script
offers to uninstall and reinstall. App-local saves may be lost; game data in
`/sdcard/lba2cc/` is kept.

Manual fallback:

```bash
adb install -r lba2cc-<version>-quest-arm64-v8a.apk
adb shell mkdir -p /sdcard/lba2cc
adb push /path/to/lba2 /sdcard/lba2cc/
```

Troubleshooting:

- No device in `adb devices`: enable Developer Mode and USB debugging, use a
  data cable, and accept the debugging prompt in the headset.
- Windows does not list the headset: install the Meta Quest ADB driver.
- Apple Silicon reports a bad CPU type: install Rosetta 2.
- Signature mismatch: rerun with `--clean` (or `-Clean`).

### Linux

Grab `lba2cc-<version>-anylinux-<arch>.AppImage`, then:

```bash
chmod +x lba2cc-*-anylinux-*.AppImage
./lba2cc-*-anylinux-*.AppImage
```

> On distros without FUSE, or for packagers wrapping the binary, a static `lba2cc-<version>-linux-<arch>.tar.gz` is published alongside the AppImage — extract and run `lba2cc`.

### macOS

Download `lba2cc-<version>-macos-arm64.dmg`, open it, drag `LBA2 Classic Community.app` to Applications. First launch needs right-click → **Open** (macOS Gatekeeper blocks unsigned apps on double-click). The DMG ships its own README covering this — read it before the first launch.

### Windows

Download `lba2cc-<version>-windows-x64.zip`, unzip anywhere, run `lba2cc.exe`. Portable build: no installer, no DLLs needed.

### Android

Download the `lba2cc-<version>-android-arm64-v8a.apk` (or `armeabi-v7a`). Requires **Android 7.0 (API 24)** or later. You must provide your own retail game data (see [Game data](#game-data) and [docs/ANDROID.md](docs/ANDROID.md) for setup).

## Building from source quick start

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
2. `make build` — configures `build/` (Ninja, Debug) and compiles `lba2cc`. Or plain CMake: `cmake -B build && cmake --build build`.
3. Point the engine at your game data — `export LBA2_GAME_DIR=/path`, `./data/` (gitignored), `--game-dir`, or bounded automatic discovery (the `lba2.hqr` marker is the only special-cased file). See [docs/GAME_DATA.md](docs/GAME_DATA.md).
4. `make run` or `./scripts/dev/build-and-run.sh` — build if needed, then run. `make run` sets `LBA2_GAME_DIR` automatically if `./data` or `../LBA2` contains `lba2.hqr`; otherwise pass `--game-dir /path/to/classic/install` to the binary.
5. `make test` — host-only tests (path resolution, parsers, ABI bounds, version checks); no retail files or Docker required.

**Windows:** Use MSYS2 (recommended; see [docs/WINDOWS.md](docs/WINDOWS.md)). Discovery and the game work the same (`LBA2_GAME_DIR`, `--game-dir`, paths with `\` or `/`). The root `Makefile` and `scripts/dev/*.sh` need a Unix-like shell (MSYS2 UCRT64, Git Bash, or WSL); alternatively run `cmake` and `build/SOURCES/lba2cc.exe` from cmd.exe / PowerShell and set the env var with `set LBA2_GAME_DIR=...`.

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

Read our [Contribution Guidelines](https://github.com/LBALab/lba2-classic-community/blob/main/CONTRIBUTING.md) and the [Code Style](https://github.com/LBALab/lba2-classic-community/blob/main/CODESTYLE.md) reference.

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
