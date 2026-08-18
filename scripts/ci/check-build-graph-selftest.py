#!/usr/bin/env python3
"""Self-test for check-build-graph.py.

Both rules it enforces pass on the tree as it stands, so a run against the real build proves only
that nothing crashed. A checker whose rules have never been seen to fire is indistinguishable from
one that cannot fire, and this one's natural failure is silent: a ninja format change or an unbuilt
tree yields zero records, zero violations and a green run.

So the rules are exercised against synthetic dependency sets, and the parser against the record
shapes ninja actually emits, including the generated source that is not under the repo root.

  scripts/ci/check-build-graph-selftest.py
"""
import importlib.util
import os
import sys

# The script's filename is hyphenated, so it cannot be imported by name.
_HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "check_build_graph", os.path.join(_HERE, "check-build-graph.py")
)
cbg = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cbg)

CASES = []


def case(name):
    def wrap(fn):
        CASES.append((name, fn))
        return fn
    return wrap


@case("rule 1 fires on a transitive engine -> game include")
def _():
    units = {"LIB386/SVGA/SAVBLOCK.CPP": {"LIB386/H/SVGA/SAVBLOCK.H", "SOURCES/DEFINES.H"}}
    v = cbg.rule_engine_never_includes_game(units)
    return v == [("LIB386/SVGA/SAVBLOCK.CPP", "SOURCES/DEFINES.H")]


@case("rule 1 passes when the engine stays inside itself")
def _():
    units = {"LIB386/SVGA/SAVBLOCK.CPP": {"LIB386/H/SVGA/SAVBLOCK.H"}}
    return cbg.rule_engine_never_includes_game(units) == []


@case("rule 1 allows the game to reach the engine")
def _():
    units = {"SOURCES/PERSO.CPP": {"LIB386/H/SYSTEM/ADELINE.H"}}
    return cbg.rule_engine_never_includes_game(units) == []


@case("rule 2 fires when the aggregate reaches one more TU")
def _():
    units = {f"SOURCES/F{i}.CPP": {cbg.DEFINES_HEADER} for i in range(cbg.DEFINES_FANIN + 1)}
    n, msg = cbg.rule_defines_fanin_only_shrinks(units)
    return n == cbg.DEFINES_FANIN + 1 and msg is not None and "only falls" in msg


@case("rule 2 asks for the constant to be lowered when it improves")
def _():
    units = {f"SOURCES/F{i}.CPP": {cbg.DEFINES_HEADER} for i in range(cbg.DEFINES_FANIN - 1)}
    n, msg = cbg.rule_defines_fanin_only_shrinks(units)
    return msg is not None and "lock the gain in" in msg


@case("rule 2 is quiet when unchanged")
def _():
    units = {f"SOURCES/F{i}.CPP": {cbg.DEFINES_HEADER} for i in range(cbg.DEFINES_FANIN)}
    return cbg.rule_defines_fanin_only_shrinks(units)[1] is None


@case("sanity rejects an empty dependency log")
def _():
    return cbg.sanity({}) != []


@case("sanity rejects a parse that found no header edges")
def _():
    # Units present, but nothing reaching LIB386: the shape a broken parse produces.
    return cbg.sanity({"SOURCES/A.CPP": set()}) != []


@case("sanity accepts a parse with the allowed direction present")
def _():
    return cbg.sanity({"SOURCES/A.CPP": {"LIB386/H/X.H"}}) == []


def main():
    failures = 0
    for name, fn in CASES:
        try:
            ok = fn()
        except Exception as exc:  # noqa: BLE001 - a raising case is a failing case
            ok, name = False, f"{name} (raised {exc!r})"
        print(f"{'PASS' if ok else 'FAIL'}  {name}")
        failures += 0 if ok else 1
    print(f"\n{len(CASES) - failures}/{len(CASES)} passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
