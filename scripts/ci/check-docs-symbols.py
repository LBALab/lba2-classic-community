#!/usr/bin/env python3
"""Check that a symbol a doc attributes to a source file is actually in that file.

Docs go stale the moment code moves, and the existing link check cannot see it: the path still
resolves, so `ReadConfigFile() in SOURCES/PERSO.CPP` stays green long after the function left.
That has happened on every extraction so far, and a reader following the doc greps the named file
and finds nothing.

What is checked: a markdown link to a file under SOURCES/ or LIB386/, and the backticked identifiers
on the same line. Each identifier must appear in the linked file.

The useful half is the report. When an identifier is missing from the file the doc names but is
defined somewhere else in the tree, that is rot with a known fix, and the checker says where it
went. An identifier found nowhere is more likely prose than a defect, so it is reported separately
and does not fail the run.

  scripts/ci/check-docs-symbols.py            # check every doc
  scripts/ci/check-docs-symbols.py docs/CONFIG.md   # check some
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CODE_DIRS = ("SOURCES", "LIB386")
CODE_EXT = (".CPP", ".H", ".C", ".ASM", ".cpp", ".h", ".c")
# The shared-global pair: anything defined here is read from all over, so a doc naming a
# reader rather than the definition is describing the code correctly.
BUS = ("SOURCES/GLOBAL.CPP", "SOURCES/C_EXTERN.H")

# A markdown link whose target lands in the source tree.
LINK = re.compile(r"\[[^\]]*\]\(([^)\s]+)\)")
# A backticked identifier, optionally with () or a trailing ::/. member. Deliberately narrow:
# an identifier must start with a letter or underscore and contain no spaces.
TICKED = re.compile(r"`([A-Za-z_][A-Za-z0-9_]*)(?:\(\))?`")

# Words that read as identifiers but are language or build vocabulary rather than symbols to find.
NOT_SYMBOLS = {
    "if", "else", "for", "while", "return", "struct", "union", "enum", "typedef", "static",
    "const", "void", "int", "char", "float", "double", "unsigned", "signed", "sizeof", "extern",
    "true", "false", "TRUE", "FALSE", "NULL", "define", "include", "ifdef", "ifndef", "endif",
    "make", "cmake", "ctest", "bash", "sh", "git", "grep", "sed", "awk", "python3",
}


# Only a line that *attributes* a symbol to a file is checked, and only in the one shape that says so
# unambiguously: the symbol, then "in" or a bare parenthesis, then the link.
#
#   `ReadConfigFile()` in [SOURCES/CONFIG_FILE.CPP](../SOURCES/CONFIG_FILE.CPP)
#   `FollowCamHDExcess()` ([FOLLOWCAM.CPP](../SOURCES/FOLLOWCAM.CPP))
#
# Anything looser was measured and rejected. Treating every symbol on a line with a link as attributed
# reported 38 references, most of them context rather than error ("`Version` is not in AMBIANCE.CPP"
# is true and means nothing). Treating a table row's later cells as attributed to its link is wrong
# too: rows here name a file in one cell and unrelated console verbs in another. Table rows are not
# checked at all, which gives up some real rot to keep every report worth reading.
ATTRIB_IN = re.compile(r"^\s*in\s+\[?[^\]]*\]?\(([^)\s]+)\)")
ATTRIB_PAREN = re.compile(r"^\s*\(\s*\[?[^\]]*\]?\(([^)\s]+)\)")


def attributes(line, match, is_row):
    """True when this occurrence of the symbol is attributing it to the linked file."""
    if is_row:
        # A file-map row names its file in one of the leading cells and that file's symbols after:
        # "| Concept | SOURCES/FOO.CPP | Bar, Baz |". A row whose link turns up further along is
        # prose that happens to be in a table, and its other cells are about something else.
        cells = line.split("|")
        link_cell = next((i for i, c in enumerate(cells) if LINK.search(c)), None)
        if link_cell is None or link_cell > 2:
            return None
        sym_cell = line.count("|", 0, match.start())
        return "mentioned" if sym_cell > link_cell else None
    tail = line[match.end():match.end() + 60]
    if ATTRIB_IN.match(tail):
        return "defined"
    if ATTRIB_PAREN.match(tail):
        return "mentioned"
    return None


# The same shapes as defines(), written to capture the name so the index is built in one pass per
# file. Testing every token in a file against every pattern is quadratic and does not finish here.
DEFINES_CAPTURING = tuple(re.compile(p, re.MULTILINE) for p in (
    r"^[A-Za-z_][A-Za-z0-9_ \t\*&]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\(",
    r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\b",
    r"^\s*(?:static|const|extern|struct|union|enum|unsigned|signed)*[\sA-Za-z_][A-Za-z0-9_\s\*]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[|=|;|,)",
    r"\}\s*([A-Za-z_][A-Za-z0-9_]*)\s*;",
    r"^\s*(?:typedef\s+)?(?:struct|union|enum)\s+([A-Za-z_][A-Za-z0-9_]*)\b",
))


def defines(body, sym):
    """True when `body` looks like it *defines* sym, rather than merely calling or including it.

    A plain substring search is not enough, and getting this wrong makes the whole check pointless:
    after ReadConfigFile moved to CONFIG_FILE.CPP the token was still all over PERSO.CPP, at the call
    site, so a doc that still named PERSO.CPP looked correct. These patterns are deliberately loose,
    since a missed definition only costs a false report, which the caller sees and can judge.
    """
    pats = (
        r"^[A-Za-z_][A-Za-z0-9_ \t\*&]*\b%s\s*\(" % sym,          # function definition at column 0
        r"^\s*#\s*define\s+%s\b" % sym,                           # macro
        r"^\s*(?:static|const|extern|struct|union|enum|unsigned|signed)*[\sA-Za-z_][A-Za-z0-9_\s\*]*?\b%s\s*(?:\[|=|;|,)" % sym,
        r"^\s*%s\s*(?:=|,|\})" % sym,                              # enum member
        r"\}\s*%s\s*;" % sym,                                     # typedef struct { ... } NAME;
        r"^\s*(?:typedef\s+)?(?:struct|union|enum)\s+%s\b" % sym,
    )
    for pat in pats:
        if re.search(pat, body, re.MULTILINE):
            return True
    return False


def code_path(doc_path, target):
    """Resolve a link target to a file inside the source tree, or None."""
    target = target.split("#")[0]
    if not target or target.startswith(("http://", "https://", "mailto:")):
        return None
    if not target.endswith(CODE_EXT):
        return None
    resolved = os.path.normpath(os.path.join(os.path.dirname(doc_path), target))
    rel = os.path.relpath(resolved, ROOT)
    if not rel.startswith(CODE_DIRS):
        return None
    return resolved if os.path.isfile(resolved) else None


def build_index():
    """symbol-ish token -> set of repo-relative files containing it."""
    index = {}
    for base in CODE_DIRS:
        for dirpath, _dirs, files in os.walk(os.path.join(ROOT, base)):
            for name in files:
                if not name.endswith(CODE_EXT):
                    continue
                full = os.path.join(dirpath, name)
                try:
                    with open(full, "r", encoding="utf-8", errors="replace") as fh:
                        text = fh.read()
                except OSError:
                    continue
                rel = os.path.relpath(full, ROOT)
                for pat in DEFINES_CAPTURING:
                    for m in pat.finditer(text):
                        index.setdefault(m.group(1), set()).add(rel)
    return index


def main(argv):
    docs = argv[1:]
    if not docs:
        docs = []
        for dirpath, _dirs, files in os.walk(os.path.join(ROOT, "docs")):
            docs += [os.path.join(dirpath, f) for f in files if f.endswith(".md")]
        docs += [os.path.join(ROOT, f) for f in ("AGENTS.md", "CODESTYLE.md", "CONTRIBUTING.md")
                 if os.path.isfile(os.path.join(ROOT, f))]

    index = build_index()
    moved, unknown = [], []

    for doc in sorted(docs):
        try:
            with open(doc, "r", encoding="utf-8", errors="replace") as fh:
                lines = fh.readlines()
        except OSError:
            continue
        rel_doc = os.path.relpath(doc, ROOT)
        for n, line in enumerate(lines, 1):
            targets = [p for p in (code_path(doc, m.group(1)) for m in LINK.finditer(line)) if p]
            if not targets:
                continue
            try:
                bodies = {t: open(t, "r", encoding="utf-8", errors="replace").read() for t in targets}
            except OSError:
                continue
            is_row = line.lstrip().startswith("|")
            for m in TICKED.finditer(line):
                sym = m.group(1)
                if sym in NOT_SYMBOLS or len(sym) < 3:
                    continue
                how = attributes(line, m, is_row)
                if how is None:
                    continue

                # Prose says where a symbol lives, so it must be defined there. A table row is a
                # map and may legitimately point at a call site, so a mention is enough. Holding
                # rows to the prose rule reports every "menu label comes from GetMultiText in
                # GAMEMENU.CPP", which is true and not an error.
                if how == "mentioned":
                    ok = any(sym in body for body in bodies.values())
                else:
                    ok = any(defines(body, sym) for body in bodies.values())
                if ok:
                    continue

                named = ", ".join(os.path.relpath(t, ROOT) for t in targets)
                elsewhere = sorted(index.get(sym, ()))

                # Two kinds of symbol are legitimately named by a file that does not define them.
                # A shared global lives on the bus and is read everywhere, and a macro, type or
                # constant lives in a header and is used everywhere. Naming the file that uses one
                # is a statement about the code, not a stale reference.
                on_the_bus = any(e in BUS for e in elsewhere)
                header_only = elsewhere and all(e.endswith((".H", ".h")) for e in elsewhere)
                if on_the_bus or header_only:
                    continue

                if elsewhere:
                    moved.append((rel_doc, n, sym, named, elsewhere[:3]))
                else:
                    unknown.append((rel_doc, n, sym, named))

    if moved:
        print("Doc names a symbol that is not in the file it links to:\n")
        for doc, n, sym, named, elsewhere in moved:
            print("  %s:%d" % (doc, n))
            print("    `%s` is not in %s" % (sym, named))
            print("    it is in: %s" % ", ".join(elsewhere))
        print("")

    if unknown:
        print("Not found anywhere in the source tree (usually prose, not a defect):")
        for doc, n, sym, named in unknown:
            print("  %s:%d  `%s`  (linked: %s)" % (doc, n, sym, named))
        print("")

    if moved:
        print("%d reference(s) point at the wrong file. Update the doc, or link the file that has it."
              % len(moved))
        return 1

    print("docs-symbols: every linked symbol is in the file its doc names.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
