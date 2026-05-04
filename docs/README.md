# Documentation

Index of documentation in this repository.

**New to the fork?** Start with the root [README](../README.md) (**First clone** + prerequisites), then [GAME_DATA.md](GAME_DATA.md) for retail files and discovery (you are not required to use a fixed layout — see overrides there).

## Engine reference

| Doc | Description |
|-----|-------------|
| [GLOSSARY.md](GLOSSARY.md) | Domain terms (Cube, Zone, T_OBJET, scripts, hero, collision, enums) with code locations. |
| [LIFECYCLES.md](LIFECYCLES.md) | Main loop order, scene load, object/hero/animation lifecycles and where they live in code. |
| [SCENES.md](SCENES.md) | All 223 cubes by island with location names; interior/exterior; object and zone counts. |
| [MENU.md](MENU.md) | Game menu flow, layout, localization, submenus, and entry points. |
| [CONFIG.md](CONFIG.md) | lba2.cfg lifecycle, keys, and what each does (original vs community). |
| [SAVEGAME.md](SAVEGAME.md) | .lba save format: lifecycle, binary layout, version compatibility, save editors, LBALab tools. |
| [CAMERA.md](CAMERA.md) | Camera system: interior (iso) vs exterior (perspective), CameraCenter, Auto camera (`FollowCamera`, community addition). |

## Build & debug

| Doc | Description |
|-----|-------------|
| [WINDOWS.md](WINDOWS.md) | Building on Windows with MSYS2; game files, toolchain. |
| [GAME_DATA.md](GAME_DATA.md) | Retail game files: `LBA2_GAME_DIR`, `--game-dir`, discovery order, dev layouts. |
| [DEBUG.md](DEBUG.md) | Original Adeline debug tools (DEBUG_TOOLS=ON): overlay, F9 screenshot, bug save/load, cheats, scene selection. |
| [CONSOLE.md](CONSOLE.md) | Quake-style debug console (CONSOLE_MODULE=ON): backtick/F12, commands and cvars. |


## Testing

| Doc | Description |
|-----|-------------|
| [TESTING.md](TESTING.md) | Test suite architecture, Docker ASM equivalence, host discovery tests, and **CI workflow** summary. |
| [SAVEGAME_ISSUE62_CHECKLIST.md](SAVEGAME_ISSUE62_CHECKLIST.md) | Tracked checklist for [#62](https://github.com/LBALab/lba2-classic-community/issues/62): save load safety, legacy layout work, tests—separate from [SAVEGAME.md](SAVEGAME.md) until merged. |

## Preservation & culture

| Doc | Description |
|-----|-------------|
| [FRENCH_COMMENTS.md](FRENCH_COMMENTS.md) | Curated French comments from the codebase with English translations. |
| [ASCII_ART.md](ASCII_ART.md) | Catalog of ASCII art banners in the original source files. |

## Porting & technical

| Doc | Description |
|-----|-------------|
| [FEATURE_WORKFLOW.md](FEATURE_WORKFLOW.md) | Reasoning and docs for big features: console commands, headless mode, menu changes, camera. |
| [AUDIO.md](AUDIO.md) | Audio system: AIL contract, SDL backend, sound scripting patterns, known issues. |
| [ASM_TO_CPP_REFERENCE.md](ASM_TO_CPP_REFERENCE.md) | Which modules are ported from ASM to C++ in this fork. |
| [COMPILER_NOTES.md](COMPILER_NOTES.md) | Calling conventions and compiler-related notes. |
| [GFX_OPTIONS.md](GFX_OPTIONS.md) | Variables and locations for graphical quality options. |

## External resources

| Resource | Description |
|----------|-------------|
| [LBA Classic Doc](https://lba-classic-doc.readthedocs.io/) | Read the Docs: engine documentation. |
| [README Links](../README.md#links) | Official site, Discord, GOG/Steam. |
