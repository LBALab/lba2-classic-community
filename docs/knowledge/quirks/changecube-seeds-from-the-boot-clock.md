---
type: Quirk
title: ChangeCube seeds the RNG from the boot clock on a load
description: On the loose-clock --load path the engine's only reseed reads TimerRefHR before LoadGame installs the save's clock, so the seed is how many milliseconds the process took to boot, and the recorder has to be handed it inside ChangeCube.
status: draft
scope: "loose clock, --load path; under --fixed-dt the seed is 0, and from boot both ends agree"
equivalence: untested
asm_origin: "SOURCES/OBJECT.CPP:ChangeCube"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-09T09:00:00Z }
owner: /subsystems/transitions.md
relates_to:
  - /decisions/rng-reproduces-glibc.md
  - /subsystems/recording.md
sources:
  - id: object-cpp
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, the reseed block in ChangeCube
  - id: recording-doc
    resource: ../../RECORDING.md
    title: Session recording (docs/RECORDING.md), "The pinned step is still required"
---

# Scope

Loose clock, `--load` path. Under `--fixed-dt` the pinned clock zeroes `TimerRefHR` before any of this runs, so the seed is 0 and deterministic, and a code comment about that configuration is not a contradiction of this one. From boot with no `--load`, a replay is already taking its clock from the file by the time cube 0 loads, so both ends reach the seed on the same reading.[^recording-doc]

# Evidence

`untested` here means read and measured, not pinned by a test. SOURCES is the original C++, so there is no disassembly to compare against; the behaviour was read from the code and its effect measured on real replays. One byte-identical recording replayed fourteen times on an idle machine seeded 40 eleven times and 41 three times, and that one millisecond decided the verdict every time: clean at 40, diverging at tick 225 at 41. Carrying the seed took twelve fresh recordings from reproducing 4 of 12 to 10 of 12, and the two replays of one file from disagreeing on 5 of 12 to 0 of 12 (Fisher two-sided p = 0.036 and 0.037).[^recording-doc] No fixture pins the seed line on its own; the record-and-replay fixture passes through it.

# Behaviour

`ChangeCube` is the engine's only reseed. Outside the attract reel it seeds from `TimerRefHR`; under `DemoSlide` the source is `NewCube` instead, so the scripted demo is canonical per cube on every host. On the `--load` path the reseed runs before `LoadGame` installs the savegame's clock further down the same function, so `TimerRefHR` at that line is boot-elapsed wall time, and two runs do not boot in the same number of milliseconds.[^object-cpp]

# Why it is load bearing

- **The seed hook has to sit inside `ChangeCube`.** The recorder starts on the first input poll, after the boot scene load, by which time the stream has been seeded and drawn from. So a replay cannot ask the file for the seed at the moment it needs it: the recording carries it as `clock.rng_seed`, `--replay` reads that one field when the flag is parsed, and `Record_SeedHook` hands it over when the load asks. Moving the hook to the recorder's own start would leave nothing to hand over. Outside a replay the hook changes nothing.[^object-cpp]
- **Reordering the reseed past `LoadGame` is not the fix.** It would put every player's load on the save's clock instead of the boot's, a gameplay change relative to retail, to save the recorder one header line. The recorder's knowledge was the cheap side.
- **Mid-session recordings meet the same fact from the other end.** `rec start` writes a snapshot and reloads it, and the reload's own cube change reseeds. The two ends do not arrive with the same clock, because writing the snapshot costs the recording a present the replay never makes (seeds 4268125 against 4268141, one 16 ms step apart), so the header carries that seed the same way.[^recording-doc]
- **A recording without the field replays exactly as it did before**, which is why the line is installed rather than compared.

# What it does not tell you

- What the seed is on a fresh boot with no `--load`. Both ends reach it on the same reading, so the file does not carry it and this concept does not name it.
- Whether the attract reel is affected. Under `DemoSlide` the seed is the cube number, canonical on every host; this concept is about normal play.
- That the carried value means the same draws on every build. `clock.rng_seed` is installed, not compared, and what a seed draws is the concern of [the RNG decision](/decisions/rng-reproduces-glibc.md).

[^object-cpp]: [SOURCES/OBJECT.CPP](../../../SOURCES/OBJECT.CPP), the comment above the reseed in `ChangeCube`.
[^recording-doc]: [docs/RECORDING.md](../../RECORDING.md), "The pinned step is still required, and not for the reason it looks like".
