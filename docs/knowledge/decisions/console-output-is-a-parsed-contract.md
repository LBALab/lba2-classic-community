---
type: Decision
title: Console output is a parsed contract
description: Every console command's output lines are matched by harness scripts, socket clients and probe sweeps, so new information is appended after the value and never spliced between an identifier and its value; the formats live in pure formatters pinned by host tests.
status: draft
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T09:00:00Z }
constrains:
  - /subsystems/console.md
sources:
  - id: console-doc
    resource: ../../CONSOLE.md
    title: Debug console (docs/CONSOLE.md), the vargame row
  - id: console-state-cpp
    resource: ../../../SOURCES/CONSOLE/CONSOLE_STATE.CPP
    title: CONSOLE_STATE.CPP, the pure formatters
  - id: state-test
    resource: ../../../tests/console/test_console_state.cpp
    title: The host test that pins the formats
---

# Context

A `vargame` read names the index when the game names it, so `vargame 30` can say which flag it is. The obvious place for the name, between the index and the value, breaks every driver that matches `vargame[<n>] = <v>`: the parse yields nothing, the caller reads a set flag as unset, and it waits for a variable that has been set the whole time. The failure is silence, not an error, and a unit suite that never runs a real client stays green through it.[^console-doc]

# Decision

- **Append, never splice, never reorder.** New information goes after the value. `vargame[30] = 1 (diploma)` is the worked example: the name is at the end, where an existing match still finds the identifier and the value.[^console-doc]
- **The format is owned by a pure formatter.** `Console_FormatVarGameLine` and `Console_FormatStatusIslandLine_FromState` in CONSOLE_STATE.CPP build the line from values, with no engine state behind them, so a host test can pin the exact string.[^console-state-cpp]
- **A host test pins each contract line.** `tests/console/test_console_state.cpp` asserts the formats, so a change to a matched line fails in CI rather than in a driver.[^state-test]
- **Test a changed line against a real client as well.** The unit test says the string is what the author meant; only a driver says it is what the callers parse.

# Where it is broken today

`screenshot` prints its path when the capture is requested, and the capture runs at the end of a frame. An engine inside a loop that never completes a frame answers with the success line and writes nothing, which breaks the contract in the direction that matters most: a parsed line asserting something that did not happen. Filed as #669, open. Until it is fixed, a caller that needs the file checks for the file.

# Non-goals

- **A structured output mode.** No JSON or key-value variant of the verbs exists; the `--dump-state` JSON is the structured surface, and a verb's line stays a line.
- **Reformatting for readability.** A line that reads awkwardly to a person stays as it is once something parses it.
- **Quoting in the tokenizer.** Output contracts are about what comes out; the tokenizer's lack of quoting is a separate constraint on what goes in, and is left as it is.

[^console-doc]: docs/CONSOLE.md, the `vargame` row of the command table.
[^console-state-cpp]: SOURCES/CONSOLE/CONSOLE_STATE.CPP.
[^state-test]: tests/console/test_console_state.cpp.
