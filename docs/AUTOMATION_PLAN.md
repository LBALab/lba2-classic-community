# Automation control surface — plan

> **Status: implemented in PR #162** as the `CONTROL` module. This document is the design
> trail; for usage see [CONTROL.md](CONTROL.md), and see "As-built notes" at the end for
> where the implementation diverged from this plan.

Phase 2 design, built on `AUTOMATION_RESEARCH.md`. Read the research doc first; this plan
assumes its findings (console `Console_Execute`, the `InitGame(-1)`→`ChangeCube()` load
sequence, `SavePNG`, the wall-clock timer, the monolithic `MainLoop`).

Target CLI (unchanged from the spec):

```
lba2 --load <slot> --exec "<cmd>;<cmd>" --tick <N> --dump-state <path> --screenshot <path> --exit
```

All flags optional and composable. With none, the game launches normally.

> Decisions settled in review: **no build flag** (always compiled, like the console);
> **no command-system refactor** (the console already is the command bus);
> **module named `CONTROL`** (the thing is a control harness, not just one use case);
> `vars` dumped **sparse**; `log` lines **included** (needs a small console getter);
> `--exec` uses the **existing console vocabulary only**; **`--screenshot` forces a
> minimum of 1 tick**. The research/plan doc filenames keep the "automation" name as
> process artifacts; the code and user doc use "control".

---

## Design framing

There are **three capabilities**, only two of which are new code:

| Capability | Status | New primitive |
|---|---|---|
| **Drive commands** | exists — `Console_Execute` is UI-independent (host tests call it headless) | — |
| **Observe state** | new | `Control_DumpState` |
| **Sequence boot + advance time** | new | boot branch + tick driver in one `MainLoop` hook |

The framing that keeps this elegant: **the console module *is* the command bus.** The
interactive overlay and `--exec` are two peer drivers of the same `Console_Execute`. No
extraction, no rename of `console` — the only change there is documenting that the bus is
UI-independent. The genuinely new, reusable primitives are *state observation* and the
*tick driver*; we write them as clean standalone functions so a future REPL/IPC/Lua
frontend (explicitly out of scope now) can reuse them without a rewrite. The CLI is their
first consumer.

### One code path, through `MainLoop`

The earlier draft proposed doing the load/exec/dump "outside the loop." On reflection that
splits into two code paths and re-enters `MainLoop` in an untested state. Cleaner: **route
everything through the loop the way the game already does it.**

- `--load`: `InitGame(-1)` calls `LoadGameNumCube`, which sets `NewCube` from the save
  (`SAVEGAME.CPP:646`). We then enter `MainLoop` with `NewCube != -1`, and the loop's own
  first-iteration `ChangeCube()` (`PERSO.CPP:494`/`539`) loads the scene — exactly the
  engine's designed entry condition. No manual `ChangeCube`, no second path.
- No `--load`: `InitGame(1)` (fresh game, `NewCube = 0`); the loop loads cube 0.
- `--exec`, `--tick`, `--dump-state`, `--screenshot` are all serviced by **one** hook at
  the top of the loop body. The hook is the single engine edit.

This means the only intrusions into game code are: the arg-parse + boot branch in `main()`,
and one `Control_TickHook()` call in `MainLoop`. The hook is a cheap no-op (one flag test)
whenever the harness isn't armed, so it costs nothing in normal play.

---

## Files to add / modify

**Add**

| File | Purpose |
|---|---|
| `SOURCES/CONTROL.H` | Public API (C linkage): parse, arm, tick hook. |
| `SOURCES/CONTROL.CPP` | Implementation: arg parse, boot/tick driver, exec batch, JSON dump, screenshot. Always compiled. |
| `docs/CONTROL.md` | User-facing doc (driving the engine; flag reference; determinism caveats). Add a row to `docs/README.md`. |
| `tests/automation/run.sh` | Shared helper: locate binary + data + a known save, run with flags, capture stdout/artifacts. |
| `tests/automation/test_*.sh` | One script per flag; assert on output. Double as usage docs. |

**Modify**

| File | Change |
|---|---|
| `SOURCES/CMakeLists.txt` | Add `CONTROL.CPP` to `SOURCES_FILES` (unconditional). |
| `SOURCES/PERSO.CPP` | (a) `Control_ParseArgs(&argc, argv)` next to `parse_save_load_test_flag` (`:2285`); (b) boot branch before `MainGameMenu` (`:2737`) when active; (c) `Control_TickHook()` at top of the `MainLoop` body (~`:570`, after the cube-change block, before `MyGetInput`). |
| `SOURCES/CONSOLE/CONSOLE.H` + `.CPP` | Add `int Console_GetScrollback(const char **out, int max)` to expose the last N scrollback lines for the `log` dump field. |

No build flag, no game-logic changes, no new dependencies, JSON hand-written.

---

## New function signatures

`SOURCES/CONTROL.H` (all `extern "C"`):

```c
/* Parse + strip control-harness flags from argv. Call early in main(), mirroring
   parse_save_load_test_flag: recognised flags and their values are removed so
   downstream positional parsing is unaffected. Stores config in module statics. */
void Control_ParseArgs(int *argc, char *argv[]);

/* Non-zero if any harness flag was supplied this run. */
int  Control_IsActive(void);

/* Set up the boot path and arm the tick hook. Called once before MainLoop() when
   active: applies --load (InitGame(-1)) or a fresh InitGame(1), records the exec
   batch / tick budget / output paths. */
void Control_Begin(void);

/* Single seam invoked at the top of the MainLoop body. No-op (one flag test) until
   armed. On the first tick it runs the --exec batch (scene loaded by then via the
   loop's ChangeCube); after N ticks it finalizes (--dump-state, --screenshot) and,
   if --exit, terminates via TheEnd(). */
void Control_TickHook(void);
```

Module-internal (`static` in `CONTROL.CPP`):

```c
static void control_run_exec(void);                 /* RegisterAll once; split --exec on ';'; Console_Execute each */
static void control_finalize(void);                  /* dump + screenshot + (optional) TheEnd(PROGRAM_OK) */
static void control_dump_state(const char *path);
static void control_screenshot(const char *path);    /* SavePNG(path, Screen, ModeDesiredX, ModeDesiredY, PtrPal) */
```

### Tick-hook semantics

At hook entry let `c` = number of times the hook has run this session. Ticks *completed*
before this entry = `c - 1` (each prior iteration ran one full simulate+render):

```
if (!armed) return;
if (c == 1) control_run_exec();        /* scene loaded; ChangeCube ran above at PERSO.CPP:539 */
if ((c - 1) >= effective_N) control_finalize();
```

- `effective_N = max(requested_tick, screenshot ? 1 : 0)`. A render-bearing artifact needs
  at least one completed `AffScene` (research §3); when we bump from 0 we print a one-line
  stderr note rather than doing it silently.
- `finalize` reads `Screen`/globals: the previous iteration already ran `AffScene` +
  present, so `Screen` holds tick-N's frame and globals hold tick-N's simulation result.
- If `--exit` is unset, `finalize` disarms and returns — the game continues interactively
  (composable: `--load X --tick 30` drops you into a settled scene, no exit).
- `ChangeCube()` resets `NewCube = -1` itself (`OBJECT.CPP:1313`), so the loop loads the
  scene once and doesn't reload.

### Boot branch in `main()`

```c
if (Control_IsActive()) {
    Control_Begin();    /* --load => InitGame(-1) (NewCube from save); else InitGame(1). Arms hook. */
    MainLoop();         /* loop's ChangeCube loads the scene; hook drives exec/tick/finalize */
    TheEnd(PROGRAM_OK, "");
    return 0;
}
MainGameMenu(load);
```

**Verify during implementation:** `FirstLoop` is set `TRUE` only inside the loop's
`NewCube` block (`PERSO.CPP:542`); confirm the first hooked tick gates `PtrDoAnim`
(`:1549`) as intended. The no-`--load` fresh-start path through `InitGame(1)` + `MainLoop`
is less-trodden than the save path — confirm cube 0 loads cleanly.

---

## JSON schema for `--dump-state`

Hand-written, one object, schema-versioned. All numeric fields are integers (positions and
angles are `S32`/`S16`), so no float-format/locale concerns.

```json
{
  "schema": 1,
  "tick": 30,
  "timer_ref_hr": 41234,
  "fps": 60,
  "scene": {
    "island": 0, "cube": 5, "cube_mode": "exterior",
    "num_objects": 42, "num_zones": 11, "num_tracks": 7, "num_patches": 3
  },
  "hero": {
    "x": 122880, "y": 6144, "z": 98304,
    "alpha": 0, "beta": 1024, "gamma": 0,
    "life": 50, "behaviour": 0, "weapon": 0,
    "body": 0, "anim": 12, "gen_anim": 1, "last_frame": 4, "nb_frames": 20,
    "move": 0, "zone_sce": -1, "flags": 32, "work_flags": 0
  },
  "inventory": {
    "magic_level": 0, "magic_point": 0,
    "gold": 0, "zlitos": 0, "keys": 0, "clover_boxes": 2
  },
  "actors": [
    { "index": 0, "x": 122880, "y": 6144, "z": 98304, "beta": 1024,
      "life": 50, "body": 0, "anim": 12, "move": 0, "flags": 32 }
  ],
  "vars": { "count": 256, "nonzero": { "12": 1, "47": 3 } },
  "log": [ "Cube 5 (change on next frame)", "Gave magicball" ]
}
```

Field sources (all globals, research §6): `tick` = hook counter;
`timer_ref_hr`/`fps` = `TimerRefHR`/`NbFramePerSecond`; `scene` = `Island`, `NumCube`,
`CubeMode` (`0`=interior / `1`=exterior, `COMMON.H:214`), `NbObjets`, `NbZones`,
`NbBrickTrack`, `NbPatches`; `hero` = `ListObjet[NUM_PERSO]` fields plus globals
`Comportement`/`Weapon`; `inventory` = `MagicLevel`/`MagicPoint`/`NbGoldPieces`/
`NbZlitosPieces`/`NbLittleKeys`/`NbCloverBox`; `actors` = loop over `ListObjet[0..NbObjets-1]`
(hero is also `actors[0]`; the `hero` block is convenience); `vars` = `ListVarGame[0..255]`
(`MAX_VARS_GAME`), **sparse**; `log` = last N console scrollback lines via the new
`Console_GetScrollback` getter.

`vars` dumps what the save format persists — research §6 flagged that per-actor scratch
(`Info`/`Info1..3`) may exist outside `ListVarGame`; that's a defensible v1 baseline, not
necessarily the complete script state.

---

## Order of work (one linear sequence; delivered as 2 PRs)

Each commit builds (`make build`); from commit 3 on, each adds a `tests/automation` script.

1. **Scaffold + inactive hook.** Add `CONTROL.{H,CPP}`, wire `CONTROL.CPP` into CMake, add
   the `Control_ParseArgs` call and the `Control_TickHook()` call site (no-op until armed),
   and the boot branch (inactive). Proves a clean build and zero behaviour change with no
   flags.
2. **`--load` + `--exit`.** `Control_Begin` does `InitGame(-1)`/`InitGame(1)`; the hook
   finalizes + exits. Test: `--load <save> --exit` returns 0 and prints a parseable line.
3. **`--exec`.** `control_run_exec`: `Console_RegisterAll()` once, split on `;`,
   `Console_Execute` each segment on the first tick; tap `Console_SetLineSinkForTests` to
   mirror output to stdout. Test: `--exec "status" --exit`, grep the island/cube line.
4. **`--tick N`.** Budget + finalize-after-N in the hook. Test: `--tick 10 --exit`.

   — **suggested PR boundary** (PR1 = the "drive" half: load, run commands, advance, exit) —

5. **`--dump-state`.** `control_dump_state` writes the JSON above; add
   `Console_GetScrollback`. Test: `--load X --tick 5 --dump-state out.json --exit`, parse
   JSON, assert `scene.cube`/`hero` are present and sane.
6. **`--screenshot`.** `control_screenshot` via `SavePNG`; enforce `effective_N >= 1`.
   Test: assert the file exists and starts with the PNG magic bytes.
7. **Docs.** `docs/CONTROL.md` + `docs/README.md` row; cross-link from `CONSOLE.md` and
   `TESTING.md`.

PR2 = commits 5–7 (the "observe" half + docs). The split is a suggestion; finalise it at PR
time.

---

## Pushback / caveats carried forward

1. **One engine hook is unavoidable.** A per-tick driver needs a per-iteration seam, and
   `MainLoop` is one monolithic `while(TRUE)` with `goto`s — no step function to wrap from
   outside. The honest minimum is one `Control_TickHook()` call that no-ops until armed.
   No game logic branches on "am I driven"; the harness owns that state.
2. **Determinism can't be promised.** The sim is wall-clock driven and `srand(TimerRefHR)`
   reseeds per cube (research §7). Run the harness (and tests) with `-DSOUND_BACKEND=null`
   and assert on **discrete/structural** fields (cube, island, life, inventory, object
   count, body/anim) — not exact coordinates — until a future deterministic mode pins the
   timer + seed (out of scope; uses existing `LockTimer`/`SetTimerHR` hooks). Document this
   in `docs/CONTROL.md`.
3. **Screenshot/dump need ≥1 rendered tick** (first-frame hazard, research §3); hence the
   `effective_N >= 1` rule when an artifact is requested.
4. **`--exec` is bounded by the 23 console commands** — no arbitrary var-poke or teleport
   in v1. If the agentic loop needs them, that's 1–2 small new console commands
   (`setvar`, `tp`) in a later pass; `CONSOLE_MAX_CMDS == 32` (23 used) leaves room.
5. **No Xbox gate now.** No GDK build exists in-tree, so there's nothing to gate against.
   The only platform-sensitive surface is the file writes (`SavePNG`, JSON `fopen`); when
   an Xbox build appears, guard those two — not the whole feature.
6. **Tests can't run in host-only CI.** The harness must boot the real engine (needs retail
   `lba2.hqr`, and a save for `--load`), absent from the repo and from `make test`. The
   `tests/automation/*.sh` scripts gate on data presence and skip cleanly when absent (like
   the Docker ASM tests need Docker). They're runnable usage docs locally, not CI gates.

---

## Test plan

Local-only scripts with real game data. Guiding principle: a test that only checks
`tick == N` is hollow — the bar is **observable state mutated by the loop body**, verified
**differentially**. Null audio, hard timeouts, discrete-vs-relational assertions.

**Harness `tests/automation/run.sh`** resolves and gates:
- Binary: `$LBA2_BIN` → `build/SOURCES/lba2` → newest `out/build/*/SOURCES/lba2`.
- Game data: `$LBA2_GAME_DIR` passed through; **skip with a message if unset**.
- Save fixture: `$LBA2_TEST_SAVE` for `--load` tests only; skip those if unset.
- Audio: `SOUND_BACKEND=null` to drop the audio thread + sample-timed script branches.
- Display: needs a real window/renderer (no dummy driver yet — anti-goal); WSLg/desktop ok.
- `timeout 60` around every run so a hang fails instead of blocking.
- JSON assertions via inline `python3`; fail loudly on malformed JSON.

Most tests need no save: `--exec "cube N"` from a fresh `InitGame(1)` jumps to a known
cube, so the data dependency for the bulk of tests is just the HQR.

**Keystone (differential — does the loop actually step?):**

```
run --exec "cube 5" --tick 1  --dump-state a.json --exit
run --exec "cube 5" --tick 60 --dump-state b.json --exit
```
- `a.scene.cube == 5 && b.scene.cube == 5` (exec took effect)
- `a.tick == 1 && b.tick == 60` (hook is *inside* the loop body → counts real iterations)
- `b.timer_ref_hr > a.timer_ref_hr` (`ManageTime`, after the hook, ran more → body executed)
- `b.hero` animation advanced vs `a.hero` (`gen_anim`/`last_frame` differ — sim step ran).

First three are hard asserts; the anim check is corroborating (soft if an idle pose is noisy).

**Per-flag (one per commit):**

| # | Command | Hard assertion | Proves |
|---|---|---|---|
| T1 | `--tick 1 --exit` | exit 0, terminates < timeout | boot→loop→exit, no hang |
| T2 | `--exec "cube 5" --tick 3 --dump-state s.json --exit` | `scene.cube == 5` | exec mutates → dump reflects |
| T3 | *(differential above)* | tick/timer/anim deltas | **the loop steps the sim** |
| T4 | `--load $SAVE --tick 2 --dump-state s.json --exit` | `cube`/`island`/inventory match save | `--load` restores |
| T5 | `--load $SAVE --exec "cube 9" --tick 5 --dump-state s.json --exit` | `scene.cube == 9` | exec runs *after* load |
| T6 | `--exec "give magicball" --tick 2 --dump-state s.json --exit` | magicball index in `vars.nonzero` | non-cube command works |
| T7 | `--exec "cube 5; give magicball" --tick 2 --dump-state s.json --exit` | both effects present | `;` batch splitting |
| T8 | `--exec "cube 5" --tick 2 --screenshot shot.png --exit` | PNG magic + size > floor; *best-effort:* not uniform color | render + read-back, effective_N≥1 |
| T9 | any dump | parses, `schema==1`, required keys | hand-written JSON well-formed |

**Negative/robustness:** bad `--load` path → graceful non-crash exit; `--tick 0
--dump-state` → post-load state, `tick == 0`; unknown exec command → no crash, dump still
written; no-flags launch → normal menu (manual smoke, can't auto-verify to completion).

**Determinism discipline:** exact asserts only on discrete fields (cube, island, life,
inventory counts, var indices, exit code, PNG magic); relational/monotonic on time/anim
fields (`b > a`, "changed"), never exact coordinates.

T1+T2+T3+T8 together exercise the full path — parse → boot → load scene → run commands →
step N ticks of real sim+render → observe → clean exit.

---

## As-built notes (PR #162)

Where the implementation differs from the plan above. The user guide
[CONTROL.md](CONTROL.md) is canonical for behaviour.

- **Load does its own `ChangeCube`, not via `MainLoop`'s `NewCube`.** The plan's "one code
  path through `MainLoop`" framing was wrong on one detail: `MainLoop` clears `FlagLoadGame`
  at its top (`PERSO.CPP:440`) *before* its first-iteration `ChangeCube`, so letting the
  loop load the scene would not restore the saved hero position. `Control_Begin` therefore
  runs `InitGame(-1)` then `ChangeCube()` itself (matching the menu's `load_game` path,
  `GAMEMENU.CPP:3571`), then enters `MainLoop` with `NewCube == -1`. The fresh-start path
  does keep the plan's approach (`InitGame(1)`; the loop's first iteration loads cube 0).
- **Boot logos are skipped** when the harness is active (`!Control_IsActive()` added to the
  logo gate at `PERSO.CPP:2471`), the same skip `--save-load-test` already uses. Not
  anticipated in the plan; without it the run hangs on the distributor/Adeline logos.
- **`Control_Begin` returns `int`** (1 ok / 0 failure) rather than `void`; `main()` skips
  `MainLoop` on failure instead of relying on `TheEnd` being noreturn. (Review hardening.)
- **A tick is one `MainLoop` iteration, not strictly one frame.** The two `goto startloop`
  sites (`PERSO.CPP:1608`, `:1629`) jump above the hook, so a script-driven cube transition
  advances the counter an extra time. Exact for advancing within a settled scene (the common
  case); over-counts by the number of cube changes when a run crosses scenes. Documented.
- **Modal commands hang a `--exit` run** (`give <item>` found-object cinematic, `playvideo`,
  `credits`, `slide`) — they wait for input. Discovered in testing; documented. Tests use
  the non-modal `give clover`.
- **Console additions:** both `Console_EnsureRegistered` (the plan flagged the double-
  registration risk) and `Console_GetScrollback` (the `log` field) landed; `Console_Toggle`
  was refactored onto `Console_EnsureRegistered`.
- **Commits:** delivered as one branch / PR (research+plan docs, the module + console hooks,
  the test scripts, the user doc, then a review-fixups commit) rather than the 7-step split
  sketched above — the per-flag *tests* are the artifacts that matter and they all landed.
- **`timer_ref_hr` is the restored save clock, not boot time** (review #6, resolved). The
  engine persists `TimerRefHR` (accumulated play time) in saves and restores it on load
  (`SAVEGAME.CPP:558`/`:845`), so a loaded game's clock starts at the save's play-time —
  it scales with progression (early save ~4 h, late ~12 h). Not double-counting; expected.
- **Deferred (noted on the PR):** capping `actors[]` for pathological scenes is left open.
  (Warn-on-non-numeric `--tick` was added.)
