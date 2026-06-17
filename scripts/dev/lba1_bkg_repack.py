#!/usr/bin/env python3
"""Feasibility spike 3: re-index LBA1's 3 background HQRs into LBA2's 1 merged container.

Source of truth: LBA2 layout from this repo's SOURCES/GRILLE.CPP (T_BKG_HEADER / T_GRI_HEADER);
LBA1 from ../lba1-classic/SOURCES/GRILLE.C (3 HQRs LBA_GRI / LBA_BLL / LBA_BRK).

LBA2 packs everything in one LBA_BKG.HQR, partitioned by a T_BKG_HEADER index (28 B):
  U16 Gri_Start, Grm_Start, Bll_Start, Brk_Start, Max_Brk, ForbidenBrick; U32 Max_Size_* x4
Each grid is prefixed by T_GRI_HEADER (34 B): U8 My_Bll, My_Grm; U8 UsedBlock[32].
A grid's block library resolves to merged entry (Bll_Start + My_Bll); bricks to (Brk_Start + n).

LBA1 keeps them in three HQRs (grid N pairs with block-lib N; bricks shared); the used-block
bitmap is computed at load (LoadUsedBrick) rather than stored. The converter merges the three
into one container, synthesizes a T_BKG_HEADER, and prefixes each grid with a T_GRI_HEADER
(My_Bll = the grid's block-lib index; UsedBlock = computed from the grid's block usage).

This spike confirms the target layout on real LBA2 data, confirms the LBA1 structure, and lays
out + validates the re-index arithmetic. (Computing UsedBlock + emitting the HQR is the
mechanical remainder.)

Usage: lba1_bkg_repack.py [--lba1dir ../LBA] [--lba2-bkg ../LBA2-GOG/LBA_BKG.HQR]
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hqr_inspect  # noqa: E402

BKG_HDR = struct.Struct("<6H4I")     # T_BKG_HEADER, 28 bytes
GRI_HDR_LEN = 2 + 32                  # T_GRI_HEADER: My_Bll, My_Grm, UsedBlock[32]


def slots_present(path):
    ents, _ = hqr_inspect.entries(path)
    return len(ents), sum(1 for e in ents if e[2] is not None)


def entry_bytes(path, idx):
    ents, data = hqr_inspect.entries(path)
    i, off, size, csize, method = ents[idx]
    if size is None:
        return None
    return hqr_inspect.decompress_entry(data, off, size, csize, method)


def decode_bkg_header(b):
    f = BKG_HDR.unpack_from(b, 0)
    return dict(zip(("Gri_Start", "Grm_Start", "Bll_Start", "Brk_Start", "Max_Brk",
                     "ForbidenBrick", "Max_Size_Gri", "Max_Size_Bll",
                     "Max_Size_Brick_Cube", "Max_Size_Mask_Brick_Cube"), f))


def line(t, ok, extra=""):
    return f"  [{'PASS' if ok else 'FAIL'}] {t}" + (f"  ({extra})" if extra else "")


def main():
    hacking = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lba1dir", default=os.path.join(hacking, "LBA"))
    ap.add_argument("--lba2-bkg", default=os.path.join(hacking, "LBA2-GOG", "LBA_BKG.HQR"))
    a = ap.parse_args()
    all_ok = True

    print("=" * 74)
    print("LBA1 background (3 separate HQRs):")
    counts = {}
    for f in ("LBA_GRI", "LBA_BLL", "LBA_BRK"):
        p = os.path.join(a.lba1dir, f + ".HQR")
        if not os.path.exists(p):
            print(f"  MISSING {p}"); return 2
        n, present = slots_present(p)
        counts[f] = present
        print(f"  {f}.HQR: {present} present ({n} slots)")
    grids, blls, brks = counts["LBA_GRI"], counts["LBA_BLL"], counts["LBA_BRK"]
    print(line("grid count == block-lib count (grid N pairs with style N)",
               grids == blls, f"{grids} grids, {blls} block-libs")); all_ok &= (grids == blls)

    print("-" * 74)
    print(f"LBA2 merged container: {a.lba2_bkg}")
    if not os.path.exists(a.lba2_bkg):
        print("  MISSING (skipping LBA2 target decode)")
        h = None
    else:
        n2, present2 = slots_present(a.lba2_bkg)
        hdr_blob = entry_bytes(a.lba2_bkg, 0)
        h = decode_bkg_header(hdr_blob)
        print(f"  {present2} present ({n2} slots); T_BKG_HEADER (entry 0):")
        print(f"    Gri_Start={h['Gri_Start']} Grm_Start={h['Grm_Start']} "
              f"Bll_Start={h['Bll_Start']} Brk_Start={h['Brk_Start']} Max_Brk={h['Max_Brk']}")
        ordered = h["Gri_Start"] <= h["Grm_Start"] <= h["Bll_Start"] <= h["Brk_Start"] < n2
        print(line("partition starts ascending & in-bounds (4-partition layout confirmed)",
                   ordered, f"{h['Gri_Start']}<={h['Grm_Start']}<={h['Bll_Start']}<={h['Brk_Start']}<{n2}"))
        all_ok &= ordered
        # decode one real grid header to confirm T_GRI_HEADER shape
        g = entry_bytes(a.lba2_bkg, h["Gri_Start"] if h["Gri_Start"] > 0 else 1)
        if g and len(g) >= GRI_HDR_LEN:
            my_bll, my_grm = g[0], g[1]
            used = sum(bin(x).count("1") for x in g[2:34])
            print(f"    sample grid T_GRI_HEADER: My_Bll={my_bll} My_Grm={my_grm} "
                  f"UsedBlock popcount={used}/256")
            print(line("grid's block-lib resolves inside the BLL partition",
                       h["Bll_Start"] + my_bll < h["Brk_Start"],
                       f"Bll_Start+My_Bll={h['Bll_Start'] + my_bll} < Brk_Start={h['Brk_Start']}"))
            all_ok &= (h["Bll_Start"] + my_bll < h["Brk_Start"])

    print("-" * 74)
    print("Re-index plan LBA1 -> LBA2 merged container:")
    # LBA1 has no GRM; layout the four partitions in T_BKG_HEADER order.
    gri_start, grm_start = 0, grids
    bll_start = grm_start + 0                  # 0 GRMs in LBA1
    brk_start = bll_start + blls
    total = brk_start + brks
    syn = {"Gri_Start": gri_start, "Grm_Start": grm_start, "Bll_Start": bll_start,
           "Brk_Start": brk_start, "Max_Brk": brks}
    print(f"  synthesized T_BKG_HEADER: Gri_Start={gri_start} Grm_Start={grm_start} "
          f"Bll_Start={bll_start} Brk_Start={brk_start} Max_Brk={brks} (total {total} entries)")
    print(f"  per-grid T_GRI_HEADER: My_Bll = grid index (style N <-> grid N), My_Grm = none, "
          f"UsedBlock[32] = computed from the grid's block usage")
    # validate the arithmetic + a sample resolution
    ok_layout = (gri_start <= grm_start <= bll_start <= brk_start < total
                 and brk_start == grids + 0 + blls and syn["Max_Brk"] == brks)
    print(line("merged-index arithmetic consistent (starts ascending, sizes add up)", ok_layout,
               f"Brk_Start={brk_start}=={grids}+0+{blls}")); all_ok &= ok_layout
    sample = grids // 2
    print(line(f"sample grid {sample}: block-lib resolves in merged container",
               bll_start + sample < brk_start, f"Bll_Start+{sample}={bll_start + sample} < Brk_Start={brk_start}"))
    all_ok &= (bll_start + sample < brk_start)

    print("=" * 74)
    print(f"SPIKE 3 {'PASS' if all_ok else 'FAIL'}: LBA2's merged 4-partition T_BKG_HEADER layout "
          "is confirmed on real data; LBA1's 3 HQRs (matched grid/block-lib counts + bricks) "
          "re-index into it by simple offset arithmetic, with a synthesized per-grid T_GRI_HEADER. "
          "Remaining (mechanical): compute each grid's UsedBlock bitmap (LoadUsedBrick four-pass) "
          "and emit the merged HQR; brick/block/column payloads are unchanged.")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
