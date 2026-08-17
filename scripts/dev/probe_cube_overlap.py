#!/usr/bin/env python3
"""Sweep every cube for camera zones that overlap in all three axes.

CheckZoneSce takes the first camera zone in list order that contains the hero and
ignores the rest, so wherever two camera boxes overlap in x, y AND z, list index
silently decides which camera wins. Nothing in the scene data advertises that.

Restarts the engine and carries on where it left off, because walking many cubes in
one process crashes intermittently (see probe_cube_crash.py) and a sweep that dies
at the first fault covers almost nothing.

Usage: probe_cube_overlap.py [first] [last]
"""
import subprocess
import sys
import time

sys.path.insert(0, "scripts/dev")
from lba2ctl import Control
from probe_zones import read_zones

SCRATCH = ("/tmp/claude-1000/-home-noctonca-code-lba-hacking-lba2-classic-community/"
           "08cebabc-9e6d-4a53-ac8f-09a3ba677474/scratchpad")
BIN = "./build-ctl/SOURCES/lba2cc"
ARGS = ["--headless", "--no-audio",
        "--game-dir", "/home/noctonca/code/lba-hacking/LBA2-GOG",
        "--user-dir", f"{SCRATCH}/probe", "--no-autosave",
        "--load", f"{SCRATCH}/probe/save/Spaceship.LBA", "--listen", "4444"]
SETTLE = 0.25

FIRST = int(sys.argv[1]) if len(sys.argv) > 1 else 0
LAST = int(sys.argv[2]) if len(sys.argv) > 2 else 255


def start_engine():
    log = open(f"{SCRATCH}/sweep.log", "wb")
    p = subprocess.Popen([BIN] + ARGS, stdout=log, stderr=log)
    for _ in range(120):
        time.sleep(0.25)
        if p.poll() is not None:
            return None, None
        try:
            return p, Control(4444, timeout=10)
        except OSError:
            continue
    return p, None


def overlaps(p, q):
    def ov(a0, a1, b0, b1):
        return a0 < b1 and b0 < a1
    return (ov(p[0], p[3], q[0], q[3]),
            ov(p[1], p[4], q[1], q[4]),
            ov(p[2], p[5], q[2], q[5]))


full, planar, seen, restarts, dead = [], [], set(), 0, []
todo = list(range(FIRST, LAST + 1))

while todo:
    proc, conn = start_engine()
    if conn is None:
        print("could not start an engine; stopping", file=sys.stderr)
        break
    try:
        conn.send("fixedtimestep 0")
        while todo:
            n = todo[0]
            try:
                conn.send(f"cube {n}")
                time.sleep(SETTLE)
                _, _, zones = read_zones(conn)
            except Exception:
                dead.append(n)
                todo.pop(0)      # skip the cube we died on, keep going
                restarts += 1
                break
            todo.pop(0)
            seen.add(n)
            cams = [z for z in zones if z.type == "camera"]
            for i in range(len(cams)):
                for j in range(i + 1, len(cams)):
                    ax, ay, az = overlaps(cams[i].box, cams[j].box)
                    if ax and ay and az:
                        full.append((n, cams[i], cams[j]))
                    elif ax and az:
                        planar.append((n, cams[i], cams[j]))
    finally:
        try:
            if proc and proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=10)
        except Exception:
            pass

print(f"cubes read: {len(seen)} of {LAST - FIRST + 1}")
print(f"engine restarts: {restarts}   cubes skipped after a fault: {dead}")
print(f"\nx/z overlap only (stacked floors, like cube 41): {len(planar)} pair(s)")
print(f"overlap in ALL THREE axes: {len(full)} pair(s)")
for n, a, b in full:
    print(f"   cube {n:>3}  [{a.idx}] {a.box}")
    print(f"             [{b.idx}] {b.box}")
