#!/usr/bin/env python3

"""Check that every setting the camera fixtures reset says its value from before.

`settings_before <prefix>` sets each row whose console name starts with the prefix to
the value the row declares as its value from before (SOURCES/SETTINGS.H), and leaves a
row that has none alone. The camera fixtures run it (CAM_PLAIN in
tests/automation/camlib.sh) to measure from the Auto camera without its later
behaviours, so that a test does not move when one of them ships on by default.

A row that leaves the field out reads the same as one that has none. A new setting that
switched a later behaviour on and forgot it would stay on in every camera fixture, and
nothing would fail: the fixtures would go on passing while measuring something else.
So each row the prefix names says which it is, SETTING_BEFORE(v) or SETTING_BEFORE_NONE,
and this check holds it to that. The prefix is read from CAM_PLAIN, not kept here.

The fixtures cannot run in CI (they need retail game data), and a host test cannot reach
the tables without most of the engine, so this reads them from the source: C initializers
taken apart by brace and parenthesis depth, with comments blanked first. A parser that
drifts fails in the worst direction, finding no rows and reporting clean, so the check
also fails when it finds no row the prefix names, or a table it could not read.

A second prefix is held to more. `settings_before fix_` is the original's rules from the
console (SOURCES/RETAILFIX.H): every retail fix off, then the master row on No. A fix_ row
with no value from before would stay on in it, so a fix_ row must give SETTING_BEFORE(v);
SETTING_BEFORE_NONE is not enough there. No fix_ row at all is fine: the cam_ rows already
catch a parser that finds nothing.

Usage: python3 scripts/ci/check-settings-before.py [--root DIR]   (or: make settings-before-check)
       scripts/ci/check-settings-before-selftest.py exercises it.
"""

from __future__ import annotations

import os
import re
import sys

SCAN_DIRS = ("SOURCES", "LIB386")
SOURCE_SUFFIXES = (".CPP", ".cpp", ".H", ".h", ".c", ".C")
CAMLIB = os.path.join("tests", "automation", "camlib.sh")

# T_SETTING's fields in order (SOURCES/SETTINGS.H); `before` is the last.
CVAR_FIELD = 2
BEFORE_FIELD = 14

CAM_PLAIN_RE = re.compile(r'^CAM_PLAIN="settings_before ([A-Za-z0-9_]+)"[ \t]*$', re.MULTILINE)
# `const` may sit on either side of the type name; a table the reader skips is a hole.
TABLE_DECL_RE = re.compile(r"\bT_SETTING\s+(?:const\s+)?(\w+)\s*\[")
TABLE_INIT_RE = re.compile(r"\bT_SETTING\s+(?:const\s+)?(\w+)\s*\[\s*\]\s*=\s*\{")
BEFORE_OK_RE = re.compile(r"^(SETTING_BEFORE\s*\(.*\)|SETTING_BEFORE_NONE)$", re.DOTALL)
BEFORE_VALUE_RE = re.compile(r"^SETTING_BEFORE\s*\(.*\)$", re.DOTALL)

# The retail fixes' prefix (SOURCES/RETAILFIX.CPP). Its rows must give a value.
FIX_PREFIX = "fix_"
STRING_RE = re.compile(r'^"((?:[^"\\]|\\.)*)"$')


def blank_comments(text: str) -> str:
    """The text with every comment turned to spaces, newlines kept, so offsets and line
    numbers still match the file. String and character literals are left as they are,
    and a comment marker inside one is not a comment."""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            i += 1
            while i < n and text[i] != c:
                i += 2 if text[i] == "\\" else 1
            i += 1
        elif text.startswith("//", i):
            while i < n and text[i] != "\n":
                out[i] = " "
                i += 1
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            for j in range(i, end):
                if text[j] != "\n":
                    out[j] = " "
            i = end
        else:
            i += 1
    return "".join(out)


def skip_literal(text: str, i: int) -> int:
    """Index just past the string or character literal that starts at i."""
    quote = text[i]
    i += 1
    while i < len(text) and text[i] != quote:
        i += 2 if text[i] == "\\" else 1
    return i + 1


def read_rows(text: str, start: int):
    """The rows of the table whose opening brace is at start, as (offset, [fields]).

    Fields are split on commas outside any nested brace or parenthesis, so a macro call
    such as SETTING_BEFORE(0) stays one field. Returns None if the braces never close."""
    rows = []
    i, n = start + 1, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            i = skip_literal(text, i)
        elif c == "}":
            return rows
        elif c == "{":
            row_at = i
            fields, field_at, depth = [], i + 1, 0
            i += 1
            while i < n:
                c = text[i]
                if c in "\"'":
                    i = skip_literal(text, i)
                    continue
                if c in "({":
                    depth += 1
                elif c in ")}":
                    if depth == 0:
                        break
                    depth -= 1
                elif c == "," and depth == 0:
                    fields.append(text[field_at:i].strip())
                    field_at = i + 1
                i += 1
            if i >= n:
                return None
            last = text[field_at:i].strip()
            if last:
                fields.append(last)
            rows.append((row_at, fields))
            i += 1
        else:
            i += 1
    return None


def cvar_of(field: str) -> str | None:
    """The console name a cvar field holds: its string literal, adjacent pieces joined.
    None for NULL, or anything else this check cannot read."""
    pieces = re.findall(r'"((?:[^"\\]|\\.)*)"', field)
    if not pieces or re.sub(r'"(?:[^"\\]|\\.)*"', "", field).strip():
        return None
    return "".join(pieces)


def source_files(root: str):
    for top in SCAN_DIRS:
        for dirpath, dirnames, filenames in os.walk(os.path.join(root, top)):
            dirnames.sort()
            for fn in sorted(filenames):
                if fn.endswith(SOURCE_SUFFIXES):
                    yield os.path.join(dirpath, fn)


def main(argv: list[str]) -> int:
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if len(argv) == 2 and argv[0] == "--root":
        root = argv[1]
    elif argv:
        print("usage: check-settings-before.py [--root DIR]", file=sys.stderr)
        return 2

    try:
        with open(os.path.join(root, CAMLIB), encoding="utf-8") as fh:
            prefixes = CAM_PLAIN_RE.findall(fh.read())
    except OSError as e:
        print(f"settings-before: cannot read {CAMLIB}: {e}")
        return 1
    if len(prefixes) != 1:
        print(f"settings-before: {CAMLIB} must set CAM_PLAIN once, as "
              f'CAM_PLAIN="settings_before <prefix>"; found {len(prefixes)}. '
              "This check reads its prefix from there.")
        return 1
    prefix = prefixes[0]

    errors, tables, named, fixes = [], 0, 0, 0
    for path in source_files(root):
        rel = os.path.relpath(path, root)
        with open(path, encoding="utf-8", errors="replace") as fh:
            text = blank_comments(fh.read())
        decls = TABLE_DECL_RE.findall(text)
        inits = list(TABLE_INIT_RE.finditer(text))
        if len(decls) != len(inits):
            errors.append(f"{rel}: {len(decls)} T_SETTING table(s) declared, {len(inits)} with an "
                          "initializer this check can read; out of step, so it would miss rows")
            continue
        for m in inits:
            rows = read_rows(text, m.end() - 1)
            if rows is None:
                errors.append(f"{rel}: table {m.group(1)}: braces never close; cannot read its rows")
                continue
            tables += 1
            for at, fields in rows:
                line = text.count("\n", 0, at) + 1
                if len(fields) <= CVAR_FIELD:
                    errors.append(f"{rel}:{line}: {m.group(1)}: a row with {len(fields)} fields; "
                                  "cannot find its console name")
                    continue
                cvar = cvar_of(fields[CVAR_FIELD])
                if cvar is None and fields[CVAR_FIELD] != "NULL":
                    errors.append(f"{rel}:{line}: {m.group(1)}: console name is "
                                  f"`{fields[CVAR_FIELD]}`, not a string or NULL; cannot tell "
                                  f"whether `settings_before {prefix}` names it")
                    continue
                if cvar is None:
                    continue
                before = fields[BEFORE_FIELD] if len(fields) > BEFORE_FIELD else None
                if cvar.startswith(FIX_PREFIX):
                    fixes += 1
                    if before is None or not BEFORE_VALUE_RE.match(before):
                        said = "leaves it out" if before is None else f"gives `{before}`"
                        errors.append(f"{rel}:{line}: {cvar} {said}. A retail fix must give "
                                      "SETTING_BEFORE(v), the value that switches it off: "
                                      f"`settings_before {FIX_PREFIX}` is the original's rules")
                if not cvar.startswith(prefix):
                    continue
                named += 1
                if before is None or not BEFORE_OK_RE.match(before):
                    said = "leaves it out" if before is None else f"gives `{before}`"
                    errors.append(f"{rel}:{line}: {cvar} {said}. Say its value from before: "
                                  "SETTING_BEFORE(v) for the value that switches its behaviour "
                                  "off, or SETTING_BEFORE_NONE (SOURCES/SETTINGS.H)")

    if not errors and named == 0:
        errors.append(f"no T_SETTING row has a console name starting `{prefix}` "
                      f"(read {tables} table(s)); the parser has drifted, or the prefix in "
                      f"{CAMLIB} names nothing")

    for e in errors:
        print(e)
    if errors:
        print(f"\nsettings-before: {len(errors)} problem(s). The camera fixtures run "
              f"`settings_before {prefix}`; a row it names that does not say its value from "
              "before stays at its default in every one of them.")
        return 1
    print(f"settings-before: clean ({named} row(s) named `{prefix}*` and {fixes} named "
          f"`{FIX_PREFIX}*` in {tables} table(s), each says its value from before)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
