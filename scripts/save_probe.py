#!/usr/bin/env python3
"""
Probe LBA2 .lba savegame headers, LZSS-compressed bodies (via built save_decompress),
fixed-offset fields, and optional text dump aligned with SaveContexte/LoadContexte.

There is NO reliable on-disk "ABI tag" for 32 vs 64: per-object T_OBJ_3D blobs differ.
Heuristic: compare plausible NbPatches + patch region size after object array.

Set LBA2_SAVE_DECOMPRESS to the save_decompress binary, or build:
  cmake --build build --target save_decompress
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import struct
import subprocess
import sys
from typing import Any, Dict, List, Optional, Tuple

SAVE_COMPRESS = 0x80
NUM_VERSION_MASK = 0x7F
SCREENSHOT_BYTES = 160 * 120

# From COMMON.H / HOLO.H / DART.H (must match engine)
MAX_VARS_GAME = 256
MAX_VARS_CUBE = 80
MAX_INVENTORY = 40
MAX_OBJECTIF = 50
MAX_CUBE = 255
MAX_DARTS = 3
MAX_PATCHES = 500
MAX_OBJETS_ENGINE = 100  # sanity; engine MAX_OBJETS may differ slightly
MAX_EXTRAS = 50  # COMMON.H MAX_EXTRAS
MAX_ZONES = 255  # COMMON.H MAX_ZONES
MAX_INCRUST_DISP = 10  # COMMON.H MAX_INCRUST_DISP
MAX_FLOWS = 10  # FLOW.H MAX_FLOWS
MAX_FLOW_DOTS = 100  # FLOW.H MAX_FLOW_DOTS

# Bytes from start of game context to NbObjets (S32), release layout (NumVersion >= 34,
# non-EDITLBA2, no DEBUG-only <34 extra reads): see SAVEGAME.CPP LoadContexte order.
# After Checksum at 1339: input block, PtrZoneClimb (4), 3 darts × 28 bytes, then NbObjets.
OFFSET_NB_OBJETS_FROM_GAME_CTX = 1478
# First byte of object #0 stream (after NbObjets S32)
OFFSET_OBJ0_FROM_GAME_CTX = OFFSET_NB_OBJETS_FROM_GAME_CTX + 4

# Per-object serialized size — empirically confirmed against the Steam classic
# corpus (50 saves, all stride=276 to land NbPatches at the correct offset).
# Engine-side `142 + sizeof(T_OBJ_3D) - sizeof(CurrentFrame)` = 142 + 136 = 278 is
# off by 2 bytes vs what retail actually wrote; the engine works because
# LoadContexte reads sequentially (LbaReadByte/Word/Long) so its per-field tally
# matches the wire even when the magic constant doesn't. The probe needs the
# real stride to walk the wire offline.
#
# Native (64-bit) stride keeps the +28-byte delta (sizeof(T_OBJ_3D) grows by that
# much when pointers widen): 276 + 28 = 304. Validated below in `forward_simulate`
# via the patch_region_size cross-check.
PER_OBJECT_32 = 276
PER_OBJECT_64 = 304

# Per-extra serialized size (T_EXTRA on the wire — one record per LbaRead in LoadContexte's
# extras section). 32-bit retail packs T_EXTRA into 68 bytes (T_EXTRA_WIRE32, pack(1));
# 64-bit native is 80 bytes (4 bytes pad before PtrBody, 4-byte pointer-slot padding to
# struct alignment). Mirrors SOURCES/SAVEGAME.CPP T_EXTRA_WIRE32.
EXTRA_SIZE_32 = 68
EXTRA_SIZE_64 = 80

# Per-flow serialized size (S_PART_FLOW). 32-bit retail = 60 bytes (14×S32 + 4-byte ptr slot);
# 64-bit native = 64 bytes (8-byte ptr slot, no padding because the leading 14 S32s already
# 8-byte align the trailing pointer).
FLOW_SIZE_32 = 60
FLOW_SIZE_64 = 64

# Fields whose size is host-independent (no pointers, no compiler-padded layouts).
ZONE_RECORD_BYTES = 16  # 4× LbaWriteLong per zone (Info1/2/3/7 — written individually, no struct read)
INCRUST_DISP_BYTES = 16  # 6×S16 + U32 = 16 bytes packed; no host variance
ONE_DOT_BYTES = 40  # 10×S32 = 40; no pointers


def read_cstring(data: bytes, start: int, max_len: int = 512) -> Tuple[str, int]:
    end = start
    limit = min(len(data), start + max_len)
    while end < limit and data[end] != 0:
        end += 1
    if end >= limit:
        return (data[start:limit].decode("latin-1", errors="replace"), limit)
    name = data[start:end].decode("latin-1", errors="replace")
    return (name, end + 1)


def parse_header(data: bytes) -> Dict[str, Any]:
    if len(data) < 6:
        raise ValueError("file too small for header")
    version_byte = data[0]
    cube = struct.unpack_from("<i", data, 1)[0]
    name, pos = read_cstring(data, 5)
    compressed = bool(version_byte & SAVE_COMPRESS)
    num_version = version_byte & NUM_VERSION_MASK
    out: Dict[str, Any] = {
        "version_byte": version_byte,
        "num_version": num_version,
        "compressed": compressed,
        "cube": cube,
        "player_name": name,
        "header_end": pos,
    }
    if compressed:
        if len(data) < pos + 4:
            raise ValueError("truncated compressed header (missing sizefile)")
        sizefile = struct.unpack_from("<i", data, pos)[0]
        out["decompressed_payload_size"] = sizefile
        out["compressed_blob_start"] = pos + 4
        out["compressed_blob_len"] = len(data) - (pos + 4)
    return out


def filename_hint(basename: str) -> str:
    bn = basename.lower()
    if re.match(r"^\d{3}[ _\-]", basename):
        return "basename_looks_like_numbered_pack (not proof of 32-bit)"
    if bn in ("autosave.lba", "current.lba"):
        return "basename_typical_of_port_runtime (not proof of 64-bit)"
    return ""


def _candidate_executable(path: str) -> bool:
    """True if path looks runnable (POSIX: +x bit; Windows: file exists)."""
    if not os.path.isfile(path):
        return False
    if os.name == "nt":
        return True
    return os.access(path, os.X_OK)


def find_save_decompress() -> Optional[str]:
    env = os.environ.get("LBA2_SAVE_DECOMPRESS")
    if env and _candidate_executable(env):
        return env
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.normpath(os.path.join(here, ".."))
    candidates = [
        os.path.join(root, "build", "tools", "save_decompress"),
        os.path.join(root, "out", "build", "linux", "tools", "save_decompress"),
        os.path.join(root, "out", "build", "linux", "save_decompress"),
    ]
    candidates.extend(sorted(glob.glob(os.path.join(root, "out", "build", "*", "tools", "save_decompress"))))
    for c in candidates:
        if _candidate_executable(c):
            return c
    # PATH
    for d in os.environ.get("PATH", "").split(os.pathsep):
        p = os.path.join(d, "save_decompress")
        if os.name == "nt":
            pexe = p + ".exe"
            if _candidate_executable(pexe):
                return pexe
        if _candidate_executable(p):
            return p
    return None


def effective_obj3d_abi(cli_choice: str) -> str:
    """CLI wins; if auto, honor LBA2_SAVE_PROBE_ABI=32|64 when set."""
    if cli_choice != "auto":
        return cli_choice
    env = (os.environ.get("LBA2_SAVE_PROBE_ABI") or "").strip().lower()
    if env in ("32", "64"):
        return env
    return "auto"


def decompress_payload(comp: bytes, decomp_size: int, helper: str) -> bytes:
    proc = subprocess.run(
        [helper, str(decomp_size)],
        input=comp,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if proc.returncode != 0:
        err = proc.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"save_decompress exit {proc.returncode}: {err}")
    out = proc.stdout
    if len(out) != decomp_size:
        raise RuntimeError(f"decompressed length {len(out)} != expected {decomp_size}")
    return out


def _hex_cap(data: bytes, max_bytes: int) -> str:
    n = min(len(data), max_bytes)
    h = data[:n].hex()
    if len(data) > n:
        return h + "..."
    return h


def dump_game_context(game_ctx: bytes, hex_cap: int) -> List[str]:
    """Text lines mirroring early LoadContexte / SAVEGAME.md offsets (release, >=34)."""
    lines: List[str] = []
    if len(game_ctx) < OFFSET_OBJ0_FROM_GAME_CTX + 4:
        lines.append("  [dump] game context too short for full prefix")
        return lines

    o = 0
    lines.append(f"  [game_ctx+{o}] ListVarGame[0..7] (S16 LE, first 8 words)")
    for i in range(8):
        v = struct.unpack_from("<h", game_ctx, o + i * 2)[0]
        lines.append(f"    [{i}] = {v}")
    o += MAX_VARS_GAME * 2
    lines.append(f"  [game_ctx+{o - MAX_VARS_GAME * 2}] ... ListVarGame total {MAX_VARS_GAME * 2} B")

    lines.append(f"  [game_ctx+{o}] ListVarCube[0..7] (U8)")
    lines.append(f"    bytes = {_hex_cap(game_ctx[o : o + 8], hex_cap)}")
    o += MAX_VARS_CUBE

    def u8(name: str) -> None:
        nonlocal o, lines
        b = game_ctx[o]
        lines.append(f"  [game_ctx+{o}] {name} (u8) = {b}")
        o += 1

    def s32(name: str) -> None:
        nonlocal o, lines
        v = struct.unpack_from("<i", game_ctx, o)[0]
        lines.append(f"  [game_ctx+{o}] {name} (s32) = {v}")
        o += 4

    def u32(name: str) -> None:
        nonlocal o, lines
        v = struct.unpack_from("<I", game_ctx, o)[0]
        lines.append(f"  [game_ctx+{o}] {name} (u32) = {v}")
        o += 4

    def u16(name: str) -> None:
        nonlocal o, lines
        v = struct.unpack_from("<H", game_ctx, o)[0]
        lines.append(f"  [game_ctx+{o}] {name} (u16) = {v}")
        o += 2

    u8("Comportement")
    u32("packed_gold_zlitos (low16 gold, high16 zlitos)")
    u8("MagicLevel")
    u8("MagicPoint")
    u8("NbLittleKeys")
    u16("NbCloverBox")
    s32("SceneStartX")
    s32("SceneStartY")
    s32("SceneStartZ")
    s32("StartXCube")
    s32("StartYCube")
    s32("StartZCube")
    u8("Weapon")
    s32("savetimerrefhr")
    u8("NumObjFollow")
    u8("SaveComportementHero")
    u8("SaveBodyHero")

    lines.append(f"  [game_ctx+{o}] TabArrow FlagHolo[0..7] (u8)")
    lines.append(f"    {_hex_cap(game_ctx[o : o + 8], hex_cap)}")
    tab_n = (MAX_OBJECTIF + MAX_CUBE)
    o += tab_n

    lines.append(f"  [game_ctx+{o}] TabInv[0] PtMagie,FlagInv,IdObj3D (4+4+2 LE)")
    if o + 10 <= len(game_ctx):
        pm, fi, oid = struct.unpack_from("<iih", game_ctx, o)
        lines.append(f"    PtMagie={pm} FlagInv={fi} IdObj3D={oid}")
    o += MAX_INVENTORY * 10

    u32("Checksum")
    s32("LastMyFire")
    s32("LastMyJoy")
    s32("LastInput")
    s32("LastJoyFlag")
    u8("Bulle")
    u8("ActionNormal")
    s32("InventoryAction")
    s32("MagicBall")
    u8("MagicBallType")
    u8("MagicBallCount")
    s32("MagicBallFlags")
    u8("FlagClimbing")
    s32("StartYFalling")
    u8("CameraZone")
    s32("InvSelect")
    s32("ExtraConque")
    u8("PingouinActif")
    u32("PtrZoneClimb (stored u32; load casts to pointer)")

    lines.append(f"  [game_ctx+{o}] ListDart[0] (7×s32 LE)")
    if o + 28 <= len(game_ctx):
        dart0 = struct.unpack_from("<7i", game_ctx, o)
        lines.append(f"    {dart0}")
    o += MAX_DARTS * 7 * 4

    nb = struct.unpack_from("<i", game_ctx, OFFSET_NB_OBJETS_FROM_GAME_CTX)[0]
    lines.append(f"  [game_ctx+{OFFSET_NB_OBJETS_FROM_GAME_CTX}] NbObjets (s32) = {nb}")
    return lines


def forward_simulate(
    game_ctx: bytes, nb_objets: int, abi: str
) -> Dict[str, Any]:
    """Walk LoadContexte landmarks under one ABI hypothesis ("32" or "64"),
    using the same MAX_ constants the engine bounds-checks against.

    Returns a dict with `ok` (True if every bounds-checked field stayed in range),
    `stopped_at` (None on success, else the first field that tripped a bound),
    `final_offset`, plus the values read at each landmark (for diagnostics +
    cross-check against the harness's actual outcome).

    This is the deterministic "what the engine would do given this ABI"
    predictor — no scoring, no heuristics.  Both ABIs simulated; exactly one
    should pass through for a non-corrupt save.

    Patch payload size: the wire carries `NbPatches`, then a U32 patch_region
    written by the writer as `(PtrSave - saveptr - 4)`.  We use it as a skip
    count rather than reproducing the per-patch Size lookup (which depends on
    in-memory ListPatches[n].Size from the loaded scene — not present here).
    """
    obj_stride = PER_OBJECT_32 if abi == "32" else PER_OBJECT_64
    extra_size = EXTRA_SIZE_32 if abi == "32" else EXTRA_SIZE_64
    flow_size = FLOW_SIZE_32 if abi == "32" else FLOW_SIZE_64

    out: Dict[str, Any] = {
        "abi": abi,
        "obj_stride": obj_stride,
        "extra_size": extra_size,
        "flow_size": flow_size,
        "ok": False,
        "stopped_at": None,
        "values": {},
    }

    def fail(field: str, detail: str = "") -> Dict[str, Any]:
        out["stopped_at"] = field
        out["stopped_detail"] = detail
        return out

    if not (0 <= nb_objets <= MAX_OBJETS_ENGINE):
        return fail("nb_objets", f"out of [0,{MAX_OBJETS_ENGINE}]")

    cur = OFFSET_OBJ0_FROM_GAME_CTX + nb_objets * obj_stride
    out["values"]["objects_end_offset"] = cur

    # NbPatches
    if cur + 4 > len(game_ctx):
        return fail("nb_patches", "truncated")
    nb_patches = struct.unpack_from("<i", game_ctx, cur)[0]
    out["values"]["nb_patches"] = nb_patches
    cur += 4
    if not (0 <= nb_patches <= MAX_PATCHES):
        return fail("nb_patches", f"{nb_patches} out of [0,{MAX_PATCHES}]")

    # patch_region_size (writer's PtrSave-saveptr-4 = sum of per-patch payloads)
    if cur + 4 > len(game_ctx):
        return fail("patch_region_size", "truncated")
    patch_region = struct.unpack_from("<i", game_ctx, cur)[0]
    out["values"]["patch_region_size"] = patch_region
    cur += 4
    if patch_region < 0 or cur + patch_region > len(game_ctx):
        return fail("patch_region_size", f"{patch_region} doesn't fit in remaining buffer")
    cur += patch_region

    # NbExtras (U8)
    if cur + 1 > len(game_ctx):
        return fail("nb_extras", "truncated")
    nb_extras = game_ctx[cur]
    out["values"]["nb_extras"] = nb_extras
    cur += 1
    if nb_extras > MAX_EXTRAS:
        return fail("nb_extras", f"{nb_extras} > {MAX_EXTRAS}")

    # ListExtra: nb_extras × extra_size (ABI-dependent)
    if cur + nb_extras * extra_size > len(game_ctx):
        return fail("list_extra", f"need {nb_extras * extra_size} B, have {len(game_ctx) - cur}")
    cur += nb_extras * extra_size

    # NbZones (S32) — the field Dark Monk tripped at 855638016 under the 64-bit hypothesis
    if cur + 4 > len(game_ctx):
        return fail("nb_zones", "truncated")
    nb_zones = struct.unpack_from("<i", game_ctx, cur)[0]
    out["values"]["nb_zones"] = nb_zones
    cur += 4
    if not (0 <= nb_zones <= MAX_ZONES):
        return fail("nb_zones", f"{nb_zones} out of [0,{MAX_ZONES}]")

    # Zones: nb_zones × 16 bytes (Info1/2/3/7 written individually, no struct read)
    if cur + nb_zones * ZONE_RECORD_BYTES > len(game_ctx):
        return fail("list_zones", "truncated")
    cur += nb_zones * ZONE_RECORD_BYTES

    # NbIncrust (U8)
    if cur + 1 > len(game_ctx):
        return fail("nb_incrust", "truncated")
    nb_incrust = game_ctx[cur]
    out["values"]["nb_incrust"] = nb_incrust
    cur += 1
    if nb_incrust > MAX_INCRUST_DISP:
        return fail("nb_incrust", f"{nb_incrust} > {MAX_INCRUST_DISP}")

    # ListIncrustDisp: nb_incrust × 16 bytes (host-independent)
    if cur + nb_incrust * INCRUST_DISP_BYTES > len(game_ctx):
        return fail("list_incrust", "truncated")
    cur += nb_incrust * INCRUST_DISP_BYTES

    # NbFlows (U8)
    if cur + 1 > len(game_ctx):
        return fail("nb_flows", "truncated")
    nb_flows = game_ctx[cur]
    out["values"]["nb_flows"] = nb_flows
    cur += 1
    if nb_flows > MAX_FLOWS:
        return fail("nb_flows", f"{nb_flows} > {MAX_FLOWS}")

    # Per-flow loop: flow_size + NbDots (U8) + NbDots × ONE_DOT_BYTES.
    # Off to be a Wizard tripped at flow #5 NbDots=254 under the 64-bit hypothesis.
    flow_dots = []
    for i in range(nb_flows):
        if cur + flow_size + 1 > len(game_ctx):
            return fail(f"flow_{i}_record", "truncated")
        cur += flow_size
        nb_dots = game_ctx[cur]
        cur += 1
        flow_dots.append(nb_dots)
        if nb_dots > MAX_FLOW_DOTS:
            return fail(f"flow_{i}_nb_dots", f"{nb_dots} > {MAX_FLOW_DOTS}")
        if cur + nb_dots * ONE_DOT_BYTES > len(game_ctx):
            return fail(f"flow_{i}_dots", "truncated")
        cur += nb_dots * ONE_DOT_BYTES
    out["values"]["flow_nb_dots"] = flow_dots

    out["ok"] = True
    out["final_offset"] = cur
    return out


def predict_abi_and_outcome(
    game_ctx: bytes, nb_objets: int
) -> Tuple[str, str, Dict[str, Any]]:
    """Return (predicted_abi, predicted_outcome, detail).

    predicted_abi: "32", "64", "ambiguous", or "corrupt"
    predicted_outcome: "ok", "ctxerr_at_<field>", or "corrupt"

    "32" / "64": exactly one ABI walks cleanly through every landmark.
    "ambiguous": both walks pass — engine's auto-retry will pick the right one,
        but the probe can't distinguish them without running the engine.
    "corrupt": neither walks pass; save is genuinely broken.
    """
    sim_32 = forward_simulate(game_ctx, nb_objets, "32")
    sim_64 = forward_simulate(game_ctx, nb_objets, "64")
    detail = {"sim_32": sim_32, "sim_64": sim_64}

    if sim_32["ok"] and sim_64["ok"]:
        return ("ambiguous", "ok", detail)
    if sim_32["ok"]:
        return ("32", "ok", detail)
    if sim_64["ok"]:
        return ("64", "ok", detail)
    # Both failed.  Pick the one that got further; that's the most likely ABI
    # for which the save is corrupt at the reported field.
    f32 = sim_32.get("final_offset", sim_32["values"].get("objects_end_offset", 0))
    f64 = sim_64.get("final_offset", sim_64["values"].get("objects_end_offset", 0))
    closer = sim_32 if f32 >= f64 else sim_64
    abi_guess = closer["abi"]
    return (
        "corrupt" if max(f32, f64) == 0 else abi_guess,
        f"ctxerr_at_{closer['stopped_at']}",
        detail,
    )


def score_abi_after_objects(game_ctx: bytes, nb_objets: int, stride: int) -> Tuple[int, Dict[str, Any]]:
    """Higher score = more plausible. Uses NbPatches + patch blob + extras count (SAVEGAME.CPP)."""
    details: Dict[str, Any] = {"stride": stride}
    if nb_objets < 0 or nb_objets > MAX_OBJETS_ENGINE:
        details["reason"] = "bad_nb_objets"
        return (-10000, details)
    off = OFFSET_OBJ0_FROM_GAME_CTX + nb_objets * stride
    if off + 8 > len(game_ctx):
        details["reason"] = "truncated_tail"
        return (-5000, details)
    nb_p = struct.unpack_from("<i", game_ctx, off)[0]
    patch_region = struct.unpack_from("<i", game_ctx, off + 4)[0]
    details["nb_patches"] = nb_p
    details["patch_region_size"] = patch_region
    rem = len(game_ctx) - off - 8  # bytes after NbPatches+patch_region_size fields
    score = 0
    if 0 <= nb_p <= MAX_PATCHES:
        score += 20
    else:
        score -= 50

    # Patch payload must fit exactly in the remaining game context (no +1024 slack).
    patch_ok = 0 <= patch_region <= rem and patch_region < 50_000_000
    if patch_ok:
        score += 28
    else:
        score -= 45

    if patch_ok and nb_p > 0 and patch_region >= nb_p:
        score += 5

    # After patch blob: U8 extra count then sizeof(T_EXTRA)*count (ABI-sized extras — loose bound).
    off_ex = off + 8 + patch_region
    if patch_ok and off_ex < len(game_ctx):
        nb_ex = game_ctx[off_ex]
        details["nb_extras_byte"] = nb_ex
        if nb_ex <= MAX_EXTRAS:
            score += 10
            # Conservative lower bound per T_EXTRA (pointers differ 32/64; this is a soft check).
            min_extra_span = 64 * nb_ex + 1
            if off_ex + min_extra_span <= len(game_ctx):
                score += 8
        else:
            score -= 15

    return (score, details)


def guess_obj3d_abi(game_ctx: bytes, nb_objets: int, mode: str) -> Tuple[str, Dict[str, Any]]:
    if mode == "32":
        return ("32", {"stride": PER_OBJECT_32, "method": "override"})
    if mode == "64":
        return ("64", {"stride": PER_OBJECT_64, "method": "override"})
    s32, d32 = score_abi_after_objects(game_ctx, nb_objets, PER_OBJECT_32)
    s64, d64 = score_abi_after_objects(game_ctx, nb_objets, PER_OBJECT_64)
    out: Dict[str, Any] = {"score_32": s32, "score_64": s64, "detail_32": d32, "detail_64": d64}
    # If neither score is positive, both alignments failed plausibility checks — do not pick "less bad".
    if s32 <= 0 and s64 <= 0:
        return ("ambiguous", {**out, "stride": None, "method": "heuristic"})
    if s32 > s64 + 2:
        return ("32", {**out, "stride": PER_OBJECT_32, "method": "heuristic"})
    if s64 > s32 + 2:
        return ("64", {**out, "stride": PER_OBJECT_64, "method": "heuristic"})
    return ("ambiguous", {**out, "stride": None, "method": "heuristic"})


def probe_payload(
    payload: bytes,
    header_end: int,
    *,
    do_dump: bool,
    hex_cap: int,
    obj3d_abi: str,
    helper: Optional[str],
    compressed_meta: Optional[Dict[str, Any]],
) -> Dict[str, Any]:
    out: Dict[str, Any] = {
        "payload_len": len(payload),
        "screenshot_bytes": SCREENSHOT_BYTES,
    }
    if compressed_meta:
        out["decompressed_size"] = compressed_meta.get("decompressed_payload_size")
        out["save_decompress_ok"] = compressed_meta.get("save_decompress_ok")

    if len(payload) < SCREENSHOT_BYTES + OFFSET_NB_OBJETS_FROM_GAME_CTX + 4:
        out["nb_objets"] = None
        out["nb_objets_note"] = "payload too short for fixed-offset NbObjets read"
        return out

    game_ctx = payload[SCREENSHOT_BYTES:]
    nb_off = OFFSET_NB_OBJETS_FROM_GAME_CTX
    nb = struct.unpack_from("<i", game_ctx, nb_off)[0]
    out["nb_objets_offset_from_file_start"] = header_end + SCREENSHOT_BYTES + nb_off
    out["nb_objets"] = nb
    if nb < 0 or nb > 256:
        out["nb_objets_sanity"] = "unlikely (misaligned or corrupt)"
    elif nb > MAX_OBJETS_ENGINE:
        out["nb_objets_sanity"] = "suspicious (> engine typical max)"
    else:
        out["nb_objets_sanity"] = "plausible"

    abi, abi_info = guess_obj3d_abi(game_ctx, nb, obj3d_abi)  # mode: auto|32|64
    out["obj3d_abi"] = abi
    out["obj3d_stride"] = abi_info.get("stride")
    out["obj3d_abi_method"] = abi_info.get("method")
    for k in ("score_32", "score_64"):
        if k in abi_info:
            out[k] = abi_info[k]

    # Forward-simulate LoadContexte under each ABI hypothesis using the engine's
    # actual MAX_ constants.  Predicts what `lba2 --save-load-test` will report:
    #   predicted_outcome=ok        → expect ok_init / ok_loaded
    #   predicted_outcome=ctxerr_at_<field>
    #                               → expect ctxerr from that bound
    # Refines obj3d_abi when the heuristic flagged "ambiguous" but exactly one
    # forward walk is clean.
    pred_abi, pred_outcome, pred_detail = predict_abi_and_outcome(game_ctx, nb)
    out["predicted_abi"] = pred_abi
    out["predicted_outcome"] = pred_outcome
    out["predicted_detail"] = {
        "sim_32_ok": pred_detail["sim_32"]["ok"],
        "sim_64_ok": pred_detail["sim_64"]["ok"],
        "sim_32_stopped_at": pred_detail["sim_32"].get("stopped_at"),
        "sim_64_stopped_at": pred_detail["sim_64"].get("stopped_at"),
        "sim_32_values": pred_detail["sim_32"].get("values"),
        "sim_64_values": pred_detail["sim_64"].get("values"),
    }
    # If the heuristic was ambiguous but the forward walk picks one cleanly,
    # promote that to the obj3d_abi answer (preserving the original under
    # obj3d_abi_heuristic for traceability).
    if abi == "ambiguous" and pred_abi in ("32", "64"):
        out["obj3d_abi_heuristic"] = "ambiguous"
        out["obj3d_abi"] = pred_abi
        out["obj3d_abi_method"] = "forward_simulate"
        out["obj3d_stride"] = PER_OBJECT_32 if pred_abi == "32" else PER_OBJECT_64

    if do_dump:
        out["dump_lines"] = dump_game_context(game_ctx, hex_cap)

    return out


def probe_one(
    path: str,
    *,
    do_dump: bool,
    hex_cap: int,
    obj3d_abi: str,
    helper: Optional[str],
) -> Dict[str, Any]:
    with open(path, "rb") as f:
        data = f.read()
    row: Dict[str, Any] = {"path": path, "size": len(data)}
    compressed_meta: Optional[Dict[str, Any]] = None
    try:
        h = parse_header(data)
        row.update(h)
        row["version_byte_hex"] = f"0x{h['version_byte']:02x}"
        if h["num_version"] < 34:
            row["layout_warning"] = (
                "num_version<34: fixed NbObjets offset and --dump follow retail NUM_VERSION>=34 "
                "(DEBUG/old branches differ — SAVEGAME.CPP LoadContexte)."
            )
        hint = filename_hint(os.path.basename(path))
        if hint:
            row["filename_hint"] = hint

        if h["compressed"]:
            row["compression_ratio"] = (
                float(len(data) - h["compressed_blob_start"]) / float(h["decompressed_payload_size"])
                if h.get("decompressed_payload_size", 0) > 0
                else None
            )
            decomp_sz = h["decompressed_payload_size"]
            comp = data[h["compressed_blob_start"] :]
            compressed_meta = {
                "decompressed_payload_size": decomp_sz,
                "save_decompress_ok": False,
            }
            if helper is None:
                row["note"] = (
                    "Compressed save: set LBA2_SAVE_DECOMPRESS or build "
                    "target save_decompress (see docs/SAVEGAME.md)."
                )
                row["save_decompress_ok"] = False
            else:
                try:
                    decompressed = decompress_payload(comp, decomp_sz, helper)
                    compressed_meta["save_decompress_ok"] = True
                    row["save_decompress_ok"] = True
                    row.update(
                        probe_payload(
                            decompressed,
                            h["header_end"],
                            do_dump=do_dump,
                            hex_cap=hex_cap,
                            obj3d_abi=obj3d_abi,
                            helper=helper,
                            compressed_meta=compressed_meta,
                        )
                    )
                except Exception as ex:
                    row["save_decompress_ok"] = False
                    row["decompress_error"] = str(ex)
                    row["note"] = f"LZSS decompress failed: {ex}"
        else:
            row.update(
                probe_payload(
                    data[h["header_end"] :],
                    h["header_end"],
                    do_dump=do_dump,
                    hex_cap=hex_cap,
                    obj3d_abi=obj3d_abi,
                    helper=helper,
                    compressed_meta=None,
                )
            )
    except Exception as e:
        row["error"] = str(e)
    return row


def format_row(r: Dict[str, Any]) -> str:
    lines: List[str] = []
    lines.append(f"path:         {r.get('path')}")
    lines.append(f"size:         {r.get('size')}")
    if "error" in r:
        lines.append(f"error:        {r['error']}")
        return "\n".join(lines)
    lines.append(
        f"version_byte: 0x{r['version_byte']:02x}  (num_version={r['num_version']}, compressed={r['compressed']})"
    )
    lines.append(f"cube:         {r['cube']}")
    lines.append(f"player_name:  {r['player_name']!r}")
    lines.append(f"header_end:   {r['header_end']}")
    if r.get("filename_hint"):
        lines.append(f"filename:     {r['filename_hint']}")
    if r.get("layout_warning"):
        lines.append(f"layout_warn:  {r['layout_warning']}")
    if r["compressed"]:
        lines.append(f"decomp_size:  {r.get('decompressed_payload_size')}")
        lines.append(f"comp_blob:    {r.get('compressed_blob_len')} bytes @ offset {r.get('compressed_blob_start')}")
        cr = r.get("compression_ratio")
        if cr is not None:
            lines.append(f"comp_ratio:   {cr:.4f} (compressed / decompressed)")
        if "save_decompress_ok" in r:
            lines.append(f"decompress:   {r.get('save_decompress_ok')}")
        if r.get("decompress_error"):
            lines.append(f"decomp_err:   {r['decompress_error']}")
    if r.get("note"):
        lines.append(f"note:         {r['note']}")

    if r.get("payload_len") is not None:
        lines.append(f"payload_len:  {r.get('payload_len')} (screenshot + game context)")
    if r.get("nb_objets") is not None:
        lines.append(f"nb_objets:    {r.get('nb_objets')}  ({r.get('nb_objets_sanity')})")
        lines.append(f"nb_obj_file:  offset {r.get('nb_objets_offset_from_file_start')}")
    elif r.get("nb_objets_note"):
        lines.append(f"nb_objets:    <n/a>  {r.get('nb_objets_note', '')}")
    if r.get("obj3d_abi"):
        lines.append(f"obj3d_abi:    {r.get('obj3d_abi')}  (stride={r.get('obj3d_stride')}, {r.get('obj3d_abi_method')})")
        if r.get("score_32") is not None:
            lines.append(f"abi_scores:   32={r.get('score_32')} 64={r.get('score_64')}")

    for dl in r.get("dump_lines") or ():
        lines.append(dl)
    return "\n".join(lines)


def collect_paths(paths: List[str], recursive: bool) -> List[str]:
    out: List[str] = []
    for p in paths:
        if os.path.isdir(p):
            if recursive:
                for root, _dirs, files in os.walk(p):
                    for name in sorted(files):
                        if name.lower().endswith(".lba"):
                            out.append(os.path.join(root, name))
            else:
                for name in sorted(os.listdir(p)):
                    if name.lower().endswith(".lba"):
                        out.append(os.path.join(p, name))
        else:
            out.append(p)
    return out


def print_summary(rows: List[Dict[str, Any]], *, helper: Optional[str], file=sys.stderr) -> None:
    """One-line counts after scanning (use with --json-lines: summary on stderr, NDJSON on stdout)."""
    n = len(rows)
    err = sum(1 for r in rows if "error" in r)
    comp = sum(1 for r in rows if r.get("compressed") is True)
    uncomp = sum(1 for r in rows if r.get("compressed") is False)
    dec_ok = sum(1 for r in rows if r.get("save_decompress_ok") is True)
    dec_miss = sum(1 for r in rows if r.get("compressed") and r.get("helper_missing"))
    dec_fail = sum(
        1
        for r in rows
        if r.get("compressed")
        and helper is not None
        and r.get("save_decompress_ok") is False
    )
    abi32 = sum(1 for r in rows if r.get("obj3d_abi") == "32")
    abi64 = sum(1 for r in rows if r.get("obj3d_abi") == "64")
    ambi = sum(1 for r in rows if r.get("obj3d_abi") == "ambiguous")
    warn = sum(1 for r in rows if r.get("layout_warning"))
    print(
        f"save_probe summary: files={n} errors={err} compressed={comp} uncompressed={uncomp} "
        f"decompress_ok={dec_ok} decompress_fail={dec_fail} helper_missing={dec_miss} "
        f"abi_32={abi32} abi_64={abi64} abi_ambiguous={ambi} layout_warn={warn}",
        file=file,
    )


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Inspect LBA2 .lba saves: header, LZSS payload (save_decompress), NbObjets, ABI guess, optional dump.",
        epilog=(
            "Environment: LBA2_SAVE_DECOMPRESS=path/to/save_decompress; "
            "LBA2_SAVE_PROBE_ABI=32|64 when --obj3d-abi auto (CLI overrides env)."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument(
        "paths",
        nargs="*",
        help=".lba files or directories (default: *.lba; use --recursive for subdirs)",
    )
    ap.add_argument(
        "--json-lines",
        action="store_true",
        help="print one JSON object per line (NDJSON) to stdout",
    )
    ap.add_argument(
        "--dump",
        action="store_true",
        help="print offset-annotated prefix of game context (LoadContexte order)",
    )
    ap.add_argument(
        "--dump-hex-cap",
        type=int,
        default=32,
        metavar="N",
        help="max hex bytes shown per abbreviated blob (default 32)",
    )
    ap.add_argument(
        "--obj3d-abi",
        choices=("auto", "32", "64"),
        default="auto",
        help="override per-object stream width heuristic (default auto)",
    )
    ap.add_argument(
        "--recursive",
        action="store_true",
        help="when argument is a directory, find *.lba in subdirectories too",
    )
    ap.add_argument(
        "--summary",
        action="store_true",
        help="print scan counts to stderr (safe with --json-lines)",
    )
    ap.add_argument(
        "--compare",
        metavar="PAIR",
        nargs="+",
        help="pairs A B A B ... print side-by-side summary for each pair",
    )
    args = ap.parse_args()

    helper = find_save_decompress()
    abi_mode = effective_obj3d_abi(args.obj3d_abi)

    paths = collect_paths(args.paths, args.recursive)

    rows = [
        probe_one(
            p,
            do_dump=args.dump,
            hex_cap=args.dump_hex_cap,
            obj3d_abi=abi_mode,
            helper=helper,
        )
        for p in paths
    ]
    for r in rows:
        if r.get("compressed") and helper is None:
            r["helper_missing"] = True

    if args.summary:
        print_summary(rows, helper=helper, file=sys.stderr)

    if args.compare:
        pairs = args.compare
        if len(pairs) % 2 != 0:
            print("error: --compare needs an even number of paths (A B A B ...)", file=sys.stderr)
            return 2
        for i in range(0, len(pairs), 2):
            a, b = pairs[i], pairs[i + 1]
            ra, rb = (
                probe_one(
                    a,
                    do_dump=False,
                    hex_cap=args.dump_hex_cap,
                    obj3d_abi=abi_mode,
                    helper=helper,
                ),
                probe_one(
                    b,
                    do_dump=False,
                    hex_cap=args.dump_hex_cap,
                    obj3d_abi=abi_mode,
                    helper=helper,
                ),
            )
            print("=== compare ===")
            print(format_row(ra))
            print("--- vs ---")
            print(format_row(rb))
            sa = ra.get("size")
            sb = rb.get("size")
            if isinstance(sa, int) and isinstance(sb, int):
                print(f"size_delta:   {sb - sa} (B - A)")
            ca = ra.get("compressed")
            cb = rb.get("compressed")
            if ca is not None and cb is not None and ca == cb == False:
                na = ra.get("nb_objets")
                nb = rb.get("nb_objets")
                if na is not None and nb is not None and na == nb:
                    print(
                        f"hint:         same NbObjets ({na}) but sizes differ → often different per-object blob sizes (ABI) or different tail state"
                    )
        return 0

    if args.json_lines:
        for r in rows:
            sys.stdout.write(json.dumps(r, ensure_ascii=False) + "\n")
        return 0

    for r in rows:
        print(format_row(r))
        print()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        try:
            sys.stdout.close()
        except Exception:
            pass
        sys.exit(0)
