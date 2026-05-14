# Changelog

All notable changes to the LBA2 Classic Community engine fork.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> Engine semver is independent from `NUM_VERSION` (save-file on-disk format,
> see `SOURCES/COMMON.H`). Save-format changes are tracked there, not here.

## [Unreleased]

### Debug tooling

- Console `give` command now actually grants inventory items instead of
  only playing the found-object cinematic. It takes an item name
  (`give conch`), with the numeric index still accepted as a fallback;
  run `give` with no arguments to list every item name. Countable items
  accept an optional count (`give gem 5`, `give money 5000`,
  `give clover 3`). See [docs/CONSOLE.md](docs/CONSOLE.md).

### Cross-platform & build

- macOS native release artifact: a portable DMG with the `.app` bundle
  inside, drag-to-Applications layout, native multi-resolution `.icns`
  icon, and an `Info.plist` populated from the same `LBA2_*` cache vars
  that drive the runtime window title, `.desktop` entry, AppImage labels,
  and Windows resource. Built for arm64 (Apple Silicon) on tag push;
  static-linked SDL3, no `.dylib` dependencies, ad-hoc codesigned to
  satisfy Apple Silicon's mandatory bundle integrity check. First launch
  still needs the right-click → Open Gatekeeper bypass (Developer ID +
  notarization deferred); documented in the README inside the DMG.
  Intel-Mac users can produce a local DMG via
  `scripts/dev/build-macos-release.sh --preset macos_x86_64` while
  hosted Intel runners are unreliable.
- Rolling `latest` pre-release: every push to `main` triggers a CI build
  of all three platforms (Linux AppImages, Windows ZIP, macOS DMG) and
  force-updates a `latest`-tagged GitHub Release. Marked pre-release; the
  most recent stable tag keeps GitHub's "Latest" promotion. Lets community
  testers grab bleeding-edge main builds without cloning + building.
  See `docs/RELEASING.md` "Rolling latest pre-release".

## [0.9.0] - 2026-05-09

First tagged release. Cross-platform builds (Linux AppImage, Windows ZIP)
published at
[v0.9.0](https://github.com/LBALab/lba2-classic-community/releases/tag/v0.9.0).

The list below is hand-curated from merged PRs and tells the story of the
fork's evolution.

> **Highlights since the fork**
>
> The biggest architectural shift is portability. The playable game now
> builds as pure C/C++ against SDL3 and `libsmacker`, with no ASM
> toolchain on the build path. The original 16/32-bit x86 assembly is
> still in the tree as the source of truth and powers the ASM↔C++
> equivalence tests, but it no longer runs in the playable binary.
> Native builds for Linux x86_64, macOS arm64/x86_64, and Windows
> (MSYS2 UCRT64). Community members are already running it on handheld
> devices in the wild.

### Stability & 64-bit hardening

- Magic-school grand-wizard crash on 64-bit hosts — fixed
  ([#78](https://github.com/LBALab/lba2-classic-community/issues/78),
  [#84](https://github.com/LBALab/lba2-classic-community/pull/84)).
  U32+pointer-wraparound trap in `CopyMask`, pinned by a host-only
  regression test.
- Renderer-side U32+pointer-wrap class named, documented, and swept
  across ~80 files in SVGA, pol_work, 3D, 3DEXT, GRILLE, INTEXT — one
  bug fixed (above), six "safe by convention" latent traps recorded
  ([#85](https://github.com/LBALab/lba2-classic-community/pull/85),
  [#86](https://github.com/LBALab/lba2-classic-community/pull/86),
  [#87](https://github.com/LBALab/lba2-classic-community/pull/87),
  [#88](https://github.com/LBALab/lba2-classic-community/pull/88),
  [#89](https://github.com/LBALab/lba2-classic-community/pull/89),
  [#90](https://github.com/LBALab/lba2-classic-community/pull/90))
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

- First Linux release artifact: AppImage built in CI from an any-linux
  base, packaged as a single portable binary
  ([#74](https://github.com/LBALab/lba2-classic-community/pull/74))
- First Windows release artifact: a portable ZIP (`lba2cc-<version>-windows-x64.zip`)
  built in CI under MSYS2 UCRT64 with SDL3 and the GCC C++ runtime
  static-linked. Drop the single `lba2cc.exe` into your existing LBA2
  install folder — no DLLs, no installer, no registry entries. Follows
  the same release-workflow conventions as the AppImage flow (matrix
  build → idempotent gh-release attach), see `docs/RELEASING.md`.
- Product metadata (executable name, display name, description, desktop
  ID) centralized into CMake cache variables so the runtime window
  title, `.desktop` entry, and AppImage script all consume one source
  ([#83](https://github.com/LBALab/lba2-classic-community/issues/83)).
  Window title now shows the version (e.g. `LBA2 Classic Community
  0.9.0-dev`) — previously just `LBA2`.
- Default binary renamed `lba2` → `lba2cc` to avoid colliding with the
  retail executables (`lba2.exe`, `tlba2.exe`, `tlba2c.exe`). Path is
  now `build/SOURCES/lba2cc[.exe]`. Override-able via
  `-DLBA2_EXECUTABLE_NAME=...` for anyone shipping a custom branded build.
- Windows `.exe` now ships with an embedded application icon and
  populated File Properties → Details (Product Name, File Description,
  Product Version, Original Filename). Strings flow from the same
  `LBA2_*` cache vars that feed the runtime window title, `.desktop`
  entry, and AppImage labels. Pre-release versions (anything carrying
  `-dev`/`-dirty`/`-rc` suffix) are flagged with `VS_FF_PRERELEASE` in
  `VERSIONINFO`.
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
- [docs/PLATFORM.md](docs/PLATFORM.md) — host-assumptions map
  (pointer width, endianness, FPU semantics, OS boundaries),
  [docs/PLATFORM_AUDITS.md](docs/PLATFORM_AUDITS.md) — per-file audit
  logs hung off it
  ([#85](https://github.com/LBALab/lba2-classic-community/pull/85),
  [#88](https://github.com/LBALab/lba2-classic-community/pull/88))
- [docs/ABI.md](docs/ABI.md) — disk-layout patterns for fat structs
  on 64-bit ([#67](https://github.com/LBALab/lba2-classic-community/pull/67))
- [docs/CRASH_INVESTIGATION.md](docs/CRASH_INVESTIGATION.md) —
  AddressSanitizer + gdb runbook
  ([#85](https://github.com/LBALab/lba2-classic-community/pull/85))
- [docs/COMPILER_NOTES.md](docs/COMPILER_NOTES.md),
  [docs/TESTING.md](docs/TESTING.md)
- [docs/FRENCH_COMMENTS.md](docs/FRENCH_COMMENTS.md) and
  [docs/ASCII_ART.md](docs/ASCII_ART.md) — preservation catalogs

### Pre-PR foundations

Most of what made this fork buildable and cross-platform predates the
PR workflow. The themes from that direct-commit era — CMake build
system and MinGW cross-compile to Windows ([@leokolln](https://github.com/leokolln)),
macOS M1 build, the bulk of the LIB386 ASM→C++ port, and the ASM↔CPP
equivalence test infrastructure ([@sergiou87](https://github.com/sergiou87)),
early renderer/SVGA/object-display ASM→CPP ports ([@xesf](https://github.com/xesf)),
all building on [@yaz0r](https://github.com/yaz0r)'s original buildable
prototype — are credited under Contributors below.

The earliest PR-tracked entries that sit alongside that work:

- File encoding preservation
  ([#1](https://github.com/LBALab/lba2-classic-community/pull/1))
- Preservation docs and build configuration clarity
  ([#13](https://github.com/LBALab/lba2-classic-community/pull/13))

### Contributors

Thanks to everyone who's worked on this fork to date:
[@gwen-gg](https://github.com/gwen-gg),
[@innerbytes](https://github.com/innerbytes),
[@leokolln](https://github.com/leokolln),
[@Link4Electronics](https://github.com/Link4Electronics),
[@noctonca](https://github.com/noctonca),
[@sergiou87](https://github.com/sergiou87),
[@xesf](https://github.com/xesf),
[@yaz0r](https://github.com/yaz0r).


Special thanks to the contributors whose foundational work this fork stands on:

- [@sergiou87](https://github.com/sergiou87) — macOS M1 build, the bulk of the LIB386 ASM→C++ port, and the ASM↔CPP equivalence test infrastructure that makes the cross-platform port verifiable.
- [@leokolln](https://github.com/leokolln) — the cross-platform build foundation: CMake build system and presets, MinGW cross-compile to Windows, UASM ASM modernisation, 64-bit Linux preset, AIL sound backend structure, and Linux CI.
- [@xesf](https://github.com/xesf) — implementation work across SVGA, 3D object rendering, brick handling, and `AffString`, plus the CopyMask Tavern crash fix and widescreen Steam Deck work.
- [@yaz0r](https://github.com/yaz0r) — the original buildable prototype that the cross-platform fork was built on.

A big thank you to Gwen Gourevich ([@gwen-gg](https://github.com/gwen-gg)) and [2.21](https://discord.gg/e2ZXpzrM) for open-sourcing the original code, and to the original Adeline
Software team whose code this builds on.

[Unreleased]: https://github.com/LBALab/lba2-classic-community/compare/v0.9.0...HEAD
[0.9.0]: https://github.com/LBALab/lba2-classic-community/compare/1f3e871...v0.9.0
