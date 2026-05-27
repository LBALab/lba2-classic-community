# Input record/replay — research

Phase 1 research for harness roadmap step 3 (`docs/CONTROL.md`): capture input per tick and
replay a whole session as a gameplay-regression net, the counterpart to the draw-call polyrec.
This builds on the now-deterministic harness (`--fixed-dt`) and the fast headless loop
(vsync off under the harness).

This is findings only — no design decision, no implementation. It started from the question
"does the game already ship canned recordings we can replay?" and maps the engine's
script/playback systems to answer it. Line numbers were verified against the working tree;
re-grep before relying on an exact number.

**Tooling note.** The committed tools from this research are the read-only data utilities under
`scripts/dev/` (`hqr_inspect.py`, `impact_disasm.py`, `flow_dump.py`, `pof_dump.py`). The
in-engine instrumentation referenced below — the env-gated `LBA2_TRACE_TRACK` / `LBA2_TRACE_LIFE`
traces, the `LBA2_TRACE_RAND` per-tick `rand()` counter, and `LBA2_FORCE_DEMOSLIDE` — was
*temporary* and is **not committed**; the cited call sites (`DoTrack`/`DoLife` dispatch,
`Control_Begin`) let you re-add a guarded `fprintf` to reproduce any trace.

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
the "impact script" Adeline used, but it plays *effects*, not gameplay or input. The format is
fully reversed (and recompilable) — see [IMPACT_SCRIPTS.md](IMPACT_SCRIPTS.md).

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
- **Life (`DoLife`, `GERELIFE.CPP:742`)** — behaviour/AI. Vocabulary `LM_*` (148 commands) +
  `LF_*` (46 condition functions). Traced below.

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

**The Life half** (traced with `LBA2_TRACE_LIFE=1` in `DoLife`, `GERELIFE.CPP:742`; opcode
numbers mapped to names from `COMMON.H`). `DoLife` runs the object's behaviour block from
`OffsetLife` each tick. Over a 600-tick demo, ~18k Life ops were overwhelmingly **conditions** —
`LM_IF`/`LM_SWIF`/`LM_AND_IF`/`LM_OR_IF` (~13k) and `LM_END_COMPORTEMENT` (the behaviour-block
terminator) — with **actions firing rarely** on state changes (`LM_SET_TRACK`,
`LM_SET_COMPORTEMENT`, `LM_SET_VAR_GAME/CUBE`, `LM_ANIM`, `LM_BETA`, `LM_CINEMA_MODE`,
`LM_SET_CAMERA`, `LM_CAM_FOLLOW`, `LM_SUICIDE`, …).

A condition opcode calls `DoFuncLife` — evaluate one of **46 `LF_*` sensor functions** (`LF_COL`
collision, `LF_DISTANCE`, `LF_ZONE`, `LF_CONE_VIEW` line-of-sight, `LF_HIT_BY`, `LF_ACTION`
button, `LF_VAR_GAME`/`LF_VAR_CUBE`, `LF_BODY`/`LF_ANIM`, …) — then `DoTest` (compare), then
branches via a 2-byte else/endif offset (`PtrPrg = PtrLife + *(s16*)PtrPrg`) if false. So Life is
an **event-driven behaviour tree**: sensors poll game state, branches resolve, actions fire on
change.

**The two interpreters together:** Life is the brain (perceives via `LF_*`, decides, sets state),
Track is the legs (executes movement). The link is `LM_SET_TRACK`: a Life behaviour engages a
Track program, which then walks the actor (the `GOTO_POINT` chains above). The demo hero is set
up this way — Life arranges the cinema camera (`LM_CINEMA_MODE`/`SET_CAMERA`/`CAM_FOLLOW`) and
puts the hero on his track; Track does the walking. This is the whole authored-content engine,
and it is deterministic under `--fixed-dt` except where a sensor reads the residual
nondeterminism (the §2d bat: `LF_HIT_BY`/`LF_COL` firing — or not — on a load-jittered hit).

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

## 2d. The Desert Island bat — load-sensitive nondeterminism (not clock/RNG/uninit/race)

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
- **It is load-sensitive and bimodal.** Run serially, six runs were byte-identical
  (`bat.x=6717`). Run 8–10× *in parallel*, `bat.x` went bimodal (6717 or 7174), clock identical.

**The cause is not the clock and not RNG** — established by elimination with an instrumented
`rand()` counter (a global incremented in the `Rnd` macro + the two bare `rand()&1` sites,
`GERETRAK.CPP`/`OBJECT.CPP`) printed per tick:

- *Clock:* `timer_ref_hr` is identical every tick across all runs, including divergent ones.
- *RNG:* two runs with an **identical per-tick `rand()` count and identical clock the entire
  run** still ended with different `bat.x`. The bat begins to diverge at **tick 141** (when its
  `GOTO_POINT` walk starts), consuming the *same* RNG stream. So the divergence is not
  `rand()`-call-count drift or value drift. (Separately, a minor ±2 `rand()`-count wobble was
  seen during the `ChangeCube` load phase — real, but *not* what moves the bat, since runs with
  identical counts still split.)
- *Threads:* the null sound backend (`-DSOUND_BACKEND=null`, used here) creates no threads —
  all SDL audio threads/callbacks live in the SDL backend (`AIL/SDL/SAMPLE.CPP`, `STREAM.CPP`).

Then two more candidates were eliminated:

- *Uninitialised memory:* Valgrind/memcheck `--track-origins=yes` on a Debug (`-g -O0`) null
  build, load + 160 ticks, reported **0 errors / 0 uninitialised reads**. So it is not an
  uninitialised read (memcheck flags those regardless of whether they cause divergence).
- *Data race:* the process is **single-threaded** under the null backend (`ps -o nlwp` = 1; all
  SDL audio threads are SDL-backend-only). No race is possible.

So: identical clock, identical RNG stream, single thread, the same binary/platform, and no
uninitialised reads — yet the result diverges under load (and under Valgrind the bat took a
*third* path, never leaving its start by tick 160, so even its move-start is environment-
sensitive). The remaining explanation is **floating-point / `long double` precision
sensitivity**: the projection/movement math uses 80-bit x87 `long double` + `lrintl()`
(`LPROJ3DF.CPP`/`LROT3DF.CPP`, see `docs/PLATFORM.md`), and a borderline rounding can flip
depending on FPU/SSE environment state, which the execution context (load, Valgrind) perturbs.
This is consistent with the documented platform-precision behaviour, but it is **not yet
confirmed** — it is the current leading hypothesis after eliminating the others.

Implications and status:

1. **Not RNG, not the clock, not uninitialised memory, not a race** — all eliminated with
   evidence. `--fixed-dt` (clock) and a pinned RNG stream would not fix it.
2. **`--jobs` parallelism induces it**, so PR #167 FLAKY counts partly measure load sensitivity;
   a serial confirmation pass would classify more honestly.
3. **Open: confirm the FP hypothesis.** Next step is to trace the bat's actual movement-math
   inputs/outputs (`GetDeltaMove`/projection) across a divergent pair to catch the value that
   flips, and to check the FPU control word / try an SSE-only (no-`long double`) build. Only then
   is there a concrete fix.

(Corrections, kept as a record of following the evidence: an earlier commit hypothesised
"rand()-call-sequence drift" — overturned by the counter experiment (same RNG stream, divergent
outcome); a second pass concluded "uninitialised memory" — overturned by the clean Valgrind run.
The honest current state is FP-precision sensitivity, still to be confirmed.)

**Provenance: this is an original-engine bug, not a port artifact.** Per project history, the
same nondeterminism exists in the original LBA2, and the `lba2-classic-ida` fork worked around it
by **making the hero invincible**. That is a *symptom* fix, and it's the useful insight: the
divergence only matters because the bat *hits the hero and interrupts his scripted track* — a
hit knocks the hero off his `GOTO_POINT` path and the playthrough desyncs. Remove the hit
(invincibility) and the scripted demo plays through deterministically regardless of the bat's
own jitter. So the deep FP root cause can be left as a documented original quirk to hunt later;
the *practical* lever for deterministic scripted playback is to neutralise the interruption.

**Implication for the regression net (step 3).** An opt-in, harness-only "demo invincibility"
(freeze hero life / ignore hits during scripted playback, mirroring the IDA fork's approach)
would make demo/attract playthroughs deterministic enough to golden, without touching the
underlying original bug — turning bat-afflicted scenes from FLAKY into usable fixtures. That is
a concrete, low-risk path to a scripted-playthrough regression net, parallel to (and simpler
than) chasing the FP precision bug to ground.

> **RESOLVED (PR #175): the cause was RNG after all.** Adding to the corrections list above —
> the "RNG ruled out" conclusion in this section was wrong. The rand-call counter compared
> the *call sequence* across runs, which was identical, and on that basis the seed itself was
> taken as not the variable. It was. `InitTimer()` at engine boot banks the wall-clock
> interval since `SDL_GetTicks()`'s epoch into `TimerRefHR`. Before #175, `Timer_EnableFixedDt()`
> seeded `FixedDtNow`/`LastTime` correctly but left `TimerRefHR`'s pre-enable value alone, so
> `srand(TimerRefHR)` at the first `ChangeCube` saw a different seed each run on the
> fresh-start path (typically 125-470 ms of variation). Same call sequence, different seeds →
> different stream outputs → different bat trajectories. Under `--load`, the save's restored
> clock overrode `TimerRefHR` after enable, so save-loaded runs already had a stable seed —
> which is why the `Desert Island.LBA` measurement here showed "identical clock, identical
> RNG stream" yet diverged: clock + call sequence agreed across save-loaded runs, but the
> divergence-versus-no-divergence flip in 8-10× parallel batches was a different perturbation
> source (likely scheduler timing affecting some other state, not the seed itself in this
> particular setup). The simpler `--demo` fresh-start path made the seed leak visible at
> ±300 ms scale. The fix (`TimerRefHR = 0` next to the existing `FixedDtNow`/`LastTime` seeding
> in `Timer_EnableFixedDt()`) closes both the fresh-start divergence and the bat scene; cube
> 65 (the canonical Desert Island bat) and cube 196 (sister wanderers in the cube-193 attract
> reel) are byte-deterministic post-#175 across sequential and parallel runs.
>
> The FP-precision investigation chase was wasted effort, but the corrections trail above is
> kept as a record of how to *not* eliminate a hypothesis: the rand-call counter measures
> call sequences, which is one slice of "is the RNG stream identical"; the seed itself is the
> other slice and was overlooked. Future investigations of this class should print the seed
> at `srand` time, not just the call count.
>
> The "demo invincibility" idea remains useful as a *separate* concept — it would let the
> harness golden a deterministic playthrough even if the bat *did* hit the hero — but it is
> no longer needed to neutralise the original bug. The original bug is fixable at the source,
> with the same `TimerRefHR`-reset mechanism extended to `DemoSlide` rather than `--fixed-dt`
> (tracked in issue #176 for the SHIFT+D / in-game attract path).
>
> **Update (post-#249):** The "extend the `TimerRefHR`-reset to `DemoSlide`" approach above —
> shipped in PR #249 as `BeginDemoSlide()` zeroing `TimerRefHR` — turned out to be
> structurally insufficient on the SHIFT+D / idle-attract path. Without `--fixed-dt`,
> `ManageTime()` is real-time and `FadeToBlack()`'s internal `SaveTimer()` / `RestoreTimer()`
> pair runs between `BeginDemoSlide()` and the first `ChangeCube()`; `SaveTimer()`'s
> `ManageTime()` banks `SDL_GetTicks() - LastTime` back into `TimerRefHR`, and that interval
> (the wall-clock work between zeroing and the fade entry) varies by host — macOS reported
> the bat scene still diverging after #249 landed. The actual fix moves the seed source for
> `DemoSlide` *off* the clock entirely: `ChangeCube` now calls `Demo_RngSeed(DemoSlide,
> TimerRefHR, NewCube)`, which returns `(U32)NewCube` while in demo mode (canonical per cube,
> host-independent). Pinned by `tests/demo_rng_seed/`. The `TimerRefHR = 0` line in
> `BeginDemoSlide()` stays as cheap defense-in-depth for any other clock-derived state read
> during the reel.
>
> **Update (post-#249, second iteration):** Even with the canonical-per-cube seed, the
> SHIFT+D reel can still produce different outcomes per host because of *cadence*, not seed.
> Under interactive play `dt` is wall-clock-derived per frame; the same seed plus a different
> number of MyRnd() consumers per scripted scene (driven by actor-update count, which varies
> with framerate jitter) lands the bat on a different trajectory. Confirmed empirically: the
> harness path (`--demo --fixed-dt 16 --exec "cube 201"`) canonically produces a "bat misses
> Twinsen" outcome (hero life=255 throughout the reel); interactive SHIFT+D on Mac (and on
> Linux when reproduced via a save-load + console `cube 201`) hits Twinsen. The fix extends
> `BeginDemoSlide()` to also arm `Timer_EnableFixedDt(16)` when fixed-dt isn't already on
> (harness with `--fixed-dt` keeps its chosen step), giving interactive SHIFT+D the same
> canonical cadence as the harness. `EndDemoSlide()` symmetrically disarms when the user
> exits to "Resume" / "Load Game". `Timer_FixedDtAdvance()` per-tick moved from
> `Control_TickHook` to `MainLoop` so the in-engine demo path is covered too.

## 2e. Mapping the demo scenes

Where the engine's demo/attract content lives, and which is usable as a deterministic fixture
(probed via a temporary env-gated `LBA2_FORCE_DEMOSLIDE` in `Control_Begin` + the per-tick cube
trace):

- **`SlideDemo` entry = cube 5** (`GAMEMENU.CPP:4156`: `cubedeb = 5`, `DemoSlide = TRUE`). A
  self-contained demo: the hero runs a scripted path *within* cube 5 (no cube transitions in
  3000 ticks). **Fully deterministic** — 4 serial and 6 parallel runs at 1000 ticks were
  byte-identical (hero ends 30956/3584/18821). So cube 5 is a ready, load-stable,
  golden-able scripted-playthrough fixture.
- **Menu attract = cube 193** (`GAMEMENU.CPP:2432`: `DemoSlide = TRUE`, `NewCube = 193`). The
  hero follows a track program (traced in §2c), but the run **hangs after ~876 ticks** on a modal
  that `DemoSlide` does not auto-advance (a cutscene/`PLAY_ACF`-class wait). Not headless-runnable
  end-to-end without handling that modal.
- **Desert Island (cube 65)** — has the bat (§2d): an original-engine load-sensitive
  nondeterminism. Short runs are fine; long runs need the invincibility lever to stay
  deterministic.
- **Full attract cycle (`demo0/1/2.lba`)** — `#ifdef DEMO` only, and those save assets are not in
  the repo or the retail data, so the canonical multi-scene attract cycle is not reachable here.

Takeaways for the regression net: **cube 5 is an immediately usable deterministic playthrough
fixture** (a ~16 s scripted run, golden-able under load). Cube 193 needs modal handling (or a
shorter tick budget that stops before the cutscene). Bat-class scenes need the invincibility
lever. The richest demo content (the demo-build cycle) is out of reach without its assets.

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
