#!/usr/bin/env python3

"""Check the architecture boundaries this project already states in prose.

Every rule here is quoted from CODESTYLE.md, AGENTS.md or docs/ENGINE_GAME_SEAM.md.
None of them is invented by this script: a rule arriving as a build failure with no
prose behind it is one person's taste with a runner attached. Each failure prints the
sentence it comes from, so the doc stays the authority and this file stays the
mechanism.

Two kinds of rule:

  gate     the tree is clean today and must stay clean.
  ratchet  the tree is not clean, and the figure may only fall. The current figure
           lives beside the rule as a constant, so an ownership PR lowers it in the
           same diff that earns it, where a reviewer reads it. A ratchet also fails
           when the tree does better than the constant, because a stale ceiling is a
           loose one.

Rationale, the measurements behind each figure, and the candidates deliberately left
out: docs/plan/ARCH_RULES_PLAN.md

Usage: python3 scripts/ci/check-arch.py   (or: make arch-check)
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
from dataclasses import dataclass

# --- what counts as a source file -------------------------------------------

SOURCE_SUFFIXES = (".CPP", ".cpp", ".H", ".h", ".c", ".C", ".hpp", ".HPP")

# Third-party code we ship but did not write.
#
# Deliberately not .clang-format-ignore, though that file looks like the same list:
# it also carries live engine sources whose hand-aligned tables the formatter would
# wreck (EXTRA.CPP, COMMON.H, MAPTOOLS.CPP). Exempting those from an architecture
# rule would leave a hole in it, and a shared list is only worth having when both
# users want the same thing.
VENDORED = (
    "LIB386/libsmacker/",
    "LIB386/AIL/SDL/stb_vorbis.",
    "LIB386/FILEIO/stb_image_write.h",
)

# --- rule 3: where a platform conditional may live --------------------------

PLATFORM_MACROS = (
    "_WIN32",
    "_WIN64",
    "WIN32",
    "__MINGW32__",
    "_MSC_VER",
    "__APPLE__",
    "__MACH__",
    "__ANDROID__",
    "__linux__",
    "__unix__",
    "__unix",
    "__GNUC__",
)

# The platform layer, as docs/ENGINE_GAME_SEAM.md labels it. These directories exist
# to know which host they are on; everyone else reaches the host through them.
PLATFORM_DIRS = (
    "LIB386/AIL/",
    "LIB386/FILEIO/",
    "LIB386/H/SYSTEM/",
    "LIB386/SMACKER/",
    "LIB386/SNAPSHOT/",
    "LIB386/SVGA/",
    "LIB386/SYSTEM/",
    "LIB386/VIDEO_AUDIO_RESAMPLE/",
)

# Platform-labelled modules that live in SOURCES/ rather than in a platform
# directory, because the flat namespace has no other place to put them. Adding a
# file here should be a line somebody reads, and it owes a label in the per-module
# table in docs/ENGINE_GAME_SEAM.md.
PLATFORM_FILES = (
    "SOURCES/CLI_ARGS.CPP",
    "SOURCES/CONTROL_SERVER.CPP",
    "SOURCES/EXIT_SCREEN.CPP",
    "SOURCES/SCAN.CPP",
    "SOURCES/TOUCH_INPUT.CPP",
)

# Engine files that carry a platform conditional anyway. Kept apart from the two
# lists above on purpose: those say "this is the platform layer, conditionals belong
# here", and this one says "this is engine code that has not been cleaned up yet".
# An entry is debt with a name on it, and the rule is only as strong as this list is
# short.
KNOWN_EXCEPTIONS = {
    "SOURCES/INITADEL.C": (
        "the boot banner compiles in the host's name, and the disc-image line picks a "
        "path separator. The first is a label rather than behaviour; the second is real "
        "coupling and wants the basename from the platform layer that already owns the path."
    ),
}

# jni.h is the narrowest form of the same rule: one translation unit knows Android
# exists. docs/ANDROID.md states it directly.
JNI_OWNER = "LIB386/SYSTEM/ANDROID.CPP"

# --- rule 2: the C++98 dialect ----------------------------------------------

STL_HEADERS = frozenset(
    """
    algorithm array bitset complex deque exception forward_list fstream functional
    iomanip ios iosfwd iostream istream iterator limits list locale map memory
    numeric ostream queue set sstream stack stdexcept streambuf string strstream
    tuple typeinfo unordered_map unordered_set utility valarray vector
    """.split()
)

# --- rule 4: the shared-state bus -------------------------------------------

GOD_HEADER = "SOURCES/C_EXTERN.H"

# 266 before the audio ownership move, 252 after it. The figure PR #563 quoted.
GOD_HEADER_EXTERNS = 252

# --- rule 5: the god header's aggregation -----------------------------------

DEFINES_HEADER = "SOURCES/DEFINES.H"

# Every module header DEFINES.H still puts in front of all 58 of its includers. A
# module leaving this set is the expensive half of an ownership move (the header has
# to become self-contained, and every real user has to name it), so the set may only
# shrink. AMBIANCE left in 3f762f37 and may not come back.
#
# "Module" means the header has a matching .CPP, which is what makes it a thing that
# could own state. Headers with no .CPP (ADDKEYS, COMMON, HOLO, PTRFUNC) are shared
# declarations, not modules, and this rule says nothing about them. INPUT is here in
# a comment rather than an include, so it does not count either.
DEFINES_AGGREGATED = frozenset(
    """
    ANIMTEX BEZIER BUGGY CHEATCOD COMPORTE CONFIG CREDITS DART DEC_XCF DISKFUNC
    EXTFUNC EXTRA FICHE FIRE FLOW FUNC GAMEMENU GERELIFE GERETRAK GRILLE IMPACT
    INCRUST INTEXT INVENT JOYSTICK KEYB LZSS MEM MESSAGE MUSIC OBJECT PATCH PERSO
    PLAYACF POF RAIN SAVEGAME SCAN SORT VALIDPOS WAGON ZV
    """.split()
)

# --- rule 6: the console's link boundary -------------------------------------

CONSOLE_DIR = "SOURCES/CONSOLE/"
CONSOLE_CMAKE = CONSOLE_DIR + "CMakeLists.txt"

# --- rule 7: what a string reaching a player may name ------------------------

# A player has the binary and nothing else. Each pattern is one way a string can
# assume otherwise.
REPO_REFERENCE = (
    (re.compile(r"\bdocs/"), "a path in the repository"),
    (re.compile(r"\.md\b"), "a markdown file, which does not ship"),
    (re.compile(r"\b(?:SOURCES|LIB386)/"), "a source path"),
    (re.compile(r"github\.com"), "the repository itself"),
    # Narrow on purpose. "#1024-GH" in the exit screen's joke is not an issue
    # reference, so the number has to end the token rather than run into a suffix.
    (re.compile(r"(?:^|\s)#\d{2,5}(?=[\s.,;:)\]]|$)"), "an issue number"),
)

INCLUDE_RE = re.compile(r'#\s*include\s*[<"]([^">]+)[">]')

# Quoted form only. DEFINES.H reaches LIB386 through <OBJECT.H> and the game's own
# module through "OBJECT.H", and only the second is an aggregation of a SOURCES
# module. Matching on the basename alone counts the engine header as the game one.
LOCAL_INCLUDE_RE = re.compile(r'#\s*include\s*"([^"]+)"')

CONDITIONAL_RE = re.compile(r"#\s*(?:if|ifdef|ifndef|elif)\b")
EXTERN_RE = re.compile(r"^\s*extern\b")


# --- reading a source file ---------------------------------------------------


@dataclass(frozen=True)
class Violation:
    path: str
    line: int
    detail: str


class Source:
    """A source file with its comments blanked out.

    Comments are replaced by whitespace rather than removed, so line numbers still
    match the file a reader opens. This matters more than it sounds: LROT3D.CPP and
    SCREEN.CPP keep the original inline assembly inside a block comment, `#ifdef`
    arms and all, and a checker reading raw lines would report dead reference code as
    a live platform conditional.

    String literals survive in `code`, because an #include is one. They are also
    collected separately, since a rule about what players read wants the literals and
    nothing around them.
    """

    def __init__(self, path: str, text: str) -> None:
        self.path = path
        self.code, self.literals = _strip_comments(text)

    def lines(self) -> list[tuple[int, str]]:
        return list(enumerate(self.code, start=1))


def _strip_comments(text: str) -> tuple[list[str], list[tuple[int, str]]]:
    """Return (code lines, [(line, literal)]).

    A small state machine over C89 comments, string and character literals, with
    backslash escapes. It does not know about raw strings or trigraphs; the engine is
    C++98 without either.
    """
    out: list[str] = []
    literals: list[tuple[int, str]] = []
    current: list[str] = []
    literal: list[str] = []
    literal_line = 0
    state = "code"
    index = 0
    length = len(text)
    line = 1

    def end_line() -> None:
        out.append("".join(current))
        current.clear()

    while index < length:
        char = text[index]
        following = text[index + 1] if index + 1 < length else ""

        if char == "\n":
            end_line()
            line += 1
            index += 1
            if state == "line_comment":
                state = "code"
            continue

        if state == "code":
            if char == "/" and following == "/":
                state = "line_comment"
                index += 2
                continue
            if char == "/" and following == "*":
                state = "block_comment"
                index += 2
                continue
            if char in ('"', "'"):
                state = "string" if char == '"' else "char"
                literal = []
                literal_line = line
                current.append(char)
                index += 1
                continue
            current.append(char)
            index += 1
            continue

        if state in ("string", "char"):
            current.append(char)
            if char == "\\":
                if following:
                    current.append(following)
                    literal.append(following)
                index += 2
                continue
            if (state == "string" and char == '"') or (state == "char" and char == "'"):
                if state == "string":
                    literals.append((literal_line, "".join(literal)))
                state = "code"
                index += 1
                continue
            literal.append(char)
            index += 1
            continue

        if state == "block_comment" and char == "*" and following == "/":
            state = "code"
            index += 2
            continue

        index += 1

    end_line()
    return out, literals


def tracked_sources() -> list[str]:
    """Tracked C and C++ files under SOURCES/ and LIB386/, vendored code excluded."""
    listing = subprocess.run(
        ["git", "ls-files", "--", "SOURCES", "LIB386"],
        capture_output=True,
        text=True,
        check=True,
    )
    paths = []
    for path in listing.stdout.splitlines():
        if not path.endswith(SOURCE_SUFFIXES):
            continue
        if any(path.startswith(prefix) for prefix in VENDORED):
            continue
        paths.append(path)
    return sorted(paths)


def read(path: str) -> Source:
    with open(path, "r", encoding="utf-8", errors="surrogateescape") as handle:
        return Source(path, handle.read())


# --- the rules ---------------------------------------------------------------


def rule_engine_never_includes_game(sources: list[Source]) -> list[Violation]:
    found = []
    for source in sources:
        if not source.path.startswith("LIB386/"):
            continue
        for line, text in source.lines():
            match = INCLUDE_RE.search(text)
            if match and "SOURCES/" in match.group(1).upper():
                found.append(Violation(source.path, line, match.group(0).strip()))
    return found


def rule_no_stl(sources: list[Source]) -> list[Violation]:
    found = []
    for source in sources:
        for line, text in source.lines():
            match = INCLUDE_RE.search(text)
            if match and match.group(1) in STL_HEADERS:
                found.append(Violation(source.path, line, match.group(0).strip()))
            elif "std::" in text:
                found.append(Violation(source.path, line, text.strip()))
    return found


def rule_platform_ifdefs(sources: list[Source]) -> list[Violation]:
    found = []
    for source in sources:
        allowed = (
            source.path in PLATFORM_FILES
            or source.path in KNOWN_EXCEPTIONS
            or any(source.path.startswith(prefix) for prefix in PLATFORM_DIRS)
        )
        for line, text in source.lines():
            match = INCLUDE_RE.search(text)
            if match and match.group(1) == "jni.h" and source.path != JNI_OWNER:
                found.append(
                    Violation(source.path, line, f"{match.group(0).strip()}, outside {JNI_OWNER}")
                )
            if allowed or not CONDITIONAL_RE.match(text.lstrip()):
                continue
            hit = [macro for macro in PLATFORM_MACROS if macro in text]
            if hit:
                found.append(Violation(source.path, line, text.strip()))
    return found


def count_externs(sources: list[Source]) -> int:
    source = next(item for item in sources if item.path == GOD_HEADER)
    return sum(1 for _, text in source.lines() if EXTERN_RE.match(text))


def aggregated_modules(sources: list[Source]) -> dict[str, int]:
    """The SOURCES modules DEFINES.H includes, mapped to the line that does it."""
    source = next(item for item in sources if item.path == DEFINES_HEADER)
    modules = {}
    for line, text in source.lines():
        match = LOCAL_INCLUDE_RE.search(text)
        if not match:
            continue
        stem = match.group(1).rsplit("/", 1)[-1]
        if not stem.upper().endswith(".H"):
            continue
        stem = stem[:-2].upper()
        if os.path.exists(f"SOURCES/{stem}.CPP"):
            modules[stem] = line
    return modules


def rule_god_header_only_shrinks(sources: list[Source]) -> list[Violation]:
    count = count_externs(sources)
    if count > GOD_HEADER_EXTERNS:
        return [
            Violation(
                GOD_HEADER,
                0,
                f"{count} extern declarations, up from {GOD_HEADER_EXTERNS}. "
                "A new module declares its own state in its own header. Raising "
                "GOD_HEADER_EXTERNS is not a fix: this ceiling only falls.",
            )
        ]
    if count < GOD_HEADER_EXTERNS:
        return [
            Violation(
                GOD_HEADER,
                0,
                f"{count} extern declarations, down from {GOD_HEADER_EXTERNS}. "
                f"Set GOD_HEADER_EXTERNS = {count} in this script to lock the gain in.",
            )
        ]
    return []


def rule_defines_aggregation_only_shrinks(sources: list[Source]) -> list[Violation]:
    lines = aggregated_modules(sources)
    modules = set(lines)

    found = []
    for stem in sorted(modules - DEFINES_AGGREGATED):
        found.append(
            Violation(
                DEFINES_HEADER,
                lines[stem],
                f'"{stem}.H" is aggregated again. Every translation unit gets the module '
                "whether it uses it or not, which is what makes ownership inert.",
            )
        )
    for stem in sorted(DEFINES_AGGREGATED - modules):
        found.append(
            Violation(
                DEFINES_HEADER,
                0,
                f"{stem} has left the aggregation. Remove it from DEFINES_AGGREGATED in "
                "this script so it cannot come back.",
            )
        )
    return found


def console_library_files() -> list[str]:
    """The translation units in the `console` static library.

    Read from the CMake target rather than listed here, so a fourth file joining the
    library is covered the day it joins rather than the day someone remembers.
    """
    with open(CONSOLE_CMAKE, "r", encoding="utf-8") as handle:
        match = re.search(r"add_library\s*\(\s*console\s+([^)]*)\)", handle.read())
    if not match:
        return []
    return [f"{CONSOLE_DIR}{name}" for name in match.group(1).split()]


def rule_console_library_names_no_game(sources: list[Source]) -> list[Violation]:
    by_path = {source.path: source for source in sources}
    found = []
    for path in console_library_files():
        source = by_path.get(path)
        if source is None:
            found.append(Violation(CONSOLE_CMAKE, 0, f"{path} is in the library but not in the tree"))
            continue
        for line, text in source.lines():
            match = LOCAL_INCLUDE_RE.search(text)
            if not match:
                continue
            target = match.group(1)
            if target.rsplit("/", 1)[-1].upper().startswith("CONSOLE"):
                continue
            found.append(
                Violation(
                    source.path,
                    line,
                    f'"{target}" is game code, and this file is in the console library. '
                    "The game fills in a hook the library declares; it does not reach the other way.",
                )
            )
    return found


def rule_no_repo_reference_in_strings(sources: list[Source]) -> list[Violation]:
    found = []
    for source in sources:
        for line, literal in source.literals:
            # An #include's target is a literal too, and naming a header is its job.
            if INCLUDE_RE.match(source.code[line - 1].lstrip()):
                continue
            for pattern, what in REPO_REFERENCE:
                if pattern.search(literal):
                    found.append(
                        Violation(source.path, line, f'"{literal.strip()}" names {what}')
                    )
                    break
    return found


@dataclass(frozen=True)
class Rule:
    number: int
    title: str
    source: str
    check: object
    # What to do about it. A failure that names the offending line but not the
    # legitimate fix leaves the cheapest green as the likeliest one, and for most of
    # these rules the cheapest green is editing this file's own allowlists.
    remedy: str = ""


RULES = (
    Rule(
        1,
        "LIB386 must not include SOURCES",
        'CODESTYLE.md "Layers": "The engine never includes the game. No file under '
        'LIB386/ includes a header from SOURCES/."',
        rule_engine_never_includes_game,
    ),
    Rule(
        2,
        "no STL in shipped code",
        'CODESTYLE.md: "No STL in shipped or per-frame code." Tests may use it.',
        rule_no_stl,
    ),
    Rule(
        3,
        "platform conditionals live in the platform layer",
        'CODESTYLE.md "Layers": "Platform conditionals live in the platform layer. '
        'Callers stay #ifdef-free." docs/PLATFORM.md gives the seam.',
        rule_platform_ifdefs,
        remedy=(
            "move the conditional behind the seam. If the directory is platform layer "
            "itself, add it to PLATFORM_DIRS and give it a row in "
            "docs/ENGINE_GAME_SEAM.md in the same diff. KNOWN_EXCEPTIONS is for engine "
            "code awaiting cleanup, not for a new backend."
        ),
    ),
    Rule(
        4,
        "the shared-state bus only shrinks",
        'CODESTYLE.md "Where new code goes": "A new module\'s globals are defined in its '
        'own .CPP and declared in its own .H, never in SOURCES/C_EXTERN.H and '
        'SOURCES/GLOBAL.CPP."',
        rule_god_header_only_shrinks,
    ),
    Rule(
        5,
        "DEFINES.H must not aggregate a module header again",
        "CODESTYLE.md \"Where new code goes\": aggregation is what makes ownership inert.",
        rule_defines_aggregation_only_shrinks,
    ),
    Rule(
        6,
        "the console library names no game code",
        'CODESTYLE.md: "its core builds as a library that touches no game state, while '
        'CONSOLE_CMD.CPP [...] is deliberately left out of that library."',
        rule_console_library_names_no_game,
    ),
    Rule(
        7,
        "no string a player reads may name the repository",
        'CODESTYLE.md "Strings a player reads": "No user-facing string names the '
        'repository: no docs/*.md, no source paths, no issue numbers."',
        rule_no_repo_reference_in_strings,
    ),
)


def main() -> int:
    root = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        capture_output=True,
        text=True,
        check=True,
    ).stdout.strip()
    os.chdir(root)

    paths = tracked_sources()
    sources = [read(path) for path in paths]

    failures = 0
    for rule in RULES:
        violations = rule.check(sources)
        if not violations:
            continue
        failures += 1
        print(f"\nrule {rule.number}: {rule.title}")
        print(f"  {rule.source}")
        if rule.remedy:
            print(f"  fix: {rule.remedy}")
        for violation in violations:
            where = f"{violation.path}:{violation.line}" if violation.line else violation.path
            print(f"    {where}")
            print(f"      {violation.detail}")

    if failures:
        print(
            f"\ncheck-arch: {failures} of {len(RULES)} rules failed over {len(paths)} files.",
            file=sys.stderr,
        )
        print("Rationale for each rule: docs/plan/ARCH_RULES_PLAN.md", file=sys.stderr)
        return 1

    # The ratchets report their figure even when they pass. A gate that prints only
    # "clean" cannot be told apart from one that scanned nothing.
    print(f"check-arch: {len(RULES)} rules over {len(paths)} files, all clean.")
    print(f"  C_EXTERN.H         {count_externs(sources)} externs (ceiling {GOD_HEADER_EXTERNS})")
    print(
        f"  DEFINES.H          {len(aggregated_modules(sources))} module headers "
        f"(ceiling {len(DEFINES_AGGREGATED)})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
