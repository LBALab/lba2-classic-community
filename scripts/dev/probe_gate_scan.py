#!/usr/bin/env python3
"""Find cubes whose cube-transition zones are actually enabled.

GereZoneChangeCube requires ZONE_ON (2), not just ZONE_INIT_ON (1): a gate can be
present in the scene and refuse to fire because the story has it shut. Cube 41's
only gate is exactly that, which is why walking from there took no transitions.

Writes one line per cube so a walk can start somewhere that can actually move.
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
ZONE_ON = 2
FIRST = int(sys.argv[1]) if len(sys.argv) > 1 else 0
LAST = int(sys.argv[2]) if len(sys.argv) > 2 else 220


def start():
    log = open(f"{SCRATCH}/gatescan.log", "wb")
    p = subprocess.Popen([BIN] + ARGS, stdout=log, stderr=log)
    for _ in range(160):
        time.sleep(0.25)
        if p.poll() is not None:
            return None, None
        try:
            return p, Control(4444, timeout=10)
        except OSError:
            continue
    return p, None


todo = list(range(FIRST, LAST + 1))
live, dead_gates, read = {}, {}, 0
tally = {"needs_brick": 0, "containment": 0}

while todo:
    proc, c = start()
    if c is None:
        break
    try:
        c.send("fixedtimestep 0")
        while todo:
            n = todo[0]
            try:
                c.send(f"cube {n}")
                time.sleep(0.25)
                _, _, zones = read_zones(c)
            except Exception:
                todo.pop(0)
                break
            todo.pop(0)
            read += 1
            gates = [z for z in zones if z.type == "cube"]
            on = [g for g in gates if g.flags & ZONE_ON]
            for g in on:
                if g.needs_brick:
                    tally["needs_brick"] += 1
                else:
                    tally["containment"] += 1
            if on:
                live[n] = [(g.num, g.box) for g in on]
            elif gates:
                dead_gates[n] = len(gates)
    finally:
        if proc and proc.poll() is None:
            proc.terminate()
            proc.wait(timeout=10)

print(f"cubes read: {read}")
print(f"cubes with at least one ENABLED gate: {len(live)}")
print(f"enabled gates that fire on containment: {tally['containment']}")
print(f"enabled gates that need a door collision: {tally['needs_brick']}")
print(f"cubes whose gates are all disabled:   {len(dead_gates)}")
print("\nbest starting points (most enabled gates):")
for n, gs in sorted(live.items(), key=lambda kv: -len(kv[1]))[:12]:
    print(f"   cube {n:>3}: {len(gs)} gate(s) -> cubes {sorted({g[0] for g in gs})}")
