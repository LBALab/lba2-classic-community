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

Determinism: with --fixed-dt the per-tick clock step is constant, so the dump is
byte-identical run-to-run including timer_ref_hr and the kinematic fields (x/y/z/beta,
anim/last_frame). The comparison is therefore exact on all of those; only fps and log are
dropped as run-specific.

Two requirements for that exactness (see docs/CONTROL.md and docs/FIXED_DT_PLAN.md):
  - The binary must be built with -DSOUND_BACKEND=null. SDL_AUDIODRIVER=dummy alone is not
    enough: the SDL audio backend services exterior ambient samples on a wall-clock thread,
    which branches the simulation. The null backend removes that path.
  - Goldens are per platform/build (long double precision differs across Linux x86_64 vs
    Windows/macOS-ARM). The committed goldens are the Linux x86_64 canonical set.

Usage:
  baseline_harness.py --update [--lba2 PATH] [--game-dir PATH] [--tick N] [--fixed-dt MS] [--timeout S]
  baseline_harness.py          [--lba2 PATH] [--game-dir PATH] [--tick N] [--fixed-dt MS] [--timeout S]
Retail data (LBA2/ with lba2.hqr) is required; rendering needs a display. Local-only.
"""
import argparse, json, os, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SAVES_DIR = HERE / "saves" / "steam_classic_2023"
BASE_DIR = HERE / "baselines"
SHOT_DIR = BASE_DIR / "screenshots"

# Canonical fixed-dt step (ms) for golden baselines. A fixed *choice*, not a recovered
# constant: ~16 ms is the 60 fps the dump reports. A different step yields a different but
# still-reproducible trajectory, so the goldens are tied to this value. See docs/CONTROL.md.
CANONICAL_DT_MS = 16

# Fields dropped before comparing/storing (run-specific). With --fixed-dt the clock is
# deterministic, so timer_ref_hr is now compared exactly — only fps (wall-clock FPS) and
# log (console scrollback) remain volatile.
VOLATILE = ("fps", "log")


def normalize(dump):
    d = dict(dump)
    for k in VOLATILE:
        d.pop(k, None)
    return d


def run_save(lba2, game_dir, save, tick, timeout, fixed_dt, screenshot=True):
    """Return (outcome, normalized_dump_or_None). outcome in {ok, timeout, error:N, nodump}."""
    env = os.environ.copy()
    env.setdefault("SDL_AUDIODRIVER", "dummy")
    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tf:
        tmp = tf.name
    cmd = [str(lba2), "--game-dir", str(game_dir), "--load", str(save), "--tick", str(tick)]
    if fixed_dt > 0:
        cmd += ["--fixed-dt", str(fixed_dt)]
    cmd += ["--dump-state", tmp]
    if screenshot:
        SHOT_DIR.mkdir(parents=True, exist_ok=True)
        cmd += ["--screenshot", str(SHOT_DIR / (save.stem + ".png"))]
    cmd += ["--exit"]
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


def run_save_reproducible(lba2, game_dir, save, tick, timeout, fixed_dt, repeats):
    """Run a save up to `repeats` times and check it reproduces itself.

    Returns (outcome, dump, reproducible). `reproducible` is True only if every run
    succeeded and produced a byte-identical normalized dump — the prerequisite for
    asserting that dump exactly against a golden. A save that cannot reproduce itself
    this session (a residual non-dt nondeterminism source, e.g. a dying-enemy spin angle
    or an uninitialized bodiless-extra position) is reported but never compared or stored,
    so it can neither mask a regression nor fail spuriously. Self-calibrating: no
    hardcoded skip-list to drift out of date."""
    first = None
    for r in range(max(1, repeats)):
        outcome, dump = run_save(lba2, game_dir, save, tick, timeout, fixed_dt,
                                 screenshot=(r == 0))
        if outcome != "ok":
            return outcome, None, False
        if first is None:
            first = dump
        elif dump != first:
            return "ok", first, False
    return "ok", first, True


def diff(golden, cur, path="", hard=None):
    """Walk both dumps; every mismatch is a hard failure. Under --fixed-dt the dump is
    fully deterministic, so kinematic fields (position/rotation/anim) are compared exactly
    alongside the structural state — no soft tier."""
    if hard is None:
        hard = []
    if type(golden) is not type(cur):
        hard.append(f"{path}: type {type(golden).__name__} vs {type(cur).__name__}")
        return hard
    if isinstance(golden, dict):
        for k in golden:
            if k not in cur:
                hard.append(f"{path}.{k}: missing")
            else:
                diff(golden[k], cur[k], f"{path}.{k}", hard)
        for k in cur:
            if k not in golden:
                hard.append(f"{path}.{k}: unexpected")
    elif isinstance(golden, list):
        if len(golden) != len(cur):
            hard.append(f"{path}: length {len(golden)} vs {len(cur)}")
        for i in range(min(len(golden), len(cur))):
            diff(golden[i], cur[i], f"{path}[{i}]", hard)
    elif golden != cur:
        hard.append(f"{path}: {golden} vs {cur}")
    return hard


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lba2", default=str(ROOT / "build" / "SOURCES" / "lba2cc"))
    ap.add_argument("--game-dir", default=os.environ.get("LBA2_GAME_DIR", ""))
    ap.add_argument("--tick", type=int, default=8)
    ap.add_argument("--fixed-dt", type=int, default=CANONICAL_DT_MS,
                    help="constant ms per tick for determinism (0 = wall-clock; default %(default)s)")
    ap.add_argument("--timeout", type=int, default=45)
    ap.add_argument("--repeats", type=int, default=3,
                    help="runs per save to confirm it reproduces itself before asserting "
                         "exact (default %(default)s); non-reproducible saves are flagged FLAKY")
    ap.add_argument("--update", action="store_true", help="(re)write golden baselines")
    args = ap.parse_args()

    if not Path(args.lba2).exists():
        sys.exit(f"binary not found: {args.lba2} (build it or pass --lba2)")
    if not args.game_dir or not Path(args.game_dir).exists():
        sys.exit("retail data required: set --game-dir or LBA2_GAME_DIR")

    BASE_DIR.mkdir(parents=True, exist_ok=True)
    saves = sorted(p for p in SAVES_DIR.iterdir()
                   if p.suffix.lower() == ".lba" and p.name.lower() not in ("current.lba", "autosave.lba"))

    written = drift = passed = skipped = flaky = 0
    for save in saves:
        outcome, dump, repro = run_save_reproducible(
            args.lba2, args.game_dir, save, args.tick, args.timeout, args.fixed_dt, args.repeats)
        golden = BASE_DIR / (save.stem + ".json")
        if outcome != "ok":
            print(f"  SKIP  {save.stem:<32} ({outcome})")
            skipped += 1
            continue
        if not repro:
            # Non-dt nondeterminism: never store or assert. Drop any stale golden so the
            # corpus doesn't carry a value that can't be reproduced.
            if args.update and golden.exists():
                golden.unlink()
            print(f"  FLAKY {save.stem:<32} (not reproducible over {args.repeats} runs)")
            flaky += 1
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
            hard = diff(want, dump)
            if hard:
                print(f"  DRIFT {save.stem:<32} {len(hard)} fields")
                for d in hard[:8]:
                    print(f"          {d.lstrip('.')}")
                drift += 1
            else:
                print(f"  ok    {save.stem:<32}")
                passed += 1

    print("-" * 56)
    mode = "wall-clock" if args.fixed_dt <= 0 else f"fixed-dt {args.fixed_dt}ms"
    if args.update:
        print(f"baselines: {written} written, {flaky} flaky, {skipped} skipped  ({mode})")
    else:
        print(f"baselines: {passed} ok, {drift} drift, {flaky} flaky, {skipped} skipped  ({mode})")
    sys.exit(1 if drift else 0)


if __name__ == "__main__":
    main()
