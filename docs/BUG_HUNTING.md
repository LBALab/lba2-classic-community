# Bug hunting runbook

How to go looking for defects nobody has reported yet. Companion to
[CRASH_INVESTIGATION.md](CRASH_INVESTIGATION.md), which is the process once you already have a
crash; this doc is the process for producing one.

Truth hierarchy: code > this document > external sources.

The short version: drive the engine the way a player does, run it under sanitizers that can
actually see the engine's memory, and distrust every green result until you have made it go red on
purpose.

## The rig

Three parts, and the third is the one people miss.

1. **The control socket.** `--listen <port>` plus `scripts/dev/lba2ctl.py` drives a live engine
   from a script. See [CONTROL.md](CONTROL.md). Send a command, let frames run, then read state.
   A command per PRESENT is the trap: the engine needs to actually tick between actions.

2. **ASan and UBSan.** Configure with
   `-fsanitize=address,undefined -fno-sanitize=alignment -fno-omit-frame-pointer -g`.
   Turn the alignment check off. Misaligned access is endemic by design here (packed Adeline data,
   `U16 *` casts on odd offsets) and floods roughly 160 reports before the title screen, which
   buries everything else. It still matters for the arm64 builds, as its own exercise.

3. **Un-merge the arena.** `InitMainBuffer()` in SOURCES/MEM.CPP hands eleven buffers out of a
   single `Malloc`. A spill from one region into its neighbour never leaves that allocation, so
   **no sanitizer will ever report it**. Set `LBA2_DBG_ISOLATE=1` and each region gets its own
   allocation, which turns the gaps into real redzones and names the writer. Off by default, so
   shipping layout is unchanged. It preserves the deliberate aliases (`BufCube` and `BufferBrick`
   live inside `PtrZBuffer`) and leaves a little slack per region, because the original ASM
   touches one element past a box edge on purpose and that known quirk would otherwise mask the
   spill you are hunting.

Neighbour order matters when reading a symptom. `TabBlock` sits immediately after `BufMap`, so a
map load that overruns lands on block data.

## Drive it like a player

The deterministic sweeps in `tests/automation/` walk one axis at a time, which makes them good at
pinning a known fault and bad at finding new ones. Players do not do that. They open the holomap
mid-scene, change resolution from the options menu, pick something up, and wander into another
cube, each against whatever state the last thing left behind.

Randomise over that: scene changes, UI modals, runtime resolution, input, item pickups, behaviour
changes, teleports, jingles, screenshots. Weight it toward the transitions that mix state.

**Seed the generator and print the seed.** A seed replay is a better A/B than any repro sequence
you derive by hand, and it costs nothing. Twice during this engine's hunt a hand-built sequence
failed to reproduce something a seed replayed on demand.

**Check how deep your runs actually got.** Any modal that waits for input will hang a headless
run, the socket times out, and the driver logs a "death" a few actions in. Runs nominally forty
actions deep were exploring about five, and the waves looked productive the whole time. `skipmodals`
handles the dialogue and found-object paths; if you add a new modal, extend it.

**Match the configuration to what people run.** Resolutions below 640x480 are catalogued but
essentially unused, and a driver that spends a third of its budget there produces real bugs nobody
will ever hit. Classic, widescreen and HD are where the risk is.

## The driver

`scripts/dev/probe_fuzzy_hunt.py` is the harness the above describes. One engine per run over
`--listen`, a randomised sequence of player-shaped actions, and the log of any run that dies or
reports:

```
export LBA2_BIN=./build-san/SOURCES/lba2cc
export LBA2_GAME_DIR=/path/to/retail
export LBA2_SAVE=/path/to/save/Spaceship.LBA
python3 scripts/dev/probe_fuzzy_hunt.py 8 45 700     # runs, actions per run, first seed
```

Build the binary with both sanitizers and run with `LBA2_DBG_ISOLATE=1`, or the arena hides the
interesting half of what it finds.

Everything is seeded from the run index, so a finding replays exactly:

```
python3 scripts/dev/probe_fuzzy_hunt.py 1 45 704     # just seed 704, same actions
```

That replay is the A/B. Point `LBA2_BIN` at a pre-fix binary and it reproduces; point it at the
fixed one and it goes quiet. Keep a copy of the binary you were hunting with, since rebuilding
under a running sweep will break it.

Two knobs worth changing per campaign: `RESOLUTIONS`, which should hold the modes people actually
run, and the weights in `actions_for()`, which decide where the budget goes.

## Oracle discipline

This is the part that decides whether the campaign is worth anything. Every check must be shown
capable of failing before its silence means a thing. Concretely, from this engine:

- **A validator that reports nothing.** Poison one entry on purpose behind a debug env var and
  confirm it fires. A load-time check over the cube map read clean on every cube, and only a
  poison probe made "clean" evidence rather than an assumption.

- **A comparison that cannot see the thing.** An ASM-versus-C test compared framebuffers while the
  fixture filled the z-buffer with `0xFFFF` and fogged to a constant colour, so depth had no
  observable effect. It passed happily while one side was fed depths off by fifty million. Compare
  the z-buffer too, seeded with a gradient.

- **A control that is not one.** "Small" inputs have to be sized from the arithmetic, not from
  intuition. Depths of +-32768 look small, but `Line_ZBuffer` shifts them to 16.16, so the clip
  product exceeds 32 bits before any multiply. That control diverged as hard as the real case and
  briefly read as evidence against the hypothesis. The real boundary was +-100.

- **A capture that is not deterministic.** `ui` screens animate. Any pixel comparison needs
  `--fixed-dt 16`, or you are diffing noise. Verify by running the *same* binary twice before you
  compare two binaries.

- **A grep that finds sites but proves nothing.** Both misses in the 64-bit multiply sweep sat
  beside code that already knew the hazard, one of them two lines under a previous fix for the same
  function, with a test whose reference model was correct and whose inputs were too small to
  overflow.

The habit that catches all of these: after any green result, ask what would have to be broken for
this to go red, then break it.

## From a report to a cause

**Bisect the trigger before diagnosing.** A fault that arrived after two resolution switches and a
UI modal looked like a widescreen bug and was written up as one. The resolution switches were
irrelevant: a `teleport` alone reproduced it. Reduce to the minimal command sequence first, then
explain it.

**Separate live from latent.** Both are worth fixing, but not with the same urgency, and saying
which is which keeps a report honest. A 16.16 depth delta overflows at ordinary values; a sphere
radius needs two million model units against a focal of 600 and never happens.

**Watch for the observer.** A per-frame checksum over 200 KB perturbs timing enough that a
one-in-three intermittent fault stops appearing. A clean run under heavy instrumentation is not the
same as a fixed bug.

## Before calling it a port bug

Every `.CPP` under LIB386 has an `.ASM` twin. Read it before concluding the C is wrong, because
sometimes the C is faithful and the original is what looks odd:

- `BOXZBUF.ASM` ends its pixel loop with `inc` / `jle`, so it deliberately processes one element
  past the box edge. ASan flags it; it is original behaviour and stays.
- `POLYLINE.ASM` multiplies with one-operand `imul`, whose product lands in EDX:EAX, and divides
  the full 64 bits with `idiv`. The C did it in 32-bit `int`. That one is a genuine mistranslation.

That second case is a class worth checking on any new translation: **one-operand `imul` or `mul`
feeding an `idiv` or `div` is 64-bit arithmetic that C's `S32 a * b / c` cannot express.** The
two-operand form does truncate to 32 bits and maps to `*` correctly, so the comma is the tell.
Correct translation is `(S32)((S64)a * b / c)`. All 95 such sites in the tree have been swept, but
new ports should be checked against it.

## Pinning it

Bug fixes land with a regression test; see
[CRASH_INVESTIGATION.md](CRASH_INVESTIGATION.md#after-the-fix-pin-it) for the mechanics and
[TESTING.md](TESTING.md) for the harnesses.

Two things worth adding when the bug was a numeric boundary:

- **Grade the test.** Sweeping depth magnitudes with the divergence count printed per band turns a
  red test into a diagnosis: it says which side of the boundary broke, not just that something did.
- **Prove the fix is a no-op where it should be.** A bounds guard that also changes supported
  configurations is a regression wearing a fix's clothing. Capture the affected screens before and
  after with `--fixed-dt` and confirm they are byte identical.

## Related files

- [CRASH_INVESTIGATION.md](CRASH_INVESTIGATION.md): process once a crash exists
- [CONTROL.md](CONTROL.md): the control socket and `lba2ctl.py`
- [DEBUG.md](DEBUG.md): debug keys, bug saves, cheat codes
- [TESTING.md](TESTING.md): test harnesses, ASM-vs-CPP builds, polyrec
- [PLATFORM.md](PLATFORM.md): the hazard classes this port keeps hitting
