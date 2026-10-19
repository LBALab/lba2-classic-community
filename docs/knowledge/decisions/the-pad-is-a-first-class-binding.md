---
type: Decision
title: The pad is a first-class binding, and any key means a deliberate button
description: Gamepad bindings sit in the same combined table as the keyboard's, so a pad button is rebuilt into Input by the funnel and honours the no-repeat latch exactly like a key, instead of being ORed in after the mask; for a "press any key" wait the pad's first newly pressed scancode stands in for Key, and for the credits only a deliberate button counts, never a stick or a trigger past its deadzone.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T19:00:00Z }
owner: /subsystems/input.md
relates_to:
  - /quirks/a-held-bit-is-masked-until-it-is-released.md
sources:
  - id: funnel-test
    resource: ../../../tests/input_funnel/test_input_funnel.cpp
    title: The funnel test, the fix under test
  - id: input-cpp
    resource: ../../../SOURCES/INPUT.CPP
    title: INPUT.CPP, the comment in MyGetInput on pad parity
  - id: credits
    resource: ../../../SOURCES/CREDITS.CPP
    title: CREDITS.CPP, the skip condition and its comment
  - id: joystick
    resource: ../../../SOURCES/JOYSTICK.CPP
    title: JOYSTICK.CPP, JoystickFirstPressedScancode and JoystickButtonPressed
  - id: pr-bindings
    resource: https://github.com/LBALab/lba2-classic-community/pull/346
    title: The pad as a first-class Input source
  - id: pr-credits
    resource: https://github.com/LBALab/lba2-classic-community/pull/367
    title: The credits skip on a deliberate pad button
---

# Context

The pad's bindings were once applied as a bolt-on: after `GetInput` had rebuilt and masked `Input`, the pad's bits were ORed in. That bypassed the latch, so a held pad button repeated where a held key did not, and every wait loop that read `Input` behaved differently by device. Separately, "press any key" surfaces tested `MyKey`, which only the keyboard wrote, so a pad could not skip an intro or answer a prompt, and the end credits could not be left at all.[^funnel-test]

# Decision

Fold `GamepadKeys` into the table `InitInput` builds beside `DefKeys`, so the funnel rebuilds `Input` from both and the latch applies to both; the host test drives the funnel with a pad binding and asserts it is masked like a key. For the raw path, `MyGetInput` sets `MyKey` to the pad's first newly pressed scancode when no keyboard key is down, so intro skips, `WaitInput` and menu prompts respond to a pad without touching each site; pad scancodes start at 1024 and cannot collide with a keyboard comparison. For the credits the bar is higher by the maintainer's choice: keyboard means any key, as it always did, and pad means a deliberate button only, through `JoystickButtonPressed`, which excludes the eight stick directions and the two triggers, because a stick resting past its deadzone or a trigger on a lap would end the credits by accident.[^input-cpp][^credits][^joystick]

# Non-goals

- **Bindable menu navigation.** The pad's menu keys are still synthesised scancodes compared raw by each screen; whether that second table should be data is an open product question, not settled here.
- **Typing on a pad.** A pad button never becomes a character; text entry is the keyboard's.
- **A stick as "any key".** Movement axes and triggers are movement, and a wait that ends on them ends by accident.

[^funnel-test]: [tests/input_funnel/test_input_funnel.cpp](../../../tests/input_funnel/test_input_funnel.cpp), the file comment.
[^input-cpp]: [SOURCES/INPUT.CPP](../../../SOURCES/INPUT.CPP), `MyGetInput`, the comment above the `GetJoys` call and the one above the `JoystickFirstPressedScancode` fallback.
[^credits]: [SOURCES/CREDITS.CPP](../../../SOURCES/CREDITS.CPP), the comment above the `JoystickButtonPressed` test in the credits loop.
[^joystick]: [SOURCES/JOYSTICK.CPP](../../../SOURCES/JOYSTICK.CPP), `JoystickFirstPressedScancode` and `JoystickButtonPressed`.
[^pr-bindings]: Pull request 346 on origin.
[^pr-credits]: Pull request 367 on origin.
