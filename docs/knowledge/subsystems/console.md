---
type: Subsystem
title: Debug console
description: The always-compiled drop-down console is the engine's runtime command bus; its verbs call existing engine functions, its output lines are a parsed contract, and the same bus answers the keyboard, the CLI's --exec, the --listen socket and a replay.
status: draft
subsystem: console
as_of: 9f3750f5
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T09:00:00Z }
relates_to:
  - /subsystems/control.md
  - /subsystems/recording.md
  - /subsystems/timing.md
  - /formats/control-socket-protocol.md
  - /decisions/console-output-is-a-parsed-contract.md
sources:
  - id: console-doc
    resource: ../../CONSOLE.md
    title: Debug console (docs/CONSOLE.md)
  - id: console-cpp
    resource: ../../../SOURCES/CONSOLE/CONSOLE.CPP
    title: CONSOLE.CPP, the core and Console_Execute
  - id: console-h
    resource: ../../../SOURCES/CONSOLE/CONSOLE.H
    title: CONSOLE.H, the public API and the line sink
---

The principles are structural. The contracts and the seams are read at `as_of`.

# Principles

- **Always there, everywhere.** Compiled into every build with no option, and usable in-game, in the menus, in the inventory, in the credits and during video playback. The legacy debug hotkeys are a separate optional layer.[^console-doc]
- **Verbs call existing engine functions.** A command reaches the engine through the function a player's action would, `LoadGameNumCube`, `PlayAcf`, `DoFoundObj`, not through a console-only path. The one recorded departure is that a cube change from the console does not autosave, so a debug teleport leaves the player's slots alone.[^console-doc]
- **Output is a parsed contract.** Harness scripts, the socket's clients and the probe sweeps match on the lines; information is appended after a value and never spliced into a prefix. See [the output decision](/decisions/console-output-is-a-parsed-contract.md).
- **One bus, four front-ends.** `Console_Execute` is fed by the keyboard, by `--exec` and `--exec-at`, by the socket, and by a replay's command records. A verb written once answers all four, which is what makes a recording able to stand in for a harness fixture.[^console-doc]
- **A cvar belongs to the module that owns the setting.** It is registered from that module's settings table so an assignment routes through the owner's own rule, and the console cannot set a value the config file would reject. The raw registration writes unchecked and is for a global with no owner.[^console-doc]

# Contracts

| Contract | Statement |
|---|---|
| Tokenizer | Splits on bytes at or below a space. No quoting, no escaping: `load Desert Island` works and `load "Desert Island"` does not. A verb's keywords share the namespace with its arguments, so every keyword a verb gains permanently steals a filename.[^console-cpp] |
| A bool cvar with no argument toggles | The rule is "no value: for bools toggle, otherwise show current value", so a bare bool name is a write that prints the new value. In an A/B the arm that fired prints 0 and the arm that did not prints 1, which reads as the fix having broken what it fixed. Read state with something that only reads: `vargame`, `status`, `dumpstate`, `flags`.[^console-cpp] |
| Scene-needing verbs refuse with a reason | `give` and `savebug` say why when no scene exists, a video is playing, or the cube is the phantom one. |
| Modal verbs block a headless run | `give` of a real item, `credits` and `slide` open something that waits for a dismiss headless never sends; `skipmodals 1` covers dialogues only. `playvideo` is not among them any more: a video plays to its end, and under a pinned step its loop mints its own steps since the video fix, so a headless recording plays one through and replays it. The comment at the top of `tests/automation/test_exec.sh` and the reference doc's list still name it as blocking. |
| Discovery | `help`, `help <name>`, `cmdlist`, `varlist`, `buildinfo`; an unknown command points at `cmdlist`. The CLI's help points here rather than repeating the verbs. |
| Persisting a setting | Through `WriteConfigValue`, never the raw buffer write. The config a run reads is its own file with the install's layered one underneath, and a bare write is refused while that layer is attached, which is every install that ships a config. A verb that applies a setting and says nothing about failing to store it reads as persistent and is not.[^console-doc] |
| Cheats by name | `life`, `magic`, `full`, `gold`, `speed`, `clover`, `box`, `pingouin` route through the same table the key sequences use. |
| The toggle key is the console's | F12 by default, `ConsoleToggleKey` in `lba2.cfg`. While the console is open, every key is the console's and gameplay receives nothing, which is also what a recording sees on those polls. |

# Seams

- `Console_FeedEvent`, registered as the SDL event filter, and `Console_PrePresent`, the compositor in the video layer's pre-present callback. The console draws into its own 8-bit overlay and never touches the game's `Log` buffer or the dirty boxes.[^console-doc]
- `MyGetInput` reserves the toggle key and, while the console is open, calls `Console_Update` and returns.
- `Console_Execute`, the bus. The recorder's command hook sits at its entry and captures every line but the recorder's own verb.
- The line sink. `Console_SetLineSinkForTests` returns the sink it displaced, which is how the socket captures one command's output without clobbering a sink already installed, and how the tests read a verb's output.[^console-h]
- The external call sites: the input path when open and the overlay mark on its redraw, the video player stalling while it is open, the main loop's filter registration and screenshot handoff, the slide show's gate, and the cheat names.[^console-doc]
- Its every-frame redraw is a present. Under the pinned step it is marked an overlay so it neither mints a step nor spends the tick's free present; see [engine timing](/subsystems/timing.md).

# What it does not tell you

- Whether a command's effect is visible yet. Driven over the socket a command runs once per presented frame, and a read in the next command can return the state from before the write. Let a tick elapse for anything the object loop owns.
- What a bare bool name did. It toggled.
- Whether a success line was true. `screenshot` prints its path the moment the capture is requested, and the capture runs at the end of a frame, so an engine inside a loop that never completes one answers three requests with three paths and writes nothing. Filed as #669, open. A caller cannot tell it from a capture without checking the path.
- Whether a setting persisted, unless the verb says so.
- That it can run without a key. There is no autoexec and no script file, so nothing runs a command without a keypress or a harness flag; on Android only a hardware keyboard opens it.

[^console-doc]: docs/CONSOLE.md, "Toggle and input", "Commands", "Implementation notes" and "Extending commands and cheats".
[^console-cpp]: SOURCES/CONSOLE/CONSOLE.CPP, the tokenizer and the cvar branch in `Console_Execute`.
[^console-h]: SOURCES/CONSOLE/CONSOLE.H, `Console_SetLineSinkForTests`.
