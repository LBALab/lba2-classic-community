---
type: Quirk
title: A stick is eight directions
description: Analog magnitude is discarded at the funnel and was in 1997, when each axis was compared against a third of its range; the port quantises a stick into eight sectors with hysteresis and a deadzone and emits digital virtual scancodes, the official 2022 builds bind the axes as digital keys too, and nothing downstream could consume a magnitude because the hero's speed is a mode.
status: draft
scope: "every stick on the binding path; the right stick bypasses the funnel as a camera axis when the analog camera, the auto camera and an exterior cube coincide"
equivalence: untested
asm_origin: "SOURCES/JOYSTICK.CPP:JoyMakeBitfield"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T19:00:00Z }
owner: /subsystems/input.md
sources:
  - id: joystick
    resource: ../../../SOURCES/JOYSTICK.CPP
    title: JOYSTICK.CPP, ApplyStickDirection, SetVirtualKeyDown and GetJoys
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, SOURCES/JOYSTICK.CPP, JoyMakeBitfield and the third-of-range thresholds
  - id: research
    resource: ../../plan/INPUT_RESEARCH.md
    title: Input system research, the gamepad section, gap G3 and the official binaries
  - id: movement-doc
    resource: ../../MOVEMENT_FRAMERATE.md
    title: Movement is frame-rate dependent, locomotion as animation
---

# Scope

Every stick that reaches the binding table. The one exception is the right stick when `GamepadCameraAnalog`, the auto camera and an exterior cube coincide: its raw axes go to the camera nudge instead, and the engine's only analog consumer got there by not using the input system.[^joystick]

# Evidence

`untested`, read against the original source. The import's `JoyMakeBitfield` compared each of six axes against a third of its range and set one of two bits per axis plus four hat bits; `GetJoys` kept that signature and the modern body discards its argument. No test pins the quantiser.[^original]

# Behaviour

`ApplyStickDirection` takes a stick's two axes, applies `GamepadDeadzone`, quantises the angle into eight sectors with hysteresis, and presses one or two virtual scancodes from the pad range at base 1024 into `TabKeys` through `SetVirtualKeyDown`. The binding table then treats them as keys. The magnitude is gone before the funnel sees it. The official 2022 builds list the stick axes as bindable digital keys in their own name table, so the stick has been eight directions in every generation of this game.[^joystick][^research]

# Why it is load bearing

- The block on an analog control scheme is deeper than the funnel. Hero locomotion is animation-driven: a direction bit selects an animation and the displacement is baked into its keyframes, so there is no speed scalar anywhere for a magnitude to fill. Keeping the magnitude would change nothing until the hero moved differently, which is the same change as decoupling locomotion from animation.[^movement-doc]
- It caps what a touch stick can mean. An eight-way on-screen stick is reachable by copying this quantiser into the overlay, because eight ways is all the hero can hear; anything finer has nowhere to go.[^research]
- The quantiser is the more careful of the lineage, with hysteresis and a configurable deadzone where 1997 had a fixed third, so "restore the analog the port lost" describes something that never existed.[^original]

# What it does not tell you

- **The magnitude.** Gone at the funnel; a recording's analog block carries the right stick and the mouse, which bypass the table, and not the quantised left stick's axes.
- **Whether the deadzone matches 1997.** A configurable value against a fixed third of range; the sectors are the same eight, the threshold is not the same number.

[^joystick]: [SOURCES/JOYSTICK.CPP](../../../SOURCES/JOYSTICK.CPP), `ApplyStickDirection`, `SetVirtualKeyDown`, `GetJoys` and the suppression of the right stick's digital directions under the analog camera.
[^original]: Commit 333929ab, the initial import, SOURCES/JOYSTICK.CPP, `JoyMakeBitfield` and the six `JoyRange` thresholds divided by three.
[^research]: [docs/plan/INPUT_RESEARCH.md](../../plan/INPUT_RESEARCH.md), "Gamepad", gap G3 and "What the official LBA2 binaries say".
[^movement-doc]: [docs/MOVEMENT_FRAMERATE.md](../../MOVEMENT_FRAMERATE.md), "The short version".
