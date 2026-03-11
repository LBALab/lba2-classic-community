# Little Big Adventure 2 - Engine source code - Community

Little Big Adventure 2 (aka Twinsen's Odyssey) is the sequel of Little Big Adventure (aka Relentless: Twinsen's Adventure) in 1997.

We are releasing this code with preservation in mind, as this piece of work was exceptional for the time and we believe it can be a valuable source of education.

The engine uses Assembly code and was originally compiled with non-open source libraries which have been excluded from the project.

## About this repository

The **original** LBA2 engine source is the **lba2-classic** codebase: it is mostly assembly, with C++ for game logic, and is the canonical historical release. **lba2-classic-community** is a **community fork** for evolving and modernizing the code: ports of assembly to C++, SDL3, libsmacker, and other updates. The goal is to preserve the history and culture of the original while making the codebase easier to build and extend. See [ASM_TO_CPP_REFERENCE.md](docs/ASM_TO_CPP_REFERENCE.md) for which modules have been ported from ASM to C++ in this fork.

## Prerequisites

- **CMake** 3.23 or later
- **UASM** assembler (used for x86 assembly sources)
- **SDL3** (shared library)
- A C/C++ compiler with C++98 support (GCC, Clang)

## Building

```bash
cmake -B build -DSOUND_BACKEND=sdl -DMVIDEO_BACKEND=smacker
cmake --build build
```

### Using CMake presets

This repository provides CMake presets for common configurations in `CMakePresets.json`. You can use them instead of specifying the build directory and toolchain flags manually:

- **Linux (native build)**:

  ```bash
  cmake --preset linux -DSOUND_BACKEND=sdl -DMVIDEO_BACKEND=smacker
  cmake --build --preset linux
  ```

- **macOS (native build)**:

  ```bash
  cmake --preset macos_arm64   # or macOS_x86_64
  cmake --build --preset macos_arm64
  ```

- **Cross-compiling Windows from Linux**:

  ```bash
  cmake --preset cross_linux2win
  cmake --build --preset cross_linux2win
  ```

### Build Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `SOUND_BACKEND` | `null`, `miles`, `sdl` | `null` | Sound backend. Use `sdl` for audio via SDL3. `miles` requires the proprietary Miles Sound System SDK. |
| `MVIDEO_BACKEND` | `null`, `smacker` | `null` | Motion video backend. Use `smacker` for FMV playback via the bundled open-source libsmacker. |

When `MVIDEO_BACKEND` is set to `smacker`, the build links in `libsmacker` and the SDL-based video/audio glue used by the FMV player. See `LIB386/SMACKER/README.md` and `LIB386/AIL/MILES/README.md` for details on the proprietary SDKs and their open-source replacements.

### Cross-compiling for Windows (from Linux)

A MinGW toolchain file is provided for cross-compiling a 32-bit Windows build from Linux:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-i686.cmake
cmake --build build
```

## Project Structure

```
lba2-classic-community/
├── CMakeLists.txt            # Root build configuration
├── cmake/                    # Cross-compilation toolchain files
├── SOURCES/                  # Main game logic
│   ├── 3DEXT/                # 3D extensions (terrain, sky, rain, decor rendering)
│   ├── CONFIG/               # Configuration and key binding
│   └── *.CPP, *.H, *.ASM    # Game modules (AI, physics, inventory, save/load, etc.)
├── LIB386/                   # Engine libraries
│   ├── 3D/                   # 3D math (projection, rotation, matrices)
│   ├── AIL/                  # Audio abstraction layer (SDL, Miles, or null backends)
│   ├── ANIM/                 # Animation system
│   ├── FILEIO/               # File I/O
│   ├── libsmacker/           # Open-source Smacker video decoder (LGPL 2.1)
│   ├── OBJECT/               # 3D object display
│   ├── pol_work/             # Polygon rendering (flat, gouraud, textured)
│   ├── SVGA/                 # Graphics / sprite scaling
│   ├── SYSTEM/               # System abstraction
│   └── H/                    # Shared headers
└── docs/                     # Additional documentation
```

## Preservation Notes

This codebase is a window into 1990s game development at Adeline Software International in Lyon, France. Beyond the technical content, the source files contain original developer artifacts worth exploring. The ASCII art and French comments documented below are from the **original** Adeline / lba2-classic codebase (same files or content preserved when porting ASM to C++ in this fork).

- **ASCII art banners** -- The developers decorated their source files with elaborate text banners in two distinct styles. See [ASCII_ART.md](docs/ASCII_ART.md) for a full catalog.
- **French comments** -- The code is written with French comments throughout, many of which are informal, humorous, or expressive in ways that reflect the personality of the team. See [FRENCH_COMMENTS.md](docs/FRENCH_COMMENTS.md) for a curated selection with English translations.

## Licence

This source code is licensed under the [GNU General Public License](https://github.com/2point21/lba2-classic-community/blob/main/LICENSE).

Please note this license only applies to **Little Big Adventure 2** engine source code. **Little Big Adventure 2** game assets (art, models, textures, audio, etc.) are not open-source and therefore aren't redistributable.

## How can I contribute?

Read our [Contribution Guidelines](https://github.com/2point21/lba2-classic-community/blob/main/CONTRIBUTING.md).

## Links

**Official Website:** https://twinsenslittlebigadventure.com/

**Discord:** https://discord.gg/gfzna5SfZ5

**Docs:** https://lba-classic-doc.readthedocs.io/

## Buy the game

[[GoG]](https://www.gog.com/game/little_big_adventure_2)  [[Steam]](https://store.steampowered.com/app/398000/Little_Big_Adventure_2/)

## Original Dev Team

Direction: Frédérick Raynal

Programmers: Sébastien Viannay / Laurent Salmeron / Cédric Bermond / Frantz Cournil / Marc Bureau du Colombier

3D Artists & Animations: Paul-Henri Michaud / Arnaud Lhomme

Artists: Yaeël Barroz, Sabine Morlat, Didier Quentin

Story & Design: Frédérick Raynal / Didier Chanfray / Yaël Barroz / Laurent Salmeron / Marc Albinet

Dialogs: Marc Albinet

Story coding: Frantz Cournil / Lionel Chaze / Pascal Dubois

Video Sequences: Frédéric Taquet / Benoît Boucher / Ludovic Rubin / Merlin Pardot

Music & Sound FX: Philippe Vachey

Testing: Bruno Marion / Thomas Ferraz / Alexis Madinier / Christopher Horwood / Bertrand Fillardet

Quality Control: Emmanuel Oualid

## Copyright

The intellectual property is currently owned by [2.21]. Copyright [2.21]

Originally developed by Adeline Software International in 1994
