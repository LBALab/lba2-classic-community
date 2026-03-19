#!/usr/bin/env python3

"""Filter a NUL-delimited git file list using .clang-format-ignore patterns.

The shell scripts feed tracked C/C++ paths into this helper so there is a
single exclusion list for local formatting and CI enforcement.
"""

from __future__ import annotations

import fnmatch
from pathlib import PurePosixPath
import sys


def load_patterns(path: str) -> list[str]:
    patterns: list[str] = []
    with open(path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            patterns.append(line)
    return patterns


def matches(path: str, pattern: str) -> bool:
    # Directory-style entries such as "build/" should exclude the whole tree.
    if pattern.endswith("/"):
        prefix = pattern.rstrip("/")
        return path == prefix or path.startswith(prefix + "/")

    pure_path = PurePosixPath(path)
    return (
        path == pattern
        or fnmatch.fnmatch(path, pattern)
        or pure_path.match(pattern)
    )


def main() -> int:
    patterns = load_patterns(".clang-format-ignore")

    for raw_path in sys.stdin.buffer.read().split(b"\0"):
        if not raw_path:
            continue

        path = raw_path.decode("utf-8", "surrogateescape")
        if any(matches(path, pattern) for pattern in patterns):
            continue

        sys.stdout.buffer.write(raw_path + b"\0")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
