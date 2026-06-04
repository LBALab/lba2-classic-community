# Boot Log + Exit Screen

**Status:** All phases done. Phases 6 (exit screen), 1 (core log + file sink), 3 (terminal sink), 4 (boot-path wiring), 5 (Funfrock header — later dropped), and 2 (console sink) implemented, then reshaped to the terse Quake-style boot log + asset preflight (see "Boot-log shape" below). Graceful optional-asset degradation remains a separate PR (B).

## Goal

Add two small, self-contained features to LBA2-CE:

1. A **structured boot log** that records each subsystem coming online during
   engine init. Lives in the in-engine console buffer, the terminal (when
   launched from one), and the log file (always). Modeled on the id Tech 4
   (Doom 3) `------ Initializing X ------` convention.
2. An **exit screen easter egg** that prints ANSI art + a rotating themed quote
   when the game exits cleanly, *only* when stdout is a real TTY.

Both serve the player who looks for them and stay invisible to everyone else.

## Non-goals

- **No progress bars, no `\r` animation, no carriage-return tricks.** The
  engine loads fast and the primary sink is an append-only console buffer.
- **No new threading.** Boot is single-threaded; the `SDL_Log` spine provides
  thread-safety for runtime additions. Don't introduce a queue or async sink.
- **No log categories/channels** ("renderer", "audio", etc.) as a call-site
  concept in v1. YAGNI until proven necessary. (Internally, one SDL log
  category is used to tag section headers — see "Architecture".)
- **No replacement of the user-facing splash screen.** That artifact stays.
  The boot log is a separate, developer-facing surface.
- **The exit screen must not fire on crash paths** — only from the clean
  shutdown path. Don't register it as an `atexit` handler.

## Architecture — SDL_Log spine with a thin `Log_*` wrapper

The fan-out is built on SDL3's logging, which the engine already uses in 46
places (the whole `LIB386/AIL/SDL/` audio stack plus `PLAYACF.CPP`).

- **Call sites** use `Log_Info("...")` etc. The wrapper `vsnprintf`s the
  message, then forwards to `SDL_LogMessage(category, priority, "%s", buf)`.
  Severity maps to SDL priority; section headers ride a dedicated SDL log
  category so each sink can render them in its own idiom without parsing text.
- **One `SDL_SetLogOutputFunction` callback owns the fan-out.** A small
  trampoline maps `(category, SDL priority) → (LogSeverity, isSectionHeader)`
  and calls a pure, SDL-free internal `log_emit(...)` (anonymous-namespace) that
  writes to every registered sink. Installing the output function also routes
  the engine's existing 46 `SDL_Log` sites and SDL's own internal messages
  through these sinks (see the filtering note below — this is why per-sink
  filtering is a v1 requirement).
- **Host-test seam.** Only three spots touch SDL: the per-severity
  `SDL_LogMessage` forward, the trampoline, and the install in `Log_Init()`. All
  are behind `#ifdef LBA_LOG_NO_SDL`. Host tests define `LBA_LOG_NO_SDL`, so the
  wrapper calls `log_emit` directly with no `SDL_Init`, and assert on sink
  output. The core (formatting, sinks, section/severity logic) is SDL-free.

## Load-bearing design decisions

These are constraints, not suggestions. Don't redesign them without raising
the question first.

1. **One log call fans out to all sinks.** `Log_Info("Loaded HQR ...")`
   writes to every registered sink. Each sink renders the same record its own
   way. Call sites never know which sinks are present. Implemented on the
   `SDL_Log` spine described above.
2. **Severity belongs to the message, not the sink.** Four levels:
   Debug / Info / Warn / Error. Call sites always state the true severity.
   **Per-sink filtering is a v1 requirement** (not an afterthought), with these
   defaults: **file and terminal sinks take Debug+** (we debug heavily while
   building this), **console sink takes Warn+** so the in-game overlay stays
   clean, with a runtime toggle to raise the console level on demand. Defaults
   matter because the `SDL_Log` spine pulls verbose audio/SFX logs into the
   pipeline; without filtering they would spam the console.
3. **Sections produce visual structure.** Each subsystem init is wrapped in a
   `BeginSection("Initializing X")` / `EndSection()` pair (or `ScopedSection`
   RAII). Sinks render section headers in their native idiom (dashes for
   terminal, `====` for file; console is monochrome in v1 — see decision 6).
   **Exit-safety caveat:** the boot path calls `exit(1)` directly on init
   failure (no C++ stack unwinding — see `INITADEL.C`), so `ScopedSection`
   destructors are best-effort only. The file sink must persist per-line (flush
   on each record, not at `EndSection`), and any init that can `exit()` should
   `EndSection()` before exiting. Don't rely on RAII close for log correctness.
4. **Terminal sink is opt-in by environment, not by code.** Activates only if
   `isatty(stderr)` is true *and* `getenv("NO_COLOR")` is unset. On Windows,
   calls `SetConsoleMode(ENABLE_VIRTUAL_TERMINAL_PROCESSING)` once at init for
   our own ANSI output. If either check fails, the sink is a silent no-op —
   never falls back to plain stderr (avoids ANSI garbage in redirected output).
   (`isatty`/`NO_COLOR`/VT-enable are all new — the only existing `NO_COLOR`
   use is the compile-time `#ifdef` in `tests/test_harness.h`; align spelling.)
5. **File sink is always on, and reuses the existing `adeline.log`.** Path comes
   from `SDL_GetPrefPath("Twinsen", "LBA2")` → `GetLogPath`, the same path
   `CreateLog` uses. **Do not add a second `lba2.log`.** The sink opens its own
   `FILE*` to that path (append) and writes file-only — *not* through `LogPrintf`
   (refined in Phase 1: routing through `LogPrintf` also echoes stdout and isn't
   unit-testable; stdout/ANSI is the Phase-3 terminal sink's job). Structured
   records and the engine's legacy `LogPrintf` lines still share the one file.
6. **Console buffer sink is the universal surface.** Works on Linux, Windows,
   macOS, Steam Deck, Xbox GDK, anywhere the engine runs. Other sinks are
   bonuses on top. **v1 constraints (research A1):** the console stores plain
   `char[256]` lines with no per-line color, so the console sink is
   **monochrome in v1** (severity not color-coded); `CONSOLE_SCROLLBACK_LINES`
   is **bumped from 128** so early boot lines survive in the ring; and because
   the ring is not thread-safe, the console sink takes **main-thread records
   only** (off-thread logs — e.g. the audio callback — are dropped from the
   console but still reach the file/terminal sinks).
7. **Exit screen is a totally separate module** from the log. It does not call
   into `Log_*`. It writes raw ANSI directly to stdout (not stderr) after
   checking `isatty(stdout)`. Lives in its own file. **Hook:** called from
   `TheEndInfo` (`SOURCES/PERSO.CPP:2199`) guarded by `End_Num == PROGRAM_OK` —
   verified crash-safe (`End_Num` inits to `-1`, `PROGRAM_OK == 2`, crash paths
   set `0`/`1`/`3`, and early `exit(EXIT_FAILURE)` paths leave `-1`). There are
   no signal handlers, so Ctrl-C never reaches this path (and never triggers it).

## Reference material

- **id Tech 4 / Doom 3 boot logs:** the `------ Initializing X ------`
  section-header convention. Each subsystem owns its own block and prints
  its own detail.
- **Quake (id Tech 2):** the `======== Quake Initialized ========` closer and
  the "log == runtime console buffer" insight.
- **Existing in-engine console** (`SOURCES/CONSOLE/`, see `docs/CONSOLE.md`):
  append via `Console_Print(const char *fmt, ...)`, read back via
  `Console_GetScrollback(...)`. The console buffer sink integrates with these.
- **The DOS exit-text fossil:** `V_ColorLine` (`SOURCES/PERSO.CPP:2149`) is a
  no-op stub still called around `TheEndInfo`'s error lines — the vestige of the
  original Watcom colored exit text, the naming precedent for exit-time framing.

## API shape (target)

C++98, and following the house new-infra pattern (`SOURCES/RES_PICKER.CPP`,
`SOURCES/CONSOLE/`): a **flat `extern "C"` public surface** (`Log_*`, mirroring
`Console_*`) with all internals — sink registry, fan-out, formatting, state — in
an anonymous namespace, the way `RES_PICKER.CPP` keeps its `PickerState` private
and exposes only `extern "C" PromptForResDir`. The only C++-visible type is the
`ScopedSection` RAII helper, declared behind `#ifdef __cplusplus` for C++
callers; C-style call sites use the explicit `Log_BeginSection`/`Log_EndSection`
pair. Plain `enum`, no `enum class`; no named public namespace (this is why all
callers — including the C++-compiled `INITADEL.C`/`PERSO.CPP` — can use it).

```c
/* SOURCES/LOG/log.h */
typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR } LogSeverity;
typedef struct LogSink LogSink;   /* opaque */

#ifdef __cplusplus
extern "C" {
#endif
void Log_Init(void);
void Log_Shutdown(void);
void Log_Info(const char *fmt, ...);   /* and Log_Debug/Log_Warn/Log_Error */
void Log_BeginSection(const char *title);
void Log_EndSection(void);
LogSink *Log_MakeConsoleBufferSink(void);
LogSink *Log_MakeTerminalSink(LogSeverity min_severity);
LogSink *Log_MakeFileSink(const char *path, LogSeverity min_severity);
void Log_AddSink(LogSink *sink);
void Log_RemoveSink(LogSink *sink);
#ifdef __cplusplus
}
/* C++-only RAII convenience over Log_BeginSection/Log_EndSection.
   Best-effort — exit() on init failure does not unwind (see caveat). */
class ScopedSection {
  public:
    explicit ScopedSection(const char *title) { Log_BeginSection(title); }
    ~ScopedSection() { Log_EndSection(); }
};
#endif
```

Sinks may be modelled as function-pointer structs (à la the console's
`Console_CmdFn`/`Console_LineSinkFn` typedefs) rather than a virtual class
hierarchy, to stay in the C-style house idiom (CODESTYLE: "plain structs and
value semantics over object hierarchies"). Kept shallow either way —
implementer's call.

## How to work on this with Claude Code

This is a planned-and-phased feature. The workflow is:

1. **Research phase** — investigate the codebase and answer a structured set of
   questions about the console, boot path, exit path, filesystem conventions,
   CLI, build system, and existing terminal code. (Done.)
2. **Report phase** — propose plan revisions based on findings; update this doc
   before any code is written. (Done — this revision.)
3. **Implementation phase** — one phase per PR, in the recommended order below.

## Phased plan

Each phase is independently mergeable. Don't combine phases into one PR.

**Recommended order (from research): 6 → 1 → 3 → 4 → 5 → 2.** Phase 6 (exit
screen) is fully self-contained and de-risks new-file / build / test / `isatty`
wiring first; the architecture decision (a/b) isn't needed for it. The console
sink (Phase 2) is the highest-effort surface (new attribute-free monochrome
path, scrollback bump, thread gating) and goes last.

New files live in `SOURCES/` (not `src/` — there is no `src/`; LIB386 can't
depend on SOURCES, and the console sink forces SOURCES placement):
`SOURCES/LOG/log.{h,cpp}` (mirrors `SOURCES/CONSOLE/` for layout; mirrors
`SOURCES/RES_PICKER.CPP` for internal style — anon-namespace internals, flat
`extern "C"` public surface) and `SOURCES/EXIT_SCREEN.{h,cpp}` (top-level, peer
of `CHEATCOD.CPP`).

> **Superseded (logging unification, U1):** the log core moved to
> `LIB386/SYSTEM/LOG.{H,CPP}`. The "console sink forces SOURCES placement"
> constraint was lifted by injecting the console printer (a `LogLineFn` passed to
> `Log_MakeConsoleBufferSink`), so the core carries no SOURCES dependency and
> `LogPrintf` (also in LIB386) can shim onto it. See
> [LOGGING_UNIFICATION.md](LOGGING_UNIFICATION.md).

Build targets: GCC (`linux` preset) and MinGW (`cross_linux2win` /
`windows_ucrt64`). **There is no MSVC preset.** No `-Werror` in the build.

### Phase 1 — Core log API + file sink — done

- Add `SOURCES/LOG/LOG.H` and `SOURCES/LOG/LOG.CPP` matching the sketched API,
  on the `SDL_Log` spine with the `LBA_LOG_NO_SDL` test seam (see Architecture).
- Implement `Log_Init`, `Log_Shutdown`, severity entry points, `Log_BeginSection`/
  `Log_EndSection`, the C++ `ScopedSection` helper, a fixed-size sink pool (max 8)
  + dispatch list, and the `SDL_SetLogOutputFunction` install + trampoline. Flat
  `extern "C"` surface, anon-namespace internals (RES_PICKER style).
- Implement **per-sink severity filtering** with the decision-2 defaults.
- Implement `Log_MakeFileSink(path, min_severity)` only — own `FILE*` to the
  `adeline.log` path (no second file; see decision 5). Plain text, severity
  prefix `[DEBUG]`/`[INFO]`/`[WARN]`/`[ERROR]`, section headers `==== Title ====`.
  Flush per-line (exit-safety).
- Build wiring: `list(APPEND SOURCES_FILES LOG/LOG.CPP)` in
  `SOURCES/CMakeLists.txt`; `add_subdirectory(logging)` for the test.
- **Verification:** host unit test `tests/logging/` (built with `LBA_LOG_NO_SDL`,
  C++11, `register_host_test`) creates a file sink, calls each severity, opens a
  section, and asserts on-disk content; per-sink filtering checked too. Not wired
  into the boot path yet (Phase 4), so `LOG.CPP` compiles into the engine but is
  dormant.

**Decisions taken (Phase 1):**

- `Log_MakeFileSink(path, min_severity)` owns its own `FILE*` (file-only, no
  stdout echo) — refines decision 5's "via `LogPrintf`": still one log file
  (`adeline.log`), but the sink is unit-testable at a temp path and the
  stdout/ANSI concern stays with the Phase-3 terminal sink. API sketch updated.
- Sinks live in a fixed static pool (max 8); `Log_AddSink`/`Log_RemoveSink`
  manage a separate dispatch list. No heap.
- Sections ride the SDL spine via a dedicated category (`LBA_CAT_SECTION`);
  `Log_EndSection` is a no-op for the file sink (bracketing sinks hook it later).
- File opened append (matches `adeline.log` semantics); records flushed per line
  so an `exit()` mid-boot still leaves them on disk.
- Verified: host test passes; full engine compiles/links `LOG.CPP` with the SDL
  spine. Dormant until Phase 4 wiring.

### Phase 2 — Console buffer sink — done

- Reuse the console API (`Console_Print`). Do not duplicate.
- `Log_MakeConsoleBufferSink()`. **Monochrome** — the console ring stores plain
  text with no per-line colour, so severity is a text tag (`[WARN]`), not a
  colour. Raw lines render verbatim; sections as `== Title ==`.
- **Bumped `CONSOLE_SCROLLBACK_LINES` 128 -> 512** so the boot log isn't evicted
  by gameplay output before the player opens the console.
- **Main-thread records only:** the main thread id is captured in `Log_Init()`;
  the console sink drops records from other threads (the ring is not
  thread-safe). They still reach the file and terminal sinks.
- Registered during `InitAdeline` alongside the file + terminal sinks.

**Decisions taken (Phase 2):**

- **Default `INFO`, not `Warn+` (revises decision 2 for this sink).** Decision 2
  chose Warn+ to keep the overlay clear of the verbose audio/SFX logs — but those
  are already gated behind `sfxLogEnabled`/`musicLogEnabled` (both default off),
  so at INFO the overlay is clean *and* the boot log (INFO/raw) actually lands in
  the ring. A literal Warn+ default would have filtered the boot log out at emit
  time, contradicting "early boot lines survive in the ring" and the verification
  below — the lines would never be captured, so lowering the level afterwards
  couldn't bring them back.
- **Runtime toggle:** the `loglevel [debug|info|warn|error]` console command
  (`Log_SetConsoleSeverity` / `Log_GetConsoleSeverity`) adjusts only this sink;
  the file and terminal sinks are unaffected.
- The console sink lives in `LOG.CPP` with the other sinks and calls
  `Console_Print`; host tests that compile `LOG.CPP` standalone provide a
  `Console_Print` stub (a recording stub in `test_log`, a no-op in
  `test_asset_preflight`).
- **Verification:** host test (`test_console_sink`: filtering, tags, raw
  verbatim, section idiom, runtime severity change); live boot — the full boot
  log appears in the scrollback via `--dump-state`'s `log` array, and `loglevel`
  responds via `--exec`.

### Phase 3 — Terminal sink — done

- Implement `Log_MakeTerminalSink(min_severity)` with the rules in decision 4
  (`isatty(stderr)`, runtime `getenv("NO_COLOR")`, Windows VT-mode enable for
  our ANSI). On any failure the sink registers but its write is a no-op.
- ANSI 16-color (not 256, not truecolor). Severity → color: Info default, Warn
  yellow, Error red, Debug dim grey. Section headers cyan.
- Section header `------ Title ------` padded to 60 chars; closer is a matching
  all-dash line (needs a section-end record — added to the core, see below).
- **Verification:** pty smoke shows coloured records + section rules on stderr;
  redirect stderr → empty; `NO_COLOR=1` → empty. Phase 1 host test still passes
  (file sink unchanged through the new record path).

**Decisions taken (Phase 3):**

- Sections now carry a *kind* (normal / begin / end) through the fan-out — over
  the SDL spine via three categories (`LBA_CAT_LOG` / `_SECTION` / `_SECTION_END`).
  The file sink ignores the end record; the terminal sink draws the closer rule.
- Terminal sink writes to `stderr` (not owned — never `fclose`d on shutdown; the
  file sink keeps `owns_fp`). Gate evaluated at `Log_MakeTerminalSink` time;
  failure swaps in a `noop_write`.
- **Not auto-registered in `Log_Init()`** (the plan's earlier "register during
  Log_Init" wording). Like the file sink, sinks are created via `Log_Make*Sink`
  and added explicitly — the boot setup wires them in Phase 4. Dormant until then.
- No host test: ANSI/TTY rendering is environmental; verified by pty smoke (the
  shared record path stays covered by the Phase 1 file-sink test).

### Phase 4 — Wire boot path — done

- `Log_Init()` + register the file sink (adeline.log) and terminal sink, and
  `atexit(Log_Shutdown)`, in `InitAdeline` right after `CreateLog`.
- Convert the boot banner + the major subsystem inits (event system, window,
  video, audio) to `Log_BeginSection`/`Log_Info`/`Log_Error`/`Log_EndSection`,
  with real detail (paths, window title, render resolution, audio status). On a
  failed init the section stays open before `exit(1)` — informative, and the
  per-line flush means the records are already on disk.
- **Verification:** headless boot writes the sections to `adeline.log`,
  interleaved with the legacy lines; a pty boot shows the cyan section rules in
  colour on stderr (terminal sink via the real SDL spine); redirected stderr has
  zero escapes; clean exit.

**Decisions taken (Phase 4):**

- **SDL filters per category before the output function.** Custom categories
  default to a high threshold, so the first headless boot dropped every
  INFO/DEBUG record. Fix: `Log_Init` lowers our three categories to
  `SDL_LOG_PRIORITY_DEBUG`; the per-sink min-severity is the real filter.
- **Legacy `LogPrintf` left in place** (the boot path still has unconverted
  lines like "Initialising Joystick", "Platform: Linux", the version footer).
  Only the banner + four major subsystems were converted; a full `LogPrintf`
  purge is deferred until the system is proven. Both writers share `adeline.log`
  (CreateLog truncates per launch; the file sink and LogPrintf both append).
- Sinks registered inside `InitAdeline` (not `main`) so they exist before the
  subsystem inits but after `CreateLog` truncates the file.
- Only the major subsystems are wrapped; minor steps (joystick, OS, CPU,
  keyboard, mouse, timer) keep their existing lines for now.

### Phase 5 — Funfrock framing header — dropped

Briefly added, then removed from the boot log. The 2-line in-universe banner
("Twinsun Central Command — Citizen Access Terminal" / "By order of Dr. FunFrock,
all activity is monitored.") read as noise at the top of every launch's log. The
flavour is better placed in the in-engine console as an easter egg later; the log
itself stays straight-faced. The build-identity + CPU/RAM line that followed the
banner is kept (it is the log's self-describing header).

**Decisions taken (Phase 5):**

- Added a `Log_Raw()` primitive (record kind `REC_RAW`, fourth SDL category) so
  banner lines render verbatim — no `[INFO]` prefix, no section dashes. Both
  sinks print it as-is.
- Banner emitted from `InitAdeline` after the sinks are added (the plan's "top
  of `Log_Init()`" predates the sink-agnostic `Log_Init`); it is the first thing
  in the log after CreateLog's own line.
- Build identity uses the existing `APPNAME` (product + version) +
  compile-time platform macro + `__DATE__`/`__TIME__`; CPU/RAM via
  `SDL_GetNumLogicalCPUCores()` / `SDL_GetSystemRAM()` (no engine-internal
  probing). Verified in `adeline.log` and via the boot run.

### Boot-log shape — Quake-style (revises Phases 4–5)

Design review: the per-subsystem `------ Initializing X ------` sections were too
heavy for an instant boot. The boot log is now a folded identity header, one
terse `Label  value` line per subsystem, an asset preflight, and a Quake closer.
Sample:

```
LBA2 Classic Community 0.11.0-dev · Linux x86_64 · 6 cores · 31 GB RAM
Built …
Assets: …
Saves:  …
Config: …
Log:    …

Events     ok
Joystick   not detected
Audio      disabled (--no-audio)
Display    848x480 fullscreen
Assets     all present
Language   Français
======== Ready in 0.50s ========
```

Decisions:

- Per-subsystem sections dropped from boot; the section primitive
  (`Log_BeginSection`/`EndSection`, terminal `------`) stays as a latent,
  tested capability (unused for now).
- **Bookend lines tinted.** The identity header and the `Ready` closer use a new
  `Log_Banner()` primitive (record kind `REC_BANNER`, fifth SDL category) that
  the terminal sink draws in cyan — the section-header colour. File and console
  sinks print it verbatim (the tint is the terminal sink's alone; `adeline.log`
  is unchanged), so the contract "call sites never know which sinks exist" holds
  — no ANSI is baked into the call site. The middle path lines stay plain
  `Log_Raw`.
- **`Config` shows the file, not the folder.** Like the `Log:` line, it now
  resolves with `CFG_NAME` (`Config: …/lba2.cfg`). `Assets`/`Saves` stay
  directories (they hold many files); `Config`/`Log` are single named files.
- **Console is now colour-coded (supersedes the "monochrome v1" of decisions 2,
  6 and Phase 2).** The console sink drops the `[INFO]`/`[WARN]`/`[ERROR]` text
  tag and conveys the level as colour instead: `LogLineFn` carries the
  `LogSeverity`, the engine adapter (`ConsoleLogLine`) maps it to a
  `CONSOLE_COL_*` ink, and `Console_PrintColored` stores a per-line colour. The
  severity inks are **palette-independent fixed RGB** resolved in the
  compositor (scene palettes are arbitrary, so "warn is yellow" can't rely on a
  palette index); default/info stays on the scene's palette-white, so existing
  text is unchanged. The log core stays palette-agnostic — it passes severity,
  not colours. `LogLineFn` also carries a `LogLineKind` so the adapter tints the
  **banner bookends and section headers cyan** in the console too, matching the
  terminal sink's structural colour (banner/section → cyan, normal → severity).
- **Console renders CP437; boot text is UTF-8.** `AffStringToBuffer` is a CP437
  bitmap font, so the `·` separator (and accented language names like
  `Français`/`Español`) is folded UTF-8 → CP437 at draw time. Stored scrollback
  stays UTF-8, so `Console_GetScrollback` / `--dump-state` remain valid UTF-8.
- **Accurate `Display` line.** Fullscreen is config-driven and applied late
  (`ReadConfigFile` -> `SetWindowFullscreen`, inside `InitProgram` — *after*
  `InitAdeline`), so reading the mode in `InitAdeline` always said "windowed".
  The single `Display WxH windowed|fullscreen` line is now logged from `main`
  after `InitProgram`, and reads the **actual** SDL window state via a new
  `Window_IsFullscreen()` (`SDL_GetWindowFlags`), not the cached
  `GetWindowFullscreen()` flag — which can disagree when a mode change fails
  (observed on WSL, where leaving fullscreen errors and the cache flips). It also
  replaces the old separate `Window`/`Render` lines, which were redundant
  (same size) and conflated render surface with window mode.
- **Asset preflight** — dedicated module `SOURCES/ASSET_PREFLIGHT.{H,CPP}`
  (`int AssetPreflight()`), called from `main` after `InitDirectories`/`Log_Init`,
  before any heavy load. Checks each expected asset with the **correct
  per-category resolver**: root `.hqr` via `GetResPath`, `video.hqr` via
  `GetMoviePath` (it lives in `video/`, not the root — `GetResPath` gave a false
  "missing"), and the `music/` and `vox/` subtrees via
  `GetJinglePath`/`GetVoicePath` (subtree presence — those are per-track /
  per-language collections the audio modules enumerate). Case-folding discovery
  handles `VIDEO/`/`Music/`/`VOX/`.
- **Classification by actual boot-fatality.** Required = the root HQRs **and
  `video.hqr`** (loaded at boot by `InitAcf`, which hard-fails without it today).
  Optional = `holomap`/`scene`/`screen`/`lba_bkg`/`music`/`vox` (loaded on
  demand; missing only matters if that feature is used).
- **Fail fast.** `AssetPreflight` returns the missing-*required* count; `main`
  logs each missing one, then on a non-zero count prints `cannot start: …` and
  `exit(EXIT_FAILURE)` — **before** the data loads and before `Ready`. So a
  missing-asset boot never prints "Ready", never dies later at an arbitrary
  load, and exits **non-zero** (unlike `TheEndCheckFile`, which exits 0).
  Optional missing → `Log_Warn`, boot continues, `Assets N missing (0 required)`.
  (Making the optional set degrade gracefully *at use time* — e.g. skip cutscenes
  when `video.hqr` is absent, which would move it back to optional — is PR B.)
- **Test:** `tests/asset_preflight/` (integration; builds a fake game tree,
  links DIRECTORIES + sys + SDL like `tests/discovery`) asserts the
  required/optional verdicts, the fail-fast count, and the `video.hqr`-in-`video/`
  resolver (the regression that shipped once). The `*_HQR_NAME` defines were
  split into `SOURCES/HQR_NAMES.H` so the module + test skip the `C_EXTERN.H`
  prelude.
- Closer `======== Ready in Xs ========` (`SDL_GetTicks`), counting **engine
  subsystems only** (InitAdeline: SDL, audio, video, window — ~0.2s). The intro
  publisher/Adeline logos that follow are a deliberate timed slideshow
  (`ShowLogo`), so they're excluded; the closer is logged right after the boot
  status lines, before that slideshow. (Measured: subsystems ~0.21s vs ~4s to
  the menu — the rest is the slideshow.)
- Silenced redundant legacy lines: `INITMODE.CPP` "Desired resolution",
  `MIDI.CPP` debug, `COMMON.CPP` audio-init `SDL_Log`, and the INITADEL
  "Please wait" / OS lines (platform is in the header). Added `JoystickGetName()`.
- **Per-type asset counts dropped:** ressource HQRs only expose a cache-slot
  budget (`MaxIndex = maxrsrc`), not the file's entry count; real counts need an
  HQR-header reader — deferred. The preflight presence check covers verification.
- Graceful runtime degradation for missing optional assets (don't crash on
  `video.hqr` at cutscene time) is **PR B**, separate from this shape work.

### Phase 6 — Exit screen module — done

- Add `SOURCES/EXIT_SCREEN.h` and `SOURCES/EXIT_SCREEN.cpp`. Does NOT depend on
  `log.h`.
- Public surface: `void PrintExitScreen(void);`
- Internal: `isatty(stdout)` check; bail silently otherwise. Otherwise pick a
  random quote from the pool and write it, in quotation marks, to stdout, then
  flush.
- **Hook:** call from `TheEndInfo` (`SOURCES/PERSO.CPP`) guarded by
  `End_Num == PROGRAM_OK`. Not registered as `atexit`. Not called on crash.
- Quote pool: the in-source `QUOTES[]` array in `SOURCES/EXIT_SCREEN.CPP`,
  curated from retail LBA1/LBA2 English TEXT.HQR (source tags stripped). One flat
  pool — no variants in v1.

**Parked for a follow-up:** half-block Unicode ANSI art (a Funfrock-regime
variant and a Twinsen-friendly variant, authored from source PNGs via
`tools/png_to_ansi/`), with the quote pool re-split into per-variant pools so the
printed line matches the art's mood. Re-enabling means: add the art array, print
it before the quote, re-add the Windows VT enable
(`ENABLE_VIRTUAL_TERMINAL_PROCESSING`), and split the pool by mood.
- **Verification:** launch and quit cleanly from a terminal — see exit screen.
  Launch with stdout redirected — see nothing on stdout. Force a crash/error
  exit — exit screen does NOT print.

**Decisions taken (Phase 6):**

- Public surface `PrintExitScreen(void)` (`extern "C"`); internals in an
  anonymous namespace (RES_PICKER style). Kept deliberately small — the gate and
  selection are inlined, no extracted helpers, no test target (verification is
  manual; see below).
- Gate: `isatty(stdout)` only. `NO_COLOR` and the Windows VT enable are dropped
  for v1 (plain-text quote, no colour) — both return with the parked ANSI art.
- Hook: `TheEndInfo` (`SOURCES/PERSO.CPP`), `if (End_Num == PROGRAM_OK)`, before
  the error/OK switch. Not `atexit`; never fires on crash/error or under the
  redirected control harness.
- v1 is quote-only: a single flat `QUOTES[]` pool, no art and no variants. The
  quote is printed in quotation marks. ANSI art (and per-variant mood pairing +
  Windows VT enable) is parked — see "Parked for a follow-up" above. Quotes
  curated from retail LBA1/LBA2 English TEXT.HQR (tags stripped) and live
  directly in the in-source `QUOTES[]` array.
- Rotation seeded from `time(NULL)` (only ever fires on interactive TTY exit).
- Verified manually (no test target for a cosmetic easter egg): full engine
  compiles/links; pty check shows the quoted line on a TTY and zero bytes when
  stdout is redirected.

## Open questions

1. ~~What is the existing in-engine console's public API?~~ **Resolved (A1):**
   `Console_Print` / `Console_GetScrollback`; 128-line × 256-char monochrome
   ring, single-threaded. See `docs/CONSOLE.md`.
2. ~~Where is the user data directory determined?~~ **Resolved (A4):**
   `SDL_GetPrefPath("Twinsen", "LBA2")` → `directoriesUserDir`; log via
   `GetLogPath` (`adeline.log`).
3. **`--quiet` flag:** extend `Control_ParseArgs` (`SOURCES/CONTROL.CPP`, the
   `--verbose` pattern). It is parsed at `main:2320`, *before* `CreateLog`
   (`INITADEL.C:74`), so it can gate logger init. Want `--quiet` to disable all
   sinks except the file sink. (`--profile-boot` similarly.)
4. ~~Quote pool curation~~ **Resolved (Phase 6):** quotes curated from retail
   LBA1/LBA2 English TEXT.HQR (source tags stripped) and embedded directly in the
   in-source `QUOTES[]` array in `SOURCES/EXIT_SCREEN.CPP`. No separate doc.
5. **Xbox GDK / Steam Deck behavior:** terminal sink is silent on both. Is the
   on-screen overlay sink needed in v1, or can it wait? (Deferred — see Out of
   scope.)

## Decisions taken

- **Architecture:** SDL_Log spine + thin `Log_*` wrapper; single
  `SDL_SetLogOutputFunction` fan-out; `LBA_LOG_NO_SDL` seam for SDL-free host
  tests. (Resolves the build-from-scratch vs reuse question.)
- **File sink:** reuse existing `adeline.log` (own `FILE*` to that path, append,
  file-only — refined in Phase 1 from "via `LogPrintf`"); no second file.
- **Per-sink filtering** is v1 and required: file/terminal Debug+, console
  Warn+ (runtime-raisable). Prevents the spine's audio/SFX logs spamming the
  console.
- **Console sink:** monochrome in v1; `CONSOLE_SCROLLBACK_LINES` bumped;
  main-thread records only (off-thread dropped from console, kept elsewhere).
- **Exit-screen hook:** `TheEndInfo` / `PERSO.CPP:2199`, `End_Num == PROGRAM_OK`
  (crash-safe, verified); placeholder quote pool for v1.
- **Placement:** `SOURCES/EXIT_SCREEN.cpp` (no `src/`). The log core started in
  `SOURCES/LOG/` and later moved to `LIB386/SYSTEM/LOG.{H,CPP}` — see the
  superseding note under "How to work on this" and
  [LOGGING_UNIFICATION.md](LOGGING_UNIFICATION.md).
- **API surface:** flat `extern "C"` `Log_*` functions (mirrors `Console_*` /
  `RES_PICKER.CPP`), anon-namespace internals, C++-only `ScopedSection` helper.
  C++98 — plain `enum`, no `enum class`, no named public namespace. Aligns with
  CODESTYLE's new-infra zone; callable from the C++-compiled boot path.
- **Build/CI:** GCC (`linux`) + MinGW (`cross_linux2win`/`windows_ucrt64`); no
  MSVC preset exists; no `-Werror`.
- **Phase order:** 6 → 1 → 3 → 4 → 5 → 2.
- **AGENTS.md** golden-rule carve-out added: new SOURCES infra (console,
  logging, harness) is host-tested, not ASM-equivalence-tested.

(Per-phase PRs append their own "decisions taken" notes here as they merge.)

## Fatal-error visibility on terminal-less launches

The boot log + asset preflight report failures to the terminal and `adeline.log`.
On a double-clicked `.app`/`.exe`, Steam Deck, or mobile there is no terminal and
nobody hunts for the log, so a fatal boot error (incomplete data, missing HQR,
out of memory) would just make the app vanish. The engine already solves the
"no game **directory**" case with a native dialog (`RES_PICKER`, explicitly to
avoid the log "buried in the terminal"); this extends the same treatment to the
other fatal paths. (Its own PR — touches the `TheEnd*` family, not the sinks.)

- `ShowFatalErrorDialog` (`PERSO.CPP`) wraps `SDL_ShowSimpleMessageBox`, gated so
  it only fires on an interactive, terminal-less launch:
  `!isatty(stderr)` (a terminal already shows the log) **and**
  `!Control_IsActive()` (never block the automation harness) **and** a display is
  available (`SDL_WasInit`/`SDL_InitSubSystem(SDL_INIT_VIDEO)` — headless falls
  back to log-only). Mobile: shown where SDL supports it, else log-only — no
  worse than today.
- Wired into `TheEndInfo` (the `atexit` every fatal path funnels through): the
  `ERROR_NOT_FOUND_FILE` / `NAME_NOT_FOUND` / `NOT_ENOUGH_MEM` cases build a
  friendly message (+ the log path) and call it. One change → every fatal exit is
  GUI-visible, not just the preflight.
- The asset-preflight abort now calls `TheEnd(ERROR_NOT_FOUND_FILE, …)` instead
  of `exit()`, so it flows through that same surface.
- **Exit codes fixed:** `TheEnd`/`TheEndCheckFile` now exit non-zero on any error
  (only `PROGRAM_OK` → 0), so launchers/scripts/Steam/CI can detect a failed
  boot. Previously every exit was 0.
- Verified: GUI-like run (no tty, not harness, display) blocks on the dialog;
  harness run exits 1 with no dialog; all-present exits 0.

### The `InitAdeline` gap (`BootFatal`)

`TheEndInfo` only covers the fatal paths that run *after* `main` registers
`atexit(TheEndInfo)` (`PERSO.CPP`) — which is *after* `InitAdeline` returns. The
subsystem-init failures inside `InitAdeline` (events, window, config write/copy,
video, screen, graphics, MIDI) used a bare `exit(1)`: no `End_Num`, no dialog,
and registered before `atexit` anyway — so they vanished silently on a
double-clicked launcher, exactly the "can't even open the window" case where a
GUI user most needs to see why.

- `BootFatal(fmt, …)` (`PERSO.CPP`, declared in `PERSO.H`) is the fix: it logs
  the reason at `Error`, reuses `ShowFatalErrorDialog` (same gating), and exits
  non-zero. Each `InitAdeline` failure now calls it with a one-line cause.
- It deliberately skips `TheEndInfo`'s teardown (`WriteConfigFile`,
  `Clear3dExt`): a subsystem that never finished initialising has nothing safe
  to tear down, and those calls would touch un-anchored state.
- A video/window-init failure can't show a graphical dialog (no display) — the
  `SDL_InitSubSystem(VIDEO)` probe fails and it degrades to log-only. The config
  / MIDI failures happen after the window is up, so those *do* get the dialog.
- Verified: forcing `SDL_VIDEODRIVER=<invalid>` makes `InitWindow` fail →
  `[ERROR] The game window could not be created.` logged, exit 1, no hang,
  dialog correctly skipped under the harness and when video is unavailable.

## Out of scope (explicitly)

- The overlay sink (deferred — open question 5).
- Log rotation / multiple session history.
- Per-category channels and filtering as a call-site concept.
- Async/queued logging.
- Replacing the splash screen.
- Migrating the 46 existing `SDL_Log` sites to `Log_*` (they flow through
  the fan-out automatically once the output function is installed; explicit
  call-site migration, if ever wanted, is a separate PR).
- A per-line color attribute channel for the console (keeps console monochrome).
- Auto-detection of game personality (LBA1 vs LBA2) for exit quotes — this
  project is LBA2-specific.
