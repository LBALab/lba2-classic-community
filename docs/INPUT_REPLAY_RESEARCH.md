# Input record/replay — research

Phase 1 research for harness roadmap step 3 (`docs/CONTROL.md`): capture input per tick and
replay a whole session as a gameplay-regression net, the counterpart to the draw-call polyrec.
This builds on the now-deterministic harness (`--fixed-dt`) and the fast headless loop
(vsync off under the harness).

This is findings only — no design decision, no implementation. It started from the question
"does the game already ship canned recordings we can replay?" and maps the engine's
script/playback systems to answer it. Line numbers were verified against the working tree;
re-grep before relying on an exact number.

---

## 1. The question: are there canned recordings to replay?

The hope was that LBA2 ships recorded input/demo data we could replay as ready-made fixtures.
After mapping the engine and the retail data, the answer is: **not as recorded input.** What
"comes with the game" is *script* data, not a controller-input stream. The distinction matters
for step 3, so the systems are inventoried below.

What was checked and ruled out:

- **No recorded-input subsystem.** `MyGetInput()` ([`INPUT.CPP:258`](../SOURCES/INPUT.CPP))
  reads live keyboard/joystick (`GetJoys`, `GetInput`, `ApplyGamepadBindings`) and writes the
  `Input`/`Key`/`MyKey` globals. There is no branch that sources those from a buffer or file. A
  broad grep for record/replay/playback and the French equivalents (`magneto`, `enregistre`,
  `rejou`) finds only the unrelated polygon recorder.
- **No recording files in the data.** No `.dem/.rec/.tas/.inp` in the GOG or Steam "Classic"
  installs. The `Speedrun/` folder is a DOSBox-wrapped DOS build (its `[autoexec]` just runs
  `lba2.EXE`; the bundled DOSBox is set up for *video* capture, not input).
- **`DemoSlide` is not input replay.** It is the runtime "non-playable demo" flag
  (`GLOBAL.CPP:21`, *"Mode démo non jouable"*). It makes the game non-interactive —
  `GERELIFE.CPP` skips dialogue choices (`LM_ASK_CHOICE`) under it — and lets the scene's own
  scripts run. The `#ifdef DEMO` attract *cycle* (`ListDemoSave` → `demo0/1/2.lba`, advanced by
  `NewDemoSave++`) is a sequence of **save states**, not input, and those assets are not in the
  repo or this data.

---

## 2. The scripting / playback systems that ship in the HQRs

Three distinct HQR-stored "script" systems exist; only the first drives gameplay.

**Life / Track scripts (gameplay).** The canonical Adeline scene scripting. Per-object
bytecode loaded from the scene HQR ([`DISKFUNC.CPP:60,314`](../SOURCES/DISKFUNC.CPP);
`ptrobj->PtrLife = PtrSce` at `:140,219`), executed every frame by `DoLife()`
([`GERELIFE.CPP`](../SOURCES/GERELIFE.CPP), `LM_*` opcodes) and `DoTrack()`
([`GERETRAK.CPP`](../SOURCES/GERETRAK.CPP), `TM_*` opcodes). This is what makes a scene "play
itself": NPC AI, scripted hero/actor movement along tracks, dialogue and state triggers. See
`docs/GLOSSARY.md` (Life Script / Track Script) and `docs/LIFECYCLES.md`. **This is the closest
thing to "canned recordings that come with the game"** — every retail scene carries it.

**IMPACT (effects).** [`IMPACT.CPP`](../SOURCES/IMPACT.CPP). A compiled command stream
(`SAMPLE`, `FLOW`, `THROW`, `THROW_OBJ`, `THROW_POF`, `SPRITE`, `FLOW_*`) run by
`DoImpact(num, x, y, z, owner)`, loaded from `RESS_IMPACT`
([`PERSO.CPP:2592`](../SOURCES/PERSO.CPP), buffer sized in `MEM.CPP:219`). It spawns particles,
thrown extras, and positional sounds at an impact point — the *visual-effects* layer. This is
the "impact script" Adeline used, but it plays *effects*, not gameplay or input.

**FLOW (effects).** [`FLOW.CPP:112`](../SOURCES/FLOW.CPP) loads `RESS_FLOW`; particle-flow
data referenced by IMPACT's `FLOW*` commands.

**ACF (cutscenes).** `PlayAcf()` ([`PLAYACF.CPP:321`](../SOURCES/PLAYACF.CPP)) plays
pre-rendered movies — canned *cinematic* playback, not gameplay.

Net: the engine "replays" **scripts** (deterministic logic) and **movies** (pre-rendered), but
never recorded controller input.

---

## 2b. The attract demo, concretely — cube 193 and the scene format

The retail (non-`DEMO`) attract demo is a normal scene driven by scripts. `GAMEMENU.CPP:2432`
(the `#else` branch) sets `DemoSlide = TRUE`, `InitGame(1)`, `NewCube = 193`, and runs
`MainLoop()` — so "Twinsen running around" is **cube 193's scripts**, not input. (`SlideDemo()`
cycles from cube 5.) This was confirmed by inspecting the data directly.

**HQR container** (`HQFILE.CPP`, `HQR.H`): the file opens with a U32 offset table (first U32 =
offset of entry 0, so `entries = first/4`); each slot is a byte offset to a
`COMPRESSED_HEADER { u32 SizeFile; u32 CompressedSizeFile; s16 CompressMethod }` (0 stored, 1
LZSS, 2 LZMIT) followed by the data. `scripts/dev/hqr_inspect.py` enumerates and decompresses
entries (LZSS ported from `LZ.CPP`, verified: decompressed sizes match headers).

`scene.hqr` holds **224 cube slots, entry N = cube N**; cube 193 is 1022 bytes (LZSS).

**Scene resource layout** (`DISKFUNC.CPP`): an ambiance/sample/jingle header, then `HERO_START`
(`CubeStartX/Y/Z`, then the hero's **Track script** and **Life script**, each `s16`-size-prefixed),
then `NbObjets` and per object: flags, file3D index, body/anim/sprite, position, beta, `SRot`,
`Move`, `Info0..3`, bonus, colour, armour, life — then that object's **Track + Life scripts**
(each size-prefixed). Then a checksum, then zones/tracks/patches.

So the "recording" that animates the attract demo is the **per-object Track (`TM_*`) and Life
(`LM_*`) bytecode** authored into each cube. The hero object (`ListObjet[0]`) carries the Track
script that walks him through cube 193 and the Life script for his behaviour. It is higher-level
authored movement/AI, not a controller-input stream — which is exactly why it is deterministic
under `--fixed-dt` and a better regression fixture than raw input would be.

## 2c. The scripting engine, observed live

Rather than disassemble the bytecode statically, the actual interpreter was instrumented
(temporary env-gated trace `LBA2_TRACE_TRACK=1` in `DoTrack`, `GERETRAK.CPP:140`) and cube 193
run under the harness. This shows the engine model directly.

**Two per-object interpreters, run every tick** for each live object:

- **Track (`DoTrack`, `GERETRAK.CPP:140`, called per object at `PERSO.CPP:1551`)** — movement.
  Vocabulary `TM_*` (`COMMON.H:1051+`): `GOTO_POINT`, `ANGLE`, `ANIM`, `WAIT_ANIM`,
  `WAIT_NB_SECOND`, `STOP`, `SPEED`, `LABEL`/`GOTO` (loops), `SAMPLE`, doors, etc.
- **Life (`DoLife`, `GERELIFE.CPP`)** — behaviour/AI. Vocabulary `LM_*` (commands) + `LF_*`
  (conditions): variables, zones, dialogue, combat, state. Larger; not traced yet.

**Execution model** (read off the trace). Each object has a program pointer `PtrTrack` + a
program counter `OffsetTrack`. Per tick, `DoTrack` loops: read opcode, advance `OffsetTrack`,
dispatch. **Blocking** opcodes (`GOTO_POINT`, `WAIT_*`, `STOP`) end the tick and *re-execute the
same offset next tick* until their condition completes — e.g. `GOTO_POINT` re-runs each tick
while the actor walks toward the waypoint, then advances. **Non-blocking** opcodes (`LABEL`,
`ANIM`, `ANGLE`, `SPEED`, `REM`) fall through and the loop keeps executing within the same tick.
That is why a 60-tick run showed `GOTO_POINT` 59× at one offset (still walking).

**Cube 193's hero program** (`ListObjet[0]`, traced in execution order, per-tick repeats
collapsed):

```
LABEL  ANIM  GOTO_POINT GOTO_POINT GOTO_POINT  REM
LABEL  STOP
LABEL  WAIT_ANIM ANIM  WAIT_NB_SECOND  REM  ANIM ANGLE ANIM  REM
       GOTO_POINT GOTO_POINT GOTO_POINT  REM  ANIM ANGLE ANIM
LABEL  STOP
```

So the attract "recording" is an authored movement program: set an animation, walk a chain of
waypoints, stop; wait, face an angle, walk another chain, stop. This is exactly what drives
"Twinsen running around" — higher-level than controller input. The harness reaches it with
`--exec "cube 193"`, and the hero follows his track without `DemoSlide` because the scene sets
his `Move` to track-following.

Two caveats observed running it:

- **The hero's scripted path is deterministic** (identical across 3 runs at 200 ticks), but a
  *fresh-start* cube-jump (`--exec "cube N"`, no `--load`) does not pin `timer_ref_hr` as cleanly
  as `--load` — a ~3 ms boot-timing residual remains and jitters the moving NPCs by ±1 unit. For
  fully deterministic runs use `--load` (which restores a fixed save clock), as the corpus
  baselines do. A scripted-playthrough fixture should therefore start from a save, not a bare
  cube jump.
- **A long run hangs without `DemoSlide`.** Past the walking phase, cube 193's Life script hits
  an interactive dialogue/cinematic that waits for input, hanging a `--exit` run (the documented
  modal caveat). The real attract framework sets `DemoSlide = TRUE` so those auto-advance on
  `TimerRefHR`; a headless playthrough of demo content needs the same.

Tooling for this lives on the branch: `scripts/dev/hqr_inspect.py` (container + LZSS) and the
`LBA2_TRACE_TRACK` trace in `DoTrack` (temporary; useful enough to keep env-gated, or drop).

## 2d. The Desert Island bat — load-sensitive RNG, the real residual nondeterminism

A known case: on Desert Island a bat sometimes hits the hero and disrupts the scripted path,
so the playthrough "isn't truly deterministic." Tracing it localised the cause precisely and,
unexpectedly, reframed the determinism work.

Setup: load `Desert Island.LBA` (island 2, cube 65, 16 objects), run 400 fixed-dt ticks, diff.

Observations:

- **Only actor 11 (`body=181` — the bat) diverges**; the other 15 objects are byte-identical.
- **The game clock is perfectly pinned** — `timer_ref_hr` is identical across every run (incl.
  the divergent ones). So timing is *not* the cause.
- **The bat's track uses `TM_WAIT_NB_SECOND_RND`** (`GERETRAK.CPP`), a random wait
  (`MyRnd → Rnd(n) = rand()%n`, `LIB386/H/SYSTEM.H:59`; single seed `srand(TimerRefHR)`,
  `OBJECT.CPP:1171`). Its *first* random draw is identical across runs, so the divergence is a
  *later* RNG-driven decision, not the seed.
- **It is load-sensitive.** Run serially, six runs were byte-identical (`bat.x=6717`). Run 8×
  *in parallel*, the result went **bimodal** — `bat.x` was 7174 in six and 6717 in two, clock
  still identical. CPU contention flips an RNG-consuming decision in the bat's logic.

Mechanism (best supported): the per-tick path consumes `rand()` outside the pinned clock (the
ambiance/sample system calls `MyRnd` heavily, and some of that is gated on wall-clock/threaded
state). Under load the number of `rand()` calls before the bat's decision drifts by one, the
bat flies slightly differently, and *sometimes* reaches the hero — interrupting his track. So
the residual nondeterminism is **`rand()`-call-sequence drift under load**, not the clock.

Two important implications:

1. **This is the same source as the ~9 FLAKY corpus saves** — they all have RNG-driven actors.
   `--fixed-dt` pins the clock; it does not pin the `rand()` *call sequence* against load/timing
   perturbation.
2. **The `--jobs` parallel harness can *induce* this flakiness** (it adds exactly the CPU
   contention that flips the decision). A save can look deterministic serially and FLAKY in
   parallel. The self-calibrating reproducibility check (PR #167) is therefore partly measuring
   load sensitivity, not just intrinsic determinism — worth keeping in mind when reading FLAKY
   counts, and an argument for a serial confirmation pass before quarantining.

True determinism for RNG-driven scenes would require pinning the `rand()` stream itself —
e.g. a deterministic per-tick reseed, or ensuring the call order is independent of wall-clock
and thread scheduling. That is the concrete next lever beyond `--fixed-dt`, and a prerequisite
for golden-exact long playthroughs.

## 3. Implication for step 3 — two complementary paths

Given there is no recorded-input data to replay, step 3 splits into two things that should not
be conflated:

### (a) Script-driven deterministic playback — uses what ships, no recording needed

The scene Life/Track scripts already drive everything. Under `--fixed-dt` (deterministic clock)
and vsync-off (fast loop), running a save's scripts forward for many ticks is a reproducible
"the game plays its own content" run. Empirically (this research): three corpus saves each ran
2000 fixed-dt ticks with no crash/hang, deterministically, in ~2.5–3 s with vsync off. So a
**script soak / playthrough regression test** is feasible today with zero new engine code — it
extends the existing `--dump-state` baseline from an 8-tick settled scene to a long scripted
run, exercising AI, tracks, animation, scene transitions, and effects.

Caveat: a long run is more likely to touch the residual non-dt nondeterminism already catalogued
(`project`/`docs/FIXED_DT_PLAN.md`: dying-enemy spin `beta`, uninitialised bodiless-extra
positions — the ~9 FLAKY corpus saves), so frame it as crash/soak + deterministic-where-it-is,
not long golden-exact, until that source is fixed.

### (b) Input record/replay — the literal roadmap item, built from scratch

What the roadmap names: capture the *player's* resolved per-tick input and replay it. The seam
already exists — `Control_TickHook()` ([`CONTROL.CPP`](../SOURCES/CONTROL.CPP), at
`PERSO.CPP:551`, before `MyGetInput()`/`ManageTime()` at `:577-578`). Sketch:

- **Record:** after `MyGetInput()` each tick, append the resolved input state
  (`Input`, `MyKey`, and the relevant `TabKeys`/gamepad bits) to a file.
- **Replay:** at the hook (before `MyGetInput` consumes hardware), or by overriding `MyGetInput`
  under a harness flag, inject the recorded input for that tick instead of reading hardware.

This is small because the engine already funnels input through `MyGetInput`, and faithful
because `--fixed-dt` makes the tick→state mapping deterministic. It is the right tool for
reproducing *player-driven* bugs (a specific button sequence), which scripts cannot express.

Recommended framing: (a) is the high-value, ships-now regression net the scripts hand us; (b) is
the genuinely new capability the roadmap asks for, and the determinism work was its prerequisite.

---

## 4. Open questions / next exploration

- **Exact input state to capture.** Is `Input` + `MyKey` sufficient to reproduce a tick, or do
  `TabKeys[]` / per-binding gamepad bits matter for some code paths? Audit what gameplay reads
  beyond `Input`/`MyKey`.
- **Replay injection point.** Hook-injection (pre-`MyGetInput`) vs a guarded early-return inside
  `MyGetInput` — which is cleaner and avoids double-reading hardware events. Tie into the
  existing console-suppression / `WaitNoKey` logic in `MyGetInput`.
- **File format.** A tiny per-tick binary (tick index + input words), versioned like
  `--dump-state` (schema field). Should it embed the originating save + `--fixed-dt` step so a
  replay is self-describing?
- **Modal interactions.** Recorded sessions that open menus/inventory/dialogue: under
  `DemoSlide` these auto-advance on `TimerRefHR`, but normal modals wait for input — a replay
  must feed the dismiss inputs. Confirm replay drives them correctly.
- **Disassemble cube 193's hero script.** Next concrete step: parse the decompressed cube
  (`hqr_inspect.py --entry 193 --dump`) through the scene layout (§2b) to extract the hero's
  Track/Life bytecode, and disassemble it with the `TM_*`/`LM_*` opcode tables (`COMMON.H`,
  `GERETRAK.CPP`/`GERELIFE.CPP`). That reveals exactly what the "recording" instructs — the
  authored playthrough — and validates the inspector against the engine's own parse.
- **Or watch it run.** Drive cube 193 under the harness (`--exec "cube 193"`, `--fixed-dt`,
  vsync-off) with `DemoSlide` enabled and dump state across ticks to observe the scripted
  playthrough deterministically.
- **Where IMPACT/FLOW are documented.** Neither is in `docs/` yet (only Life/Track are, in
  GLOSSARY/LIFECYCLES). If we lean on script playback for testing, a short doc of the
  effects-scripting systems is worth adding.
- **Residual nondeterminism.** Step 3's golden-exact value depends on closing the ~9 FLAKY-save
  source (separate follow-up). Replay reproduces faithfully only as far as the sim is
  deterministic.

---

## Summary

- No recorded-input system or recording files ship with the game. `MyGetInput` reads live
  hardware; `DemoSlide` is script-driven, not input replay.
- The HQRs carry **scripts**: Life/Track (gameplay, `DoLife`/`DoTrack`), IMPACT/FLOW (effects,
  `DoImpact`), and ACF (cutscenes). The "impact script" is the effects layer; the gameplay
  "canned content" is the per-scene Life/Track bytecode.
- Step 3 is two things: **(a)** deterministic *script* playback as a soak/playthrough net —
  available now via `--fixed-dt` + vsync-off, no new format; and **(b)** true *input*
  record/replay built at the `Control_TickHook`/`MyGetInput` seam — small, faithful under
  fixed-dt, and the right tool for player-driven repro.
