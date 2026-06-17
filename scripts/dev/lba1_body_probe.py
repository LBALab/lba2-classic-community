#!/usr/bin/env python3
"""Feasibility spike: transcode an LBA1 body to the lba2cc engine's format (docs/LBA1_PORT_PLAN.md §6).

Source of truth for the LBA1 format is ../lba1-classic (the original Adeline source):
  - body stream walk: LIB386/LIB_3D/P_OBJET.ASM  (AffObjet / AnimNuage / RotateNuage /
    ComputeStaticNormal / ComputeAnimNormal); poly/line/sphere records in AffObjet
  - section layout & 38-byte group: LIB386/LIB_3D/P_ANIM.ASM (SetAngleGroupe); P_DEFINE.ASH
    (INFO_ANIM=2). ComputeAnimNormal shows group[+18] = NbNormal.
Target format: this repo's LIB386/H/AFF_OBJ.INC (STRUC_OBJ_*) and H/OBJECT/AFF_OBJ.H
(T_BODY_HEADER). twin-e is only a readable cross-check, never authoritative.

LBA1 body layout (P_OBJET.ASM):
  u16 Info; s16 bbox[6]; u16 zone_size; <zone_size bytes>;
  u16 nbPoints;  point[nbPoints]    = {s16 X,Y,Z}                      (6 B)
  if (Info & INFO_ANIM): u16 nbGroups; group[nbGroups]                 (38 B each)
  u16 nbNormals; normal[nbNormals]  = {s16 X,Y,Z,range}                (8 B)
  u16 nbPolys;   poly[nbPolys]      (variable; material-tagged, n-gon)
  u16 nbLines;   line[nbLines]      = {u16 mat,coul; idx p1,p2}        (8 B)
  u16 nbSpheres; sphere[nbSpheres]  = {u16 mat,coul,rayon; idx p1}     (8 B)
  (+ up to a couple of trailing padding bytes AffObjet never reads)
Poly point indices are pre-multiplied by SIZE_LIST_POINT (6); LBA2 uses plain indices.
For animated bodies points are group-LOCAL, so they do NOT reproduce the header bbox.

Material (AffObjet): mat>=9 gouraud (per-vertex normals), 7..8 flat (one face normal),
<7 solid/triste/trame (color only).

Usage:
  lba1_body_probe.py [--lba1 LBA/BODY.HQR] [--lba2 LBA2-GOG/BODY.HQR]
                     [--entry N] [--emit OUT.bin]
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hqr_inspect  # noqa: E402

INFO_ANIM = 2
SIZE_LIST_POINT = 6
GROUP_SIZE = 38
GROUP_OFF_NBNORM = 18          # ComputeAnimNormal: cx = [group+18]
LBA2_HDR = 96
POLY_SOLID, POLY_FLAT, POLY_GOURAUD = 0, 1, 4
MASK_QUADRILATERE = 1 << 15
PAD_TOLERANCE = 2              # trailing bytes AffObjet never reads

LBA1_BODY1_INFO = 0x0003
LBA1_BODY1_BBOX = (-253, 260, 0, 1240, -153, 200)


def u16(b, o): return struct.unpack_from("<H", b, o)[0]
def s16(b, o): return struct.unpack_from("<h", b, o)[0]


def hqr_entry(path, idx):
    ents, data = hqr_inspect.entries(path)
    if idx >= len(ents):
        raise IndexError(f"{path}: entry {idx} out of range (has {len(ents)} slots)")
    _, off, size, csize, method = ents[idx]
    if size is None:
        raise ValueError(f"{path}: entry {idx} is empty")
    meth = {0: "stored", 1: "LZSS", 2: "LZMIT"}.get(method, method)
    blob = hqr_inspect.decompress_entry(data, off, size, csize, method)
    if len(blob) != size:
        raise ValueError(f"{path}: entry {idx} decompressed {len(blob)} != declared {size}")
    return blob, size, meth


# ---------------------------------------------------------------- LBA1 decode

def decode_lba1_body(b):
    off = {}
    info = u16(b, 0)
    bbox = struct.unpack_from("<6h", b, 2)
    zone_size = u16(b, 14)
    p = 16 + zone_size

    off["points"] = p
    nb_points = u16(b, p); p += 2
    points = [struct.unpack_from("<3h", b, p + 6 * i) for i in range(nb_points)]
    p += 6 * nb_points

    groups = []
    if info & INFO_ANIM:
        off["groups"] = p
        nb_groups = u16(b, p); p += 2
        for _ in range(nb_groups):
            groups.append(b[p:p + GROUP_SIZE]); p += GROUP_SIZE

    off["normals"] = p
    nb_norm = u16(b, p); p += 2
    normals = [struct.unpack_from("<4h", b, p + 8 * i) for i in range(nb_norm)]
    p += 8 * nb_norm

    off["polys"] = p
    nb_poly = u16(b, p); p += 2
    polys = []
    for _ in range(nb_poly):
        mat = b[p]; nbp = b[p + 1]; p += 2
        coul = u16(b, p); p += 2
        kind = "gouraud" if mat >= 9 else "flat" if mat >= 7 else "solid"
        face_normal = None
        verts = []
        if kind == "flat":
            face_normal = u16(b, p); p += 2
            for _ in range(nbp):
                verts.append(u16(b, p) // SIZE_LIST_POINT); p += 2
        elif kind == "gouraud":
            for _ in range(nbp):
                verts.append(u16(b, p + 2) // SIZE_LIST_POINT); p += 4
        else:
            for _ in range(nbp):
                verts.append(u16(b, p) // SIZE_LIST_POINT); p += 2
        polys.append({"mat": mat, "kind": kind, "coul": coul,
                      "face_normal": face_normal, "verts": verts})

    off["lines"] = p
    nb_line = u16(b, p); p += 2
    lines = [{"mat": u16(b, p + 8 * i), "coul": u16(b, p + 8 * i + 2),
              "p1": u16(b, p + 8 * i + 4) // SIZE_LIST_POINT,
              "p2": u16(b, p + 8 * i + 6) // SIZE_LIST_POINT} for i in range(nb_line)]
    p += 8 * nb_line

    off["spheres"] = p
    nb_sph = u16(b, p); p += 2
    spheres = [{"mat": u16(b, p + 8 * i), "coul": u16(b, p + 8 * i + 2),
                "rayon": u16(b, p + 8 * i + 4),
                "p1": u16(b, p + 8 * i + 6) // SIZE_LIST_POINT} for i in range(nb_sph)]
    p += 8 * nb_sph
    off["end"] = p

    return {"info": info, "bbox": bbox, "zone_size": zone_size,
            "points": points, "groups": groups, "normals": normals,
            "polys": polys, "lines": lines, "spheres": spheres,
            "consumed": p, "off": off}


def analyze_groups(groups, nb_points, nb_normals):
    """Recover the per-group point/normal counts. group[+18]=NbNormal is known from
    ComputeAnimNormal; find the point-count word by cumulative sum == nb_points."""
    out = {"pts_off": None, "norm_off": None, "norm_sum": None}
    if not groups:
        return out
    out["norm_sum"] = sum(u16(g, GROUP_OFF_NBNORM) for g in groups)
    if out["norm_sum"] == nb_normals:
        out["norm_off"] = GROUP_OFF_NBNORM
    for cand in (16, 0, 2, 4, 6):                 # skip type(8)/angles(10,12,14)
        if sum(u16(g, cand) for g in groups) == nb_points:
            out["pts_off"] = cand
            break
    return out


def validate_lba1(b, d, ga):
    checks = []
    leftover = len(b) - d["consumed"]
    checks.append((f"parse consumed body (leftover {leftover} B pad)",
                   0 <= leftover <= PAD_TOLERANCE, f"{d['consumed']}/{len(b)}"))
    pts = d["points"]
    if not (d["info"] & INFO_ANIM):
        xs = [q[0] for q in pts]; ys = [q[1] for q in pts]; zs = [q[2] for q in pts]
        got = (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs))
        checks.append(("points bbox == header ZV (static)", got == tuple(d["bbox"]), ""))
    else:
        checks.append(("points bbox == header ZV", True, "N/A (animated: group-local points)"))
    n = len(pts)
    idx_ok = all(0 <= v < n for pol in d["polys"] for v in pol["verts"]) \
        and all(0 <= ln["p1"] < n and 0 <= ln["p2"] < n for ln in d["lines"]) \
        and all(0 <= sp["p1"] < n for sp in d["spheres"])
    checks.append((f"all point indices in [0,{n})", idx_ok, ""))
    if d["groups"]:
        checks.append(("group[+18] sums to nbNormals (group decode)",
                       ga["norm_off"] is not None, f"sum={ga['norm_sum']} nbNorm={len(d['normals'])}"))
        checks.append(("a group word sums to nbPoints (skinning recovered)",
                       ga["pts_off"] is not None,
                       f"pts_off={ga['pts_off']}" if ga["pts_off"] is not None else "not found"))
    return checks


# ---------------------------------------------------------------- LBA2 emit

def emit_lba2_body(d, ga):
    pts, groups, norms = d["points"], d["groups"], d["normals"]
    nb_points = len(pts)

    pt_group = [0] * nb_points
    g_table = []
    if groups and ga["pts_off"] is not None:
        cur = 0
        for gi, g in enumerate(groups):
            cnt = u16(g, ga["pts_off"])
            nbn = u16(g, GROUP_OFF_NBNORM) if ga["norm_off"] else 0
            for i in range(cur, min(cur + cnt, nb_points)):
                pt_group[i] = gi
            g_table.append((0, cur, cnt, nbn))    # OrgGroupe(parent) stub=0
            cur += cnt
        skin = f"per-group (pts_off={ga['pts_off']}, {len(groups)} groups)"
    else:
        g_table.append((0, 0, nb_points, len(norms)))
        skin = "SINGLE-GROUP fallback"

    base_type = {"solid": POLY_SOLID, "flat": POLY_FLAT, "gouraud": POLY_GOURAUD}
    buckets = {}
    for pol in d["polys"]:
        v = pol["verts"]; t = base_type[pol["kind"]]; coul = pol["coul"]
        faces = [v] if len(v) <= 4 else [[v[0], v[i], v[i + 1]] for i in range(1, len(v) - 1)]
        for f in faces:
            buckets.setdefault((t, len(f) == 4), []).append((f, coul, 0))

    off_groups = LBA2_HDR
    off_points = off_groups + len(g_table) * 8
    off_norms = off_points + nb_points * 8
    off_normfaces = off_norms + len(norms) * 8
    off_polys = off_normfaces
    poly_blob = bytearray()
    for (t, is_quad), recs in buckets.items():
        poly_blob += struct.pack("<HHI", t | (MASK_QUADRILATERE if is_quad else 0),
                                 len(recs), 8 + len(recs) * 12)
        for f, coul, normale in recs:
            p4 = f[3] if is_quad else 0
            poly_blob += struct.pack("<6H", f[0], f[1], f[2], p4, coul, normale)
    off_lines = off_polys + len(poly_blob)
    off_spheres = off_lines + len(d["lines"]) * 8
    off_textures = off_spheres + len(d["spheres"]) * 8

    lba2_info = (d["info"] & 0xff) | (0x100 if (d["info"] & INFO_ANIM) else 0)
    hdr = struct.pack("<i hh 6i", lba2_info, LBA2_HDR, 0, *d["bbox"])
    hdr += struct.pack("<16i",
                       len(g_table), off_groups, nb_points, off_points,
                       len(norms), off_norms, 0, off_normfaces,
                       sum(len(r) for r in buckets.values()), off_polys,
                       len(d["lines"]), off_lines, len(d["spheres"]), off_spheres,
                       0, off_textures)
    assert len(hdr) == LBA2_HDR, len(hdr)

    body = bytearray(hdr)
    for og, op, nbp, nbn in g_table:
        body += struct.pack("<4H", og, op, nbp, nbn)
    for (x, y, z), g in zip(pts, pt_group):
        body += struct.pack("<4h", x, y, z, g)
    for nx, ny, nz, _rng in norms:
        body += struct.pack("<4h", nx, ny, nz, 0)
    body += poly_blob
    for ln in d["lines"]:
        body += struct.pack("<4H", ln["mat"], ln["coul"], ln["p1"], ln["p2"])
    for sp in d["spheres"]:
        body += struct.pack("<4H", sp["mat"], sp["coul"], sp["p1"], sp["rayon"])
    return bytes(body), skin, len(poly_blob), buckets


def decode_lba2_header(b):
    f = struct.unpack_from("<i hh 6i 16i", b, 0)
    names = ["Groupes", "Points", "Normales", "NormFaces",
             "Polys", "Lines", "Spheres", "Textures"]
    sec = f[9:]
    return {"Info": f[0], "SizeHeader": f[1], "bbox": f[3:9],
            "sections": {names[k]: (sec[2 * k], sec[2 * k + 1]) for k in range(8)}}


def validate_lba2(blob, nb_points):
    h = decode_lba2_header(blob)
    checks = [("SizeHeader == 96", h["SizeHeader"] == LBA2_HDR, str(h["SizeHeader"]))]
    s = h["sections"]
    offs = [s[k][1] for k in ("Groupes", "Points", "Normales", "NormFaces",
                              "Polys", "Lines", "Spheres", "Textures")]
    mono = all(LBA2_HDR <= offs[i] <= offs[i + 1] for i in range(len(offs) - 1)) and offs[-1] <= len(blob)
    checks.append(("section offsets monotonic & in-bounds", mono, str(offs)))
    p = s["Polys"][1]; end = s["Lines"][1]; ok = True; seen = 0
    while p < end and ok:
        type_word, nbpoly, nextoff = struct.unpack_from("<HHI", blob, p)
        is_quad = bool(type_word & MASK_QUADRILATERE)
        rp = p + 8
        for _ in range(nbpoly):
            vs = struct.unpack_from("<4H", blob, rp)[:4 if is_quad else 3]
            if any(v >= nb_points for v in vs):
                ok = False
            seen += 1; rp += 12
        p += nextoff
    checks.append((f"emitted poly indices < {nb_points} ({seen} tris/quads)", ok, ""))
    return checks


def default_data():
    here = os.path.abspath(__file__)
    hacking = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(here))))
    lba1 = os.path.join(hacking, "LBA", "BODY.HQR")
    for cand in ("LBA2-GOG", os.path.join("LBA2", "Common")):
        pth = os.path.join(hacking, cand, "BODY.HQR")
        if os.path.exists(pth):
            return lba1, pth
    return lba1, os.path.join(hacking, "LBA2-GOG", "BODY.HQR")


def line(t, ok, extra=""):
    return f"  [{'PASS' if ok else 'FAIL'}] {t}" + (f"  ({extra})" if extra else "")


def main():
    d1, d2 = default_data()
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lba1", default=d1)
    ap.add_argument("--lba2", default=d2)
    ap.add_argument("--entry", type=int, default=1)
    ap.add_argument("--emit", default=None)
    a = ap.parse_args()
    all_ok = True

    print("=" * 72)
    print(f"LBA1 body  : {a.lba1}  (entry {a.entry})")
    if not os.path.exists(a.lba1):
        print("  MISSING. Pass --lba1 /path/to/LBA/BODY.HQR"); return 2
    b1, size1, meth1 = hqr_entry(a.lba1, a.entry)
    print(f"  HQR read  : {size1} bytes, {meth1}")
    d = decode_lba1_body(b1)
    ga = analyze_groups(d["groups"], len(d["points"]), len(d["normals"]))
    bb = d["bbox"]
    anim = "animated" if d["info"] & INFO_ANIM else "static"
    print(f"  Info=0x{d['info']:04x} ({anim})  bbox X[{bb[0]},{bb[1]}] Y[{bb[2]},{bb[3]}] Z[{bb[4]},{bb[5]}]")
    print(f"  decoded   : {len(d['points'])} points, {len(d['groups'])} groups, "
          f"{len(d['normals'])} normals, {len(d['polys'])} polys, "
          f"{len(d['lines'])} lines, {len(d['spheres'])} spheres")
    nshape = {}
    for pol in d["polys"]:
        nshape[len(pol["verts"])] = nshape.get(len(pol["verts"]), 0) + 1
    print(f"  section offsets: {d['off']}")
    print(f"  poly vert counts: {dict(sorted(nshape.items()))}  (n>4 triangulated)")
    print("  LBA1 decode validation (source of truth = lba1-classic P_OBJET.ASM):")
    for t, ok, extra in validate_lba1(b1, d, ga):
        print(line(t, ok, extra)); all_ok &= ok
    if a.entry == 1:
        v = (d["info"] == LBA1_BODY1_INFO and tuple(bb) == LBA1_BODY1_BBOX)
        print(line("body-1 header matches documented retail values", v)); all_ok &= v

    print("-" * 72)
    print(f"LBA2 body  : {a.lba2}  (entry {a.entry}, reference target)")
    if os.path.exists(a.lba2):
        try:
            b2, size2, meth2 = hqr_entry(a.lba2, a.entry)
            h2 = decode_lba2_header(b2)
            print(f"  {size2} B, {meth2}; SizeHeader={h2['SizeHeader']}; "
                  + ", ".join(f"{k}={v[0]}@{v[1]}" for k, v in h2["sections"].items()))
        except (ValueError, IndexError, struct.error) as e:
            print(f"  NOTE: LBA2 side not decoded ({e})")
    else:
        print("  MISSING (skipping). Pass --lba2 /path/to/LBA2/BODY.HQR")

    print("-" * 72)
    print("Transcode LBA1 -> LBA2 T_BODY_HEADER:")
    emitted, skin, polybytes, buckets = emit_lba2_body(d, ga)
    print(f"  emitted {len(emitted)} B; {sum(len(r) for r in buckets.values())} LBA2 tris/quads "
          f"in {len(buckets)} type-groups ({polybytes} B)")
    print(f"  group skinning: {skin}")
    print("  validation:")
    for t, ok, extra in validate_lba2(emitted, len(d["points"])):
        print(line(t, ok, extra)); all_ok &= ok
    print("  STUBBED (flagged; needed before on-engine render): per-poly face Normale + "
          "NormFaces synthesis; fine material->type map (dither/trame/env); normal range; group parent.")
    if a.emit:
        with open(a.emit, "wb") as f:
            f.write(emitted)
        print(f"  wrote {a.emit}")

    print("=" * 72)
    print(f"SPIKE {'PASS' if all_ok else 'FAIL'}: LBA1 body decoded + validated against the "
          "source-of-truth layout (incl. group skinning); geometry transcoded to a valid LBA2 "
          "body. Next: synthesize face normals, then render headless (Control_*/SNAPSHOT).")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
