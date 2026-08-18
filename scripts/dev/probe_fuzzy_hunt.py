#!/usr/bin/env python3
"""Drive the engine through messy, player-shaped sequences and keep whatever breaks.

The deterministic sweeps walk one axis at a time, which is what makes them good at
pinning a known fault down and bad at finding new ones. A player does not do that:
they open the holomap mid-scene, change resolution from the options menu, watch a
video, load a save, and wander into another cube, all against whatever state the
last thing left behind. This drives those combinations over the control socket
under ASan and UBSan, one engine per run, and keeps the log of any run that dies
or reports.

Usage: probe_fuzzy_hunt.py [runs] [actions-per-run] [seed-base]
"""
import os
import random
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lba2ctl import Control, engine_binary, env_path, game_dir

BIN = engine_binary()
SAVE = env_path("LBA2_SAVE", "a save to boot into, as a full path")
PORT = 4461
RUNS = int(sys.argv[1]) if len(sys.argv) > 1 else 6
ACTIONS = int(sys.argv[2]) if len(sys.argv) > 2 else 40
SEED_BASE = int(sys.argv[3]) if len(sys.argv) > 3 else 0

# Cubes that load cleanly; 222+ are absent from the data and exit 1 by design.
CUBES = [0, 1, 2, 4, 5, 7, 8, 10, 11, 13, 14, 16, 17, 19, 20, 22, 23, 25, 26, 27,
         29, 31, 32, 34, 35, 37, 38, 40, 41, 50, 60, 70, 80, 90, 100, 120, 150, 180]
RESOLUTIONS = ["640x480", "768x480", "1024x480", "1280x720", "1920x1080", "1024x768", "1152x648", "960x540"]
ITEMS = ["magicball", "sabre", "holomap", "tunic", "protopack", "ferryticket"]
BEHAVIOURS = ["0", "1", "2", "3"]
INPUTS = ["up 6", "down 6", "left 6", "right 6", "action 4", "search 4", "throw 4"]


def actions_for(rng, shots):
    """One player-shaped action, weighted toward the transitions that mix state."""
    pick = rng.random()
    if pick < 0.30:
        return f"cube {rng.choice(CUBES)}"
    if pick < 0.45:
        # UI modals composite over the live scene and some memset shared buffers.
        modal = rng.choice(["inventory", "holomap", "menu-options", "menu-main", "config"])
        return f"ui {modal} {shots}/ui_{modal}_{rng.randrange(1 << 20)}.png"
    if pick < 0.58:
        # Runtime resolution reallocates the framebuffers under everything.
        return f"resolution {rng.choice(RESOLUTIONS)}"
    if pick < 0.68:
        return f"input {rng.choice(INPUTS)}"
    if pick < 0.76:
        return f"give {rng.choice(ITEMS)}"
    if pick < 0.82:
        return f"behaviour {rng.choice(BEHAVIOURS)}"
    if pick < 0.87:
        return f"teleport {rng.randrange(0, 60000)} {rng.choice([256, 1536, 3584])} {rng.randrange(0, 60000)}"
    if pick < 0.91:
        return f"playjingle {rng.randrange(1, 27)}"
    if pick < 0.95:
        return f"screenshot {shots}/shot_{rng.randrange(1 << 20)}.png"
    if pick < 0.965:
        return f"weapon {rng.randrange(0, 4)}"
    if pick < 0.975:
        # Scripts read these, so poking them exercises life/track code paths.
        return f"vargame {rng.randrange(0, 40)} {rng.randrange(0, 4)}"
    if pick < 0.985:
        return f"varcube {rng.randrange(0, 40)} {rng.randrange(0, 4)}"
    if pick < 0.995:
        return f"useitem {rng.randrange(0, 8)}"
    return "status"


def run(seed):
    rng = random.Random(seed)
    user_dir = tempfile.mkdtemp(prefix=f"lba2-hunt{seed}-")
    shots = os.path.join(user_dir, "shots")
    os.makedirs(shots, exist_ok=True)
    log_path = os.path.join(user_dir, "engine.log")
    env = dict(os.environ,
               ASAN_OPTIONS="detect_leaks=0",
               UBSAN_OPTIONS="print_stacktrace=1",
               LBA2_DBG_ISOLATE="1")

    with open(log_path, "wb") as log:
        p = subprocess.Popen(
            [BIN, "--headless", "--no-audio", "--game-dir", game_dir(),
             "--user-dir", user_dir, "--no-autosave", "--load", SAVE,
             "--listen", str(PORT)],
            stdout=log, stderr=log, env=env)

    conn = None
    for _ in range(240):
        time.sleep(0.25)
        if p.poll() is not None:
            break
        try:
            conn = Control(PORT, timeout=30)
            break
        except OSError:
            continue
    if conn is None:
        return seed, None, "engine never came up", log_path

    history = []
    died_on = None
    try:
        # Without this, the first `give` opens a found-object dialog that waits
        # for a keypress no headless run will ever send, the socket times out,
        # and the run reports a death a handful of actions in. Every wave before
        # this was exploring a fraction of its intended depth.
        conn.send("skipmodals 1")
        for _ in range(ACTIONS):
            cmd = actions_for(rng, shots)
            history.append(cmd)
            try:
                conn.send(cmd)
                # Let the engine actually run frames; a command per PRESENT is the trap.
                time.sleep(rng.choice([0.15, 0.3, 0.6]))
                conn.send("status")
            except Exception:
                died_on = cmd
                break
    finally:
        if p.poll() is None:
            try:
                p.terminate()
                p.wait(timeout=10)
            except Exception:
                p.kill()

    return seed, died_on, history, log_path


def main():
    findings = 0
    for i in range(RUNS):
        seed = SEED_BASE + i
        seed, died_on, history, log_path = run(seed)

        reports = []
        try:
            with open(log_path, "r", errors="replace") as fh:
                for line in fh:
                    if "ERROR: AddressSanitizer" in line or "runtime error:" in line:
                        reports.append(line.strip())
        except OSError:
            pass

        if died_on or reports:
            findings += 1
            print(f"seed {seed}: died_on={died_on!r} reports={len(reports)}")
            for r in reports[:4]:
                print(f"   {r[:160]}")
            print(f"   log: {log_path}")
            if isinstance(history, list):
                print(f"   last actions: {history[-6:]}")
        else:
            print(f"seed {seed}: clean")

    print(f"{findings}/{RUNS} runs produced something")


main()
