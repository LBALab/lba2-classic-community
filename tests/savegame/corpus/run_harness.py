#!/usr/bin/env python3
"""
Drive lba2 --save-load-test on every entry in manifest.json (with abi=auto and
abi=32) and write the per-save outcome into manifest.json.

Each save runs in a subprocess: SIGSEGV is reported as the exit signal, hangs
are caught by --timeout. The lba2 binary prints SAVE_LOAD_TEST: stage=… lines
which we parse for {flagload, nb_objets, nb_patches} and the latest stage
reached (so we can tell a crash that happened *during* LoadGame from one that
happened after).

Usage: tests/savegame/corpus/run_harness.py [--lba2 PATH] [--game-dir PATH] [--timeout N]
"""
import argparse, json, os, re, subprocess, sys, signal
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
MANIFEST = HERE / "manifest.json"

STAGE_RE = re.compile(r"SAVE_LOAD_TEST:\s*stage=(\S+)(.*)$")
KV_RE = re.compile(r"(\w+)=(\S+)")

def run_one(lba2, game_dir, save_path, timeout, abi):
    env = os.environ.copy()
    if abi == "32":
        env["LBA2_SAVE_LOAD_ABI"] = "32"
    else:
        env.pop("LBA2_SAVE_LOAD_ABI", None)
    cmd = [lba2, "--game-dir", game_dir, "--save-load-test", save_path]
    try:
        p = subprocess.run(cmd, env=env, capture_output=True, timeout=timeout, text=True)
        out = p.stdout
        rc = p.returncode
        timed_out = False
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode("utf-8", errors="replace") if isinstance(e.stdout, (bytes, bytearray)) else (e.stdout or "")
        rc = -signal.SIGKILL
        timed_out = True
    last_stage = None
    fields = {}
    for ln in out.splitlines():
        m = STAGE_RE.search(ln)
        if not m:
            continue
        last_stage = m.group(1)
        for k, v in KV_RE.findall(m.group(2)):
            try:
                fields[k] = int(v)
            except ValueError:
                fields[k] = v
    # Classify outcome
    if timed_out:
        outcome = "timeout"
    elif rc < 0:
        outcome = f"signal_{-rc}"
    elif rc == 139 or rc == -11:
        outcome = "segv"
    elif last_stage == "done":
        fl = fields.get("flagload")
        if fl == 0:
            outcome = "ok_init"
        elif fl == 1:
            outcome = "ok_loaded"
        elif fl == -2:
            outcome = "ctxerr"
        elif fl is None:
            outcome = "done_no_flagload"
        else:
            outcome = f"ok_other_{fl}"
    elif last_stage in ("after_loadgame", "after_initgame", "start", "open"):
        outcome = f"crash_after_{last_stage}"
    else:
        outcome = f"rc={rc}_stage={last_stage}"
    return {"outcome": outcome, "rc": rc, "last_stage": last_stage, **fields}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lba2", default=str(ROOT / "build" / "SOURCES" / "lba2"))
    ap.add_argument("--game-dir", default=os.environ.get("LBA2_GAME_DIR", str(ROOT.parent / "LBA2")))
    ap.add_argument("--timeout", type=int, default=30)
    ap.add_argument("--abis", default="auto,32", help="comma-sep set of ABI modes")
    args = ap.parse_args()

    manifest = json.loads(MANIFEST.read_text())
    save_root = manifest["save_root"]
    abis = args.abis.split(",")

    by_outcome = {a: {} for a in abis}
    for entry in manifest["entries"]:
        save_path = os.path.join(save_root, entry["rel_path"])
        results = {}
        for abi in abis:
            print(f"[{abi:>4}] {entry['name']:42}", end=" ", flush=True)
            r = run_one(args.lba2, args.game_dir, save_path, args.timeout, abi)
            results[abi] = r
            print(r["outcome"])
            by_outcome[abi].setdefault(r["outcome"], 0)
            by_outcome[abi][r["outcome"]] += 1
        entry["harness"] = results
        # Pick canonical expected_load: prefer auto, fall back to 32
        for abi in abis:
            if results[abi]["outcome"].startswith("ok"):
                entry["expected_load"] = results[abi]["outcome"]
                entry.setdefault("notes", "")
                entry["notes"] = f"loads with abi={abi}"
                break
        else:
            entry["expected_load"] = results[abis[0]]["outcome"]

    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"\nwrote {MANIFEST}")
    for abi in abis:
        print(f"\n=== abi={abi} ===")
        for k, v in sorted(by_outcome[abi].items()):
            print(f"  {k:20} {v}")

if __name__ == "__main__":
    main()
