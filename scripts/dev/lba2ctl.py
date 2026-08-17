#!/usr/bin/env python3
"""Client for the engine's --listen console socket.

Run the game with a control socket, then talk to it:

    lba2cc --game-dir <dir> --user-dir /tmp/probe --no-autosave \\
           --load "Desert Island" --listen 4444 &
    scripts/dev/lba2ctl.py status
    scripts/dev/lba2ctl.py                       # REPL

As a module, for driving a probe loop:

    from lba2ctl import Control
    with Control(4444) as c:
        for pitch in range(0, 40, 4):
            c.send(f"cam_hd_pitch {pitch}")
            print(pitch, c.send("camtrace 1"))

Two things to know before reading any timing off a session:

* An uncapped headless run renders ~1500 fps, tens of frames per sim tick, and
  harness input is metered in sim ticks. Send `fixedtimestep 40` first.
* A command runs once per presented frame, not once per sim tick. Writing state and
  reading it back in consecutive commands can happen inside one tick, and the read
  then returns the state from before the write — repeatably, so it reads as a real
  measurement. Wait for a tick before reading anything the object loop owns
  (zones, collision, animation, position).
* The console tokenizer does not understand quotes, so save names with spaces go
  unquoted: `load Desert Island`, never `load "Desert Island"`.

Send `exit` to shut the engine down through its own clean-shutdown path; it is
the last command a connection gets an answer to, because the process is gone
before a reply can be written.
"""

import argparse
import os
import socket
import sys

TERMINATOR = "<<END>>"
EVENT_PREFIX = "! "
DEFAULT_PORT = 4444


def env_path(name, what):
    """Read a path from the environment, or say which one is missing.

    The sweeps below spawn their own engines, so they need a binary, game data
    and a save. None of those have a location this repository can assume, and a
    default pointing at one machine is worse than no default: it fails somewhere
    further in, as a missing file rather than as a missing setting.
    """
    value = os.environ.get(name)
    if not value:
        raise SystemExit(f"set {name} to {what}")
    return value


def engine_binary():
    """Path to an lba2cc built with -DLBA2_CONTROL_SERVER=ON."""
    return env_path("LBA2_BIN", "an lba2cc built with -DLBA2_CONTROL_SERVER=ON")


def game_dir():
    """Path to the retail game data (see docs/GAME_DATA.md)."""
    return env_path("LBA2_GAME_DIR", "the retail game data directory")


class Control:
    """One connection to a running engine."""

    def __init__(self, port=DEFAULT_PORT, host="127.0.0.1", timeout=30.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self._buf = ""
        self.closed = False
        self.events = []  # pushed log records, oldest first, after `stream on`
        self.greeting = self._read_response()

    def _read_response(self):
        """Read lines until the terminator. Returns them without it.

        A command can end the process — `exit` never returns — so a close with no
        terminator is not treated as an error. Whatever arrived first is returned
        and `closed` is set, which keeps a genuine mid-command crash visible
        instead of silently indistinguishable from a clean shutdown.

        A process going away shows up differently per platform: POSIX closes the
        socket on the way out, so recv sees a clean EOF, while Windows tears the
        connection down and recv raises ECONNRESET. Both mean the same thing here,
        and so does an engine that segfaulted, which is the other way to get an
        RST and the reason this cannot just be special-cased for `exit`.
        """
        while True:
            lines = self._buf.split("\n")
            for i, line in enumerate(lines[:-1]):
                if line == TERMINATOR:
                    self._buf = "\n".join(lines[i + 1:])
                    return self._split_events(lines[:i])
            try:
                chunk = self.sock.recv(65536)
            except ConnectionResetError:
                chunk = b""
            if not chunk:
                self.closed = True
                partial = [ln for ln in self._buf.split("\n") if ln]
                self._buf = ""
                return self._split_events(partial)
            self._buf += chunk.decode("utf-8", "replace")

    def _split_events(self, lines):
        """Peel pushed log records off, leaving the command's own output."""
        out = []
        for line in lines:
            if line.startswith(EVENT_PREFIX):
                self.events.append(line[len(EVENT_PREFIX):])
            else:
                out.append(line)
        return out

    def drain_events(self):
        """Collect events that arrived since the last call, and return them.

        The server only writes queued records when it services the socket, so a
        cheap command is the way to pump the stream while nothing else is going on.
        """
        self.send("stream")
        got, self.events = self.events, []
        return got

    def send(self, command):
        """Run one console command; return its output lines."""
        self.sock.sendall((command.rstrip("\n") + "\n").encode())
        return self._read_response()

    def close(self):
        self.sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("-p", "--port", type=int, default=DEFAULT_PORT)
    ap.add_argument("-t", "--timeout", type=float, default=30.0,
                    help="seconds to wait for a reply (default 30)")
    ap.add_argument("command", nargs="*",
                    help="console command; repeat for several. Omit for a REPL")
    args = ap.parse_args()

    try:
        conn = Control(args.port, timeout=args.timeout)
    except OSError as e:
        print(f"lba2ctl: cannot reach 127.0.0.1:{args.port}: {e}", file=sys.stderr)
        print("is the engine running with --listen, and past the menu into a scene?",
              file=sys.stderr)
        return 1

    with conn as c:
        for line in c.greeting:
            print(line)
        if args.command:
            for cmd in args.command:
                for line in c.send(cmd):
                    print(line)
                if c.closed:
                    print(f"(engine closed the connection after {cmd!r})")
                    break
            return 0
        while True:
            try:
                cmd = input("> ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                return 0
            if cmd in ("quit", "\\q"):
                return 0
            if not cmd:
                continue
            try:
                for line in c.send(cmd):
                    print(line)
                if c.closed:
                    print(f"(engine closed the connection after {cmd!r})")
                    return 0
            except (ConnectionError, OSError) as e:
                print(f"lba2ctl: {e}", file=sys.stderr)
                return 1


if __name__ == "__main__":
    sys.exit(main())
