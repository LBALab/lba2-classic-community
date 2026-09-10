---
type: Reference
title: Input tests, host and automation
description: The host tests around the binding table and the automation fixtures that drive input through a real engine, the attesters behind the input Quirks; the layer the host tests once skipped, the binding table itself, has had a test since the cfg round trip was pinned.
status: draft
resource: ../../../tests/input_funnel/test_input_funnel.cpp
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T19:00:00Z }
sources:
  - id: funnel
    resource: ../../../tests/input_funnel/test_input_funnel.cpp
    title: The funnel and its latch, on a synthetic table
  - id: device
    resource: ../../../tests/input_device/test_input_device.cpp
    title: The save menu's device classification
  - id: keynav
    resource: ../../../tests/menu_keynav/test_menu_keynav.cpp
    title: The keypad legend for raw-key screens
  - id: bindings
    resource: ../../../tests/input_bindings/test_input_config.cpp
    title: The cfg round trip of the binding table
  - id: melee
    resource: ../../../tests/automation/test_melee_throttle.sh
    title: A press on a skipped frame
  - id: blowgun
    resource: ../../../tests/automation/test_blowgun_release_throttle.sh
    title: A release on a skipped frame
  - id: hold
    resource: ../../../tests/automation/test_input_hold_throttle.sh
    title: A hold over equal game time at two throttles
  - id: spells
    resource: ../../../tests/automation/test_spell_keys_bypass.sh
    title: One key rise, zero action rises
  - id: combos
    resource: ../../../tests/automation/test_input_combos.sh
    title: Contradictory pairs resolved by code order
  - id: baseline
    resource: ../../../tests/automation/test_combo_baseline.sh
    title: The recorded combo sessions replayed
  - id: pad
    resource: ../../../tests/automation/test_pad_bindings.sh
    title: Pad scancodes through the key hold, no hardware
  - id: device-rec
    resource: ../../../tests/automation/test_record_input_device.sh
    title: The device flag carried in a recording
  - id: ascii
    resource: ../../../tests/automation/test_getascii_text_input.sh
    title: Text entry through the poll
---

# What they pin

Host tests, no retail data, in CI:

- `tests/input_funnel` pins the funnel's latch rules and that a pad binding in the combined table is masked like a key; it builds its own table so it pins the rules and not the retail layout.[^funnel]
- `tests/input_device` pins the device classification the save menu gates on.[^device]
- `tests/menu_keynav` pins the one statement of the Num-Lock-off keypad legend for the screens that compare raw scancodes.[^keynav]
- `tests/input_bindings` pins the cfg round trip of the keyboard bindings, including the all-zero guard and the first-session rebind that once vanished; it links `SOURCES/INPUT.CPP`, which the research survey had found in no test.[^bindings]

Automation fixtures, a real engine headless, retail data required:

- The throttle pair places a melee press and a weapon release on frames the simulation skips and asserts the hero acts; both fail against a neutered whitelist.[^melee][^blowgun]
- The hold fixture drives the same game-time hold at two throttles and asserts the distances agree; the spell fixture reads the second boundary's counters for one key rise against zero action rises; the combo fixtures press contradictory pairs and assert which wins, and the baseline replays two recorded combo sessions with their bindings.[^hold][^spells][^combos][^baseline]
- The pad fixture drives pad scancodes through the key hold with no controller attached; the device fixture records a session that typed a name and reads the save folder after the replay, not the exit code; the text-entry fixture types through the poll.[^pad][^device-rec][^ascii]

What none of them pins: the consumption of the 93 raw scancode sites, which needs a screen driven from open to closed headlessly, and a pad on hardware.

[^funnel]: [tests/input_funnel/test_input_funnel.cpp](../../../tests/input_funnel/test_input_funnel.cpp), the file comment.
[^device]: [tests/input_device/test_input_device.cpp](../../../tests/input_device/test_input_device.cpp), the file comment.
[^keynav]: [tests/menu_keynav/test_menu_keynav.cpp](../../../tests/menu_keynav/test_menu_keynav.cpp), the file comment.
[^bindings]: [tests/input_bindings/test_input_config.cpp](../../../tests/input_bindings/test_input_config.cpp), the file comment, and [tests/input_bindings/CMakeLists.txt](../../../tests/input_bindings/CMakeLists.txt).
[^melee]: [tests/automation/test_melee_throttle.sh](../../../tests/automation/test_melee_throttle.sh).
[^blowgun]: [tests/automation/test_blowgun_release_throttle.sh](../../../tests/automation/test_blowgun_release_throttle.sh).
[^hold]: [tests/automation/test_input_hold_throttle.sh](../../../tests/automation/test_input_hold_throttle.sh).
[^spells]: [tests/automation/test_spell_keys_bypass.sh](../../../tests/automation/test_spell_keys_bypass.sh).
[^combos]: [tests/automation/test_input_combos.sh](../../../tests/automation/test_input_combos.sh).
[^baseline]: [tests/automation/test_combo_baseline.sh](../../../tests/automation/test_combo_baseline.sh), with [combo-set.rec](../../../tests/automation/recordings/combo-set.rec) and [combo-controls.rec](../../../tests/automation/recordings/combo-controls.rec).
[^pad]: [tests/automation/test_pad_bindings.sh](../../../tests/automation/test_pad_bindings.sh).
[^device-rec]: [tests/automation/test_record_input_device.sh](../../../tests/automation/test_record_input_device.sh).
[^ascii]: [tests/automation/test_getascii_text_input.sh](../../../tests/automation/test_getascii_text_input.sh).
