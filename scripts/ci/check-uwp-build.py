#!/usr/bin/env python3
"""Check a UWP build of lba2cc for what a native run would find too late.

imports  Every function the exe imports must be one a UWP app may call: one
         that llvm-mingw's libwindowsapp.a or libucrtapp.a offers, from the
         DLL that library names, or one of the few extras listed below. A
         desktop-only call links fine with the wrong library and only fails
         when the app starts on the console.

table    The UWP backend builds its callback table by named fields, and no
         host can run it, so test_platform_callbacks cannot either. Every
         callback LIB386/H/PLATFORM/PLATFORM.H declares must be assigned in
         LIB386/PLATFORM/UWP/BACKEND.CPP.

Usage:
  check-uwp-build.py imports --toolchain LLVM_MINGW_ROOT EXE
  check-uwp-build.py table [--header H] [--backend B]
  check-uwp-build.py selftest --toolchain LLVM_MINGW_ROOT
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# Allowed for UWP apps by Microsoft's API reference but absent from
# llvm-mingw's libwindowsapp.a. Each one is named, not a whole DLL.
EXTRA_ALLOWED = {
    ("d2d1.dll", "D2D1CreateFactory"),
}

IMPORT_LIBRARIES = ("libwindowsapp.a", "libucrtapp.a")


def tool(root: Path, name: str) -> str:
    path = root / "bin" / name
    if not path.exists():
        sys.exit(f"check-uwp-build: {path} not found")
    return str(path)


def allowed_imports(root: Path) -> set[tuple[str, str]]:
    """(dll, function) pairs the UWP import libraries define."""
    allowed = set(EXTRA_ALLOWED)
    lib_dir = root / "x86_64-w64-mingw32" / "lib"
    nm = tool(root, "llvm-nm")
    for name in IMPORT_LIBRARIES:
        out = subprocess.run([nm, "-A", str(lib_dir / name)], check=True, capture_output=True, text=True).stdout
        for line in out.splitlines():
            # .../libwindowsapp.a:api-ms-win-core-file-l1-1-0.dll: 00000000 T __imp_GetFileAttributesA
            # (D for the few the .def marks DATA, such as _fpreset; still an import)
            match = re.match(r".*?\.a:([^:]+\.dll): +[0-9a-f]+ [TD] __imp_(\S+)$", line, re.IGNORECASE)
            if match:
                allowed.add((match.group(1).lower(), match.group(2)))
    if len(allowed) < 1000:
        sys.exit(f"check-uwp-build: only {len(allowed)} allowed imports read from {lib_dir}; the nm output changed")
    return allowed


def exe_imports(root: Path, exe: Path) -> list[tuple[str, str]]:
    out = subprocess.run(
        [tool(root, "llvm-readobj"), "--coff-imports", str(exe)], check=True, capture_output=True, text=True
    ).stdout
    imports = []
    dll = None
    for line in out.splitlines():
        line = line.strip()
        if line.startswith("Name: "):
            dll = line[len("Name: ") :].lower()
        elif line.startswith("Symbol: ") and dll:
            symbol = line[len("Symbol: ") :].rsplit(" (", 1)[0]
            imports.append((dll, symbol))
    if not imports:
        sys.exit(f"check-uwp-build: no imports read from {exe}")
    return imports


def check_imports(root: Path, exe: Path) -> int:
    allowed = allowed_imports(root)
    imports = exe_imports(root, exe)
    bad = [pair for pair in imports if pair not in allowed]
    for dll, symbol in bad:
        print(f"{exe.name}: {dll}!{symbol} is not available to UWP apps", file=sys.stderr)
    if bad:
        return 1
    dlls = sorted({dll for dll, _ in imports})
    print(f"imports: {len(imports)} functions from {len(dlls)} DLLs, all available to UWP apps")
    return 0


def declared_callbacks(header: Path) -> set[str]:
    text = header.read_text()
    callbacks = set()
    for group, body in re.findall(r"typedef struct Platform(\w+) \{(.*?)\} Platform\1;", text, re.DOTALL):
        if group in ("Event", "Frame", "Capabilities", "Interface"):
            continue
        for name in re.findall(r"\(\*(\w+)\)", body):
            callbacks.add(f"{group.lower()}.{name}")
    if len(callbacks) < 40:
        sys.exit(f"check-uwp-build: only {len(callbacks)} callbacks read from {header}; the header's layout changed")
    return callbacks


def check_table(header: Path, backend: Path) -> int:
    declared = declared_callbacks(header)
    assigned = set(re.findall(r"\bapi\.(\w+\.\w+)\s*=", backend.read_text()))
    assigned = {name for name in assigned if not name.startswith("capabilities.")}
    missing = sorted(declared - assigned)
    unknown = sorted(assigned - declared)
    for name in missing:
        print(f"{backend.name}: callback {name} is never assigned", file=sys.stderr)
    for name in unknown:
        print(f"{backend.name}: api.{name} is not in {header.name}", file=sys.stderr)
    if missing or unknown:
        return 1
    print(f"table: all {len(declared)} callbacks assigned")
    return 0


def selftest(root: Path) -> int:
    """The checks must fail when they should: a desktop-only import, and a
    table missing one callback."""
    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        probe = Path(tmp) / "probe.c"
        probe.write_text(
            "#include <windows.h>\n"
            "int WINAPI wWinMain(HINSTANCE a, HINSTANCE b, PWSTR c, int d) {\n"
            "    (void)a; (void)b; (void)c; (void)d;\n"
            "    return MessageBoxA(0, \"\", \"\", 0);\n"
            "}\n"
        )
        exe = Path(tmp) / "probe.exe"
        # MessageBoxA is hidden from app builds; declare it as a desktop build
        # would, which is the mistake the check exists to catch.
        print("selftest: these should be rejected:", flush=True)
        subprocess.run(
            [
                tool(root, "x86_64-w64-mingw32uwp-clang"),
                "-UWINAPI_FAMILY",
                "-DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP",
                "-municode",
                "-mwindows",
                str(probe),
                "-o",
                str(exe),
                "-luser32",
            ],
            check=True,
        )
        if check_imports(root, exe) == 0:
            print("selftest: a MessageBoxA import passed the import check", file=sys.stderr)
            failures += 1

        backend = (REPO / "LIB386/PLATFORM/UWP/BACKEND.CPP").read_text()
        incomplete = Path(tmp) / "BACKEND.CPP"
        incomplete.write_text(backend.replace("    api.input.gamepad_axis = Uwp_GamepadAxis;\n", "", 1))
        if incomplete.read_text() == backend:
            print("selftest: could not remove a callback from the table", file=sys.stderr)
            failures += 1
        elif check_table(REPO / "LIB386/H/PLATFORM/PLATFORM.H", incomplete) == 0:
            print("selftest: a table missing gamepad_axis passed the table check", file=sys.stderr)
            failures += 1
    if failures == 0:
        print("selftest: both checks reject what they should")
    return 1 if failures else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    imports = sub.add_parser("imports")
    imports.add_argument("--toolchain", type=Path, required=True)
    imports.add_argument("exe", type=Path)
    table = sub.add_parser("table")
    table.add_argument("--header", type=Path, default=REPO / "LIB386/H/PLATFORM/PLATFORM.H")
    table.add_argument("--backend", type=Path, default=REPO / "LIB386/PLATFORM/UWP/BACKEND.CPP")
    test = sub.add_parser("selftest")
    test.add_argument("--toolchain", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "imports":
        return check_imports(args.toolchain, args.exe)
    if args.command == "table":
        return check_table(args.header, args.backend)
    return selftest(args.toolchain)


if __name__ == "__main__":
    sys.exit(main())
