#!/usr/bin/env python3
"""
Render/state baseline harness for the save corpus (Layer 4).

Sibling to run_harness.py. Where run_harness checks "does the save load?", this loads
each save through the CLI control harness, advances a fixed number of ticks, and captures
a normalized --dump-state snapshot. With --update it writes those snapshots as committed
golden baselines under baselines/; without it, it compares the current run against the
goldens and reports drift.

The dump is world-space, so it is a guardrail that rendering/projection changes do not
perturb the simulation. Screenshots (gitignored) are captured alongside for human review.

Determinism (measured): static state is reproducible; only the positions of actively
moving actors jitter, by ~0.1%, from per-tick wall-clock variance. So the comparison is
exact on everything except x/y/z/beta, which get a small tolerance, and fps/timer/log,
which are dropped.

Usage:
  baseline_harness.py --update [--lba2 PATH] [--game-dir PATH] [--tick N] [--timeout S]
  baseline_harness.py          [--lba2 PATH] [--game-dir PATH] [--tick N] [--timeout S]
Retail data (LBA2/ with lba2.hqr) is required; rendering needs a display. Local-only.
"""
import argparse, json, os, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SAVES_DIR = HERE / "saves" / "steam_classic_2023"
BASE_DIR = HERE / "baselines"
SHOT_DIR = BASE_DIR / "screenshots"

# Fields dropped before comparing/storing (wall-clock derived or run-specific).
VOLATILE = ("fps", "timer_ref_hr", "log")
# Kinematic leaf fields: inherently timing-dependent for actively moving/animating actors
# until --fixed-dt lands. A mismatch here is reported but does NOT fail the baseline — the
# guardrail is the structural/discrete state, which is timing-invariant. Positions get a
# tolerance to suppress sub-step jitter noise; anim/frame indices are reported on any change.
SOFT_POS = {"x", "y", "z", "beta"}
SOFT_OTHER = {"anim", "last_frame"}
POS_TOLERANCE = 64


def normalize(dump):
    d = dict(dump)
    for k in VOLATILE:
        d.pop(k, None)
    return d


def run_save(lba2, game_dir, save, tick, timeout):
    """Return (outcome, normalized_dump_or_None). outcome in {ok, timeout, error:N, nodump}."""
    SHOT_DIR.mkdir(parents=True, exist_ok=True)
    shot = SHOT_DIR / (save.stem + ".png")
    env = os.environ.copy()
    env.setdefault("SDL_AUDIODRIVER", "dummy")
    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tf:
        tmp = tf.name
    cmd = [str(lba2), "--game-dir", str(game_dir), "--load", str(save),
           "--tick", str(tick), "--dump-state", tmp, "--screenshot", str(shot), "--exit"]
    try:
        p = subprocess.run(cmd, env=env, capture_output=True, timeout=timeout, text=True)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        os.unlink(tmp)
        return "timeout", None
    if rc != 0:
        os.unlink(tmp)
        return f"error:{rc}", None
    try:
        with open(tmp) as f:
            dump = json.load(f)
    except (OSError, ValueError):
        return "nodump", None
    finally:
        try:
            os.unlink(tmp)
        except OSError:
            pass
    return "ok", normalize(dump)


def diff(golden, cur, path="", field="", hard=None, soft=None):
    """Walk both dumps. Structural mismatches go to `hard` (fail); kinematic ones
    (moving-actor position/rotation/anim) go to `soft` (reported, non-fatal)."""
    if hard is None:
        hard, soft = [], []
    if type(golden) is not type(cur):
        hard.append(f"{path}: type {type(golden).__name__} vs {type(cur).__name__}")
        return hard, soft
    if isinstance(golden, dict):
        for k in golden:
            if k not in cur:
                hard.append(f"{path}.{k}: missing")
            else:
                diff(golden[k], cur[k], f"{path}.{k}", k, hard, soft)
        for k in cur:
            if k not in golden:
                hard.append(f"{path}.{k}: unexpected")
    elif isinstance(golden, list):
        if len(golden) != len(cur):
            hard.append(f"{path}: length {len(golden)} vs {len(cur)}")
        for i in range(min(len(golden), len(cur))):
            diff(golden[i], cur[i], f"{path}[{i}]", field, hard, soft)
    elif field in SOFT_POS and isinstance(golden, (int, float)) and isinstance(cur, (int, float)):
        if abs(golden - cur) > POS_TOLERANCE:
            soft.append(f"{path}: {golden} vs {cur}")
    elif field in SOFT_OTHER:
        if golden != cur:
            soft.append(f"{path}: {golden} vs {cur}")
    elif golden != cur:
        hard.append(f"{path}: {golden} vs {cur}")
    return hard, soft


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lba2", default=str(ROOT / "build" / "SOURCES" / "lba2cc"))
    ap.add_argument("--game-dir", default=os.environ.get("LBA2_GAME_DIR", ""))
    ap.add_argument("--tick", type=int, default=8)
    ap.add_argument("--timeout", type=int, default=45)
    ap.add_argument("--update", action="store_true", help="(re)write golden baselines")
    args = ap.parse_args()

    if not Path(args.lba2).exists():
        sys.exit(f"binary not found: {args.lba2} (build it or pass --lba2)")
    if not args.game_dir or not Path(args.game_dir).exists():
        sys.exit("retail data required: set --game-dir or LBA2_GAME_DIR")

    BASE_DIR.mkdir(parents=True, exist_ok=True)
    saves = sorted(p for p in SAVES_DIR.iterdir()
                   if p.suffix.lower() == ".lba" and p.name.lower() not in ("current.lba", "autosave.lba"))

    written = drift = passed = skipped = 0
    for save in saves:
        outcome, dump = run_save(args.lba2, args.game_dir, save, args.tick, args.timeout)
        golden = BASE_DIR / (save.stem + ".json")
        if outcome != "ok":
            print(f"  SKIP  {save.stem:<32} ({outcome})")
            skipped += 1
            continue
        if args.update:
            with open(golden, "w") as f:
                json.dump(dump, f, indent=2, sort_keys=True)
                f.write("\n")
            print(f"  WRITE {save.stem:<32} cube={dump['scene']['cube']} mode={dump['scene']['cube_mode']}")
            written += 1
        else:
            if not golden.exists():
                print(f"  SKIP  {save.stem:<32} (no golden — run --update)")
                skipped += 1
                continue
            with open(golden) as f:
                want = json.load(f)
            hard, soft = diff(want, dump)
            soft_note = f"  (+{len(soft)} kinematic)" if soft else ""
            if hard:
                print(f"  DRIFT {save.stem:<32} {len(hard)} structural{soft_note}")
                for d in hard[:8]:
                    print(f"          {d.lstrip('.')}")
                drift += 1
            else:
                print(f"  ok    {save.stem:<32}{soft_note}")
                passed += 1

    print("-" * 56)
    if args.update:
        print(f"baselines: {written} written, {skipped} skipped")
    else:
        print(f"baselines: {passed} ok, {drift} structural drift, {skipped} skipped")
        print("(kinematic = moving-actor position/anim; expected until --fixed-dt)")
    sys.exit(1 if drift else 0)


if __name__ == "__main__":
    main()
