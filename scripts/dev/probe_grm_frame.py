#!/usr/bin/env python3
"""Walk cubes at gameplay pace, deterministically, then record the frame's draw calls.

Two constraints shape this. `--exec` fires every command in one go, which is too
fast for DoLife to run the zone triggers that incrust GRMs, and `--exec-at` caps
at 16 entries, so neither can pace a long walk. The socket can.

The pacing has to be counted, not waited for. Sleeping between commands lets a
variable number of frames elapse, and the recording then differs run to run by a
few hundred bytes, which is wide enough to swallow the difference you are trying
to measure. The control server runs exactly one command per frame ("spending a
frame per line ... makes each command's effect its own", CONTROL_SERVER.CPP), so
sending N no-op commands advances exactly N frames. With `--fixed-dt` pinning the
step size too, the whole walk is reproducible.

Needs an ENABLE_POLY_RECORDING build.

Usage: probe_grm_frame.py <out.lba2polyrec> [last-cube] [frames-per-cube] [attempts]
"""
import os
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lba2ctl import Control, engine_binary, env_path, game_dir

BIN = engine_binary()
SAVE = env_path("LBA2_SAVE", "a save to boot into, as a full path")
PORT = 4463
OUT = sys.argv[1]
LAST = int(sys.argv[2]) if len(sys.argv) > 2 else 40
FRAMES = int(sys.argv[3]) if len(sys.argv) > 3 else 18
ATTEMPTS = int(sys.argv[4]) if len(sys.argv) > 4 else 5


def step(conn, frames):
    """Advance exactly `frames` frames: one command consumed per frame."""
    for _ in range(frames):
        conn.send("status")


def one_run(attempt):
    user_dir = tempfile.mkdtemp(prefix=f"lba2-grmframe{attempt}-")
    log_path = os.path.join(user_dir, "engine.log")
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
    with open(log_path, "wb") as log:
        p = subprocess.Popen(
            [BIN, "--headless", "--no-audio", "--fixed-dt", "16",
             "--game-dir", game_dir(), "--user-dir", user_dir,
             "--no-autosave", "--load", SAVE, "--listen", str(PORT)],
            stdout=log, stderr=log, env=env)

    conn = None
    for _ in range(240):
        time.sleep(0.25)
        if p.poll() is not None:
            break
        try:
            conn = Control(PORT, timeout=60)
            break
        except OSError:
            continue
    if conn is None:
        return None, log_path, "engine never came up"

    try:
        conn.send("skipmodals 1")
        for n in range(0, LAST + 1):
            conn.send(f"cube {n}")
            step(conn, FRAMES)
        conn.send(f"polyrec {OUT}")
        step(conn, 8)  # the recorder captures the frame after the request
    except Exception as exc:
        return None, log_path, f"died during the walk: {exc}"
    finally:
        if p.poll() is None:
            p.terminate()
            p.wait(timeout=10)

    return (OUT if os.path.exists(OUT) else None), log_path, None


for attempt in range(ATTEMPTS):
    out, log_path, err = one_run(attempt)
    if out:
        print(f"attempt {attempt}: recorded {out} ({os.path.getsize(out)} bytes)")
        print(f"   log: {log_path}")
        sys.exit(0)
    print(f"attempt {attempt}: {err or 'no recording written'}  log: {log_path}")

print("no frame recorded")
sys.exit(1)
