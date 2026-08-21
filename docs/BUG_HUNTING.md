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
capable of failing before its silence means a thing -- and **shown to fail for each thing it claims
to detect, not for the first one**. A check usually makes several claims, and breaking the obvious
one proves only the obvious one.

The specimen, from a recording fixture: an arm asserted that a scripted scene change had actually
happened by reading the recording back for it, written as `grep -c "cube 3->154"` guarded by
`|| fail "could not read the recording back"`. **`grep -c` exits 1 when it counts nothing**, so the
single failure that check existed for -- the scene change not arriving, leaving the arm testing
nothing -- reported the *reader* as broken, and the count comparison underneath it was unreachable.
The arm had been validated by breaking the engine and watching it go red, and never by breaking the
precondition. Break each branch: with the change, without it, and with an unreadable file.

Concretely, the other shapes, from this engine:

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

- **A control that is silently a second arm.** An A/B over an env-gated branch set the control's
  variable to the empty string, and the gate read `getenv(...) ? 1 : 0`, so empty-but-set was on
  and both arms were the arm. Every surface failed in both, which reads as a far larger finding
  than the real one. Unset the variable rather than emptying it, and prove the switch by breaking
  it on purpose in both directions: a control that cannot be shown to differ from the arm is not a
  control.

- **A harness exercises the form a script writes, not the form a person types.** A fix to the
  replay path was measured working over a dozen runs and did nothing at all for `--replay <bare
  name>`, because every arm passed an absolute path -- which is what a script writes and not what a
  player types. The same blind spot showed up from the other side the same evening, in six
  player-made recordings that were all started with no path. List the argument forms a real user
  produces and make sure one arm covers each.

- **A guard whose quantity moved under it.** A test asserts that at most a tenth of a recording's
  polls carry an analog block, reading the count out of `dump_recording.py`. The reader gained a
  hold-last branch that synthesises a row for every poll without a block, so the number the guard
  reads counts held rows alongside written ones, and it passes only because the sessions it runs on
  report zero analog blocks, where the two happen to agree. A check that reads its quantity from a
  tool it does not own has to be re-validated when that tool changes, and one that is currently
  vacuous cannot tell you it has stopped meaning anything.

- **A grep that finds sites but proves nothing.** Both misses in the 64-bit multiply sweep sat
  beside code that already knew the hazard, one of them two lines under a previous fix for the same
  function, with a test whose reference model was correct and whose inputs were too small to
  overflow.

The habit that catches all of these: after any green result, ask what would have to be broken for
this to go red, then break it.

Those are all checks that could not fail. There is a second family, where the check is sound and you
never see what it said:

- **A build that cannot observe the fault.** A Release build has no sanitizers. A test written to
  document the `DISTANCE.CPP` integer overflow walked into it, and the Release run was green: only
  the `linux_sanitize` leg went red. Anything touching arithmetic wants
  `cmake --preset linux_sanitize` and `ctest -L host_quick` before it is believed.

- **A result nobody prints.** `ctest` shows nothing for a test that passes. A test that *measures*
  rather than asserts, which is what a platform whose arithmetic differs needs, is therefore
  invisible while reporting success. It needs `ctest -R <name> -V`, or the measurement it exists to
  produce never reaches the log.

- **A branch nothing compiles.** Code behind a platform predicate is never built on the platform you
  are sitting at. Force the predicate locally and build it, or it ships unexecuted and the first
  machine to reach it is a CI runner.

There is a third family, and it is the hardest to see, because the check ran and the answer looked
like an answer. **Where the honest result was "I do not know" or "I stopped looking", something
confident, specific and wrong was produced instead.** All of these are from the session recorder
work, and each was found by somebody walking into it rather than by looking:

- **A truncation that reads as completeness.** `Control_StateDigest` mixes unconditionally and
  stores behind `if (s_teleN < TELE_MAX)`, so past the cap the digest keeps judging correctly while
  the report silently stops naming fields. A short list then reads as "nothing else differs". Same
  shape: a replay that ends before its summary prints looks exactly like one that passed, and a
  divergence report capped at three ticks by eight fields names 24 values however broad the failure.

- **A batch of edits that stopped part way, and a commit message written from the intent.** Six
  edits were applied from one script; it stopped at the first hunk that did not match, so the fifth
  and sixth never ran. The failing hunk was then fixed by hand and the script never re-run. `git add`
  over three files where only one had changed committed cleanly and said nothing. The message claimed
  four fixes; the diff carried two, and it merged that way. **A commit message is a claim about a
  diff, not a description of one** -- and the code is fixable afterwards where the merged message is
  not. `git show --stat` before believing your own message costs one command and would have shown one
  file against a message naming three.

- **A `head` on an exhaustive question.** `grep -rn 'SaveTimer()' ... | head -40` stopped inside one
  file; the two files sorting after it were never seen, and their absence was reasoned from as an
  answer. A pipe that samples a "find every call site" query is a silent answer to a question that
  had a complete one. Count the matches, or drop the pipe.

- **A rewind that restores one of a pair.** `RestoreTimer` puts `TimerRefHR` back and never touches
  `LastTime`, which is what `ManageTime` banks the next delta against. The clock the caller compares
  looks restored; the clock's own memory of where it was does not come back, and the residual leaks
  into the next tick. Whenever state is saved and restored, ask what else the restored value is read
  against.

- **A diagnosis chosen when none was available.** A telemetry comparison whose two sides hold
  different counts reports "the actor list differs" by name. A truncation on one side is then given
  a specific, confident, wrong cause. "Counts differ, no diagnosis possible" is the honest output.

- **A verdict whose preconditions were never met.** A replay given no save to load into used to
  diverge and report a tick rather than refuse, and four measurements were published off it before
  anyone noticed. A settings mismatch the tool had already detected and named still ran 901 ticks
  and reported `first hash mismatch 0`. **A check that cannot establish its own preconditions must
  refuse and name the precondition, never proceed and report a number.**

- **A local measurement of a tree that is not the judging tree.** A build-graph improvement was
  claimed from a count taken with `LBA2_BUILD_TESTS` off, one translation unit short of what CI
  measures. It reached a commit message, a PR body and two docs before CI caught it. "I measured it
  locally" is a specific claim about a tree that may not be the one that judges you.

- **A check scoped to one change, read as a property of the tree.** "My diff moved no line numbers"
  is not "the line numbers are current", and the more carefully the check is scoped the more
  authoritative its wrong answer sounds. Re-grep on a schedule, not when something plausibly
  relevant merges.

- **A stack that says where a run stopped, read as saying it should not have been there.** A
  from-boot headless run with no `--load`, no `--demo` and no `skipmodals` stops retiring ticks once
  simulated time passes about four seconds, in `DoLife` -> `Dial` -> `SpeakAnimation`. That is the
  opening dialogue waiting for a keypress, it is engine-correct, and it is documented. An arm asking
  for 800 ticks at 16 ms is 12.8 seconds of game time, so it walks into it every time. The evidence
  read as a defect: a reproducible timeout, a clean A/B across two builds, and a real stack. The
  correct reading was that the arm was misconfigured in a way this repo wrote up months ago.
  **Before reading a stack as a bug, check whether the run was configured to reach the state you
  think it is stuck in.** The stack tells you where it stopped, never whether it belonged there.

- **A timeout that cannot tell slow from stuck.** A paced run and a stalled run both exit 124, and
  only wall time or progress separates them. `tests/automation/lib.sh` applies one
  `LBA2_TEST_TIMEOUT`, 90 seconds by default, to every arm, and states that a timeout is reported as
  a failure rather than as a blocked terminal. So an arm whose runtime changes for a legitimate
  reason -- a recorded run pacing itself to real time rather than fast-forwarding, say -- wants its
  cap resized deliberately rather than inherited. Measured after that change landed: 31 engine runs
  in the recording suite, longest single run 14.7 s against the 90 s cap, next four 13.0, 12.9, 10.2
  and 10.1. A sixfold margin, so nothing is near flaky today; the hazard is the shared cap, not the
  current numbers. **The cheap way to check is a wrapper**: point `LBA2_BIN` at a script that times
  the real binary and appends to a log, and the suite reports its own distribution without being
  modified.

- **An impossibility concluded from one failed approach.** `gdb -p` cannot attach here because
  `ptrace_scope` is 1, and that was read as "no stack is available". Running the binary *under* gdb
  works: `timeout --foreground -s INT 25 gdb -batch -nx -ex run -ex "bt 40" -ex kill --args ...`,
  where `--foreground` is what stops `timeout` signalling the inferior instead of gdb.

Two more about evidence rather than about tools, because both cost a couple of minutes and both
nearly landed in published work here:

- **A citation supports the sentence it is attached to, not the paragraph around it.** A true line
  about `FlagChgCube` and the `SaveTimer` lock was read as licensing a claim about which fades run
  at a scene change. The two are unrelated: the fade gate is a different predicate entirely. Both
  statements were true and the chain between them was invented. The same shape catches a reader who
  cites one clause of a two-clause sentence and never sees the second.

- **A peer's citation is exactly as much evidence as your own grep, and it arrives sounding
  pre-verified.** Reading it back cost two minutes and split it: the quote held, the inference beside
  it did not, and the replacement site offered instead dissolved on a second read as well. Work
  received from someone careful is the easiest to publish unchecked, which is what makes it worth
  the two minutes.

Three of those arrived in a single change: a truncated grep, a `grep -c` whose zero-count exit fired
the wrong branch, and a patch script that stopped early. Each time the tool reported that it had
stopped and the output was read as the whole answer. That is the family in miniature, and it is why
the remedies below are worth more than the individual entries.

Two habits fall out of the family, and they are cheaper than the individual lessons:

**Prefer the reading that has no preconditions.** An offline reader of a file has no save to forget,
no settings to match and no run to get wrong, so it cannot produce this class of answer at all. In
the recorder work every finding that survived came from reading a file or the source, and every one
withdrawn came from a replay whose preconditions had not been established.

**Ask what a tool says where it cannot answer.** If the answer is a specific claim rather than a
refusal, that is the next instance, and it can be found by looking rather than by being bitten.

### What a gate can and cannot reach

Worth separating, because the pessimistic version of the above stops people building the half that
is cheap.

**Structure is gateable, and this repo already does it.** `scripts/ci/check-docs-symbols.py` parses
prose, pulls the backticked identifiers off a line that links a source file, and asserts each is in
that file. Pointed at a sample output it would compare key sets and catch a field added, renamed or
removed without the doc following. Better still, generate the sample from a real run rather than
writing it by hand, so the derivation is the check.

**Values are not.** A `--dump-state` sample once printed `clock_src_ms` equal to `timer_ref_hr` when
the two read 4792 and 4,272,829. The field was right, the test was right, CI was green, and only the
example was wrong. A plausible integer beside another plausible integer is checkable against
nothing, because the checker would have to know what the number means. Every measured figure in
every doc here is that half.

**A line number is not a citation, and it can be checked like one.** Verify each by reading the
cited line back and matching a substring of what the prose claims is there. A rebase across a few
merges moved three citations in one branch, one of them by 72 lines, and every one of those still
pointed at real code -- which is why nothing complained. The number is the part that rots; the text
on the line is the part that means something.

**A document that cites the code it changes drifts against itself inside one branch.** Inserting a
line above a cited one rots the citation with no rebase and no external event to prompt a re-check,
and the doc and the code are in the same diff. One branch moved four of its own citations by 7 to 11
lines that way, every one still landing on real code. So the rule is not "re-check after a rebase",
it is **re-check after anything that changes line counts in a file you cite, including your own
diff**.

**And some defects live in a pair, where neither artefact is wrong.** An advice string in
`dump_recording.py` was correct when written and made wrong hours later by a flag rename that never
touched it. The dependency runs backwards through time: tests, types and review all point forwards,
and none of them ask who is already depending on a meaning about to change. A whole-tree grep on the
name of anything being renamed is the blunt instrument that works.

## From a report to a cause

**Bisect the trigger before diagnosing.** A fault that arrived after two resolution switches and a
UI modal looked like a widescreen bug and was written up as one. The resolution switches were
irrelevant: a `teleport` alone reproduced it. Reduce to the minimal command sequence first, then
explain it.

**Separate live from latent.** Both are worth fixing, but not with the same urgency, and saying
which is which keeps a report honest. A 16.16 depth delta overflows at ordinary values; a sphere
radius needs two million model units against a focal of 600 and never happens.

**When you can name the wrong value, watch the write instead of reading the readers.** A camera
field differing at tick 0 between two runs of one save was read as a digest membership defect, then
as a load-path asymmetry, then correctly as one function answering differently depending on its
caller. Three reads of the source, three wrong answers, and a watchpoint on the field named the
writer immediately. **A wrong value in a compared field looks like a comparison problem from every
distance**, so the layers between the comparison and the write all read as plausible causes. A
watchpoint skips them rather than walking down them one theory at a time, and it costs less than the
first theory did.

**Price a hypothesis by what it costs to kill, not by how likely it is.** Two rounds of a recorder
determinism hunt were each decided by a wrong hypothesis that came with a cheap discriminator. The
first was that a wait-loop clock leak drove the reproduction rate; the negative was one measurement,
`Timer_FixedDtPump` called zero times on the path in question, and it bounded the search to what the
recording file does not carry, where the cause was. The second was that a tick-1 mismatch was the
digest reading globals no savegame carries; the discriminator was one run, because the telemetry
names which field moved. It named an actor's animation anchors and the sub-step carry, one
millisecond apart, which is a real divergence and not a membership defect. A likelier hypothesis
with no cheap way to kill it would have decided neither.

**A bug that fires five percent of the time is usually not a five percent bug.** It is a certain bug
behind a rare precondition, and the two want completely different experiments. Name the precondition,
instrument it so every run is a labelled sample, force it, and report the conditional; the natural
rate then follows from how often the precondition holds, and needs no large K. A recorder
determinism fault measured this way went from an unreadable 1 failure against 1 at K=25 to 15 of 15
before and 1 of 14 after, on the same number of runs, once the condition it needs was held open
deliberately.

**The tell is that one input always gives one verdict.** What varies is then a gate upstream that
has not been identified, not the thing under test. This is the exact complement of the rule that a
stochastic pipeline needs K fresh inputs each processed more than once, and the two are easy to
confuse: ask whether one fixed input can give two answers. If it can, raise K. If it cannot, stop
raising K and go find the gate.

**Moving a lookup earlier can poison the lookup it was copied from.** A name had to be resolved
before the load ran, so the resolver was called at flag-parse time as well. It did not merely fail
there: resolving before the directories were up left the later call unable to open the same name at
all, turning a diverging replay into one that could not start. Fix by moving the work to its
consumer rather than moving the dependency earlier, and check what the original call site does after
your new one has run.

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
- [plan/RECORDER_OBSERVER_REVIEW.md](plan/RECORDER_OBSERVER_REVIEW.md): where the third family above
  was collected, and what it says about a tool whose whole value is being believed
