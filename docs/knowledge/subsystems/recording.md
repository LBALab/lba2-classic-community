---
type: Subsystem
title: Session recording
description: The recorder captures a played session at the input waist and replays it into the same simulation, with a per-tick digest that names the first tick that stops matching.
status: draft
subsystem: recording
as_of: d9cf303d
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T16:00:00Z }
relates_to:
  - /quirks/changecube-seeds-from-the-boot-clock.md
  - /subsystems/transitions.md
  - /subsystems/save.md
sources:
  - id: recording-doc
    resource: ../../RECORDING.md
    title: Session recording (docs/RECORDING.md)
  - id: observer-review
    resource: ../../plan/RECORDER_OBSERVER_REVIEW.md
    title: The recorder as an observer, architecture review
  - id: research
    resource: ../../plan/RECORDING_RESEARCH.md
    title: Recording research and prototype measurements
  - id: record-cpp
    resource: ../../../SOURCES/RECORD.CPP
    title: RECORD.CPP, the clock hook and the reload path
  - id: control-cpp
    resource: ../../../SOURCES/CONTROL.CPP
    title: CONTROL.CPP, the harness replay's menu handling
  - id: replay-test
    resource: ../../../tests/automation/test_record_replay.sh
    title: The record-and-replay fixture, the movement and loose arms
---

The principles are structural. The contracts and the seams are read at the commit in `as_of`, and the recorder is under active change: the two clock rows and the verdict row moved within a day of writing.

# Principles

- **The observer is passive; the scheduler and the session manager are borrowed.** The recorder is three roles under one name. Sampling the poll, digesting state per tick and writing the file can be passive, and nearly are. Pinning the step and pacing frames, and writing a snapshot, reloading it and swapping the binding tables, cannot be, because the engine has no simulation tick and no resumable state of its own. Each of those is improved by handing it back to the engine, not by improving the recorder.[^observer-review]
- **The oracle is the file, not the input.** Input is about one byte a poll and the per-tick digest is most of the stream. A matching digest is evidence rather than proof: state has been seen to diverge and re-converge.[^research]
- **Prefer the reading that has no preconditions.** A replay that cannot establish a precondition refuses by name (an unknown digest version, a torn chunk, a mode line that differs) rather than reporting a divergence with no cause in it.[^observer-review]

# Contracts

| Contract | Statement |
|---|---|
| Index | The stream is indexed by input poll, not by tick. A modal can spin hundreds of polls inside one tick (453 measured on the cube 193 demo reel), and a menu records polls but no ticks.[^research] |
| Seam | The recorder samples and injects at the tail of `UpdateKeyboardState`, the one point every modal loop reads through. A replay overwrites all of `TabKeys`, so it is hermetic against live input, the harness and the touch overlay.[^research] |
| Analog | Mouse motion, the right stick and the pad's first-pressed scancode bypass the key table and are carried as a block of their own, restored beside the harness keys. A recording without it drops both analog cameras.[^research] |
| Commands | A command is recorded where it ran and replayed from the same point in the frame: a harness command from the tick hook beside `--exec-at`, a console command from the tail of the poll. Both readers stage and two drains run what was staged. The position matters by one minted step, and a verb that opens a modal spends enough clock inside the tick to move the digest when it is out of position; a verb that spends none does not show it. Running one from inside the poll hook re-enters the reader.[^research] |
| Pinned step | Record and replay under `--fixed-dt`. The step does two things and only one is masking. Structurally, the recorder's clock hold, its main perturbation of a run, is bypassed under a pinned step: `Record_ClockHook` returns before touching the reading while the step is active.[^record-cpp] What the step then hides is simulation state no save carries, and each such value found and carried (`clock.sim_carry`, `clock.rng_seed`, the hero's animation anchor) is one less thing it hides.[^recording-doc] Pinning both ends of a comparison also aligns where the two runs arm the step, which removes an asymmetry rather than hiding a defect. Do not pin a comparison arm to make it green: the movement arm records unpinned on purpose so that it walks the hero across mismatched arming, and the flag would blind it; the arming arm is the other comparison across that regime, and it asserts that the mismatched pair reproduces.[^replay-test] Under the step, game time is frame count times dt with a ceiling and no floor, so a renderer below 62.5 fps at 16 ms runs the game slow by the shortfall.[^observer-review] |
| Loose clock | Two configurations share the word. The console verb `rec start` arms the step for itself at the tick it runs, even with no `--fixed-dt` on the command line, so the recorded portion is pinned and only the window before it is host-sampled; that configuration reproduces, and the header records the arming tick as `mode.step_armed_tick`, carried and not compared.[^record-cpp] A command-line `--record` without the flag is host-sampled throughout, and there the recorder holds the clock at the last input poll; the suite's loose arm runs that configuration through a fade and asserts termination only, because two host-sampled runs do not reach identical state and a comparison would be flaky by construction.[^replay-test] RECORDING.md's figure of 0 ms drift on `TimerRefHR` for a loose recording does not say which of the two it measured; read it as the first until it does.[^recording-doc] |
| Digest membership | Every field the digest mixes declares why a replay can establish it. See [digest membership](/decisions/digest-membership.md). |
| Same binary, same mode | A replay is repeatable to the tick, audio on or off. It is not stable across optimisation levels (329 of 2932 ticks differ between Debug and RelWithDebInfo on one session), and an audio-off replay is not an oracle for an audio-on recording (843 of 2851 ticks differ). Record and replay with the same build and the same mode block.[^recording-doc] |
| Verdict | The exit code is not a verdict. [The coverage gate](/decisions/the-verdict-withholds-success.md) refuses a run that checked too little of the file, and a run that stalls inside a modal can still end clean; read the ticks-checked line against the `holds about N ticks` line.[^recording-doc] A harness replay survives the in-game menu, and a run counts as cut short only when it left for a menu and the stream had not run out, so a session that ended at a menu replays to its last poll and is entitled to say so.[^control-cpp] |

# Seams

The hooks into LIB386 are weak symbols, so LIB386 carries no dependency on SOURCES. That is the judgment; where each hook lives is a grep away, since every hook has one definition in [RECORD.CPP](../../../SOURCES/RECORD.CPP) and one empty default beside its call.

- `Record_PollHook`, at the tail of `UpdateKeyboardState`: the poll, in and out. The recorder starts here rather than at the tick hook because the tick hook runs only inside `MainLoop`, which is dark for menus and cinematics.
- `Record_ClockHook` and `Record_WaitHook`, beside `ManageTime`: the reading about to be banked, and a wait that has to move a pinned clock.
- A third kind of seam, which is an absence. `ManageTime` hands the clock hook the sample it is about to bank and never `TimerRefHR`, and `RestoreTimer` assigns `TimerRefHR` directly after that call returns. A write that bypasses `ManageTime` cannot be recorded, so a replay performs its own restores rather than reproducing the recording's. The first two classes are a read a replay injects into and a callback a modal starves; this is a write the hook cannot see. See [engine timing](/subsystems/timing.md).
- `Record_CommandHook`, in `Console_Execute`: every console line except the recorder's own verb, so a replay can stand in for a harness-driven fixture.
- `Record_SeedHook`, in `ChangeCube`: the seed a replay installs, for the reason in [the ChangeCube quirk](/quirks/changecube-seeds-from-the-boot-clock.md).
- `Record_TickHook` and `Record_ExecHook`, in `Control_TickHook`: the tick record and its digest, and the harness commands.

RECORD.CPP is the session manager and the stream codec in one translation unit, and RECORD_FORMAT.CPP is the buffer-only framing the host tests reach. The one refactor the review found worth doing is a pending-start struct: on the mid-session path `Record_Start` writes a snapshot and returns, recording begins two ticks later, and every option has to cross that gap in a module-level static.[^observer-review]

# What it does not tell you

- Which fields the digest mixes. CONTROL.CPP owns that list and the telemetry record names it; [digest membership](/decisions/digest-membership.md) owns why each field may be compared.
- Whether a replay reproduced. The exit code does not say, and the verdict row says what does.
- What a session did. The keyframes and `scripts/dev/dump_recording.py` answer that; the digest only says when two runs parted.
- Any clock write that bypasses `ManageTime`. The stream holds samples, never installs, so a restore, a load's clock or an arming appears in it only through what was sampled afterwards.
- A wait loop that neither polls nor reaches the tick hook. Nothing is injected into it and nothing staged for it lands, and a poll-counting stall detector cannot see it either. The save-name screen was one, reading SDL's keyboard array directly, until `GetAscii` took the polled key instead; a stall there was reported as a hang, not named by the engine.
- Anything after `as_of`. The recorder is under active change, and this concept is silent past that commit.

[^recording-doc]: [docs/RECORDING.md](../../RECORDING.md), "The pinned step is still required" and "Limits worth knowing".
[^observer-review]: [docs/plan/RECORDER_OBSERVER_REVIEW.md](../../plan/RECORDER_OBSERVER_REVIEW.md), "The finding", "The perturbation ledger" and "The module, and the one refactor worth doing".
[^research]: [docs/plan/RECORDING_RESEARCH.md](../../plan/RECORDING_RESEARCH.md), the prototype measurements, and [docs/plan/ENGINE_TICK_POLICY_SURVEY.md](../../plan/ENGINE_TICK_POLICY_SURVEY.md), "A modal recording does not replay, and the cause is not the clock".
[^record-cpp]: [SOURCES/RECORD.CPP](../../../SOURCES/RECORD.CPP), `Record_ClockHook` and the comment above the reload path's `Timer_EnableFixedDt`.
[^control-cpp]: [SOURCES/CONTROL.CPP](../../../SOURCES/CONTROL.CPP), `Control_ReplayCutShort`.
[^replay-test]: [tests/automation/test_record_replay.sh](../../../tests/automation/test_record_replay.sh), the comments above the movement arm and the loose arm.
