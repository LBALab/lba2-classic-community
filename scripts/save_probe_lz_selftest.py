#!/usr/bin/env python3
"""
Golden vectors for ExpandLZ (MinBloc=2), same cases as tests/SYSTEM/test_lz.cpp.
Requires built target save_decompress. Run from repo root:

  cmake --build build --target save_decompress
  python3 scripts/save_probe_lz_selftest.py
"""

from __future__ import annotations

import glob
import os
import subprocess
import sys
from typing import Optional


def find_save_decompress() -> Optional[str]:
    env = os.environ.get("LBA2_SAVE_DECOMPRESS")
    if env and os.path.isfile(env):
        if os.name == "nt" or os.access(env, os.X_OK):
            return env
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.normpath(os.path.join(here, ".."))
    for c in [
        os.path.join(root, "build", "tools", "save_decompress"),
        *sorted(glob.glob(os.path.join(root, "out", "build", "*", "tools", "save_decompress"))),
    ]:
        if os.path.isfile(c) and (os.name == "nt" or os.access(c, os.X_OK)):
            return c
    return None


def run_expand(helper: str, comp: bytes, decomp_size: int) -> bytes:
    proc = subprocess.run(
        [helper, str(decomp_size)],
        input=comp,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.decode("utf-8", errors="replace"))
    if len(proc.stdout) != decomp_size:
        raise RuntimeError(f"len {len(proc.stdout)} != {decomp_size}")
    return proc.stdout


def main() -> int:
    h = find_save_decompress()
    if not h:
        print("save_probe_lz_selftest: skip (build save_decompress first)", file=sys.stderr)
        return 0

    tests = [
        ("test_all_literals", bytes([0xFF]) + b"ABCDEFGH", 8, b"ABCDEFGH"),
        ("test_single_literal", bytes([0x01, ord("X")]), 1, b"X"),
        ("test_back_reference", bytes([0x07, ord("A"), ord("B"), ord("C")]), 3, b"ABC"),
    ]
    for name, comp, sz, exp in tests:
        got = run_expand(h, comp, sz)
        if got != exp:
            print(f"FAIL {name}: expected {exp!r} got {got!r}", file=sys.stderr)
            return 1
        print(f"ok {name}")
    print("save_probe_lz_selftest: all ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
