# Code style

Canonical reference for code-style *decisions* in this repository — the rules and the reasoning behind them. It is the single source of truth that [CONTRIBUTING.md](CONTRIBUTING.md) and [AGENTS.md](AGENTS.md) point to, so a rule lives here once instead of drifting across both.

This covers *what* the code should look like and *why*. The *how* of running the formatter — `clang-format` scripts, the pre-commit hook, `.clang-format-ignore`, EditorConfig, VS Code setup — is a development-environment concern and stays in [CONTRIBUTING.md "Code style"](CONTRIBUTING.md#code-style).

Overriding rule when in doubt: **match the style of the file you are editing.**

## Language dialect — C-in-`.cpp`, by zone

The `.CPP`/`.H` extensions and `extern "C"` blocks are a compilation and linkage detail, not a signal that the engine is object-oriented. The whole engine compiles as **C++98** (`CMAKE_CXX_STANDARD 98` in [CMakeLists.txt](CMakeLists.txt)); there is no STL, no templates, and no class hierarchy in shipped code, and that is deliberate.

**Guiding principles.** Whatever the zone, the house dialect favours restraint over machinery:

- Reach for a language feature only when it earns its keep; prefer the simplest construct that does the job.
- Plain structs and value semantics over object hierarchies. No deep inheritance, no RTTI, no exceptions.
- No STL in shipped or per-frame code. If a container is unavoidable, keep it explicit and allocation-aware. [scripts/ci/check-arch.py](scripts/ci/check-arch.py) holds `SOURCES/` and `LIB386/` to this on every push; `tests/` is exempt, as above.
- Optimise for readability and predictable performance, not for abstraction.

Within that, which features to reach for depends on the zone you are in:

- **Ported original code** (`LIB386/`, `SOURCES/3DEXT/`, and game logic in `SOURCES/` that mirrors the 1997 source or the ASM): stays closest to its origin. It must keep mapping recognisably to the ASM the equivalence tests pin and to the original Adeline source — that correspondence is what the `--bisect` workflow and the byte-for-byte tests depend on — so preserve its C-style structure rather than reshaping it into classes or `std::` containers. `extern "C"` stays.
- **New infrastructure with no ASM original** (console, logging, control harness, resolution/discovery helpers, future tooling): the guiding principles above are the whole rule. Conservative C++98 features are welcome where they earn their keep — anonymous namespaces for internal linkage, references, small helper classes / RAII for resource lifetimes. This is the zone where [`SOURCES/RES_PICKER.CPP`](SOURCES/RES_PICKER.CPP)'s `namespace {}` already lives; that is the intended style, not an inconsistency.
- **Tests** (`tests/`): free to use newer C++ (C11/C++11) and `std::` — they verify the engine, they are not shipped with it.

Rule of thumb: if a file has an ASM counterpart or a line-for-line ancestor in the original, it stays C. If you are writing something new that never existed in 1997, idiomatic (but conservative) C++98 is fine.

## Where new code goes

The zone rule above only decides anything if a file belongs to one zone, and the overriding "match the file you are editing" only helps if the file has one style to match. An original file that has accumulated new infrastructure has neither. [SOURCES/PERSO.CPP](SOURCES/PERSO.CPP) is the cautionary case: it is 1997 code that happens to hold `main()`, so camera, console, path, config and boot work landed beside the entry point until roughly a third of the file was code with no 1997 ancestor. Inside a file like that both halves of the dialect rule apply at once and neither wins.

Two rules keep it from recurring.

**A new subsystem gets its own translation unit.** Do not grow an original file with something that never existed in 1997. When the natural call site is inside original code, put the call there and the implementation in a new `.CPP`. The Auto camera is the worked example: [SOURCES/FOLLOWCAM.CPP](SOURCES/FOLLOWCAM.CPP), driven from the main loop through four named entry points, with its tunables in `FOLLOWCAM_CFG.H` and its arithmetic in `FOLLOWCAM_MATH.H`.

**A module owns its own state.** A new module's globals are defined in its own `.CPP` and declared in its own `.H`, never in [SOURCES/C_EXTERN.H](SOURCES/C_EXTERN.H) and [SOURCES/GLOBAL.CPP](SOURCES/GLOBAL.CPP). The rule is mechanical, so it is checkable: *the header declares exactly what the `.CPP` defines*. It also makes callers admit what they use. Moving the Auto camera's settings out of the god-header put `#include "FOLLOWCAM.H"` in the seven files that genuinely read or write camera state, and stopped the other ninety-odd from being able to touch it by accident.

**That second benefit has a precondition, and for most modules it is not met yet.** A caller's include list only says something when the header is not already in front of it. [SOURCES/DEFINES.H](SOURCES/DEFINES.H) aggregates 46 local headers, 42 of which are modules with a matching `.CPP`, and [SOURCES/C_EXTERN.H](SOURCES/C_EXTERN.H) includes DEFINES.H, and 58 files include that. So every one of those module headers is in front of every translation unit in the game. Moving a global from the god-header into such a module's header still satisfies the mechanical half, but nothing has to admit it uses the module and nothing is stopped from reaching it by accident.

The camera escaped this only because FOLLOWCAM.H was a new file DEFINES.H had never heard of. For a module that predates the aggregation, the order is: make the module header self-contained (an aggregated header can name types it never included, because DEFINES.H only ever included it after they had arrived), remove it from DEFINES.H, add the include to the files that use it, and only then move the state. [SOURCES/AMBIANCE.H](SOURCES/AMBIANCE.H) is the worked example: three explicit includers before, 25 after, and about a dozen include lines to get there. Doing it the other way round produces a diff that looks like the rule and buys none of it.

**Both halves are held, so neither can be undone by accident.** [scripts/ci/check-arch.py](scripts/ci/check-arch.py) refuses a rise in C_EXTERN.H's extern count, and refuses a module header returning to DEFINES.H once it has left. The figures live in that script as constants beside the rule, so a move that earns a lower one lowers it in the same diff. Nothing there demands a decrease: a global that several subsystems write has no owner to move it to, and the ratchet says nothing about it.

Corollaries:

- **Group by filename prefix, not by directory.** `RES_*`, `SAVEGAME_*`, `MENU_*`, `FOLLOWCAM_*` are the existing clusters. A new prefix costs nothing and matches what the tree already does. A module earns a subdirectory when it acquires a boundary the flat namespace cannot express, which is not a line count. Two count: a **privacy boundary**, where some of its files have no includer outside the module, and a **link boundary**, where part of it builds on its own. [SOURCES/CONSOLE/](SOURCES/CONSOLE/) has both: `CONSOLE_STATE.H` and `CONSOLE_GIVE.H` are included nowhere outside the directory while `CONSOLE.H` is the public face, and the core builds as its own library with its own compile definitions. Every `FOLLOWCAM_*` file, by contrast, is included from outside, so a directory there would name a boundary that does not exist.
- **Original files keep their paths.** Do not relocate 1997 code to mark it as original. `git log --follow` on those files is the evidence trail the bit-exactness work depends on, and the original/new split is per-line inside them anyway. A file is new because it has no ancestor in the initial import, not because of where it sits.
- **Boot-time work is not exempt.** New infrastructure gravitates to the entry point because `main()` is there. It still does not belong there: config file I/O, path and profile resolution, asset discovery and fatal-error plumbing each want their own TU.
- **State that is genuinely shared stays shared.** The rule is about ownership, not about emptying the god-header for its own sake. A global that several subsystems write (the camera angles themselves, `FirstTime`, the cube coordinates) has no single owner to move it to yet; leave it until one exists. See [docs/ARCHITECTURE_GLOBALS.md](docs/ARCHITECTURE_GLOBALS.md) for the wider plan.

### Cross-cutting files invert the second rule

Some files exist precisely to touch everyone else's state: the cfg reader and writer, the save serialiser, a `--dump-state` report. "Own your state" says nothing useful to them, because owning none of it is the job. Applied naively it produces a file that gives itself the first rule's benefit and none of the second's: one translation unit naming three dozen globals belonging to five different owners.

Invert it there. **The cross-cutting file must not name another module's settings; the module exposes an entry point and the cross-cutting file calls it.** [SOURCES/FOLLOWCAM.CPP](SOURCES/FOLLOWCAM.CPP) has `FollowCam_ReadConfig` / `FollowCam_WriteConfig` for its own cfg keys, and the config file module calls them without knowing a key name. Adding a camera setting is then one file, not two, and a clamp cannot end up in a different translation unit from the arithmetic that requires it.

Two things to expect when doing this:

- **A serialised format's ordering can outrank the rule.** `lba2.cfg` is rewritten in key order, so a module can only take a contiguous run of keys without reordering every player's file. Where a key sits apart from its module's block, leave it and say why, as `FollowCamera` is left in FOLLOWCAM.H. Cosmetic churn in a user's file is a worse outcome than an imperfect boundary.
- **A module's private state usually has one leak.** Both extractions found exactly one: the main loop assigning the camera's smoothing state, and boot assigning the cfg reader's `StoredLanguage`. Give it a named entry point rather than widening the header to expose the variable. The name is the documentation for a coupling that was previously invisible.

### Features and surfaces

The two rules above are the same rule seen from either end, and naming the two kinds of file says when to apply which.

A **feature** is specific: the Auto camera, the save system, resolution switching. A **surface** is generic and serves every feature: the cfg reader and writer, the console, the CLI flag table and the control harness, the options menu. A feature has state and behaviour; a surface has a calling convention and no opinion about who uses it.

That asymmetry fixes the dependency direction. **A surface must not name a feature's variables; it offers a registration or serialisation hook, and the feature fills it in.** Otherwise every new setting costs an edit in each generic file, and those edits are what turned the entry point into a 4000-line file in the first place.

The Auto camera is the worked example because it now meets every surface this way, each through one entry point in [SOURCES/FOLLOWCAM.CPP](SOURCES/FOLLOWCAM.CPP):

| Surface | Entry point |
|---|---|
| lba2.cfg | `FollowCam_ReadConfig` / `FollowCam_WriteConfig` |
| Console cvars | `FollowCam_RegisterCvars` |
| Console commands | `FollowCam_RegisterCommands` |
| Main loop | `UpdateFollowCameraExt`, `FollowCamTrace`, `FollowCamResetToView`, `FollowCamSyncTarget` |

Neither [SOURCES/CONFIG_FILE.CPP](SOURCES/CONFIG_FILE.CPP) nor `CONSOLE/CONSOLE_CMD.CPP` names an Auto-camera variable. Adding a camera setting is one edit, in the module.

Two things worth knowing before applying this:

- **Some surfaces already work this way, and are the better precedent.** [SOURCES/CLI_ARGS.CPP](SOURCES/CLI_ARGS.CPP) names no engine state at all; it is a flag table for `--help` and unknown-flag rejection. [SOURCES/CONTROL.CPP](SOURCES/CONTROL.CPP) keeps every per-run override in its own file-scope state and exposes `Control_HasX` / `Control_GetX`, which is this rule from the other side: the owner offers the accessor, so `CONFIG_FILE.CPP` asks rather than reaches.
- **The payoff is composability, and it is already load-bearing.** Because the console is a surface rather than a set of special cases, the camera can be driven through it without a camera-specific test hook: the fixtures in `tests/automation` orbit the camera with `--exec "camnudge ..."`, which is CLI into control into console into the feature. A feature that meets its surfaces properly becomes testable for free.

The rule is about ownership of storage, not about forbidding reads. `GAMEMENU.CPP` reading `FollowCamera` through FOLLOWCAM.H to choose a menu label is a plain use of a public setting and needs no hook.

**The same split runs inside a feature.** [SOURCES/CONSOLE/](SOURCES/CONSOLE/) shows the mature form: its core builds as a library that touches no game state, while `CONSOLE_CMD.CPP`, which binds console verbs to game globals, is deliberately left out of that library and compiled with the game. Generic mechanism on one side of a link boundary, game-specific bindings on the other. The cheat codes are the worked example of a verb the core cannot know: the library declares `Console_SetVerbClaim` and the binding file fills it in, so the name resolution runs the game's way round. [scripts/ci/check-arch.py](scripts/ci/check-arch.py) holds the library's files to including nothing outside the module, reading the file list from the CMake target rather than a list of its own. Note that a static archive links with undefined symbols, so this boundary has to be checked; it does not announce itself as a build failure.

That split is worth reaching for because it is the same line as the testable one. Code that runs free of engine globals can be linked into a host test and run in CI on every platform; code that reads `ListObjet` and writes `BetaCam` can only be exercised by a fixture that boots the engine and needs retail data. [SOURCES/FOLLOWCAM_MATH.H](SOURCES/FOLLOWCAM_MATH.H) is the camera sitting on that line already: it is the angle arithmetic with no engine state, it has a host test, and it is the only part of the camera CI can see. When deciding what to pull out of a feature next, pull along that line first.

## Layout and naming

- **Indentation:** 4 spaces in C/C++. Tabs are preserved in ASM. The original used tabs; there is an ongoing migration to 4 spaces, so new contributions use 4 spaces.
- **Types:** use the fixed-width aliases `S32`/`U32`/`S16`/`U16`/`U8` from [`LIB386/H/SYSTEM/ADELINE_TYPES.H`](LIB386/H/SYSTEM/ADELINE_TYPES.H), not bare `int`/`unsigned`, in ported and engine code.
- **Standard:** C++98 for engine and game code; tests may use C11/C++11.

## Preservation of original code

This project is a preservation effort. When modifying original source files:

- Do not remove original French comments — they are part of the codebase's history. See [docs/FRENCH_COMMENTS.md](docs/FRENCH_COMMENTS.md).
- Do not remove or alter ASCII art banners in source files. See [docs/ASCII_ART.md](docs/ASCII_ART.md).
- Add clarifying comments alongside the originals rather than replacing them.
- When documenting history or culture, attribute to the original codebase (lba2-classic) and note when content was preserved during an ASM→C++ port in this fork.
