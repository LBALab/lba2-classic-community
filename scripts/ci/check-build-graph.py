#!/usr/bin/env python3
"""Architecture rules that only the build graph can answer.

[check-arch.py](check-arch.py) reads the tracked sources with the standard library and no
toolchain, which is why it runs in `format.yml` with nothing installed. That buys a lot, and it
costs two things this script exists to recover.

**Direct includes are not the include graph.** `check-arch.py`'s "the engine never includes the
game" greps `LIB386/` for `#include "SOURCES/..."`. A path through a second header
(`LIB386/X.CPP` -> `LIB386/Y.H` -> `SOURCES/Z.H`) satisfies the grep and breaks the rule. The
compiler already recorded what each object really pulled, so ask it instead.

**A ratchet on header text is not a ratchet on coupling.** `rule_god_header_only_shrinks` counts
externs and `rule_defines_aggregation_only_shrinks` tracks which modules `DEFINES.H` lists. Both
can be satisfied while nothing improves: `FOLLOWCAM.H` was never aggregated, and `FOLLOWCAM.CPP`
still pulls 184 headers because it opens with `C_EXTERN.H`, which includes `DEFINES.H`. The number
[docs/plan/REFACTOR_ROADMAP.md](../../docs/plan/REFACTOR_ROADMAP.md) actually wants is how many
translation units end up with the aggregate in front of them, and that only exists once something
has been compiled.

Needs a configured, built tree, so it belongs in a job that already has one rather than in
`format.yml`. Both rules below pass today; they are regression insurance and a progress meter, not
a backlog.

  scripts/ci/check-build-graph.py                 # find a build tree
  scripts/ci/check-build-graph.py out/build/linux # or name one
  scripts/ci/check-build-graph.py --report        # fan-out table, non-gating
"""
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Measured on the tree `linux.yml`'s build job produces: the `linux` preset with
# LBA2_BUILD_TESTS=ON and LBA2_BUILD_ASM_EQUIV_TESTS=OFF, after building lba2cc and
# host_tests. A different target set or preset compiles a different number of translation
# units, so this number is only meaningful against that build.
#
# Translation units that end up with SOURCES/DEFINES.H in front of them. This ceiling only
# falls: every module that leaves the aggregation takes TUs off it. Raising it is not a fix.
DEFINES_FANIN = 57
DEFINES_HEADER = "SOURCES/DEFINES.H"

CANDIDATES = ("build", "build-clang", "out/build/linux", "out/build/linux_clang")


def find_build_dir(explicit):
    if explicit:
        return explicit
    env = os.environ.get("LBA2_BUILD_DIR")
    if env:
        return env
    for c in CANDIDATES:
        if os.path.exists(os.path.join(ROOT, c, ".ninja_deps")):
            return os.path.join(ROOT, c)
    return None


def read_deps(build_dir):
    """Return {source_rel: set(header_rel)} from ninja's dependency log.

    The first dependency ninja records is the translation unit's own source, which is a more
    reliable attribution than parsing the object path: a test target compiles
    `SOURCES/FOO.CPP` into `tests/x/CMakeFiles/.../__/__/SOURCES/FOO.CPP.o`.
    """
    proc = subprocess.run(["ninja", "-C", build_dir, "-t", "deps"],
                          capture_output=True, text=True)
    if proc.returncode != 0:
        raise SystemExit(f"error: ninja -t deps failed in {build_dir}:\n{proc.stderr.strip()}")

    units = {}
    source = None
    awaiting_source = False
    for line in proc.stdout.splitlines():
        if not line:
            continue
        if not line.startswith(" "):
            # "<object>: #deps N, deps mtime ... (VALID)" starts a record; a stale record
            # carries no dependency lines, so it simply contributes nothing.
            source = None
            awaiting_source = True
            continue
        dep = line.strip()
        if awaiting_source:
            # The first dependency is the translation unit's own source, wherever it lives.
            # It has to be consumed as the source even when it is outside the repo, or a
            # generated source (embedded_lba2_cfg_data.cpp is built into the tree) would be
            # skipped and its first repo header promoted to source in its place.
            awaiting_source = False
            if dep.startswith(ROOT + os.sep):
                source = dep[len(ROOT) + 1:]
                units.setdefault(source, set())
            else:
                source = None  # generated outside the tree; not ours to judge
            continue
        if source is not None and dep.startswith(ROOT + os.sep):
            units[source].add(dep[len(ROOT) + 1:])
    return units


def sanity(units):
    """Refuse to report on a parse that found nothing.

    The failure this guards against is silent: a ninja format change, an unbuilt tree or a
    wrong directory all yield zero records, zero violations and a green run. So require both
    that translation units were seen and that the *allowed* direction is present, which proves
    the traversal reaches headers at all.
    """
    problems = []
    if not units:
        problems.append("no translation units in the dependency log (is the tree built?)")
    allowed = sum(1 for src, hs in units.items()
                  if src.startswith("SOURCES/")
                  for h in hs if h.startswith("LIB386/"))
    if units and allowed == 0:
        problems.append("no SOURCES -> LIB386 header edges, which cannot be right; "
                        "the parse is not reaching dependencies")
    return problems


def rule_engine_never_includes_game(units):
    """LIB386 is the engine. It may not reach the game, by any path."""
    found = []
    for src, headers in sorted(units.items()):
        if not src.startswith("LIB386/"):
            continue
        for h in sorted(headers):
            if h.startswith("SOURCES/"):
                found.append((src, h))
    return found


def rule_defines_fanin_only_shrinks(units):
    n = sum(1 for hs in units.values() if DEFINES_HEADER in hs)
    if n > DEFINES_FANIN:
        return n, (f"{n} translation units include {DEFINES_HEADER}, up from {DEFINES_FANIN}. "
                   "A new TU that opens with C_EXTERN.H inherits the whole aggregation. "
                   "Include what the file uses instead. Raising DEFINES_FANIN is not a fix: "
                   "this ceiling only falls.")
    if n < DEFINES_FANIN:
        return n, (f"{n} translation units include {DEFINES_HEADER}, down from {DEFINES_FANIN}. "
                   f"Set DEFINES_FANIN = {n} in this script to lock the gain in.")
    return n, None


def report(units):
    sizes = sorted((len(hs), src) for src, hs in units.items())
    if not sizes:
        return
    med = sizes[len(sizes) // 2][0]
    print(f"\ntranslation units: {len(sizes)}, median headers pulled: {med}")
    print("heaviest:")
    for n, src in sizes[-12:][::-1]:
        print(f"  {n:4d}  {src}")
    print("\nAn extraction that lands near the median decoupled something. One that lands near "
          "the top inherited the aggregate, whatever its own include list says.")


def main(argv):
    want_report = "--report" in argv
    argv = [a for a in argv if a != "--report"]

    if not shutil.which("ninja"):
        print("error: ninja is not on PATH", file=sys.stderr)
        return 1
    build_dir = find_build_dir(argv[0] if argv else None)
    if not build_dir or not os.path.exists(os.path.join(build_dir, ".ninja_deps")):
        print("error: no built ninja tree found. Configure and build one, then pass its path:\n"
              "         cmake --preset linux -DLBA2_BUILD_TESTS=ON && cmake --build --preset linux\n"
              "         scripts/ci/check-build-graph.py out/build/linux", file=sys.stderr)
        return 1

    units = read_deps(build_dir)
    for problem in sanity(units):
        print(f"error: {problem}", file=sys.stderr)
    if sanity(units):
        return 1

    failures = 0

    print("rule 1: the engine never includes the game, by any include path")
    violations = rule_engine_never_includes_game(units)
    if violations:
        failures += 1
        for src, header in violations:
            print(f"  {src} pulls {header}")
        print("  LIB386 is the engine and SOURCES is the game. check-arch.py rule 1 states the\n"
              "  rule; this one sees the transitive path it cannot.")
    else:
        print(f"  ok: 0 of {sum(1 for s in units if s.startswith('LIB386/'))} engine units")

    print(f"\nrule 2: {DEFINES_HEADER} reaches no more translation units than before")
    n, message = rule_defines_fanin_only_shrinks(units)
    if message:
        failures += 1
        print(f"  {message}")
    else:
        print(f"  ok: {n} translation units, unchanged")

    if want_report:
        report(units)

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
