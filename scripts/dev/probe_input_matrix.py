#!/usr/bin/env python3
"""Does harness input move the hero? A/B across several saves.

Each save runs twice under identical conditions, once with `input up N` injected
and once without. Same final position both ways means the injection did nothing.
"""
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lba2ctl import engine_binary, env_path, game_dir

BIN = engine_binary()
GAME_DIR = game_dir()
SAVE_DIR = env_path("LBA2_SAVE_DIR", "the folder holding the saves named below")
SAVES = ["Desert Island", "Spaceship", "Emerald Moon", "School of Magic", "Undergas"]
HOLD = int(sys.argv[1]) if len(sys.argv) > 1 else 60

POS = re.compile(r"Pos: \((-?\d+),(-?\d+),(-?\d+)\)")
CUBE = re.compile(r"Cube: (\d+)")


def run(save, inject):
    with tempfile.TemporaryDirectory() as tmp:
        cmd = [BIN, "--headless", "--no-audio", "--game-dir", GAME_DIR,
               "--user-dir", tmp, "--no-autosave",
               "--load", f"{SAVE_DIR}/{save}.LBA", "--fixed-dt", "16"]
        if inject:
            cmd += ["--exec-at", "5", f"input up {HOLD}"]
        cmd += ["--exec-at", str(HOLD + 30), "status", "--tick", str(HOLD + 40), "--exit"]
        r = subprocess.run(cmd, capture_output=True, text=True)
        out = r.stdout + r.stderr
        pos = POS.search(out)
        cube = CUBE.search(out)
        return (cube.group(1) if cube else "?",
                pos.groups() if pos else None)


print(f"hold = {HOLD} sim ticks\n")
print(f"{'save':<18} {'cube':>5}  {'idle position':>24}  {'with input':>24}  moved")
for save in SAVES:
    cube_a, idle = run(save, False)
    cube_b, moved = run(save, True)
    same = idle == moved
    print(f"{save:<18} {cube_a:>5}  {str(idle):>24}  {str(moved):>24}  "
          f"{'no' if same else 'YES'}")
