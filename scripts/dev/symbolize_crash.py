#!/usr/bin/env python3
"""Turn the CRASH block in adeline.log into function names, files and lines.

The block names each frame as module+offset and each module by its identity: the
GNU build ID on Linux and Android, LC_UUID on macOS, and the link timestamp with
the image size on Windows. This matches every module to a symbol file carrying
the same identity and asks a symbolizer about the offsets.

Release builds publish their symbol files as <exe>-<version>-<platform>-<arch>-
symbols.tar.xz: attached to the GitHub release for a tag, and as a workflow
artifact named *-symbols, kept 90 days, for every build including the rolling
one. --fetch downloads the ones for the block's build line with `gh`, into the
cache. A build from a dirty tree, or without git, cannot be matched to a run.

Symbol files are found by identity, never by name, so pointing --symbols at a
folder of many builds is safe: a frame is only named from the exact binary that
crashed. An unstripped binary works too, and names functions without lines.

Tools, the first found: llvm-symbolizer (PATH, Homebrew's llvm, the Android
NDK), then atos for macOS and addr2line for Linux, Android and Windows. Frame 0
is looked up at its own address and every other frame one byte back, since those
are return addresses.

Usage:
  symbolize_crash.py [LOG] [--symbols PATH]... [--fetch] [--all]
  symbolize_crash.py --id FILE...

  LOG             adeline.log, adeline.prev.log or a pasted block; stdin if omitted
  --symbols PATH  a symbol file, a folder searched recursively, or a .tar.xz,
                  .tar.gz or .zip archive; repeatable. The cache is always searched
  --fetch         download the block's symbol archives with gh into the cache
  --repo OWNER/NAME   where --fetch looks (default LBALab/lba2-classic-community)
  --cache DIR     default $XDG_CACHE_HOME/lba2cc/symbols, else ~/.cache/lba2cc/symbols
  --all           every block in the log, not only the last
  --full-paths    print source paths as the build recorded them
  --id FILE...    print the identity a crash block would give each file, and exit
"""
import argparse
import glob
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import zipfile

DEFAULT_REPO = "LBALab/lba2-classic-community"

# Which artifact and asset names can hold a platform's symbols. The AppImage and
# the tarball are separate builds of the same platform, so Linux has two.
PLATFORM_KEYWORDS = {
    "Linux": ("linux", "appimage"),
    "macOS": ("macos",),
    "Windows": ("windows",),
    "Android": ("android",),
}

# ---------------------------------------------------------------------------
# Identity, as LIB386/SYSTEM/CRASH_*.CPP writes it.


class Image:
    """One identity inside a file: a fat Mach-O holds several."""

    def __init__(self, path, kind, identity, link_base):
        self.path = path
        self.kind = kind  # "elf", "macho" or "pe"
        self.identity = identity
        # What the block's offsets are relative to, as a link-time address: an ELF
        # module's base is its load bias, a Mach-O module's its header, which
        # __TEXT maps, and a PE module's the image base.
        self.link_base = link_base


def read_images(path):
    try:
        with open(path, "rb") as f:
            head = f.read(4)
            if head == b"\x7fELF":
                return elf_images(path, f)
            if head[:2] == b"MZ":
                return pe_images(path, f)
            if head in (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe"):
                return macho_images(path, f, 0)
            if head == b"\xca\xfe\xba\xbe":
                return fat_images(path, f)
    except (OSError, struct.error, ValueError):
        pass
    return []


def elf_images(path, f):
    f.seek(0)
    ident = f.read(16)
    is64 = ident[4] == 2
    end = "<" if ident[5] == 1 else ">"
    if is64:
        shoff, = struct.unpack(end + "Q", pread(f, 0x28, 8))
        shentsize, shnum = struct.unpack(end + "HH", pread(f, 0x3A, 4))
    else:
        shoff, = struct.unpack(end + "I", pread(f, 0x20, 4))
        shentsize, shnum = struct.unpack(end + "HH", pread(f, 0x2E, 4))
    for i in range(shnum):
        entry = pread(f, shoff + i * shentsize, shentsize)
        sh_type, = struct.unpack(end + "I", entry[4:8])
        if sh_type != 7:  # SHT_NOTE
            continue
        if is64:
            offset, size = struct.unpack(end + "QQ", entry[24:40])
            align, = struct.unpack(end + "Q", entry[48:56])
        else:
            offset, size = struct.unpack(end + "II", entry[16:24])
            align, = struct.unpack(end + "I", entry[32:36])
        data = pread(f, offset, size)
        pad = 8 if align == 8 else 4
        pos = 0
        while pos + 12 <= len(data):
            namesz, descsz, note_type = struct.unpack(end + "III", data[pos:pos + 12])
            name_at = pos + 12
            desc_at = name_at + (namesz + pad - 1) // pad * pad
            if note_type == 3 and data[name_at:name_at + namesz] == b"GNU\x00":
                return [Image(path, "elf", data[desc_at:desc_at + descsz].hex(), 0)]
            pos = desc_at + (descsz + pad - 1) // pad * pad
    return []


def macho_images(path, f, start):
    magic = pread(f, start, 4)
    is64 = magic == b"\xcf\xfa\xed\xfe"
    ncmds, = struct.unpack("<I", pread(f, start + 16, 4))
    pos = start + (32 if is64 else 28)
    uuid = None
    text = 0
    for _ in range(ncmds):
        cmd, size = struct.unpack("<II", pread(f, pos, 8))
        if cmd == 0x1B:  # LC_UUID
            raw = pread(f, pos + 8, 16).hex().upper()
            uuid = "-".join((raw[:8], raw[8:12], raw[12:16], raw[16:20], raw[20:]))
        elif cmd in (0x19, 0x1):  # LC_SEGMENT_64, LC_SEGMENT
            name = pread(f, pos + 8, 16).rstrip(b"\x00")
            if name == b"__TEXT":
                fmt = "<Q" if cmd == 0x19 else "<I"
                text, = struct.unpack(fmt, pread(f, pos + 24, 8 if cmd == 0x19 else 4))
        pos += size
    return [Image(path, "macho", uuid, text)] if uuid else []


def fat_images(path, f):
    count, = struct.unpack(">I", pread(f, 4, 4))
    images = []
    for i in range(count):
        offset, = struct.unpack(">I", pread(f, 8 + i * 20 + 8, 4))
        images.extend(macho_images(path, f, offset))
    return images


def pe_images(path, f):
    lfanew, = struct.unpack("<I", pread(f, 0x3C, 4))
    if pread(f, lfanew, 4) != b"PE\x00\x00":
        return []
    section_count, = struct.unpack("<H", pread(f, lfanew + 6, 2))
    stamp, symbols, symbol_count = struct.unpack("<III", pread(f, lfanew + 8, 12))
    optional_size, = struct.unpack("<H", pread(f, lfanew + 20, 2))
    optional = lfanew + 24
    magic, = struct.unpack("<H", pread(f, optional, 2))
    if magic == 0x20B:
        image_base, = struct.unpack("<Q", pread(f, optional + 24, 8))
    else:
        image_base, = struct.unpack("<I", pread(f, optional + 28, 4))
    alignment, = struct.unpack("<I", pread(f, optional + 32, 4))
    size, = struct.unpack("<I", pread(f, optional + 56, 4))
    sizes = [size]

    # A PE image maps its DWARF sections, so stripping them shrinks SizeOfImage,
    # which is half the identity the running image reports. The shipped size is
    # where the sections that are not debug info end, plus the one page objcopy
    # adds for .gnu_debuglink; the debug file keeps every section's address.
    loaded_end = 0
    has_debug = False
    for i in range(section_count):
        header = pread(f, optional + optional_size + i * 40, 40)
        name = header[:8].rstrip(b"\x00")
        if name.startswith(b"/") and symbols:
            strings = symbols + symbol_count * 18
            f.seek(strings + int(name[1:]))
            name = f.read(64).split(b"\x00")[0]
        virtual_size, address = struct.unpack("<II", header[8:16])
        if name.startswith((b".debug", b"/")):
            has_debug = True
        elif name != b".gnu_debuglink":
            loaded_end = max(loaded_end, address + virtual_size)
    if has_debug and alignment:
        stripped = (loaded_end + alignment - 1) // alignment * alignment
        sizes += [stripped, stripped + alignment]
    return [Image(path, "pe", "%08x%08x" % (stamp, s), image_base) for s in dict.fromkeys(sizes)]


def pread(f, offset, size):
    f.seek(offset)
    data = f.read(size)
    if len(data) != size:
        raise ValueError("short read")
    return data


# ---------------------------------------------------------------------------
# Finding symbol files.


def cache_dir(arg):
    if arg:
        return arg
    base = os.environ.get("XDG_CACHE_HOME") or os.path.join(os.path.expanduser("~"), ".cache")
    return os.path.join(base, "lba2cc", "symbols")


ARCHIVE_SUFFIXES = (".tar.xz", ".tar.gz", ".tgz", ".zip")


def unpack(archive, cache):
    """Extract an archive once into the cache; returns the folder."""
    name = os.path.basename(archive)
    dest = os.path.join(cache, "unpacked", name)
    marker = os.path.join(dest, ".unpacked")
    if os.path.exists(marker) and os.path.getmtime(marker) >= os.path.getmtime(archive):
        return dest
    shutil.rmtree(dest, ignore_errors=True)
    os.makedirs(dest)
    if name.endswith(".zip"):
        with zipfile.ZipFile(archive) as z:
            z.extractall(dest)
    else:
        with tarfile.open(archive, "r:*") as t:
            if hasattr(tarfile, "data_filter"):
                t.extractall(dest, filter="data")
            else:
                t.extractall(dest)
    open(marker, "w").close()
    return dest


def index_symbols(paths, cache):
    """Maps identity to the best Image for it: one with debug info over a bare
    binary, so an unstripped copy lying next to its symbol file never wins."""
    found = {}
    for root in paths:
        if not os.path.exists(root):
            print("symbolize_crash: no such path: %s" % root, file=sys.stderr)
            continue
        if os.path.isfile(root) and root.endswith(ARCHIVE_SUFFIXES):
            root = unpack(root, cache)
        files = [root] if os.path.isfile(root) else walk(root, cache)
        for path in files:
            for image in read_images(path):
                current = found.get(image.identity)
                if current is None or (not has_debug_info(current) and has_debug_info(image)):
                    found[image.identity] = image
    return found


def walk(root, cache):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for name in sorted(filenames):
            path = os.path.join(dirpath, name)
            if name.endswith(ARCHIVE_SUFFIXES):
                yield from walk(unpack(path, cache), cache)
            elif os.path.getsize(path) >= 64:
                yield path


def has_debug_info(image):
    if image.kind == "macho":
        return "/Contents/Resources/DWARF/" in image.path.replace(os.sep, "/")
    return image.path.endswith(".debug")


# ---------------------------------------------------------------------------
# Fetching.


def gh(*args):
    result = subprocess.run(["gh"] + list(args), capture_output=True, text=True)
    return result.returncode, result.stdout, result.stderr


def fetch(build, repo, cache):
    """Downloads the symbol archives a build could have published. Returns a
    list of folders to search."""
    if not shutil.which("gh"):
        print("symbolize_crash: --fetch needs the GitHub CLI, gh", file=sys.stderr)
        return []
    version, commit, platform = build["version"], build["commit"], build["platform"]
    if commit == "unknown" or version.endswith("-dirty"):
        print("symbolize_crash: this build (%s %s) came from a tree without git or with local "
              "changes, so nothing was published for it" % (version, commit), file=sys.stderr)
        return []
    keywords = PLATFORM_KEYWORDS.get(platform, ())
    root = os.path.join(cache, "downloads", commit)
    folders = []

    if re.fullmatch(r"\d+\.\d+\.\d+", version):
        dest = os.path.join(root, "v" + version)
        os.makedirs(dest, exist_ok=True)
        rc, _, err = gh("release", "download", "v" + version, "--repo", repo,
                        "--pattern", "*-symbols.tar.xz", "--dir", dest, "--skip-existing")
        if rc == 0:
            folders.append(dest)
        else:
            print("symbolize_crash: release v%s: %s" % (version, err.strip()), file=sys.stderr)

    rc, sha, err = gh("api", "repos/%s/commits/%s" % (repo, commit), "--jq", ".sha")
    if rc != 0:
        print("symbolize_crash: commit %s: %s" % (commit, err.strip()), file=sys.stderr)
        return folders
    rc, out, err = gh("api", "--paginate", "repos/%s/actions/runs?head_sha=%s&per_page=100" % (repo, sha.strip()),
                      "--jq", ".workflow_runs[].id")
    if rc != 0:
        print("symbolize_crash: runs for %s: %s" % (commit, err.strip()), file=sys.stderr)
        return folders
    names_seen = 0
    for run in out.split():
        rc, arts, _ = gh("api", "--paginate", "repos/%s/actions/runs/%s/artifacts?per_page=100" % (repo, run),
                         "--jq", '.artifacts[] | select(.expired | not) | .name')
        for name in arts.split():
            if not name.endswith("-symbols") or not any(k in name.lower() for k in keywords):
                continue
            names_seen += 1
            dest = os.path.join(root, "run-%s" % run, name)
            if not os.path.isdir(dest):
                rc, _, err = gh("run", "download", run, "--repo", repo, "--name", name, "--dir", dest + ".part")
                if rc != 0:
                    print("symbolize_crash: %s from run %s: %s" % (name, run, err.strip()), file=sys.stderr)
                    shutil.rmtree(dest + ".part", ignore_errors=True)
                    continue
                os.rename(dest + ".part", dest)
            folders.append(dest)
    if not names_seen and not folders:
        print("symbolize_crash: no symbols published for %s %s on %s; workflow artifacts expire after "
              "90 days" % (version, commit, platform), file=sys.stderr)
    return folders


# ---------------------------------------------------------------------------
# Symbolizing.


def find_tool(name):
    path = shutil.which(name)
    if path:
        return path
    patterns = [
        "/opt/homebrew/opt/llvm*/bin/" + name,
        "/usr/local/opt/llvm*/bin/" + name,
        "/usr/lib/llvm-*/bin/" + name,
    ]
    for var in ("ANDROID_NDK_HOME", "ANDROID_NDK", "ANDROID_NDK_ROOT"):
        if os.environ.get(var):
            patterns.append(os.path.join(os.environ[var], "toolchains/llvm/prebuilt/*/bin", name))
    for var in ("ANDROID_HOME", "ANDROID_SDK_ROOT"):
        if os.environ.get(var):
            patterns.append(os.path.join(os.environ[var], "ndk/*/toolchains/llvm/prebuilt/*/bin", name))
    for pattern in patterns:
        matches = sorted(glob.glob(pattern))
        if matches:
            return matches[-1]
    return None


def symbolize(image, addresses):
    """Returns {address: [(function, file, line, column), ...]}, innermost first."""
    llvm = find_tool("llvm-symbolizer")
    if llvm:
        return with_llvm_symbolizer(llvm, image, addresses)
    if image.kind == "macho" and shutil.which("atos"):
        return with_atos(image, addresses)
    addr2line = find_tool("addr2line") or find_tool("x86_64-w64-mingw32-addr2line")
    if addr2line and image.kind != "macho":
        # Measured with binutils 2.46 on an LTO build: whether an address gets its
        # line depended on the addresses looked up before it in the same call.
        print("symbolize_crash: using addr2line, which can miss lines in an LTO build; "
              "llvm-symbolizer does not", file=sys.stderr)
        return with_addr2line(addr2line, image, addresses)
    print("symbolize_crash: no symbolizer for %s; install llvm (llvm-symbolizer)" % image.path, file=sys.stderr)
    return {}


def with_llvm_symbolizer(tool, image, addresses):
    command = [tool, "--obj=" + image.path, "--inlines", "--demangle", "--output-style=JSON"]
    result = subprocess.run(command + ["0x%x" % a for a in addresses], capture_output=True, text=True)
    frames = {}
    for record in json.loads(result.stdout or "[]"):
        address = int(record["Address"], 16)
        frames[address] = [(s["FunctionName"], s["FileName"], s["Line"], s["Column"])
                           for s in record.get("Symbol", []) if s.get("FunctionName")]
    return frames


def with_addr2line(tool, image, addresses):
    result = subprocess.run([tool, "-f", "-C", "-i", "-a", "-e", image.path] + ["0x%x" % a for a in addresses],
                            capture_output=True, text=True)
    frames = {}
    current = None
    lines = result.stdout.splitlines()
    i = 0
    while i < len(lines):
        if re.fullmatch(r"0x[0-9a-fA-F]+", lines[i]):
            current = int(lines[i], 16)
            frames[current] = []
            i += 1
            continue
        function = lines[i]
        location = lines[i + 1] if i + 1 < len(lines) else "??:0"
        file, _, line = location.rpartition(":")
        line = line.split(" ")[0]
        if current is not None and function != "??":
            frames[current].append((function, "" if file == "??" else file, int(line) if line.isdigit() else 0, 0))
        i += 2
    return frames


def with_atos(image, addresses):
    frames = {}
    for address in addresses:
        result = subprocess.run(["atos", "-i", "-o", image.path, "-l", "0x%x" % image.link_base, "0x%x" % address],
                                capture_output=True, text=True)
        symbols = []
        for line in result.stdout.splitlines():
            m = re.match(r"(.+?) \(in [^)]*\)(?: \((.+):(\d+)\))?", line)
            if m:
                symbols.append((m.group(1), m.group(2) or "", int(m.group(3) or 0), 0))
        frames[address] = symbols
    return frames


# ---------------------------------------------------------------------------
# The block.

FRAME = re.compile(r"CRASH frame (\d+) (\S+)\+0x([0-9a-fA-F]+)\s*$")
MODULE = re.compile(r"CRASH module (\S+) base=0x[0-9a-fA-F]+ id=(\S*)")
BUILD = re.compile(r"CRASH build (\S+) (\S+) (\S+) (\S+)")


def blocks(text):
    found = []
    current = None
    for line in text.splitlines():
        at = line.find("CRASH ")
        if at < 0:
            continue
        line = line[at:].rstrip()
        if line.startswith("CRASH ==== fatal"):
            current = []
            found.append(current)
        if current is not None:
            current.append(line)
        if line.startswith("CRASH ==== end"):
            current = None
    return found


def source_path(path, full):
    if full or not path:
        return path
    m = re.search(r"(?:^|[/\\])((?:SOURCES|LIB386|tests)[/\\].*)$", path)
    return m.group(1).replace("\\", "/") if m else path


def report(block, index, full_paths):
    build = None
    modules = {}
    for line in block:
        m = BUILD.match(line)
        if m:
            build = dict(zip(("version", "commit", "platform", "arch"), m.groups()))
        m = MODULE.match(line)
        if m:
            modules[m.group(1)] = m.group(2)

    wanted = {}
    for line in block:
        m = FRAME.match(line)
        if m and modules.get(m.group(2)) in index:
            number, offset = int(m.group(1)), int(m.group(3), 16)
            image = index[modules[m.group(2)]]
            lookup = image.link_base + offset - (1 if number > 0 else 0)
            wanted.setdefault(image.identity, set()).add(lookup)
    resolved = {identity: symbolize(index[identity], sorted(addresses)) for identity, addresses in wanted.items()}

    out = []
    width = max([len("%s+0x%s" % (m.group(2), m.group(3))) for m in map(FRAME.match, block) if m] or [0])
    for line in block:
        m = FRAME.match(line)
        if not m:
            mm = MODULE.match(line)
            if mm:
                image = index.get(mm.group(2))
                line += "  <- %s" % image.path if image else "  (no symbol file)"
            out.append(line)
            continue
        number, name, offset = int(m.group(1)), m.group(2), int(m.group(3), 16)
        where = "%s+0x%x" % (name, offset)
        image = index.get(modules.get(name))
        symbols = []
        if image:
            lookup = image.link_base + offset - (1 if number > 0 else 0)
            symbols = resolved.get(image.identity, {}).get(lookup, [])
        if not symbols:
            out.append("CRASH frame %02d %-*s  ?" % (number, width, where))
            continue
        for depth, (function, file, line_number, column) in enumerate(symbols):
            location = source_path(file, full_paths)
            if location and line_number:
                location += ":%d" % line_number + (":%d" % column if column else "")
            text = function + ("  " + location if location else "")
            if depth == 0:
                out.append("CRASH frame %02d %-*s  %s" % (number, width, where, text))
            else:
                out.append("CRASH           %-*s    inlined into %s" % (width, "", text))
    return build, "\n".join(out)


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("log", nargs="?")
    parser.add_argument("--symbols", action="append", default=[])
    parser.add_argument("--fetch", action="store_true")
    parser.add_argument("--repo", default=DEFAULT_REPO)
    parser.add_argument("--cache")
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--full-paths", action="store_true")
    parser.add_argument("--id", nargs="+")
    parser.add_argument("-h", "--help", action="store_true")
    args = parser.parse_args()
    if args.help:
        print(__doc__)
        return 0

    if args.id:
        status = 0
        for path in args.id:
            images = read_images(path)
            if not images:
                print("%s: no identity" % path)
                status = 1
            for image in images:
                print("%s %s %s" % (image.identity, image.kind, path))
        return status

    text = open(args.log, errors="replace").read() if args.log else sys.stdin.read()
    found = blocks(text)
    if not found:
        print("symbolize_crash: no CRASH block in %s" % (args.log or "stdin"), file=sys.stderr)
        return 1
    cache = cache_dir(args.cache)
    os.makedirs(cache, exist_ok=True)

    status = 0
    for block in found if args.all else found[-1:]:
        build = next((dict(zip(("version", "commit", "platform", "arch"), m.groups()))
                      for m in map(BUILD.match, block) if m), None)
        paths = list(args.symbols)
        if args.fetch and build:
            paths.extend(fetch(build, args.repo, cache))
        paths.append(cache)
        index = index_symbols(paths, cache)
        _, text = report(block, index, args.full_paths)
        print(text)
        engine = next((m.group(2) for m in map(MODULE.match, block) if m), None)
        if engine not in index:
            print("symbolize_crash: no symbol file for the engine module (id=%s); try --fetch or --symbols"
                  % engine, file=sys.stderr)
            status = 1
    return status


if __name__ == "__main__":
    sys.exit(main())
