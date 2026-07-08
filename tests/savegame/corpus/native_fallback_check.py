#!/usr/bin/env python3
"""
Regression check for the legacy native-save read path (issue #64 / PR #386).

Drives `lba2cc` over the native-format fixtures in saves/native_port64/ and a
sample of the retail-wire corpus, and asserts each outcome. Requires retail game
data, so it is not a public-CI host test; run it locally or in the retail-gated
job.

  tests/savegame/corpus/native_fallback_check.py [--lba2 PATH] [--game-dir PATH]

Checks:
  native fixtures, --load    -> no SIGSEGV (the crash-regression guard: an early
                                wire-first reader crashed on the ambiguous
                                multi_object save)
  native fixtures, auto      -> load (flagload=0): read correctly as native
  native fixtures, ABI=64    -> load (flagload=0): forced-native read is valid
  steam corpus (sample), ABI=64 -> reject: forced-native cannot read wire records
"""
import argparse, os, re, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
KV_RE = re.compile(r"flagload=(-?\d+)")


def find_lba2(cli):
    cands = [cli, os.environ.get("LBA2CC")]
    cands += [str(ROOT / d / "SOURCES" / "lba2cc") for d in ("build", "build-nullsnd")]
    cands += [str(p) for p in ROOT.glob("out/build/*/SOURCES/lba2cc")]
    for c in cands:
        if c and Path(c).is_file() and os.access(c, os.X_OK):
            return c
    return None


def find_game_dir(cli):
    for c in [cli, os.environ.get("LBA2_GAME_DIR"),
              str(ROOT.parent / "LBA2" / "Common"), str(ROOT / "data")]:
        if not c:
            continue
        d = Path(c)
        if (d / "lba2.hqr").exists() or (d / "LBA2.HQR").exists():
            return c
    return None


def _env(abi):
    e = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    if abi:
        e["LBA2_SAVE_LOAD_ABI"] = abi
    else:
        e.pop("LBA2_SAVE_LOAD_ABI", None)
    return e


def flagload(lba2, game_dir, save, abi):
    p = subprocess.run([lba2, "--game-dir", game_dir, "--save-load-test", save],
                       env=_env(abi), capture_output=True, timeout=120, text=True)
    m = KV_RE.findall(p.stdout)
    return int(m[-1]) if m else None


def load_rc(lba2, game_dir, save, abi):
    """Full --load path; returns the exit code (139 = SIGSEGV)."""
    p = subprocess.run([lba2, "--game-dir", game_dir, "--load", save, "--tick", "5", "--exit"],
                       env=_env(abi), capture_output=True, timeout=120)
    return p.returncode


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lba2")
    ap.add_argument("--game-dir")
    args = ap.parse_args()

    lba2 = find_lba2(args.lba2)
    game_dir = find_game_dir(args.game_dir)
    if not lba2 or not game_dir:
        print("SKIP: need a built lba2cc and retail game data "
              "(--lba2, --game-dir, or LBA2CC / LBA2_GAME_DIR)")
        return 0

    nat = HERE / "saves" / "native_port64"
    steam = HERE / "saves" / "steam_classic_2023"
    natives = [nat / "multi_object.lba", nat / "single_object.lba"]

    checks = []  # (ok, description)
    for s in natives:
        if not s.exists():
            checks.append((False, f"missing {s.name}"))
            continue
        rc = load_rc(lba2, game_dir, str(s), None)
        checks.append((rc != 139, f"{s.name} --load: no SIGSEGV (rc={rc})"))
        checks.append((flagload(lba2, game_dir, str(s), None) == 0,
                       f"{s.name} auto: loads"))
        checks.append((flagload(lba2, game_dir, str(s), "64") == 0,
                       f"{s.name} ABI=64: loads"))

    # Forced-native must reject retail wire saves (audit).
    for s in sorted(steam.glob("*.LBA"))[:5]:
        fl = flagload(lba2, game_dir, str(s), "64")
        checks.append((fl not in (0, 1), f"retail {s.name} ABI=64: rejects (flagload={fl})"))

    fails = 0
    for ok, desc in checks:
        print(f"{'PASS' if ok else 'FAIL'}  {desc}")
        fails += 0 if ok else 1
    print(f"\n{'OK' if fails == 0 else 'FAILED'}: {len(checks) - fails}/{len(checks)} checks")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
