# Code style

Canonical reference for code-style *decisions* in this repository — the rules and the reasoning behind them. It is the single source of truth that [CONTRIBUTING.md](CONTRIBUTING.md) and [AGENTS.md](AGENTS.md) point to, so a rule lives here once instead of drifting across both.

This covers *what* the code should look like and *why*. The *how* of running the formatter — `clang-format` scripts, the pre-commit hook, `.clang-format-ignore`, EditorConfig, VS Code setup — is a development-environment concern and stays in [CONTRIBUTING.md "Code style"](CONTRIBUTING.md#code-style).

Overriding rule when in doubt: **match the style of the file you are editing.**

## Language dialect — C-in-`.cpp`, by zone

The `.CPP`/`.H` extensions and `extern "C"` blocks are a compilation and linkage detail, not a signal that the engine is object-oriented. The whole engine compiles as **C++98** (`CMAKE_CXX_STANDARD 98` in [CMakeLists.txt](CMakeLists.txt)); there is no STL, no templates, and no class hierarchy in shipped code, and that is deliberate.

**Guiding principles.** Whatever the zone, the house dialect favours restraint over machinery:

- Reach for a language feature only when it earns its keep; prefer the simplest construct that does the job.
- Plain structs and value semantics over object hierarchies. No deep inheritance, no RTTI, no exceptions.
- No STL in shipped or per-frame code. If a container is unavoidable, keep it explicit and allocation-aware.
- Optimise for readability and predictable performance, not for abstraction.

Within that, which features to reach for depends on the zone you are in:

- **Ported original code** (`LIB386/`, `SOURCES/3DEXT/`, and game logic in `SOURCES/` that mirrors the 1997 source or the ASM): stays closest to its origin. It must keep mapping recognisably to the ASM the equivalence tests pin and to the original Adeline source — that correspondence is what the `--bisect` workflow and the byte-for-byte tests depend on — so preserve its C-style structure rather than reshaping it into classes or `std::` containers. `extern "C"` stays.
- **New infrastructure with no ASM original** (console, logging, control harness, resolution/discovery helpers, future tooling): the guiding principles above are the whole rule. Conservative C++98 features are welcome where they earn their keep — anonymous namespaces for internal linkage, references, small helper classes / RAII for resource lifetimes. This is the zone where [`SOURCES/RES_PICKER.CPP`](SOURCES/RES_PICKER.CPP)'s `namespace {}` already lives; that is the intended style, not an inconsistency.
- **Tests** (`tests/`): free to use newer C++ (C11/C++11) and `std::` — they verify the engine, they are not shipped with it.

Rule of thumb: if a file has an ASM counterpart or a line-for-line ancestor in the original, it stays C. If you are writing something new that never existed in 1997, idiomatic (but conservative) C++98 is fine.

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
