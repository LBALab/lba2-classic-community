# Changelog

All notable changes to the LBA2 Classic Community engine fork.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> Engine semver is independent from `NUM_VERSION` (save-file on-disk format,
> see `SOURCES/COMMON.H`). Save-format changes are tracked there, not here.

## [Unreleased]

_Changes accumulated since the fork was created. The first tagged release
will move this section to a dated heading. See
[docs/RELEASING.md](docs/RELEASING.md) for the cut recipe._

The list below is hand-curated from merged PRs and tells the story of the
fork's evolution.

> **Highlights since the fork**
>
> The biggest architectural shift is portability. The playable game now
> builds **without an ASM toolchain** — pure C/C++ on the build path,
> linked against SDL3 and `libsmacker`. The original 16/32-bit x86
> assembly is still in the tree (it remains the source of truth and is
> required to run the ASM↔C++ equivalence tests), but it is **no longer
> in the build path of the playable binary**. Combined with the SDL3
> port, this unlocked native builds for **Linux x86_64**, **macOS
> arm64/x86_64**, and **Windows (MSYS2 UCRT64)**, and keeps the door
> open to other SDL platforms. Community members have already started
> building it on handheld devices in the wild.

### Stability & 64-bit hardening

- Endgame credits segfault on 64-bit hosts — fixed
  ([#65](https://github.com/LBALab/lba2-classic-community/issues/65),
  [#66](https://github.com/LBALab/lba2-classic-community/pull/66))
- Legacy 32-bit `.lba` saves load safely on 64-bit builds, with a corpus
  harness to prevent regressions
  ([#62](https://github.com/LBALab/lba2-classic-community/issues/62),
  [#63](https://github.com/LBALab/lba2-classic-community/pull/63))
- 32-bit on-disk ABI rule and credits HQR layout codified in docs
  ([#67](https://github.com/LBALab/lba2-classic-community/pull/67))
- Linux exterior draw-order bug — `SinTab`/`CosTab` arrays merged into a
  single contiguous block to match the original ASM layout
  ([#55](https://github.com/LBALab/lba2-classic-community/pull/55))
- Compiler warnings cleaned up across GCC, Clang, and MinGW
  ([#52](https://github.com/LBALab/lba2-classic-community/pull/52))
- Bezier C++ memory corruption — fixed
  ([#8](https://github.com/LBALab/lba2-classic-community/pull/8))

### New features (opt-in)

All new features default-off or default-to-original-behavior, in line with
[AGENTS.md Principle 3](AGENTS.md#principles).

- Quake-style debug console: built-in, always available, configurable
  toggle key, host-tested
  ([#23](https://github.com/LBALab/lba2-classic-community/issues/23),
  [#24](https://github.com/LBALab/lba2-classic-community/pull/24),
  [#60](https://github.com/LBALab/lba2-classic-community/pull/60)) —
  see [docs/CONSOLE.md](docs/CONSOLE.md)
- SDL3 gamepad support
  ([#38](https://github.com/LBALab/lba2-classic-community/issues/38),
  [#58](https://github.com/LBALab/lba2-classic-community/pull/58))
- Optional exterior auto-camera centering (`FollowCamera`)
  ([#37](https://github.com/LBALab/lba2-classic-community/issues/37),
  [#50](https://github.com/LBALab/lba2-classic-community/pull/50)) —
  see [docs/CAMERA.md](docs/CAMERA.md)
- Menu mouse QoL: hover, selection, marquee for long text, keyboard
  takes priority over mouse hover
  ([#41](https://github.com/LBALab/lba2-classic-community/pull/41),
  [#53](https://github.com/LBALab/lba2-classic-community/pull/53),
  [#56](https://github.com/LBALab/lba2-classic-community/pull/56))
- Original Adeline debug tools re-enabled behind `DEBUG_TOOLS=ON`
  ([#17](https://github.com/LBALab/lba2-classic-community/issues/17),
  [#22](https://github.com/LBALab/lba2-classic-community/pull/22)) —
  see [docs/DEBUG.md](docs/DEBUG.md)

### Game data, config, and UX

- Auto-discovery of retail game data (`./data`, `../LBA2`,
  `LBA2_GAME_DIR`, `--game-dir`); embedded default config; clearer
  startup errors
  ([#54](https://github.com/LBALab/lba2-classic-community/pull/54)) —
  see [docs/GAME_DATA.md](docs/GAME_DATA.md)
- Menus reorganized to match LBA2 Original from GOG
  ([#42](https://github.com/LBALab/lba2-classic-community/pull/42))
- Menu language selection
  ([#42](https://github.com/LBALab/lba2-classic-community/pull/42))
- Menu music and jingle handling — fixed
  ([#43](https://github.com/LBALab/lba2-classic-community/issues/43))
- Case-sensitive music paths on Linux — fixed
  ([#12](https://github.com/LBALab/lba2-classic-community/pull/12))

### Audio/video backends
- SDL audio backend with Miles parity gap tracking
  ([#27](https://github.com/LBALab/lba2-classic-community/issues/27),
  [#28](https://github.com/LBALab/lba2-classic-community/pull/28),
  [#9](https://github.com/LBALab/lba2-classic-community/pull/9))
- SDL renderer presentation path with LTO and inline rotates
  ([#11](https://github.com/LBALab/lba2-classic-community/pull/11),
  [#51](https://github.com/LBALab/lba2-classic-community/pull/51))
- Early SDL bring-up for renderer/audio in the fork's first
  cross-platform phase, plus follow-up SDL3 AIL-linking fixes
  ([#6](https://github.com/LBALab/lba2-classic-community/pull/6),
  [#9](https://github.com/LBALab/lba2-classic-community/pull/9),
  [#11](https://github.com/LBALab/lba2-classic-community/pull/11),
  [e9ba7d8](https://github.com/LBALab/lba2-classic-community/commit/e9ba7d8))
- Smacker FMV via libsmacker; video audio routed through the AIL backend
  ([#10](https://github.com/LBALab/lba2-classic-community/pull/10),
  [#29](https://github.com/LBALab/lba2-classic-community/issues/29),
  [#35](https://github.com/LBALab/lba2-classic-community/pull/35))

### Ported (ASM → C++)

- Object display, rendering helpers, and supporting math via the
  ASM-validation framework, with byte-for-byte equivalence tests
  ([#31](https://github.com/LBALab/lba2-classic-community/pull/31),
  [#32](https://github.com/LBALab/lba2-classic-community/pull/32))
- Early renderer/SVGA/object-display ASM→CPP ports (polygon fillers,
  blitters/sprites, and object rendering), which established the base for
  later validation-driven porting work
  ([#6](https://github.com/LBALab/lba2-classic-community/pull/6))
- See [docs/ASM_TO_CPP_REFERENCE.md](docs/ASM_TO_CPP_REFERENCE.md) for the
  full port-status table.

### Cross-platform & build

- macOS CI build (arm64); macOS x86_64 preset
  ([#52](https://github.com/LBALab/lba2-classic-community/pull/52))
- Portable Windows build via MSYS2 UCRT64
  ([#14](https://github.com/LBALab/lba2-classic-community/pull/14)) —
  see [docs/WINDOWS.md](docs/WINDOWS.md)
- CMake presets for Linux, macOS arm64/x86_64, Windows UCRT64, and
  cross-compile Linux→Windows
  ([#21](https://github.com/LBALab/lba2-classic-community/pull/21))
- SDL3 throughout (audio, video, renderer)
- Default Windows preset switched to Release
- `clang-format` with checked-in config; ASM and `LIB386/libsmacker/`
  excluded
  ([#36](https://github.com/LBALab/lba2-classic-community/pull/36))
- Host-only discovery tests run on Linux/macOS/Windows in CI without
  Docker or retail files
  ([#54](https://github.com/LBALab/lba2-classic-community/pull/54))

### Tests

- ASM↔CPP equivalence test framework with byte-for-byte assertions and
  randomized stress rounds
  ([#31](https://github.com/LBALab/lba2-classic-community/pull/31)) —
  see [docs/TESTING.md](docs/TESTING.md)
- Polyrec replay with `--bisect` for finding the first divergent draw
  call between ASM and CPP renderers
- Tightened assertions across `POLYFLAT`, `POLYGOUR`, `POLYDISC`,
  `POLYTEXT`, `POLYGTEX`, `AFF_OBJ`, FPU precision, and the resampler
  ([#44](https://github.com/LBALab/lba2-classic-community/pull/44))

### Documentation

- [AGENTS.md](AGENTS.md) — principles, "never" list, "when modifying X
  do Y" table for AI assistants and human contributors
  ([#40](https://github.com/LBALab/lba2-classic-community/pull/40))
- [docs/GLOSSARY.md](docs/GLOSSARY.md),
  [docs/LIFECYCLES.md](docs/LIFECYCLES.md),
  [docs/SCENES.md](docs/SCENES.md) — engine reference
  ([#25](https://github.com/LBALab/lba2-classic-community/pull/25),
  [#26](https://github.com/LBALab/lba2-classic-community/issues/26))
- [docs/MENU.md](docs/MENU.md), [docs/CONFIG.md](docs/CONFIG.md),
  [docs/SAVEGAME.md](docs/SAVEGAME.md) — subsystem references
  ([#30](https://github.com/LBALab/lba2-classic-community/pull/30))
- [docs/CONSOLE.md](docs/CONSOLE.md), [docs/DEBUG.md](docs/DEBUG.md),
  [docs/CAMERA.md](docs/CAMERA.md),
  [docs/FEATURE_WORKFLOW.md](docs/FEATURE_WORKFLOW.md)
- [docs/FRENCH_COMMENTS.md](docs/FRENCH_COMMENTS.md) and
  [docs/ASCII_ART.md](docs/ASCII_ART.md) — preservation catalogs

### Earliest fork work

- File encoding preservation
  ([#1](https://github.com/LBALab/lba2-classic-community/pull/1))
- SDL audio, Smacker FMV, SDL renderer first land
  ([#9](https://github.com/LBALab/lba2-classic-community/pull/9),
  [#10](https://github.com/LBALab/lba2-classic-community/pull/10),
  [#11](https://github.com/LBALab/lba2-classic-community/pull/11))
- Preservation docs and build configuration clarity
  ([#13](https://github.com/LBALab/lba2-classic-community/pull/13))

### Contributors

Thanks to everyone who's worked on this fork to date:
[@innerbytes](https://github.com/innerbytes),
[@noctonca](https://github.com/noctonca),
[@sergiou87](https://github.com/sergiou87),
[@xesf](https://github.com/xesf).


Special thanks to [@xesf](https://github.com/xesf), one of the owners of LBALab, for foundational early ASM→C++ renderer/SVGA/object-display porting work that helped establish the modern cross-platform baseline of this fork.

A big thank you to [2.21](https://discord.gg/e2ZXpzrM) for open-sourcing the original code and the original Adeline
Software team whose code this builds on.

[Unreleased]: https://github.com/LBALab/lba2-classic-community/compare/main...HEAD
