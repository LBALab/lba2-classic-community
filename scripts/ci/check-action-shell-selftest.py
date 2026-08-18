#!/usr/bin/env python3
"""Self-test for check-action-shell.py.

The checker hand-rolls a block-scalar parser so that it stays stdlib-only, and a hand-rolled parser
that drifts fails in the worst direction available: it finds no blocks, checks nothing, and reports
clean. Most of the cases below pin that direction rather than the happy path.

Fixtures are written to a temporary directory instead of living in the tree. A fixture named
action.yml under .github/actions/ would be picked up by the real checker and by actionlint, so the
suite would break the thing it tests.

  scripts/ci/check-action-shell-selftest.py
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CHECKER = os.path.join(ROOT, "scripts", "ci", "check-action-shell.py")

CLEAN = """\
name: Clean
description: One well-formed bash block.
runs:
  using: composite
  steps:
    - name: Do the thing
      shell: bash
      env:
        FROM_ENV: value
      run: |
        set -euo pipefail
        echo "${FROM_ENV}"
"""

# SC2046: word splitting on an unquoted substitution. Warning severity, so it clears the -S warning
# bar the *.sh job also uses.
FINDING = """\
name: Finding
description: A block with a warning-level shellcheck finding.
runs:
  using: composite
  steps:
    - name: Do the thing
      shell: bash
      run: |
        set -euo pipefail
        echo $(ls /tmp) > /dev/null
"""

# The expression is not shell. If it reached shellcheck unsubstituted it would parse as an unclosed
# brace expansion and the block would fail for a reason that is not a defect.
EXPRESSIONS = """\
name: Expressions
description: GitHub expressions inside the body.
runs:
  using: composite
  steps:
    - name: Do the thing
      shell: bash
      run: |
        set -euo pipefail
        echo "built ${{ inputs.thing }} on ${{ runner.os }}"
"""

NOT_BLOCK_SCALAR = """\
name: Inline run
description: A run: shape the parser does not accept.
runs:
  using: composite
  steps:
    - name: Do the thing
      shell: bash
      run: echo inline
"""

NO_SHELL = """\
name: No shell
description: A run: with no shell: to interpret it.
runs:
  using: composite
  steps:
    - name: Do the thing
      run: |
        echo hello
"""

USES_ONLY = """\
name: Uses only
description: No shell anywhere, which is legitimate.
runs:
  using: composite
  steps:
    - uses: actions/checkout@v5
"""

# A body line whose own text starts with `run:`. The crude key count sees two, the parser sees one,
# and the checker must refuse rather than report on a file it has mis-read.
PARSER_DRIFT = """\
name: Drift
description: A script line that looks like a run key.
runs:
  using: composite
  steps:
    - name: Do the thing
      shell: bash
      run: |
        echo one
        run: this is script text, not a key
"""

# shellcheck suppresses SC2164 when set -e is present. The wrapper must not second-guess the tool.
CD_UNDER_SET_E = """\
name: Bare cd
description: A cd that stock shellcheck deliberately does not flag under set -e.
runs:
  using: composite
  steps:
    - name: Do the thing
      shell: bash
      run: |
        set -euo pipefail
        cd /tmp
"""

# name, fixture, expected exit, expected substring in combined output
CASES = [
    ("clean block passes", CLEAN, 0, "clean"),
    ("warning-level finding fails", FINDING, 1, "SC2046"),
    ("finding names its step", FINDING, 1, "[step: Do the thing]"),
    ("github expressions do not trip the parse", EXPRESSIONS, 0, "clean"),
    ("non-block-scalar run: errors", NOT_BLOCK_SCALAR, 1, "only block scalars"),
    ("run: without shell: errors", NO_SHELL, 1, "no `shell:`"),
    ("uses-only action passes with a note", USES_ONLY, 0, "nothing to check"),
    ("parser/file disagreement errors", PARSER_DRIFT, 1, "out of step"),
    ("bare cd under set -e is not flagged", CD_UNDER_SET_E, 0, "clean"),
]


def run_case(text):
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "action.yml")
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(text)
        proc = subprocess.run(
            [sys.executable, CHECKER, path], capture_output=True, text=True, cwd=ROOT,
        )
        return proc.returncode, proc.stdout + proc.stderr


def main():
    failures = 0
    for name, fixture, want_rc, want_text in CASES:
        rc, out = run_case(fixture)
        ok = rc == want_rc and want_text in out
        print(f"{'PASS' if ok else 'FAIL'}  {name}")
        if not ok:
            failures += 1
            print(f"      expected exit {want_rc} and {want_text!r}")
            print(f"      got exit {rc}, output:")
            for line in out.strip().splitlines():
                print(f"        {line}")
    total = len(CASES)
    print(f"\n{total - failures}/{total} passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
