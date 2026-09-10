---
type: Subsystem
title: Input
description: Two funnels reach the same consumers, a bindable action bitfield rebuilt from one combined key table each poll and a raw scancode path that three devices depend on by design; sampling is level state once a poll, the edge baseline advances per rendered frame while the edge consumers run per simulated step, and the hero's speed is a mode, so a stick's magnitude has nowhere to go.
status: draft
subsystem: input
as_of: b7612652
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T19:00:00Z }
relates_to:
  - /subsystems/recording.md
  - /subsystems/control.md
  - /subsystems/console.md
  - /subsystems/timing.md
  - /subsystems/save.md
  - /formats/rec.md
  - /decisions/the-harness-meters-input-in-sim-ticks.md
  - /decisions/a-missing-field-is-not-a-value.md
sources:
  - id: research
    resource: ../../plan/INPUT_RESEARCH.md
    title: Input system research, the two funnels and the nine gaps
  - id: sim-plan
    resource: ../../plan/INPUT_SIM_PLAN.md
    title: Input simulation and the throttle-drop class
  - id: funnel
    resource: ../../../LIB386/SYSTEM/INPUT.CPP
    title: INPUT.CPP, the funnel and the latch
  - id: keyboard
    resource: ../../../LIB386/SYSTEM/KEYBOARD.CPP
    title: KEYBOARD.CPP, the poll and its three hooks
  - id: input-cpp
    resource: ../../../SOURCES/INPUT.CPP
    title: INPUT.CPP, the cfg reader and writer and MyGetInput
  - id: bindings
    resource: ../../../SOURCES/INPUT_BINDINGS.CPP
    title: INPUT_BINDINGS.CPP, the retail layout and the combined table
  - id: joystick
    resource: ../../../SOURCES/JOYSTICK.CPP
    title: JOYSTICK.CPP, the pad's two paths
  - id: touch
    resource: ../../../SOURCES/TOUCH_INPUT.CPP
    title: TOUCH_INPUT.CPP, the overlay's virtual keys
  - id: perso
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, the edge baseline, the injector and the throttle
  - id: input-h
    resource: ../../../SOURCES/INPUT.H
    title: INPUT.H, the action bits
  - id: input-flow
    resource: ../../../LIB386/H/SYSTEM/INPUT_FLOW.H
    title: INPUT_FLOW.H, the three boundaries a press can be lost at
  - id: control-h
    resource: ../../../SOURCES/CONTROL.H
    title: CONTROL.H, the harness's three injectors
  - id: gamemenu
    resource: ../../../SOURCES/GAMEMENU.CPP
    title: GAMEMENU.CPP, the save-name gate and the text-entry note
  - id: movement-doc
    resource: ../../MOVEMENT_FRAMERATE.md
    title: Movement is frame-rate dependent, the animation-driven locomotion
---

Distilled from [docs/plan/INPUT_RESEARCH.md](../../plan/INPUT_RESEARCH.md), which stays the survey of the system device by device and owns the issue clustering, and from [docs/plan/INPUT_SIM_PLAN.md](../../plan/INPUT_SIM_PLAN.md), which owns the simulator's design and measurements. The research was measured at 97e9f303; its counts were re-run at `as_of` and hold, with one poll loop more. The principles are structural. The counts, the gaps and the test coverage are read at `as_of`.

# Principles

- **Two funnels, and only one is bindable.** `InitInput` folds `DefKeys` and `GamepadKeys` into one flat table of key and mask, and `GetInput` walks it to rebuild the `Input` action bitfield; 154 sites read `Input` and inherit every binding and alternate for free. Beside it, `TabKeys`, `Key` and `MyKey` carry physical scancodes and 93 sites compare them directly; that path knows no binding and no alternate, and it is where the input defects of the last months were found. It is not legacy: the touch overlay presses scancodes into `TabKeys`, the pad synthesises scancodes for menu navigation, and text entry converts `Key`, so routing the raw sites through the binding layer is a question about how three devices work.[^research]
- **Sampling is level state, once a poll.** `UpdateKeyboardState` clears `TabKeys` and re-reads the whole keyboard, so a press and a release that both land between two polls never existed. `Key` is the lowest scancode held, which makes "the" key among simultaneous presses an artefact of scan order.[^keyboard]
- **The edge baseline is the rendered frame and the edge consumers are the simulated step.** `LastInput = Input` runs once per iteration of `MainLoop`; the hero's press and release edges are read in the object loop, which the fixed-timestep throttle runs only on stepped frames. The two were one thing in 1997 and the port made them different; see [the edge-baseline Quirk](/quirks/the-edge-baseline-is-the-rendered-frame.md) and [the whitelist Decision](/decisions/the-force-step-whitelist-stays.md).[^perso]
- **Speed is a mode, not an axis.** A direction bit selects an animation and the displacement is baked into its keyframes, so nothing downstream could consume a stick's magnitude if the funnel kept it. The mechanism belongs to a movement Subsystem not yet written; its consequence for input is [the eight-directions Quirk](/quirks/a-stick-is-eight-directions.md).[^movement-doc]
- **Suppression has one owner and a hundred writers.** `NoRepeatInput` is a single static in a 59-line translation unit and 109 sites arm or clear it; any of them can clear what another set, and nothing records who did. See [the latch Quirk](/quirks/a-held-bit-is-masked-until-it-is-released.md).[^funnel]

# Contracts

| Contract | Statement |
|---|---|
| The poll | `UpdateKeyboardState` pumps events, clears `TabKeys`, re-reads SDL's keyboard state setting `Key` to the lowest scancode down, then runs three hooks in a fixed order: the touch overlay's `ApplyVirtualKeys`, the harness's `ApplyHarnessKeys`, the recorder's `Record_PollHook`. A replay therefore overwrites what the two injectors wrote, and an injected or replayed key is a real key as far as the binding table is concerned. The flow counters sample before and after the hooks so an injected key does not read as a press the event side lost.[^keyboard] |
| The funnel | `GetInput` rebuilds `Input` from the combined table and applies the latch. `MyGetInput`, the game's wrapper, first has `GetJoys` write the pad's state into the joystick tail of `TabKeys`, then sets `MyKey` to `Key` or, when no keyboard key is down, to the pad's first newly pressed scancode, so "press any key" waits and menu prompts answer a pad; it reserves the console's toggle key from gameplay and isolates all input while the console is open.[^input-cpp] |
| 36 slots, 32 bits | `DefKeysDefault95` is the retail layout and the only place the 1997 control scheme is written down: 36 slots of two keys. The low 32 map to `Input` bits; slots 32 to 35, the spells, never reach `Input`, see [the spell-keys Quirk](/quirks/the-spell-keys-bypass-the-bitfield.md).[^bindings] |
| The pad's two paths | Gameplay goes through the binding layer: `GamepadKeys` sits beside `DefKeys` in the combined table, so a pad button honours the latch exactly like a key, by [decision](/decisions/the-pad-is-a-first-class-binding.md). Menus do not: `JoystickMenuNavOnly` and `JoystickMenuAction` return `K_UP`, `K_DOWN`, `K_LEFT`, `K_RIGHT` and `K_ENTER`, which screens compare as raw scancodes, so pad navigation is a second, unbindable table and a screen must handle both paths to be pad-complete. A stick is quantised to eight directions on its way in. The one analog consumer, the right-stick camera under `GamepadCameraAnalog` with the auto camera on an exterior, bypasses the funnel entirely.[^joystick] |
| The overlay | Fourteen on-screen buttons, each with a hardcoded default scancode; `ApplyVirtualKeys` re-applies them into `TabKeys` after the poll cleared it and leaves `Key` alone. So a button reaches any consumer that reads `Input` or `CheckKey` and no site that tests `Key` or `MyKey`, and nothing in TOUCH_INPUT.CPP reads `DefKeys`, so rebinding the keyboard silently disconnects the overlay. Visibility follows the last device through `LastInputWasTouch`, which the keyboard flag cannot supply because a pad and a finger both leave it zero.[^touch] |
| Text entry | `GetAscii` converts the poll's first held scancode through SDL's keymap and consults no binding; three call sites, the save-name field twice and the cheat codes. The original Windows module drained a character buffer filled by window messages; the port reads the poll. No device but the keyboard writes `Key`, so a pad or touch player never sees the field and is routed by `!LastInputWasKeyboard` to a datetime stem, silently.[^gamemenu] |
| Device facts | `LastInputWasKeyboard` is 1 after a real key-down event and 0 after the mouse, the pad or the overlay. It is in no digest, the save menu gates on it, and the recorder carries it by [its own decision](/decisions/a-missing-field-is-not-a-value.md).[^keyboard] |
| Bindings on disk | `Input%d_1`, `Input%d_2`, `Gamepad%d` and `Gamepad%d_2` in lba2.cfg, read by `ReadInputConfig` and written by `WriteInputConfig`, the only settings group outside the `SETTINGS.H` table and so outside its provenance rules. The reader reads through the layered cfg, the install's under the player's, and the writer writes every slot unconditionally, so a fresh profile inherits an install's remaps and stamps them into its own file on first exit. The round trip, with its all-zero guard, is host-tested.[^input-cpp] |
| Three injectors | The harness reaches the poll three ways. `Control_KeyHold` holds a scancode for a span of polls, up to eight at once, and writes `Key` too, so it reaches the raw sites the overlay cannot. The `input` timeline holds action masks metered in simulated ticks, or in rendered frames for `fseq`, by [decision](/decisions/the-harness-meters-input-in-sim-ticks.md). `Control_MouseMove` drives the accumulator the analog camera reads, which no scancode reaches, one move at a time.[^control-h] |
| The recorder | Samples and injects at the poll's tail, carries both binding tables and compares their digest, records the device flag at the start and on each change, and carries the analog block beside the key table; [the .rec Format](/formats/rec.md) owns the lines.[^keyboard] |

# Seams

- **The three hooks at the tail of `UpdateKeyboardState`**, weak symbols with an empty default, in the order overlay, harness, recorder.[^keyboard]
- **The combined table**, handed to the funnel by `DefineInputKeys` from `InitInput`; a test that wants the funnel without the real table builds its own, which is how `tests/input_funnel` works.[^bindings]
- **The flow counters** in INPUT_FLOW.H, at the three boundaries a press can be lost: event to polled state, polled state to action bits, rendered frame to simulated step. Each loss is invisible from inside the layer that dropped it, and the counters exist to see it from outside.[^input-flow]
- **The force-step gate** in `MainLoop`, where a frame the throttle would skip is promoted to a step when it carries one of the named one-frame signals.[^perso]
- **The pad parity helper**, `JoystickFirstPressedScancode`, and its stricter sibling `JoystickButtonPressed` for the credits.[^joystick]
- **The keypad legend**, `MenuKey_NumpadToNav` in MENU_KEYNAV.CPP, the one statement of the Num-Lock-off mapping for the screens that compare raw scancodes.[^research]
- **The save menu's gate** on `LastInputWasKeyboard` in `ChoosePlayerName`, and the `TODO(input)` beside it that records why no other device can type.[^gamemenu]

# What it does not tell you

- **Which device pressed, or which key.** `Input` is actions. A pad's Left and a keyboard's Right merge into one word before anything resolves them, and a contradictory pair resolves by code order, which the combo fixtures pin.
- **That a raw-key site is broken.** Before calling one so, check whether the same loop also tests an `Input` bit. `DefKeysDefault95` pairs a keypad twin for six bindings only, the four directions, the action and the return, so for those the keypad works by default and only a player who rebound is exposed.[^research]
- **That a dropped edge happened.** A press on a frame the throttle skipped, outside the whitelist, is silent; the boundary counters are the only instrument, and they are instrumentation rather than a check.
- **Where a fresh run's pad bindings come from.** Editing the compiled pad defaults was measured to change nothing on a fresh user directory while editing the cfg changes the pad; the source of a fresh run's values is unexplained at `as_of`, so a fixture about pad bindings edits the cfg, not the table.[^sim-plan]
- **What the original keyboard driver did.** The port's `Key` is the lowest scancode SDL reports down; the 1997 DOS handler was an interrupt routine writing the same table, and which key it named first is not established here.

[^research]: [docs/plan/INPUT_RESEARCH.md](../../plan/INPUT_RESEARCH.md), "The finding that organises everything else", "Per device", "Gaps this research found that no issue records" and "Reproduce".
[^sim-plan]: [docs/plan/INPUT_SIM_PLAN.md](../../plan/INPUT_SIM_PLAN.md), "Why framestep still gobbles input" and "Phase 0 findings"; the pad-defaults measurement is in [docs/plan/INPUT_PLAN.md](../../plan/INPUT_PLAN.md).
[^funnel]: [LIB386/SYSTEM/INPUT.CPP](../../../LIB386/SYSTEM/INPUT.CPP), `GetInput`, `NoRepeatInput` and `ClearNoRepeatInput`.
[^keyboard]: [LIB386/SYSTEM/KEYBOARD.CPP](../../../LIB386/SYSTEM/KEYBOARD.CPP), `UpdateKeyboardState`, the three weak hooks above it, `GetAscii` and `LastInputWasKeyboard`.
[^input-cpp]: [SOURCES/INPUT.CPP](../../../SOURCES/INPUT.CPP), `MyGetInput`, `ReadInputConfig` and `WriteInputConfig`.
[^bindings]: [SOURCES/INPUT_BINDINGS.CPP](../../../SOURCES/INPUT_BINDINGS.CPP), `DefKeysDefault95`, `DefKeys`, `GamepadKeys` and `InitInput`.
[^joystick]: [SOURCES/JOYSTICK.CPP](../../../SOURCES/JOYSTICK.CPP), `ApplyStickDirection`, `JoystickMenuNavOnly`, `JoystickMenuAction`, `JoystickFirstPressedScancode` and `JoystickButtonPressed`.
[^touch]: [SOURCES/TOUCH_INPUT.CPP](../../../SOURCES/TOUCH_INPUT.CPP), the button table and `ApplyVirtualKeys`; `LastInputWasTouch` is set there.
[^perso]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), `MainLoop`: the `LastInput` assignment before `MyGetInput`, the `Control_InputCurrentMask` block, and the `Timer_ForceStepIfPending` call with its comment.
[^input-h]: [SOURCES/INPUT.H](../../../SOURCES/INPUT.H), the `I_` bits, `I_JOY`, `I_FIRE` and `I_ACTION_EDGE`.
[^input-flow]: [LIB386/H/SYSTEM/INPUT_FLOW.H](../../../LIB386/H/SYSTEM/INPUT_FLOW.H), the file comment.
[^control-h]: [SOURCES/CONTROL.H](../../../SOURCES/CONTROL.H), `Control_KeyHold`, `Control_InputSchedule`, `Control_InputScheduleFrame` and `Control_MouseMove` with their comments.
[^gamemenu]: [SOURCES/GAMEMENU.CPP](../../../SOURCES/GAMEMENU.CPP), the `TODO(input)` above `InputPlayerName` and the `LastInputWasKeyboard` test in `ChoosePlayerName`.
[^movement-doc]: [docs/MOVEMENT_FRAMERATE.md](../../MOVEMENT_FRAMERATE.md), "The short version".
