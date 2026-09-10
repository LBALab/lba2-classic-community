---
type: Format
title: Control socket line protocol
description: One command per line in; that command's console output back verbatim and a line reading <<END>>; pushed log records prefixed "! "; loopback only, one client, alive only on a build that enabled it and a run that asked for it.
status: draft
equivalence: tested
as_of: 9f3750f5
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T09:00:00Z }
owner: /subsystems/control.md
constrains:
  - /subsystems/control.md
verified_against:
  - /references/harness-tests.md
relates_to:
  - /decisions/the-socket-is-a-transport.md
  - /subsystems/console.md
sources:
  - id: proto-h
    resource: ../../../SOURCES/CONTROL_PROTO.H
    title: CONTROL_PROTO.H, the wire mechanism with no socket in it
  - id: server-h
    resource: ../../../SOURCES/CONTROL_SERVER.H
    title: CONTROL_SERVER.H, the protocol and the traps
  - id: control-doc
    resource: ../../CONTROL.md
    title: CLI control harness (docs/CONTROL.md), "Driving a running engine"
---

# Evidence

`tested`: the three buffers that carry the protocol, the input assembler, the response buffer and the event ring, have no socket in them and build into a host test that runs in CI on every platform. The two cases that were wrong are one assertion each: a read that overran the input buffer spliced the bytes either side of the gap into one command, and the notice saying a response was truncated was appended through the buffer that had just refused a line, so it was dropped exactly when it was the only thing left saying so.[^proto-h] The socket half is not under test; it is out of every shipped binary.

# Layout

| Direction | Shape |
|---|---|
| In | One command per line, at most 255 bytes. A longer line is discarded. A full buffer with no newline means the peer is not speaking this protocol and the connection is dropped rather than its bytes kept, because discarding the excess would splice the two sides of the gap into a command the driver never sent. A burst of several lines queues; one runs per presented frame.[^proto-h] |
| Out | The command's console output lines verbatim, then `<<END>>`. Anything the engine logs while the command runs is part of the response, since the log fans out to the console: a warning raised by a command is part of its answer.[^server-h] |
| Pushed | With `stream on`, every log record as it happens, each prefixed `! ` so a driver can tell it from command output, from a ring of 256 slots of 192 bytes that discards oldest-first and reports how many it dropped. This is how the trace verbs reach a driver live.[^proto-h] |
| Limits | A response holds 64K. A line that does not fit is dropped whole, never cut, and the notice that something was dropped takes its room back from the tail, so the notice cannot itself be dropped.[^proto-h] |
| Session | Bound to 127.0.0.1 only; one client at a time; `exit` is the last command a connection gets an answer to. Dead unless the build defined `LBA2_CONTROL_SERVER` and the run passed `--listen`; both gates are deliberate and neither substitutes for the other. A driver that stops reading is dropped rather than allowed to hold a frame.[^server-h] |

Commands run on the game thread, polled once per presented frame from the pre-present chain, so a command reads engine globals exactly as safely as the tick hook does. A command that itself presents frames would re-enter the poll from inside itself, and the poll refuses to do anything when it finds that.[^server-h]

# What it does not tell you

- When a command ran. It ran at whatever presented frame it arrived, which is not a tick, so a session does not replay. See [the socket decision](/decisions/the-socket-is-a-transport.md).
- Whether a read saw the write before it. Presents outnumber ticks by one to two orders of magnitude, so a command that changes simulation state and one that reads the consequence can both land inside one tick, and the read returns what was true before the write. It looks like a stable measurement, not a race.[^server-h]
- Whether the response is complete, other than by the notice. A reader that does not look for it reads a short answer as the whole one.
- What real time means in the session. Headless and uncapped, the engine renders on the order of 1500 frames a second while harness input is metered in sim ticks, so `input up 120` spends itself in a fraction of the wall time a driver expects. Pin the throttle before reading a timing.[^server-h]

[^proto-h]: SOURCES/CONTROL_PROTO.H, the header comment and the buffer contracts.
[^server-h]: SOURCES/CONTROL_SERVER.H, the header comment.
[^control-doc]: docs/CONTROL.md, "Protocol" and "Two traps worth knowing".
