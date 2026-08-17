#!/usr/bin/env python3
"""Cost of one probe: over the control socket vs one process per probe.

The socket only pays for itself if the setup it amortises is the dominant cost.
This measures that directly, sweeping one camera cvar and reading state back.

Needs an engine already running with --listen on the given port, plus the same
game data the boot baseline uses.
"""
import os
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lba2ctl import Control, engine_binary, env_path, game_dir

BIN = engine_binary()
GAME_DIR = game_dir()
SAVE = env_path("LBA2_SAVE", "a save to boot into, as a full path")
PORT = 4444
VALUES = [0, 20, 40, 60, 80, 100, 120, 140]


def bench_socket():
    with Control(PORT) as c:
        t0 = time.time()
        for v in VALUES:
            c.send(f"cam_hd_pitch {v}")
            c.send("status")
        return time.time() - t0


def bench_process():
    t0 = time.time()
    with tempfile.TemporaryDirectory() as tmp:
        for v in VALUES:
            subprocess.run(
                [BIN, "--headless", "--no-audio", "--game-dir", GAME_DIR,
                 "--user-dir", tmp, "--no-autosave", "--load", SAVE,
                 "--fixed-dt", "16", "--exec", f"cam_hd_pitch {v}; status",
                 "--tick", "5", "--exit"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    return time.time() - t0


n = len(VALUES)
sock = bench_socket()
proc = bench_process()
print(f"{n} probes over the socket:   {sock:8.3f} s  ({sock / n * 1000:7.1f} ms each)")
print(f"{n} probes, process per probe:{proc:8.3f} s  ({proc / n * 1000:7.1f} ms each)")
print(f"ratio: {proc / sock:.0f}x")
