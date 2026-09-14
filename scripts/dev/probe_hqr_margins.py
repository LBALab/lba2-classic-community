#!/usr/bin/env python3
"""Check every fixed-size Load_HQR destination against the data it receives.

HQF_LoadClose() (LIB386/SYSTEM/HQFILE.CPP) decompresses in place: a compressed
entry is read to dest + SizeFile - CompressedSizeFile + RECOVER_AREA and expanded
back towards dest, so the destination must hold SizeFile + RECOVER_AREA bytes. A
stored entry needs only SizeFile. This walks the destinations that are not sized
from the data at runtime, takes the worst entry each one can receive in each game
directory given, and reports whether it fits.

The table is maintained by hand from the call sites. A destination that is sized
from HQF_ResSize() + RECOVER_AREA, or allocated by LoadMalloc_HQR(), is correct by
construction and is not listed. Needs retail data, so it runs locally.

The last row is a negative control: the island plan's camera buffer as it was
before it was given the margin. It must report OVER, or the check is not checking.

A second pass checks the margin itself. The output chases the input through one
buffer, so reading compressed byte s after writing d output bytes is safe while
d <= SizeFile - CompressedSizeFile + margin + s. For every compressed entry it
reports the smallest margin that holds, per file, against the 512 bytes of
RECOVER_AREA, or the 500 that LoadUsedBrick() (SOURCES/GRILLE.CPP) uses for bricks.

Usage:
  probe_hqr_margins.py <game-data-dir> [<game-data-dir> ...]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hqr_inspect import entries, expand_lz  # noqa: E402

RECOVER_AREA = 512  # LIB386/H/SYSTEM/LZ.H
BRICK_MARGIN = 500  # SOURCES/GRILLE.CPP LoadUsedBrick


def find(game_dir, name):
    for dirpath, _, files in os.walk(game_dir):
        for f in files:
            if f.upper() == name.upper():
                return os.path.join(dirpath, f)
    return None


def load_table(game_dir, name):
    path = find(game_dir, name)
    if path is None:
        return None, None
    table, data = entries(path)
    return {i: (off, size, method) for i, off, size, _, method in table if size is not None}, data


def stored_bytes(game_dir, name, index):
    table, data = load_table(game_dir, name)
    off, size, method = table[index]
    assert method == 0, f"{name}[{index}] is compressed; this read assumes it is stored"
    return data[off + 10: off + 10 + size]


def need(size, method):
    return size + RECOVER_AREA if method in (1, 2) else size


def destinations(game_dir):
    """(where, destination, file, indices(table) -> list, capacity in bytes)."""
    even = lambda t: [i for i in t if i % 2 == 0]  # noqa: E731
    odd = lambda t: [i for i in t if i % 2 == 1]  # noqa: E731
    every = lambda t: list(t)  # noqa: E731
    rows = [
        ("MESSAGE.CPP InitDial", "BufOrder", "TEXT.HQR", even, 1024 + RECOVER_AREA),
        ("MESSAGE.CPP InitDial", "BufText", "TEXT.HQR", odd, 40000 + RECOVER_AREA),
        ("IMAGE_LOAD.CPP Image_Load*", "ImageStaging", "SCREEN.HQR", every, 640 * 480 + RECOVER_AREA),
        ("IMAGE_LOAD.CPP Image_Load*", "ImageStaging", "HOLOMAP.HQR", every, 640 * 480 + RECOVER_AREA),
        ("IMAGE_LOAD.CPP Image_Load*", "ImageStaging", "SCRSHOT.HQR", every, 640 * 480 + RECOVER_AREA),
        ("MEM.CPP AdjustLba2Mem", "&PtrSceneMem", "SCENE.HQR", lambda t: [0], 4),
        ("DISKFUNC.CPP LoadSceneCubeXY", "ScreenAux (buggy scene)", "SCENE.HQR", lambda t: [i for i in t if i > 0], 640 * 480 + RECOVER_AREA),
        ("EXTFUNC.CPP InitGrilleExt", "SkySeaTexture", "RESS.HQR", lambda t: list(range(11, 22)) + [26], 256 * 256 + RECOVER_AREA),
        ("MEM.CPP AdjustHQRMem", "&sizeinfo", "RESS.HQR", lambda t: [2], 16),
        ("AMBIANCE.CPP ChoicePalette", "PtrXplPalette", "RESS.HQR", lambda t: list(range(27, 38)) + [42], 131884 + RECOVER_AREA),
        ("HOLOGLOB.CPP InitHoloMap", "TabArrow", "HOLOMAP.HQR", lambda t: [12], 305 * 32),
        ("GRILLE.CPP InitBufferCube", "&BkgHeader", "LBA_BKG.HQR", lambda t: [0], 28),
        ("GAMEMENU.CPP AdelineLogo, ShowLogo, EffectPcx", "PalettePcx", "SCREEN.HQR", odd, 768 + RECOVER_AREA),
        ("GAMEMENU.CPP SlideShow", "pal", "SCRSHOT.HQR", odd, 768 + RECOVER_AREA),
        ("HOLOPLAN.CPP InitHoloPlan", "buffer", "HOLOMAP.HQR", lambda t: list(range(19, 46, 2)), 36 + RECOVER_AREA),
    ]

    # Sized from the data's own header entries, read per directory.
    if find(game_dir, "SCENE.HQR"):
        (max_scc,) = struct.unpack_from("<I", stored_bytes(game_dir, "SCENE.HQR", 0), 0)
        rows.append(("DISKFUNC.CPP LoadScene", "PtrScene", "SCENE.HQR",
                     lambda t: [i for i in t if i > 0], max_scc + RECOVER_AREA))
    if find(game_dir, "LBA_BKG.HQR"):
        _, grm, bll, brk, max_brk = struct.unpack_from("<5H", stored_bytes(game_dir, "LBA_BKG.HQR", 0), 0)
        rows.append(("GRILLE.CPP InitBufferCube", "TabAllCube", "LBA_BKG.HQR", lambda t: [brk + max_brk], 256 * 2))
        rows.append(("GRILLE.CPP IncrustGrm", "Screen (GRM)", "LBA_BKG.HQR", lambda t: list(range(grm, bll)), 640 * 480 + RECOVER_AREA))
    for dirpath, _, files in os.walk(game_dir):
        for f in sorted(files):
            if f.upper().endswith(".ILE"):
                rows.append(("LOADISLE.CPP LoadIsland", "IsleMapIndex", f.upper(), lambda t: [0], 256))
                rows.append(("LOADISLE.CPP LoadIsland", "GroundTexture/ObjTexture", f.upper(), lambda t: [1, 2], 256 * 256 + RECOVER_AREA))
    return rows


CONTROL = ("HOLOPLAN.CPP before the margin", "S32 buffer[20]", "HOLOMAP.HQR", lambda t: list(range(19, 46, 2)), 80)


def worst(game_dir, name, pick):
    table, _ = load_table(game_dir, name)
    if table is None:
        return None
    best = None
    for i in pick(table):
        if i in table:
            _, size, method = table[i]
            n = need(size, method)
            if best is None or n > best[0]:
                best = (n, i, size, method)
    return best


def needed_margin(src, size, min_bloc):
    """Smallest margin for which ExpandLZ never reads a byte it has already overwritten."""
    s = d = 0
    gap = 0
    n = len(src)
    while d < size and s < n:
        if d - s > gap:
            gap = d - s
        flag = src[s]
        s += 1
        for _ in range(8):
            if d >= size:
                break
            if d - s > gap:
                gap = d - s
            if flag & 1:
                s += 1
                d += 1
            else:
                length = (src[s] & 0x0F) + min_bloc
                s += 2
                d = min(size, d + length)
            flag >>= 1
    csize = n
    return gap - (size - csize)


def expand_in_place(src, size, min_bloc, margin):
    """ExpandLZ run the way HQF_LoadClose runs it: input and output in one buffer."""
    buf = bytearray(size + margin + len(src))
    s = size - len(src) + margin
    buf[s: s + len(src)] = src
    d = 0
    while d < size:
        flag = buf[s]
        s += 1
        for _ in range(8):
            if d >= size:
                break
            if flag & 1:
                buf[d] = buf[s]
                s += 1
                d += 1
            else:
                length = (buf[s] & 0x0F) + min_bloc
                offset = (buf[s + 1] << 4) | (buf[s] >> 4)
                s += 2
                for _ in range(length):
                    buf[d] = buf[d - offset - 1]
                    d += 1
                    if d >= size:
                        break
            flag >>= 1
    return bytes(buf[:size])


def check_overlap(game_dir):
    """Per file, the largest margin any compressed entry needs; returns the number short."""
    short = 0
    tightest = None
    for dirpath, _, files in os.walk(game_dir):
        for f in sorted(files):
            if not f.upper().endswith((".HQR", ".ILE", ".OBL")):
                continue
            table, data = entries(os.path.join(dirpath, f))
            worst = None
            count = 0
            for i, off, size, csize, method in table:
                if method not in (1, 2) or not size:
                    continue
                count += 1
                m = needed_margin(data[off + 10: off + 10 + csize], size, method + 1)
                if worst is None or m > worst[0]:
                    worst = (m, i, size, csize, method)
            if worst is None:
                continue
            if tightest is None or worst[0] > tightest[0]:
                tightest = worst + (data, table)
            limit = BRICK_MARGIN if f.upper() == "LBA_BKG.HQR" else RECOVER_AREA
            status = "ok" if worst[0] <= limit else "SHORT"
            short += status == "SHORT"
            print(f"   {status:5} {f.upper():14} {count:6} compressed, worst needs {worst[0]:4} of {limit}"
                  f"  [{worst[1]}] size {worst[2]} compressed {worst[3]} method {worst[4]}")
    if tightest is not None:
        # Control: the tightest entry must decompress correctly in place at the margin
        # it was measured to need, and come out wrong one byte below it.
        need_m, idx, size, csize, method, data, table = tightest
        off = table[idx][1]
        src = data[off + 10: off + 10 + csize]
        ref = expand_lz(src, size, method + 1)
        at = expand_in_place(src, size, method + 1, need_m) == ref
        below = expand_in_place(src, size, method + 1, need_m - 1) == ref
        caught = at and not below
        print(f"   {'control' if caught else 'BROKEN':7} tightest entry decompresses in place at {need_m}: {at}, at {need_m - 1}: {below}")
        short += not caught
    return short


def main():
    dirs = sys.argv[1:]
    if not dirs:
        sys.exit(__doc__.strip().splitlines()[-1].strip())
    failures = 0
    for game_dir in dirs:
        print(f"== {game_dir}")
        for where, dest, name, pick, cap in destinations(game_dir):
            w = worst(game_dir, name, pick)
            if w is None:
                print(f"   skip  {dest:28} {name} not present")
                continue
            status = "ok" if w[0] <= cap else "OVER"
            failures += status == "OVER"
            print(f"   {status:5} {dest:28} cap {cap:7}  needs {w[0]:7}  {name}[{w[1]}] size {w[2]} method {w[3]}  ({where})")
        where, dest, name, pick, cap = CONTROL
        w = worst(game_dir, name, pick)
        if w is not None:
            caught = w[0] > cap
            print(f"   {'control' if caught else 'BROKEN':7} {dest}: needs {w[0]} against {cap}, expected OVER")
            failures += not caught
        print("   -- margin each compressed entry needs")
        failures += check_overlap(game_dir)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
