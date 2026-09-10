#!/usr/bin/env python3
"""Lint the knowledge bundle under docs/knowledge against the rules in its SCHEMA.md.

The bundle is markdown with YAML frontmatter, one concept per file, in the Open Knowledge
Format. Its schema lists fourteen rules CI fails on; this is the script that fails. Each
finding names the rule, the file and what was wrong, so a reader can go from the line to
the paragraph of SCHEMA.md that owns it.

What the rules catch, in one line each:

   1  a non-reserved .md without frontmatter or without `type`
   2  a `type` outside the catalogue
   3  missing `title`, `description` or `generated`
   4  a relation or body link whose target is not in the bundle, anchor included
   5  a concept past `stale_after` that is not `deprecated` or `draft`
   6  a `human:` actor where an agent writes, or a non-human actor in `verified`
   7  Obsidian-only syntax outside a code span
   8  a generated Porting Status that differs from what its generator produces
   9  `supersedes` pointing at a concept that is not `deprecated`
  10  a concept whose `equivalence` disagrees with its routine's Porting Status row
  11  a key or body section the concept's type requires, missing
  12  an `owner` that is not one path to a Subsystem, or a Subsystem listing its own child
  13  `verified_against` present at `untested`, or absent at `tested` and `partial`
  14  a concept not listed in index.md

Plus the body conventions the schema states in prose: no em-dashes, footnotes keyed to a
`sources[].id`, cited resources that exist, `asm_origin` as file and routine and never a
line number. Rule 8 runs the generator's own --check and is skipped on a shallow clone,
because the generator dates its output from git history.

  scripts/ci/check-knowledge.py               # lint the bundle, exit 1 on any finding
  scripts/ci/check-knowledge.py --drift       # report concepts whose cited files moved since as_of
  scripts/ci/check-knowledge.py --drift v1.2  # the same against a ref other than HEAD

Drift is a report and never a failure: a concept is current at its `as_of` and silent about
everything after, and which of its claims a later commit touched is a judgment the reader
makes. The report says where to look.
"""
import datetime
import glob
import os
import re
import subprocess
import sys

try:
    import yaml
except ImportError:  # pragma: no cover
    sys.exit("check-knowledge: PyYAML is needed (python3 -m pip install pyyaml)")

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BUNDLE = os.path.join(ROOT, "docs", "knowledge")
RESERVED = ("index.md", "log.md")
GENERATOR = os.path.join(ROOT, "scripts", "dev", "knowledge_porting.py")

TYPES = ("Subsystem", "Decision", "Format", "Quirk", "Porting Status", "Reference")
KEYS = {
    "Subsystem": ["subsystem"],
    "Decision": ["owner"],
    "Format": ["owner", "equivalence"],
    "Quirk": ["owner", "equivalence", "asm_origin", "scope"],
    "Porting Status": ["subsystem", "sources", "stale_after"],
    "Reference": ["resource"],
}
SECTIONS = {
    "Subsystem": ["# Contracts", "# Seams", "# What it does not tell you"],
    "Decision": ["# Decision", "# Non-goals"],
    "Format": ["# Layout", "# What it does not tell you"],
    "Quirk": ["# Why it is load bearing", "# What it does not tell you"],
}
RELATIONS = ("owner", "ports", "constrains", "supersedes", "verified_against", "manifests", "relates_to")
EQUIVALENCE = ("tested", "partial", "untested")

findings = []


def fail(rule, rel, msg):
    findings.append("rule %-2s %s: %s" % (rule, rel, msg))


def split(text):
    m = re.match(r"^---\n(.*?)\n---\n(.*)$", text, re.S)
    return (m.group(1), m.group(2)) if m else (None, text)


def strip_code(s):
    s = re.sub(r"```.*?```", "", s, flags=re.S)
    return re.sub(r"`[^`\n]*`", "", s)


def slug(heading):
    return re.sub(r"[^a-z0-9 -]", "", heading.lower()).replace(" ", "-")


def headings(path):
    with open(path, encoding="utf-8") as f:
        return [slug(h) for h in re.findall(r"^#+ (.*)$", f.read(), re.M)]


def as_list(v):
    return v if isinstance(v, list) else [v]


def load(path):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    fm, body = split(text)
    if fm is None:
        return None, body, text
    try:
        return yaml.safe_load(fm) or {}, body, text
    except yaml.YAMLError as e:
        return e, body, text


def resolve(rel, target, rule):
    """A bundle-relative target such as /subsystems/timing.md#seams; returns its path or None."""
    path, _, anchor = target.partition("#")
    if not path.startswith("/"):
        fail(rule, rel, "target is not bundle-relative: %s" % target)
        return None
    p = os.path.join(BUNDLE, path.lstrip("/"))
    if not os.path.isfile(p):
        fail(rule, rel, "target does not exist: %s" % target)
        return None
    if anchor and anchor not in headings(p):
        fail(rule, rel, "anchor does not exist: %s" % target)
        return None
    return p


def porting_status(porting_path, anchor):
    """The `equivalence` word in the routine's section of a generated Porting Status page."""
    with open(porting_path, encoding="utf-8") as f:
        text = f.read()
    for m in re.finditer(r"^## (.*)$", text, re.M):
        if slug(m.group(1)) != anchor:
            continue
        section = text[m.end():]
        nxt = re.search(r"^## ", section, re.M)
        section = section[: nxt.start()] if nxt else section
        row = re.search(r"^\| `equivalence` \| `(\w+)` \|", section, re.M)
        return row.group(1) if row else None
    return None


def shallow():
    try:
        out = subprocess.check_output(["git", "rev-parse", "--is-shallow-repository"], cwd=ROOT, text=True)
    except (OSError, subprocess.CalledProcessError):
        return True
    return out.strip() == "true"


def check():
    files = sorted(glob.glob(os.path.join(BUNDLE, "**", "*.md"), recursive=True))
    index_text = open(os.path.join(BUNDLE, "index.md"), encoding="utf-8").read()
    concepts = {}

    for path in files:
        rel = os.path.relpath(path, ROOT)
        d, body, text = load(path)
        for ch in ("—", "―"):
            if ch in text:
                fail("c", rel, "em-dash, %d of them" % text.count(ch))
        if os.path.basename(path) in RESERVED:
            if "[[" in strip_code(text):
                fail(7, rel, "wikilink outside a code span")
            continue
        if d is None:
            fail(1, rel, "no frontmatter")
            continue
        if isinstance(d, Exception):
            fail(1, rel, "frontmatter is not YAML: %s" % str(d).splitlines()[0])
            continue
        concepts[path] = d
        t = d.get("type")
        if not t:
            fail(1, rel, "no `type`")
            continue
        if t not in TYPES:
            fail(2, rel, "type %r is not in the catalogue" % t)
            continue
        for k in ("title", "description", "generated"):
            if k not in d:
                fail(3, rel, "missing `%s`" % k)
        gen = d.get("generated")
        if isinstance(gen, dict):
            if not {"by", "at"} <= set(gen):
                fail(3, rel, "`generated` needs `by` and `at`")
            if str(gen.get("by", "")).startswith("human:"):
                fail(6, rel, "`generated.by` is a human actor; an agent or a process writes, a human verifies")
        for v in as_list(d.get("verified") or []):
            if not isinstance(v, dict) or not str(v.get("by", "")).startswith("human:"):
                fail(6, rel, "`verified.by` is not a `human:` actor: %r" % v)
        for k in KEYS[t]:
            if k not in d:
                fail(11, rel, "%s requires `%s`" % (t, k))
        for h in SECTIONS.get(t, []):
            if not re.search(r"^%s\s*$" % re.escape(h), body, re.M):
                fail(11, rel, "%s requires a section `%s`" % (t, h))
        if "[[" in strip_code(body) or re.search(r"^```dataview", body, re.M):
            fail(7, rel, "Obsidian-only syntax outside a code span")
        sa = d.get("stale_after")
        if sa is not None and d.get("status", "stable") not in ("deprecated", "draft"):
            when = sa if isinstance(sa, datetime.datetime) else datetime.datetime.fromisoformat(str(sa).replace("Z", "+00:00"))
            if when.tzinfo is None:
                when = when.replace(tzinfo=datetime.timezone.utc)
            if when < datetime.datetime.now(datetime.timezone.utc):
                fail(5, rel, "past `stale_after` %s with status %s" % (sa, d.get("status", "stable")))
        eq = d.get("equivalence")
        if eq is not None and eq not in EQUIVALENCE:
            fail(11, rel, "`equivalence` %r is not in the legend" % eq)
        if eq in ("tested", "partial") and "verified_against" not in d:
            fail(13, rel, "`equivalence: %s` without `verified_against`" % eq)
        if eq == "untested" and "verified_against" in d:
            fail(13, rel, "`equivalence: untested` with `verified_against`")
        ao = d.get("asm_origin")
        if ao is not None and re.search(r":\d+\s*$", str(ao)):
            fail("c", rel, "`asm_origin` cites a line number: %s" % ao)
        if not re.search(r"\]\(%s(#[^)]*)?\)" % re.escape(os.path.relpath(path, BUNDLE)), index_text):
            fail(14, rel, "not listed in index.md")

        # relations
        for key in RELATIONS:
            v = d.get(key)
            if v is None:
                continue
            for target in as_list(v):
                tp = resolve(rel, str(target), 4)
                if tp is None:
                    continue
                if key == "supersedes":
                    td, _, _ = load(tp)
                    if not isinstance(td, dict) or td.get("status") != "deprecated":
                        fail(9, rel, "supersedes %s, whose status is not deprecated" % target)
                if key == "ports" and eq is not None:
                    anchor = str(target).partition("#")[2]
                    row = porting_status(tp, anchor) if anchor else None
                    if row is None:
                        fail(10, rel, "no `equivalence` row at %s" % target)
                    elif row != eq:
                        fail(10, rel, "equivalence %s disagrees with the row %s at %s" % (eq, row, target))
        own = d.get("owner")
        if own is not None:
            if not isinstance(own, str):
                fail(12, rel, "`owner` must be exactly one path")
            else:
                op = resolve(rel, own, 12)
                if op is not None:
                    od, _, _ = load(op)
                    ot = od.get("type") if isinstance(od, dict) else None
                    allowed = ("Subsystem", "Porting Status") if "ports" in d else ("Subsystem",)
                    if ot not in allowed:
                        fail(12, rel, "`owner` %s is a %s, not %s" % (own, ot, " or ".join(allowed)))
                    else:
                        mine = "/" + os.path.relpath(path, BUNDLE)
                        if mine in as_list(od.get("relates_to") or []):
                            fail(12, rel, "its owner %s lists it in relates_to; the list is derived" % own)

        # body links into the bundle
        for target in re.findall(r"\]\((/[^)\s]+)\)", strip_code(body)):
            resolve(rel, target, 4)

        # footnotes and sources
        ids = {s["id"] for s in as_list(d.get("sources") or []) if isinstance(s, dict) and "id" in s}
        for label in set(re.findall(r"\[\^([A-Za-z0-9-]+)\]", body)):
            if label not in ids:
                fail("c", rel, "footnote [^%s] has no sources[].id" % label)
        for s in as_list(d.get("sources") or []):
            r = s.get("resource", "") if isinstance(s, dict) else ""
            if r and not r.startswith("http") and not os.path.exists(os.path.normpath(os.path.join(os.path.dirname(path), r))):
                fail("c", rel, "sources resource does not exist: %s" % r)
        r = d.get("resource")
        if r and not str(r).startswith("http") and not os.path.exists(os.path.normpath(os.path.join(os.path.dirname(path), str(r)))):
            fail("c", rel, "resource does not exist: %s" % r)

    # rule 8: generated concepts reproduce
    generated = sorted(glob.glob(os.path.join(BUNDLE, "porting", "*.md")))
    if generated:
        if shallow():
            print("rule 8 skipped: shallow clone, the generator dates its output from git history")
        else:
            names = [os.path.splitext(os.path.basename(g))[0].upper() for g in generated]
            r = subprocess.run([sys.executable, GENERATOR, "--check"] + names, cwd=ROOT, capture_output=True, text=True)
            if r.returncode != 0:
                for line in (r.stdout + r.stderr).strip().splitlines():
                    fail(8, os.path.relpath(BUNDLE, ROOT), line)

    for line in findings:
        print(line)
    print("check-knowledge: %d files, %d problems" % (len(files), len(findings)))
    return 1 if findings else 0


def drift(ref):
    """Concepts with as_of whose cited in-tree files changed between as_of and ref."""
    files = sorted(glob.glob(os.path.join(BUNDLE, "**", "*.md"), recursive=True))
    any_drift = False
    for path in files:
        if os.path.basename(path) in RESERVED:
            continue
        d, _, _ = load(path)
        if not isinstance(d, dict) or "as_of" not in d:
            continue
        rel = os.path.relpath(path, ROOT)
        base = str(d["as_of"])
        if subprocess.run(["git", "cat-file", "-e", base + "^{commit}"], cwd=ROOT, capture_output=True).returncode:
            print("%s: as_of %s is not in this clone" % (rel, base))
            continue
        cited = []
        for s in as_list(d.get("sources") or []):
            r = s.get("resource", "") if isinstance(s, dict) else ""
            if r and not r.startswith("http"):
                cited.append(os.path.relpath(os.path.normpath(os.path.join(os.path.dirname(path), r)), ROOT))
        if not cited:
            continue
        out = subprocess.check_output(["git", "diff", "--name-only", base, ref, "--"] + cited, cwd=ROOT, text=True)
        moved = out.strip().splitlines()
        if moved:
            any_drift = True
            print("%s: as_of %s, %d cited file(s) changed by %s:" % (rel, base, len(moved), ref))
            for m in moved:
                print("    " + m)
    if not any_drift:
        print("drift: no cited file has changed since its concept's as_of")
    return 0


def main(argv):
    if argv and argv[0] == "--drift":
        return drift(argv[1] if len(argv) > 1 else "HEAD")
    if argv:
        sys.exit(__doc__)
    return check()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
