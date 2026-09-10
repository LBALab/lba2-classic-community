---
type: Quirk
title: Sample fades end on the wall clock
description: HQ_PauseSamples and HQ_ResumeSamples spin with empty bodies until the fade reports done, and the fade reads SDL_GetTicks directly; that wall-clock read is the loop's only termination, so moving it onto the virtual clock source without a pump leaves the loop unable to end under a pinned step.
status: draft
scope: "the trap is under a pinned step only; on a host clock the loops end as written"
equivalence: untested
asm_origin: "SOURCES/AMBIANCE.CPP:HQ_PauseSamples"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T18:00:00Z }
owner: /subsystems/timing.md
manifests: /decisions/two-pumps-polled-and-unpolled.md
relates_to:
  - /subsystems/recording.md
sources:
  - id: ambiance-cpp
    resource: ../../../SOURCES/AMBIANCE.CPP
    title: AMBIANCE.CPP, the two wait loops
  - id: sample-cpp
    resource: ../../../LIB386/AIL/SDL/SAMPLE.CPP
    title: SAMPLE.CPP, FadeOutSamples and FadeInSamples
  - id: timer-cpp
    resource: ../../../LIB386/SYSTEM/TIMER.CPP
    title: TIMER.CPP, Timer_ClockSource and the pumps
---

# Scope

The loops are reached in every build. The trap opens only under a pinned step, where the virtual clock source moves only when something mints a step, and only if someone changes what the loops read.

# Evidence

`untested`, read from the code. The loops are original C++; the fade functions are the SDL sample backend. No fixture found drives these two loops under a pinned step, which is also why the trap is a trap: nothing would catch the change.

# Behaviour

`HQ_PauseSamples` is `while (FadeOutSamples(200));` followed by the pause, and `HQ_ResumeSamples` is the resume followed by `while (FadeInSamples(200) != 200);`. Both bodies are empty: no `ManageTime`, no pump, no input poll.[^ambiance-cpp] `FadeOutSamples` returns how much of the fade remains and `FadeInSamples` how much has elapsed, each computed from `SDL_GetTicks` against a start time taken on the first call.[^sample-cpp] The wall clock moving is the only thing that ends either loop.

# Why it is load bearing

- The shipped fix for the video path replaced `SDL_GetTicks` with `Timer_ClockSource` so a replay reproduces the frames a video eats, and paired it with a polled pump. Applied here for consistency, without a pump, the same substitution turns two working loops into two that cannot end under a pinned step: `Timer_ClockSource` returns `FixedDtNow`, and `FixedDtNow` advances only inside a step that these loops never mint.[^timer-cpp]
- The wall-clock read is also exactly what invites the substitution. It is a direct host read on the audio path that a replay does not reproduce, and reproducing it is the obvious tidy-up.
- If the loops are ever moved onto the virtual clock, the change pairs the clock source with `Timer_FixedDtPump`, the form for a wait that does not poll, as the canonical fade in MUSIC.CPP does. The pump is the fix; the clock source alone is the trap.

# What it does not tell you

- Which paths reach these loops, or whether any of them runs under a pinned step today. Not traced here.
- Whether the fade is audible in a replay, which is a question about the audio thread rather than the clock.

[^ambiance-cpp]: SOURCES/AMBIANCE.CPP, `HQ_PauseSamples` and `HQ_ResumeSamples`.
[^sample-cpp]: LIB386/AIL/SDL/SAMPLE.CPP, `FadeOutSamples` and `FadeInSamples`.
[^timer-cpp]: LIB386/SYSTEM/TIMER.CPP, `Timer_ClockSource`, `FixedDtStep` and `Timer_FixedDtPump`.
