#!/usr/bin/env python3
"""Self-test for check-settings-before.py.

The checker takes C initializers apart by hand so that it stays stdlib-only, and a
hand-rolled parser that drifts fails in the worst direction available: it finds no rows,
checks nothing, and reports clean. Most of the cases below pin that direction, or a row
read wrongly because a comma, brace or comment sat somewhere a field split would trip on.

Each case is a small tree written to a temporary directory: one source file under SOURCES/
and the camlib.sh the prefix is read from. Fixtures in the real tree would be read by the
real checker.

  scripts/ci/check-settings-before-selftest.py
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CHECKER = os.path.join(ROOT, "scripts", "ci", "check-settings-before.py")

CAMLIB = 'CAM_PLAIN="settings_before cam_"\n'

HEAD = "static const T_SETTING Rows[] = {\n"
TAIL = "};\n"


def row(cvar, tail=""):
    """A row with every field up to help, then whatever tail adds."""
    return (f'    {{"Key", NULL, {cvar}, &Value, SETTING_CLAMP, 0, 0, 1, NULL, NULL, NULL, NULL,\n'
            f'     "help"{tail}}},\n')


SAYS_BEFORE = row('"cam_a"', ", SETTING_RECORDED, SETTING_BEFORE(0)")
SAYS_NONE = row('"cam_b"', ", SETTING_RECORDED, SETTING_BEFORE_NONE")
LEAVES_OUT = row('"cam_c"')

CLEAN = HEAD + SAYS_BEFORE + SAYS_NONE + TAIL
MISSING = HEAD + SAYS_BEFORE + LEAVES_OUT + TAIL
OTHER_PREFIX_MISSING = HEAD + SAYS_BEFORE + row('"pad_x"') + TAIL
NULL_CVAR = HEAD + SAYS_BEFORE + row("NULL") + TAIL
SHORT_ROW = HEAD + SAYS_BEFORE + '    {"MenuMouse", NULL, NULL, &F, SETTING_TRUTHY, TRUE, 0, 1, NULL, NULL, NULL, NULL},\n' + TAIL
# The literal is what leaving the field out compiles to; spelled out it still does not
# say which of the two it means.
LITERAL = HEAD + SAYS_BEFORE + row('"cam_d"', ", SETTING_RECORDED, {1, 0}") + TAIL
# Only the record column given: one short of the before field.
RECORD_ONLY = HEAD + SAYS_BEFORE + row('"cam_e"', ", SETTING_RECORDED") + TAIL
# A row commented out must not count, and must not end the table early.
COMMENTED = (HEAD + SAYS_BEFORE + '    /* {"K", NULL, "cam_old", &V}, } */\n'
             + '    // {"K", NULL, "cam_older", &V},\n' + SAYS_NONE + TAIL)
# Commas, braces and comment markers inside a help string are text, not structure.
TRICKY_HELP = (HEAD + '    {"K", NULL, "cam_f", &V, SETTING_CLAMP, 0, 0, 1, NULL, NULL, NULL, NULL,\n'
               + '     "a, b {c} /* d */ // e", SETTING_RECORDED, SETTING_BEFORE(0)},\n' + TAIL)
# The cvar on a line of its own after a comment, and a before value with nested parens.
SPREAD = (HEAD + '    {"K", NULL, /* console */\n     "cam_g", &V, SETTING_CLAMP, 0, 0, 1,\n'
          + '     NULL, NULL, NULL, NULL, "h", SETTING_RECORDED, SETTING_BEFORE((1 + 2))},\n' + TAIL)
# Missing rows reported for a second table in the same file too.
TWO_TABLES = CLEAN + "static const T_SETTING More[] = {\n" + LEAVES_OUT + TAIL
NO_ROWS_NAMED = HEAD + row('"pad_x"') + TAIL
# A table declared in a form the reader does not take must not be skipped quietly.
UNREAD_TABLE = CLEAN + "static const T_SETTING Later[2];\n"
UNCLOSED = HEAD + SAYS_BEFORE
CVAR_MACRO = HEAD + SAYS_BEFORE + row("CAM_NAME") + TAIL
# `const` after the type name is the same table and must be read, not skipped.
CONST_AFTER = CLEAN + "static T_SETTING const More[] = {\n" + LEAVES_OUT + TAIL

# A retail fix's row must give a value; "none" would leave it on under retail rules.
FIX_VALUE = CLEAN + "static const T_SETTING Fixes[] = {\n" + row('"fix_a"', ", SETTING_RECORDED, SETTING_BEFORE(0)") + TAIL
FIX_NONE = CLEAN + "static const T_SETTING Fixes[] = {\n" + row('"fix_b"', ", SETTING_RECORDED, SETTING_BEFORE_NONE") + TAIL
FIX_MISSING = CLEAN + "static const T_SETTING Fixes[] = {\n" + row('"fix_c"') + TAIL

# name, source, camlib, expected exit, expected substring in combined output
CASES = [
    ("rows that say a value or none pass", CLEAN, CAMLIB, 0, "clean (2 row(s)"),
    ("a fix_ row with a value passes", FIX_VALUE, CAMLIB, 0, "and 1 named `fix_*`"),
    ("a fix_ row saying none fails", FIX_NONE, CAMLIB, 1, "fix_b gives `SETTING_BEFORE_NONE`"),
    ("a fix_ row leaving it out fails", FIX_MISSING, CAMLIB, 1, "fix_c leaves it out"),
    ("a cam_ row that leaves it out fails", MISSING, CAMLIB, 1, "cam_c leaves it out"),
    ("the failure names the file and line", MISSING, CAMLIB, 1, "SOURCES/T.CPP:5: cam_c"),
    ("a row the prefix does not name is not held to it", OTHER_PREFIX_MISSING, CAMLIB, 0, "clean (1 row(s)"),
    ("the prefix comes from CAM_PLAIN", OTHER_PREFIX_MISSING,
     'CAM_PLAIN="settings_before pad_"\n', 1, "pad_x leaves it out"),
    ("a row with no console name is not held to it", NULL_CVAR, CAMLIB, 0, "clean (1 row(s)"),
    ("a row that stops before help is read", SHORT_ROW, CAMLIB, 0, "clean (1 row(s)"),
    ("a literal in place of the macro fails", LITERAL, CAMLIB, 1, "cam_d gives `{1, 0}`"),
    ("the record column alone is not enough", RECORD_ONLY, CAMLIB, 1, "cam_e leaves it out"),
    ("commented-out rows are not rows", COMMENTED, CAMLIB, 0, "clean (2 row(s)"),
    ("commas and braces in a string are text", TRICKY_HELP, CAMLIB, 0, "clean (1 row(s)"),
    ("a row spread over lines with nested parens", SPREAD, CAMLIB, 0, "clean (1 row(s)"),
    ("every table in a file is read", TWO_TABLES, CAMLIB, 1, "cam_c leaves it out"),
    ("a table written `T_SETTING const` is read", CONST_AFTER, CAMLIB, 1, "cam_c leaves it out"),
    ("no row named by the prefix is an error, not clean", NO_ROWS_NAMED, CAMLIB, 1, "has drifted"),
    ("a table it cannot read is an error", UNREAD_TABLE, CAMLIB, 1, "out of step"),
    ("unclosed braces are an error", UNCLOSED, CAMLIB, 1, "never close"),
    ("a console name it cannot read is an error", CVAR_MACRO, CAMLIB, 1, "not a string or NULL"),
    ("no CAM_PLAIN is an error", CLEAN, "CAM_SAVE=x\n", 1, "must set CAM_PLAIN once"),
    ("CAM_PLAIN in another shape is an error", CLEAN, 'CAM_PLAIN="settings_before cam_; camtrace 1"\n',
     1, "must set CAM_PLAIN once"),
]


def run_case(source, camlib):
    with tempfile.TemporaryDirectory() as tmp:
        os.makedirs(os.path.join(tmp, "SOURCES"))
        os.makedirs(os.path.join(tmp, "tests", "automation"))
        with open(os.path.join(tmp, "SOURCES", "T.CPP"), "w", encoding="utf-8") as fh:
            fh.write("/* fixture */\n" + source)
        with open(os.path.join(tmp, "tests", "automation", "camlib.sh"), "w", encoding="utf-8") as fh:
            fh.write(camlib)
        proc = subprocess.run(
            [sys.executable, CHECKER, "--root", tmp], capture_output=True, text=True, cwd=ROOT,
        )
        return proc.returncode, proc.stdout + proc.stderr


def main():
    failures = 0
    for name, source, camlib, want_rc, want_text in CASES:
        rc, out = run_case(source, camlib)
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
