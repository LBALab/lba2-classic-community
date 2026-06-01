# Logging unification

**Status:** In progress. U1 (relocate core to `LIB386`) and U2 (shim
`LogPrintf`/`LogPuts` onto the fan-out) merged; U3 (severity pass) and an
SDL-spine test are in review. Builds on the structured boot log
(#269/#270/#272 — see [BOOT_LOG_PLAN.md](BOOT_LOG_PLAN.md)). This unifies the
legacy `LogPrintf`/`LogPuts` path into that same fan-out so there is one logging
system, not two coexisting ones.

## Goal

One logging path. Today three coexist:

- **`Log_*`** (~49 sites) and **`SDL_Log`** (44, the audio stack) already fan out
  to the file / terminal / console sinks (the `SDL_SetLogOutputFunction` install
  in `Log_Init` routes `SDL_Log` too).
- **`LogPrintf` / `LogPuts`** (**109 sites**) *bypass* the fan-out: they write
  straight to `adeline.log` (own `fopen` per call) **and** raw `stdout` (unless
  `QuietLog`), with a manual CRLF pass. No severity, no TTY gating, no console
  overlay, and they double-write the file the file sink already owns (which is
  also why `adeline.log` has mixed line endings — `\r\n` from `LogPrintf`, `\n`
  from the sink).

Routing the 109 legacy sites through the one fan-out gives every line the same
sinks, per-sink severity filtering, TTY-gated colour, and the F12 console — and
removes the double-write and the line-ending split.

## Why this is worth doing

This is **consolidation + correctness**, not a new feature.

1. **It's the follow-through that justifies the boot-log core.** #269 added the
   `Log_*` fan-out, but only ~49 sites use it while 109 still bypass it. That
   half-adopted state — *two* logging systems — is the worst outcome. Having
   built the core, unifying is the coherent completion; skipping it leaves a
   permanent "use `Log_*` for new code, `LogPrintf` is legacy" fork.
2. **It fixes concrete defects, not just aesthetics:**
   - a **latent buffer overflow** in `LogPrintf`'s CRLF expansion (`LogStr[256]`
     grown in place by `memmove`, no bounds check);
   - **stdout pollution** — `LogPrintf` writes stdout unconditionally, colliding
     with stdout-as-data modes (e.g. the LZ self-test); the fan-out keeps logs
     off stdout;
   - mixed line endings in `adeline.log`; two dead globals (`Log2File`,
     `QuietLog`).
3. **Extends the boot-log benefits to all diagnostics.** Severity filtering,
   TTY-gated colour, and the F12 console reach ~49 sites today. Unifying brings
   the 109 legacy lines (the `RES_PICKER`/`DISCOVERY`/`WINDOW` data-setup
   diagnostics users actually hit) onto the same surfaces — valuable especially
   on **terminal-less ports (Android, #261)**, where a clean, severity-tagged
   `adeline.log` is the main debugging channel.
4. **One way to log** — lower contributor friction (a stated project value):
   new code uses one idiom, not "which logger?".

**Scoping:** U1 + U2 carry the value (the consolidation + the fixes
above). **U3 (the severity pass) is polish** on mostly-rarely-fired lines — do it
opportunistically; fine to defer or skip. If appetite is low, U1 + U2 alone is a
complete, defensible win.

## Current topology (verified 2026-06-01, `main`)

| Path | Where | Routed through fan-out? |
|------|-------|-------------------------|
| `Log_*` core + file/terminal/console sinks | `SOURCES/LOG/LOG.{H,CPP}` | n/a (it *is* the fan-out) |
| `SDL_Log` (audio) | `LIB386/AIL/SDL/*` | yes (auto, via the output fn) |
| `LogPrintf`/`LogPuts`/`CreateLog` | `LIB386/SYSTEM/LOGPRINT.CPP` (lib `sys`) | **no** |

Legacy clusters: `PERSO.CPP` (28), `RES_PICKER`/`RES_DISCOVERY`/`RES_SWITCH`
(21), `WINDOW.CPP` (7), MILES audio (16 — likely a dead backend in
`SOUND_BACKEND=sdl` builds), plus a long tail.

## The load-bearing decision — where the core lives

`LogPrintf` is in `LIB386`; the `Log_*` core is in `SOURCES`; **`LIB386` cannot
depend on `SOURCES`** (the console sink is what forced the core into `SOURCES` in
the first place). So `LogPrintf` cannot shim over `Log_*` while the core sits in
`SOURCES`.

**Fix: move the core into `LIB386/SYSTEM` (lib `sys`), next to `LOGPRINT.CPP`.**
The core (fan-out + sink pool + file sink + terminal sink) has *no* SOURCES
dependency — its only one is the console sink's `Console_Print` calls.

**Console dependency → inject it.** Keep the console-sink logic in the core, but
call `Console_Print` through a function pointer supplied at registration:

```c
typedef void (*LogLineFn)(const char *line);          /* in LOG.H */
LogSink *Log_MakeConsoleBufferSink(LogLineFn printer); /* was: (void) */
```

`SOURCES`/`InitAdeline` passes a one-line adapter:

```c
static void console_log_line(const char *s) { Console_Print("%s", s); }
...
Log_AddSink(Log_MakeConsoleBufferSink(console_log_line));
```

Zero `LIB386`→`SOURCES` edge; the sink keeps its formatting + main-thread gate.

*Alternatives considered:* a public callback-sink API exposing the record-kind
enum (rejected — leaks an internal type); leaving the console sink in `SOURCES`
behind a generic core primitive (more API surface than the one injected
pointer). Injection is the minimal change.

## Phases (each its own PR)

### U1 — Relocate the core to `LIB386`, inject the console printer — merged
Pure move + decouple, **behaviour identical**.
- `SOURCES/LOG/LOG.H` → `LIB386/H/SYSTEM/LOG.H`; `SOURCES/LOG/LOG.CPP` →
  `LIB386/SYSTEM/LOG.CPP` (add to lib `sys`).
- Replace the `#include "../CONSOLE/CONSOLE.H"` + direct `Console_Print` with the
  injected `LogLineFn`; update `InitAdeline` to pass the adapter.
- Rewrite the ~5 `#include "LOG/LOG.H"` sites to `<SYSTEM/LOG.H>`; move the two
  host tests (`tests/logging`, the `test_asset_preflight` stub).
- **Verify:** all presets build (this touches LIB386 layering + includes); host
  tests; boot log byte-identical; `--dump-state` scrollback unchanged.

### U2 — Shim `LogPrintf`/`LogPuts` over the fan-out — merged
The high-leverage step — unifies routing for all 109 sites in one file.
- `LogPrintf`/`LogPuts` `vsnprintf` (into a 1024 buffer, up from the static
  `LogStr[256]`) then emit as **`REC_RAW`** (verbatim — they are pre-formatted;
  no severity tag). Drop the direct `fopen` + `printf` and the `\n`→`\r\n`
  expansion (which also removes the **latent overflow** in that in-place loop;
  the fan-out file sink writes plain `\n`, fixing the mixed line endings too).
- **Split on `\n`, one `REC_RAW` per line** — like `Console_Print` already does.
  69/77 `LogPrintf` literals carry a trailing `\n` and some embed several
  (blank-line separators); since each sink appends its own `\n` per record, a
  naive single-record emit would double every newline. Splitting preserves the
  line structure exactly. `LogPuts` keeps its `"%s\n"` (so `%` in the string
  stays safe — its one real advantage over `LogPrintf`).
- **stdout is intentionally dropped, not lost.** `LogPrintf`'s unconditional
  `printf(stdout)` goes away; lines now reach the file sink (always) + terminal
  sink (stderr, TTY-gated). Verified nothing depends on `LogPrintf` reaching
  stdout (the control harness mirrors *console* output separately; the only
  stdout-piping script uses stdout as a **binary data channel** `LogPrintf` was
  polluting — so this is a correctness gain).
- `CreateLog` stays (truncate-per-launch); the file sink owns the file.
- **Subtlety — logging before `Log_Init`:** a few `LogPrintf` fire before any
  sink exists (`main()`'s `SDL_Init` failure, `CreateLog`'s own failure branch).
  The shim must **fall back to direct `stderr`/file when the fan-out has no
  sinks**, so nothing is lost. (Track with an "initialised" flag the core
  exposes.)
- **Delete the vestigial globals** `Log2File` (declared, never used/set) and
  `QuietLog` (read once under `DEBUG_TOOLS`, never set TRUE — dead branch).
- **Verify:** `adeline.log` content matches (same lines, minus the dups, uniform
  `\n`, no doubled blank lines); redirected stdout has no ANSI; host tests;
  `--fixed-dt` determinism unaffected.

### U3 — Severity pass (gradual, per subsystem) — in review
Make severity meaningful now that everything flows through the fan-out. Landed
in slices rather than one sweep:
- **Format-string fixes** (the 2 non-literal-format sites `LogPrintf(End_Error)`
  / `LogPrintf(titre)`, plus rejoining `DebugHQR`'s two-call line).
- **Display errors** (`WINDOW`, `SDL`) → `Log_Error`/`Log_Warn`. Safe because they
  fire inside `InitAdeline`, after the sinks are registered.
- **Early-boot fallback + data-dir errors.** The game-data-directory errors
  (`RES_DISCOVERY`, `DIRECTORIES`) fire in `main()` *before* the sinks exist, so
  converting them needed a fallback first: with no sink registered, `Log_*` now
  writes directly to stderr (bypassing the SDL spine, which isn't installed
  yet). Mirrors `LogPrintf`'s fallback. Only then were those errors converted.
- **Deliberately left raw:** deep-internal diagnostics (`S_MALLOC`, `LOADSAVE`,
  `EVENTS`, …) — low traffic, fine as text. The 44 audio `SDL_Log` sites keep
  their text tags (`[SFX]`, `[MUSIC]`); severity-tagging them is optional and
  not done.

## Out of scope

- Removing `LogPrintf`/`LogPuts` (they stay as the legacy + early-boot entry
  points — now thin shims).
- The MILES backend's logs (dead in `SOUND_BACKEND=sdl` builds) unless it's
  reactivated.
- Async/threaded logging, log rotation, overlay colour (unchanged from
  BOOT_LOG_PLAN.md non-goals).

## Verification (whole campaign)

- Boot-log content diff before/after (`adeline.log` + terminal + `--dump-state`
  scrollback): same information, no duplicate lines, uniform line endings.
- Redirected output (`2>file`) carries no escape codes.
- Host suite green; `--fixed-dt` byte-identical dumps (logging must stay off the
  determinism path).
- Cross-platform build (`linux`, `cross_linux2win`) — U1 moves code across the
  LIB386 boundary and rewrites include paths.
