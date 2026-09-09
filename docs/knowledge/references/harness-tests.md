---
type: Reference
title: Harness and console host tests
description: The host tests behind the control harness and the console, covering the socket protocol's three buffers, the CLI flag table's accept and reject shapes and both help tiers, and the console's commands, completion, input, render and output formatters.
status: draft
resource: ../../../tests/control_proto/test_control_proto.cpp
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T09:00:00Z }
sources:
  - id: proto
    resource: ../../../tests/control_proto/test_control_proto.cpp
    title: The protocol buffers
  - id: cli
    resource: ../../../tests/cli_args/test_cli_args.cpp
    title: The flag table
  - id: console
    resource: ../../../tests/console/test_console_state.cpp
    title: The console's output formatters
---

# What they pin

- `tests/control_proto` builds the input assembler, the response buffer and the event ring with no socket, and asserts the two cases that were wrong: an overrun read must not splice bytes across the gap into one command, and the truncation notice must survive the buffer that refused the line it reports.[^proto]
- `tests/cli_args` asserts the accept and reject shapes of the flag table, both help tiers, and that neither tier names a repository path.[^cli]
- `tests/console` covers commands, tab completion, the `give` table, the input path, the overlay render, and the pure output formatters whose strings harness scripts parse.[^console]

All three run with the host tests in CI on every platform. None of them needs retail data, a window or a socket.

[^proto]: tests/control_proto/test_control_proto.cpp.
[^cli]: tests/cli_args/test_cli_args.cpp.
[^console]: tests/console/test_console_state.cpp and its siblings.
