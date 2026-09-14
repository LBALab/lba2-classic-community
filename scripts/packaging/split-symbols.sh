#!/usr/bin/env bash
# Move the debug info out of release binaries into one symbol archive.
#
# Called by the bundlers on their staged copy of the binary, so the build tree
# keeps its debug info and bundling twice gives the same result. The binary is
# rewritten in place without its debug info, and the archive receives:
#
#   ELF, PE   <name>.debug   objcopy --only-keep-debug; the binary gains a
#                            .gnu_debuglink naming it
#   Mach-O    <name>.dSYM    dsymutil; the binary is stripped of its debug map
#                            with strip -S and signed again, ad hoc
#
# The symbol table stays in the binary either way, so the functions in a crash
# block can still be named without the archive. The archive adds files, lines and
# inlined frames: scripts/dev/symbolize_crash.py matches its files to a crash
# block by build ID, UUID, or link timestamp and image size. The split keeps the
# build ID, the UUID and the timestamp, and this checks them before it writes the
# archive; the image size shrinks, and the symbolizer accounts for that.
#
# The build needs -DLBA2_RELEASE_SYMBOLS=ON, or there is no debug info to move
# and this fails rather than publish an archive without any.
#
# Usage:
#   split-symbols.sh --binary <file> [--binary <file> ...] \
#                    --output <name-symbols.tar.xz> \
#                    [--objcopy <objcopy>]
#
# --objcopy defaults to $OBJCOPY, then objcopy, then llvm-objcopy; the Android
# NDK's comes from CMAKE_OBJCOPY. The matching readelf and objdump are found
# beside it.
set -euo pipefail

BINARIES=()
OUTPUT=""
OBJCOPY="${OBJCOPY:-}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --binary) BINARIES+=("$2"); shift 2 ;;
        --output) OUTPUT="$2"; shift 2 ;;
        --objcopy) OBJCOPY="$2"; shift 2 ;;
        -h|--help)
            sed -n '/^# Usage:/,/^set -e/p' "$0" | sed 's/^# \?//' | head -n -1
            exit 0
            ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

if [[ ${#BINARIES[@]} -eq 0 || -z "$OUTPUT" ]]; then
    echo "split-symbols.sh: need at least one --binary and an --output" >&2
    exit 2
fi

die() {
    echo "split-symbols.sh: $*" >&2
    exit 1
}

# objcopy -> readelf, x86_64-w64-mingw32-objcopy -> x86_64-w64-mingw32-objdump,
# llvm-objcopy -> llvm-readelf.
sibling() {
    local tool="$1" name="$2"
    local candidate="${tool%objcopy}${name}"
    if [[ "$candidate" != "$tool" ]] && command -v "$candidate" >/dev/null 2>&1; then
        echo "$candidate"
    elif command -v "$name" >/dev/null 2>&1; then
        echo "$name"
    elif command -v "llvm-$name" >/dev/null 2>&1; then
        echo "llvm-$name"
    else
        die "no $name beside $tool"
    fi
}

objcopy_tool() {
    if [[ -n "$OBJCOPY" ]]; then
        echo "$OBJCOPY"
    elif command -v objcopy >/dev/null 2>&1; then
        echo objcopy
    elif command -v llvm-objcopy >/dev/null 2>&1; then
        echo llvm-objcopy
    else
        die "no objcopy or llvm-objcopy on PATH; pass --objcopy"
    fi
}

format_of() {
    local magic
    magic=$(od -An -tx1 -N4 "$1" | tr -d ' \n')
    case "$magic" in
        7f454c46) echo elf ;;
        4d5a*) echo pe ;;
        cffaedfe | cafebabe) echo macho ;;
        *) die "$1: not an ELF, PE or Mach-O file (magic $magic)" ;;
    esac
}

# Lines of a command's output matching a pattern. Not `| grep -q`: under pipefail
# grep's early exit fails the command it reads from, and the check with it.
count() {
    local pattern="$1"
    shift
    "$@" 2>/dev/null | grep -c -- "$pattern" || true
}

# A PE image's TimeDateStamp, as a decimal: 4 bytes, 8 past the PE signature,
# whose offset is at 0x3c. Little-endian, as every host that builds a release is.
pe_stamp() {
    local signature
    signature=$(od -An -tu4 -j60 -N4 "$1" | tr -d ' ')
    od -An -tu4 -j$((signature + 8)) -N4 "$1" | tr -d ' '
}

size_of() {
    du -sh "$1" | cut -f1
}

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

for binary in "${BINARIES[@]}"; do
    [[ -f "$binary" ]] || die "binary not found: $binary"
    name=$(basename "$binary")
    before=$(size_of "$binary")

    case "$(format_of "$binary")" in
        elf)
            tool=$(objcopy_tool)
            readelf=$(sibling "$tool" readelf)
            debug="$STAGE/$name.debug"
            "$tool" --only-keep-debug "$binary" "$debug"
            [[ $(count "\.debug_info" "$readelf" -S "$debug") -gt 0 ]] \
                || die "$binary has no debug info; build with -DLBA2_RELEASE_SYMBOLS=ON"
            "$tool" --strip-debug --add-gnu-debuglink="$debug" "$binary"
            id_binary=$("$readelf" -n "$binary" 2>/dev/null | awk '/Build ID/ {print $3}')
            id_debug=$("$readelf" -n "$debug" 2>/dev/null | awk '/Build ID/ {print $3}')
            [[ -n "$id_binary" ]] || die "$binary has no GNU build ID, so no crash block can be matched to it"
            ;;
        pe)
            tool=$(objcopy_tool)
            objdump=$(sibling "$tool" objdump)
            debug="$STAGE/$name.debug"
            # A crash block names a PE image by its link timestamp and SizeOfImage.
            # objcopy writes the time it runs into both outputs unless
            # SOURCE_DATE_EPOCH says otherwise, so without this the shipped image
            # and its debug file carry two different times, neither the link's.
            stamp=$(pe_stamp "$binary")
            SOURCE_DATE_EPOCH="$stamp" "$tool" --only-keep-debug "$binary" "$debug"
            [[ $(count "\.debug_info" "$objdump" -h "$debug") -gt 0 ]] \
                || die "$binary has no debug info; build with -DLBA2_RELEASE_SYMBOLS=ON"
            SOURCE_DATE_EPOCH="$stamp" "$tool" --strip-debug --add-gnu-debuglink="$debug" "$binary"
            # Stripping the mapped debug sections shrinks the size, which
            # symbolize_crash.py recovers from the debug file's sections, so only
            # the timestamp can be compared here, against the link's.
            id_binary=$(pe_stamp "$binary")
            id_debug=$(pe_stamp "$debug")
            [[ "$id_binary" == "$stamp" ]] \
                || die "$name: stripping changed the link timestamp ($stamp -> $id_binary)"
            ;;
        macho)
            debug="$STAGE/$name.dSYM"
            dsymutil "$binary" -o "$debug"
            dwarf="$debug/Contents/Resources/DWARF/$name"
            # With LTO the objects dsymutil reads are gone unless the link kept
            # them, and it then writes a dSYM with no debug info, warning only.
            [[ $(count DW_TAG_compile_unit dwarfdump --debug-info "$dwarf") -gt 0 ]] \
                || die "$debug holds no debug info; build with -DLBA2_RELEASE_SYMBOLS=ON"
            strip -S "$binary" 2>/dev/null
            case "$binary" in
                *.app/Contents/MacOS/*) codesign --force --deep --sign - "${binary%%.app/*}.app" ;;
                *) codesign --force --sign - "$binary" ;;
            esac
            id_binary=$(dwarfdump --uuid "$binary" | awk '{print $2}' | paste -sd ' ' -)
            id_debug=$(dwarfdump --uuid "$dwarf" | awk '{print $2}' | paste -sd ' ' -)
            ;;
    esac

    [[ "$id_binary" == "$id_debug" ]] \
        || die "$name: the symbol file's identity ($id_debug) is not the binary's ($id_binary)"
    echo "[split-symbols] $name: $before -> $(size_of "$binary"), symbols $(size_of "$debug"), id $id_binary"
done

mkdir -p "$(dirname "$OUTPUT")"
OUTPUT="$(cd "$(dirname "$OUTPUT")" && pwd)/$(basename "$OUTPUT")"
rm -f "$OUTPUT"
(cd "$STAGE" && tar -cJf "$OUTPUT" -- *)
echo "[split-symbols] done: $OUTPUT ($(size_of "$OUTPUT"))"
