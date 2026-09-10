#!/usr/bin/env python3
"""Add the knowledge bundle's edges to the graph graphify built.

graphify reads docs/knowledge as documents: one node per file and per heading, and an
edge for each markdown link to another document. It does not read the typed relations in
a concept's frontmatter, and it resolves a link only to another document, so a concept's
citation of SOURCES/TIMER.CPP is not an edge to the TIMER.CPP node the code extractor
already made. Measured: the bundle sat in the graph as an island with no edge into
SOURCES or LIB386, and `graphify explain "SaveTimer()"` could not reach the Quirk about
it.

This adds two kinds of edge to graph.json in place, both derived from the bundle and
labelled as such, so a query for a function reaches the concept that guards it:

  concept --<relation>--> concept        every typed relation in frontmatter, by its name
  concept --cites--> file or function    every footnote link into SOURCES or LIB386, and
                                         every backticked routine in that footnote that
                                         is a node of the linked file

Node ids are graphify's own, so nothing is invented for a file or routine it already
knows; a routine it has no node for gets no edge. `graphify merge-graphs` is not used
because it namespaces the second graph's ids, which makes copies rather than edges.
Re-running is idempotent. Usage, after a normal update:

  graphify update .
  scripts/dev/knowledge_graph.py             # graphify-out/graph.json, in place
  scripts/dev/knowledge_graph.py --dry-run   # count only

The bundle stays downstream: nothing here writes into a concept, and a concept never
cites what this produced.
"""
import glob
import json
import os
import re
import sys

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BUNDLE = os.path.join(ROOT, "docs", "knowledge")
RELATIONS = ("owner", "ports", "constrains", "supersedes", "verified_against", "manifests", "relates_to")
LINK = re.compile(r"\]\((\.\./[^)\s]+)\)")
TICKED = re.compile(r"`([A-Za-z_][A-Za-z0-9_]*)(?:\(\))?`")
FOOTNOTE = re.compile(r"^\[\^[A-Za-z0-9-]+\]:")


def main(argv):
    dry = "--dry-run" in argv
    paths = [a for a in argv if not a.startswith("--")]
    path = paths[0] if paths else os.path.join(ROOT, "graphify-out", "graph.json")
    g = json.load(open(path, encoding="utf-8"))
    nodes = g["nodes"]
    edges = g.get("edges") if g.get("edges") is not None else g.setdefault("links", [])

    # file node: the node whose label is the file's own name, or its path suffix with the parent
    # directory (graphify 0.9.57 labels a file that way when its basename collides); routine
    # nodes: by (file, label)
    file_node, routine_node = {}, {}
    for n in nodes:
        sf = n.get("source_file") or ""
        if not sf:
            continue
        label = n.get("label") or ""
        if (label in (os.path.basename(sf), sf) or sf.endswith("/" + label)) and sf not in file_node:
            file_node[sf] = n["id"]
        routine_node[(sf, label.rstrip("()"))] = n["id"]

    seen = {(e["source"], e["target"], e.get("relation")) for e in edges}
    added = {}

    def add(src, dst, rel, ctx, sf, loc):
        key = (src, dst, rel)
        if key in seen:
            return
        seen.add(key)
        edges.append({"source": src, "target": dst, "relation": rel, "confidence": "EXTRACTED",
                      "weight": 1, "context": ctx, "source_file": sf, "source_location": loc})
        added[rel] = added.get(rel, 0) + 1

    for f in sorted(glob.glob(os.path.join(BUNDLE, "**", "*.md"), recursive=True)):
        rel = os.path.relpath(f, ROOT)
        if os.path.basename(f) in ("index.md", "log.md") or rel not in file_node:
            continue
        text = open(f, encoding="utf-8").read()
        m = re.match(r"^---\n(.*?)\n---\n(.*)$", text, re.S)
        if not m:
            continue
        fm = yaml.safe_load(m.group(1)) or {}
        me = file_node[rel]
        for key in RELATIONS:
            v = fm.get(key)
            if v is None:
                continue
            for t in (v if isinstance(v, list) else [v]):
                tp = os.path.normpath(os.path.join("docs/knowledge", str(t).lstrip("/").partition("#")[0]))
                if tp in file_node:
                    add(me, file_node[tp], key, "%s in frontmatter" % key, rel, "L1")
        offset = m.group(1).count("\n") + 3
        for i, line in enumerate(m.group(2).splitlines(), 1):
            if not FOOTNOTE.match(line):
                continue
            links = [(lm.start(), lm.group(1)) for lm in LINK.finditer(line)]
            for pos, target in links:
                tp = os.path.relpath(os.path.normpath(os.path.join(os.path.dirname(f), target)), ROOT)
                if not tp.startswith(("SOURCES", "LIB386")) or tp not in file_node:
                    continue
                add(me, file_node[tp], "cites", line[:120], rel, "L%d" % (i + offset))
                # routines named after this link and before the next one
                nxt = min([p for p, _ in links if p > pos] or [len(line)])
                for sym in TICKED.findall(line[pos:nxt]):
                    rid = routine_node.get((tp, sym))
                    if rid and rid != file_node[tp]:
                        add(me, rid, "cites", line[:120], rel, "L%d" % (i + offset))

    print("%s: +%d edges %s" % ("would add" if dry else "added", sum(added.values()), added))
    if not dry and added:
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(g, fh)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
