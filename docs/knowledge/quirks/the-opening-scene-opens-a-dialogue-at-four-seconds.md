---
type: Quirk
title: The opening scene opens a dialogue about four seconds in
description: Object 4's Life script in the first cube runs a MESSAGE opcode about four seconds of game time after a fresh start; the dialogue spins in its own poll loop, retires no ticks, and waits for any held input to be released before it will take a press, so a headless run with no --load wedges there and an escape armed early does not help.
status: draft
scope: "a fresh start with no --load and no --demo; the trigger is game time, not tick count"
equivalence: untested
asm_origin: "SOURCES/MESSAGE.CPP:Dial"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T09:00:00Z }
owner: /subsystems/control.md
relates_to:
  - /subsystems/timing.md
sources:
  - id: control-doc
    resource: ../../CONTROL.md
    title: CLI control harness (docs/CONTROL.md), "Notes and limits"
  - id: message-cpp
    resource: ../../../SOURCES/MESSAGE.CPP
    title: MESSAGE.CPP, Dial and its waits
  - id: gerelife-cpp
    resource: ../../../SOURCES/GERELIFE.CPP
    title: GERELIFE.CPP, the MESSAGE opcode in DoLife
---

# Scope

A fresh start with no `--load` and no `--demo`. A load lands in another cube and never meets the script; the demo reel auto-advances its dialogues on the clock. The trigger is game time, so a larger `--fixed-dt` buys no headroom: the wall is about 3.9 seconds of simulated time at any step.

# Evidence

`untested`, measured and read. At 16 ms a tick, 244 ticks complete and 248 do not; at 8 ms, 480 and 520; at 32 ms, 110 and 130. A backtrace of the wedged process runs from the Life opcode through `Dial` into `SpeakAnimation` and its present. `--exec-at 240 "key esc 40"` completes a 300-tick run; `--exec "key esc 400"` from tick 1 does not.[^control-doc] No fixture pins the wedge; the fixtures route around it.

# Behaviour

The opcode hands the text to `Dial`, which spins in its own loops, pumping the clock and polling input itself, and retires no `MainLoop` ticks, so a tick budget can never be reached while it is open.[^gerelife-cpp] At each of its entry points `Dial` first waits for the fire, action and menu inputs to be released, so a key already held when the dialogue opens is consumed as already down and never counts as a press; the press has to land inside. `esc` dismisses it and `return` does not. The skip-modals flag gates its three blocking waits.[^message-cpp]

# Why it is load bearing

- The script and the release-wait are the original game's, and both are correct for a player: a held key does not skip a line of dialogue the player has not read. The harness cannot change them without changing the game.
- Every from-boot headless recipe routes around it, by `--load`, by `--demo`, by `skipmodals 1`, or by a press armed to land inside the modal. A recipe that does none of these spins at a full core until the timeout, and the timeout reads as a hang in the engine.
- The same release-wait is why the `key` verb's delay argument is usually necessary rather than optional at every menu: whatever is held when a modal opens is masked.

# What it does not tell you

- That the run failed. It spins on one core and exits 124 on the timeout with no complaint; `--verbose` names the modal on stderr before it hangs, and that line is the whole diagnosis.
- Whether other scenes have their own. The demo reel ends on a cutscene the reel does not advance, and any scene whose script opens a dialogue has the same shape.
- Where the run is when it stops. A stack says where it stopped, not whether the run was configured to get past it.

[^control-doc]: [docs/CONTROL.md](../../CONTROL.md), "Notes and limits", the fresh-start modal.
[^message-cpp]: [SOURCES/MESSAGE.CPP](../../../SOURCES/MESSAGE.CPP), `Dial`, the release-waits at its entry points and the skip-modals gates.
[^gerelife-cpp]: [SOURCES/GERELIFE.CPP](../../../SOURCES/GERELIFE.CPP), the `LM_MESSAGE` case in `DoLife`.
