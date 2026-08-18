#!/usr/bin/env python3
"""Run shellcheck over the `run:` blocks of composite actions.

actionlint shells out to shellcheck for every `run:` block it finds, which is why the Lint workflow
installs both. It only walks `.github/workflows/`, so the shell inside `.github/actions/*/action.yml`
is checked by nothing: not actionlint, which never opens the file, and not the shellcheck job, which
enumerates `*.sh`. That left the composite actions as the least-checked code in the repo while being
the code that decides whether anything else runs. Three defects landed in that blind spot before this
existed.

This closes it the same way actionlint does: pull each block out, hand it to shellcheck, and map the
findings back to the line they came from so the output is clickable.

`${{ ... }}` expressions are substituted with a placeholder identifier before the block is checked.
They are not shell, and shellcheck would parse `${{` as an unclosed brace expansion.

A step's `env:` names are declared ahead of the body so a variable that arrives from the environment
does not read as undefined. Passing inputs that way is the pattern the actions use, and flagging it
would train people out of it.

Anything this cannot parse is an error, never a skip. A checker that silently stops checking is worse
than no checker: the run still goes green. That includes a cross-check that the number of `run:` keys
in the file matches the number of blocks parsed out of it. The one false positive that can produce is
a script line whose own text begins with `run:`, which errors rather than passing; move the line or
teach the parser, and note which way the check is deliberately biased.

What this does not do is catch logic. shellcheck reads quoting, word splitting and misused
constructs. It has nothing to say about a guard that `set -e` makes unreachable, a tool that is
absent on one runner, or a killed apt leaving dpkg mid-transaction, which is the class that has
actually bitten. Treat a green run here as the floor, not the ceiling.

  scripts/ci/check-action-shell.py             # check every composite action
  scripts/ci/check-action-shell.py path/to/action.yml
"""
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ACTIONS_DIR = os.path.join(".github", "actions")

# shellcheck understands these; anything else in a `shell:` (pwsh, python) it cannot read.
SHELLS = {"bash", "sh"}

EXPR = re.compile(r"\$\{\{.*?\}\}", re.DOTALL)
STEP_NAME = re.compile(r"^(\s*)-?\s*name:\s*(.+?)\s*$")


def find_actions(args):
    """Every action.yml under .github/actions, or the ones named on the command line."""
    if args:
        return args
    out = []
    for dirpath, _dirnames, filenames in os.walk(os.path.join(ROOT, ACTIONS_DIR)):
        for fn in filenames:
            if fn in ("action.yml", "action.yaml"):
                out.append(os.path.relpath(os.path.join(dirpath, fn), ROOT))
    return sorted(out)


def parse_blocks(path):
    """Yield (step_name, shell, env_names, first_body_line, body_lines) for each `run:` block.

    Hand-rolled rather than via PyYAML: every other script under scripts/ci/ is stdlib-only, so a
    local run needs no virtualenv and the Lint workflow needs no install step. The parser only
    accepts the shapes the repo actually writes, and raises on anything else.
    """
    with open(os.path.join(ROOT, path), encoding="utf-8") as fh:
        lines = fh.read().splitlines()

    blocks = []
    i = 0
    step_name = "<unnamed step>"
    while i < len(lines):
        line = lines[i]

        m = STEP_NAME.match(line)
        if m:
            step_name = m.group(2)

        stripped = line.strip()
        if stripped.startswith("run:"):
            rest = stripped[len("run:"):].strip()
            if rest not in ("|", "|-", ">", ">-"):
                raise ValueError(
                    f"{path}:{i + 1}: only block scalars are supported for `run:`, got `run: {rest}`. "
                    "Rewrite it as `run: |` or teach this parser the new shape."
                )
            indent = len(line) - len(line.lstrip())
            # Walk back for this step's `shell:` and `env:`, which sit at the same indent.
            shell, env_names = scan_step_context(lines, i, indent)
            body, first, i = take_block(lines, i + 1, indent)
            blocks.append((step_name, shell, env_names, first, body))
            continue
        i += 1
    return blocks


def scan_step_context(lines, run_idx, indent):
    """Find the `shell:` and `env:` keys belonging to the step whose `run:` is at run_idx."""
    shell = None
    env_names = []
    j = run_idx - 1
    while j >= 0:
        line = lines[j]
        if not line.strip():
            j -= 1
            continue
        cur = len(line) - len(line.lstrip())
        if cur < indent:
            break  # left the step's key block
        if cur == indent:
            s = line.strip()
            if s.startswith("shell:"):
                shell = s[len("shell:"):].strip()
            elif s.startswith("env:"):
                # Keys are the more-indented lines directly below.
                k = j + 1
                while k < len(lines):
                    kline = lines[k]
                    if not kline.strip():
                        k += 1
                        continue
                    kindent = len(kline) - len(kline.lstrip())
                    if kindent <= indent:
                        break
                    key = kline.strip().split(":", 1)[0].strip()
                    if key:
                        env_names.append(key)
                    k += 1
            elif s.startswith("- name:") or s.startswith("- "):
                break  # start of this step
        j -= 1
    return shell, env_names


def take_block(lines, start, indent):
    """Collect the block-scalar body starting at `start`, returning (body, first_line_no, next_idx)."""
    body = []
    i = start
    body_indent = None
    while i < len(lines):
        line = lines[i]
        if not line.strip():
            body.append("")
            i += 1
            continue
        cur = len(line) - len(line.lstrip())
        if cur <= indent:
            break
        if body_indent is None:
            body_indent = cur
        body.append(line[body_indent:])
        i += 1
    # Trailing blanks belong to the YAML, not the script.
    while body and not body[-1].strip():
        body.pop()
    return body, start + 1, i


def check(path, keep_going):
    problems = 0
    try:
        blocks = parse_blocks(path)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    # An action built only from `uses:` steps has no shell, and that is fine. What is not fine is the
    # parser finding nothing in a file that plainly contains `run:` keys: that reads as "clean" while
    # checking nothing. Count them independently and make the two agree.
    with open(os.path.join(ROOT, path), encoding="utf-8") as fh:
        crude = sum(1 for line in fh if line.strip().startswith("run:"))
    if crude != len(blocks):
        print(f"error: {path}: found {crude} `run:` key(s) but parsed {len(blocks)} block(s). "
              "The parser is out of step with the file; fix it rather than trusting this result.",
              file=sys.stderr)
        return 1
    if not blocks:
        print(f"note: {path}: no `run:` blocks, nothing to check")
        return 0

    for step_name, shell, env_names, first_line, body in blocks:
        if shell is None:
            print(f"error: {path}: step '{step_name}' has a `run:` but no `shell:`", file=sys.stderr)
            problems += 1
            continue
        if shell not in SHELLS:
            print(f"note: {path}: step '{step_name}' uses shell '{shell}', which shellcheck "
                  f"cannot read; not checked")
            continue

        # One prelude line, so mapping back is a constant offset.
        prelude = "#!/usr/bin/env " + shell
        if env_names:
            prelude += "\n" + " ".join(f'{n}=""' for n in env_names)
        offset = len(prelude.splitlines())

        script = prelude + "\n" + EXPR.sub("GH_EXPR", "\n".join(body)) + "\n"
        proc = subprocess.run(
            ["shellcheck", "-s", shell, "-S", "warning", "-f", "gcc", "-"],
            input=script, capture_output=True, text=True, cwd=ROOT,
        )
        if proc.returncode == 0:
            continue

        problems += 1
        for line in proc.stdout.splitlines():
            # gcc format: -:LINE:COL: level: message
            m = re.match(r"^-:(\d+):(\d+):\s*(.*)$", line)
            if m:
                real = first_line + int(m.group(1)) - offset - 1
                print(f"{path}:{real}:{m.group(2)}: {m.group(3)}  [step: {step_name}]")
            else:
                print(line)
        if proc.stderr.strip():
            print(proc.stderr.strip(), file=sys.stderr)
        if not keep_going:
            break

    return 1 if problems else 0


def main(argv):
    if not shutil.which("shellcheck"):
        print("error: shellcheck is not on PATH", file=sys.stderr)
        return 1
    paths = find_actions(argv)
    if not paths:
        print(f"note: no composite actions found under {ACTIONS_DIR}")
        return 0
    rc = 0
    for path in paths:
        rc |= check(path, keep_going=True)
    if rc == 0:
        print(f"ok: {len(paths)} composite action(s) clean")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
