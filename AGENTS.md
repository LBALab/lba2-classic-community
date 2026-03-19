# Agent Guidance — LBA2 Classic Community

This document helps AI coding assistants (Cursor, Copilot, Claude, etc.) work effectively on this codebase. Read it before making changes.

## Principles

1. **Truth hierarchy:** Code > project docs > external sources. Prefer `docs/GLOSSARY.md`, `docs/LIFECYCLES.md`, etc. over inference. When code and docs conflict, code wins—then update the docs.

2. **Document accuracy:** When you change behavior, update the relevant docs. Keep `ASM_VALIDATION_PROGRESS.md` and `ASM_TO_CPP_REFERENCE.md` current.

3. **Preserve the nature of the game:** Do not change gameplay, feel, or behavior by default. Fix bugs that alter behavior; do not "improve" behavior without explicit agreement.

4. **Optional features behind flags:** New behavior or features that change the experience must be opt-in (build options, config flags, runtime toggles). Default builds should match original behavior.

5. **Quality over speed:** Equivalence tests must pass. Do not relax tests to unblock. Prefer smaller, well-tested changes. Fix bugs before merging.

6. **Cross-platform priority:** Support Linux, macOS, and Windows. Avoid platform-specific assumptions. Prefer portable C/C++. No inline x86 in library code.

## Never

- Never relax equivalence tests or add approximate comparisons
- Never remove French comments or ASCII art
- Never add `__asm__` / inline x86 in LIB386 C++ files
- Never change gameplay, timing, or feel without explicit approval
- Never assume platform; never add unapproved dependencies
- Never "modernize" C++98 to C++11+ in game code (tests may use newer)

## Golden Rules

- **ASM is the source of truth.** All equivalence tests compare ASM vs C++ byte-for-byte. Use `ASSERT_ASM_CPP_EQ_INT` for scalars, `ASSERT_ASM_CPP_MEM_EQ` for buffers.
- **Preservation:** French comments, ASCII art, attribution—these are historical artifacts. Add clarifying comments alongside originals; do not replace them.
- **Documentation first:** Use `docs/GLOSSARY.md` for domain terms, `docs/LIFECYCLES.md` for lifecycles, `ASM_TO_CPP_REFERENCE.md` for port status, `ASM_VALIDATION_PROGRESS.md` for test coverage.
- **Think like an archaeologist:** This codebase is a window into 1990s game dev at Adeline Software. Surface historical context when relevant. See `docs/FRENCH_COMMENTS.md` and `docs/ASCII_ART.md`.

## Project Overview

- **What:** Community fork of the LBA2 (Twinsen's Odyssey) engine for preservation and modernization.
- **Main work:** Porting x86 assembly to C++ while preserving exact behavior. ASM remains the source of truth.
- **Stack:** C++98 game logic, LIB386 engine libraries (C linkage), x86 ASM (UASM), SDL3, libsmacker.
- **Structure:** `SOURCES/` = game logic; `LIB386/` = engine (3D, audio, rendering); `docs/` = reference documentation.

## Build & Test

- **Build:** `cmake -B build && cmake --build build` (or presets: `linux`, `macos_arm64`, `windows_ucrt64`)
- **Tests:** Run via Docker only: `./run_tests_docker.sh`. Tests require 32-bit x86 + UASM.
- **Filter:** `./run_tests_docker.sh test_getang2d test_lirot3df`
- **Bisect:** `./run_tests_docker.sh --bisect` to find first divergent draw call
- **Before considering done:** Run `./run_tests_docker.sh`. If modifying formatted files, run clang-format check.

## When Modifying X, Do Y

| Modifying | Do | See |
|-----------|-----|-----|
| LIB386 C++ port | Check ASM_VALIDATION_PROGRESS Notes; add/update equivalence test | ASM_VALIDATION_PROGRESS.md |
| SOURCES/3DEXT/ | Same; check ASM_TO_CPP_REFERENCE | docs/ASM_TO_CPP_REFERENCE.md |
| Adding ASM↔CPP test | Use `add_asm_cpp_test()`, include stress test, update ASM_VALIDATION_PROGRESS | docs/TESTING.md, .github/copilot-instructions.md |
| Audio/video | AIL in LIB386/AIL/; backends SDL, Miles, null | docs/AUDIO.md |
| Debug tools | DEBUG_TOOLS, CONSOLE_MODULE | docs/DEBUG.md, docs/CONSOLE.md |
| File with French comments or ASCII art | Preserve; add new comments alongside | docs/FRENCH_COMMENTS.md, docs/ASCII_ART.md |

## Code Conventions

- Indentation: 4 spaces (C/C++); tabs preserved in ASM
- Formatting: clang-format with checked-in `.clang-format`; ASM and `LIB386/libsmacker/` excluded
- Types: S32, U32 from `LIB386/H/SYSTEM/ADELINE_TYPES.H`
- C++98 for game code; tests may use C11/C++11

## Further Reading

- [CONTRIBUTING.md](CONTRIBUTING.md) — Code style, preservation, PR workflow
- [.github/copilot-instructions.md](.github/copilot-instructions.md) — Detailed ASM↔CPP workflow, polyrec debugging, test patterns, common pitfalls
- [docs/README.md](docs/README.md) — Full documentation index
