#!/usr/bin/env python3
"""Disassemble the Life (LM_*) and Track (TM_*) scripts in SCENE.HQR.

Operand layouts are the runtime's own reads: DoLife, DoFuncLife and DoTest in
SOURCES/GERELIFE.CPP for Life, DoTrack and SearchOffsetTrack in
SOURCES/GERETRAK.CPP for Track. Opcode numbers are parsed from SOURCES/COMMON.H.
The format is written up in docs/SCRIPTS.md.

Usage:
  scene_disasm.py <SCENE.HQR> --cube N      text listing of one cube's scripts
  scene_disasm.py <SCENE.HQR> --check       decode every script, verify, print totals
  scene_disasm.py <SCENE.HQR> --find OP...  list every use of the named opcodes (LM_/TM_ names)
  scene_disasm.py --table                   print the operand tables as Markdown (docs/SCRIPTS.md)

--check fails (exit 1) unless every script decodes exactly to its stored length,
ends with its only END, every jump lands on an instruction start, and every
SET_TRACK, SET_TRACK_OBJ and SET_COMPORTEMENT_OBJ offset lands on an instruction
start in the script it names. Cube N is HQR entry N + 1; entry 0
holds the size of the largest scene.
"""
import argparse
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hqr_inspect  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

# Operand types: B u8, b s8, h s16, H u16, J Life jump (s16, absolute in this object's
# Life), G Track jump (s16, absolute in this object's Track), Z zero-terminated string,
# C condition (DoFuncLife + DoTest), F switch function (DoFuncLife only), T switch test
# (DoTest at the width of the last LM_SWITCH), M move (u8; DoLife's AdjustDirObject reads
# one more byte for some moves).
LIFE = {
    "END": [], "END_LIFE": [], "RETURN": [], "END_COMPORTEMENT": [],
    "COMPORTEMENT": [("num", "B")],
    "FALLABLE": [("on", "B")],
    "COMPORTEMENT_HERO": [("comportement", "B")],
    "SET_MAGIC_LEVEL": [("level", "B")],
    "SUB_MAGIC_POINT": [("points", "B")],
    "CAM_FOLLOW": [("obj", "B")],
    "KILL_OBJ": [("obj", "B")],
    "SUICIDE": [],
    "SET_DIR": [("move", "M")],
    "SET_DIR_OBJ": [("obj", "B"), ("move", "M")],
    "SET_COMPORTEMENT": [("life", "J")],
    "SET_COMPORTEMENT_OBJ": [("obj", "B"), ("life", "h")],
    "SET_LIFE_POINT_OBJ": [("obj", "B"), ("points", "B")],
    "SUB_LIFE_POINT_OBJ": [("obj", "B"), ("points", "B")],
    "ADD_LIFE_POINT_OBJ": [("obj", "B"), ("points", "B")],
    "HIT_OBJ": [("obj", "B"), ("force", "B")],
    "SET_TRACK": [("track", "h")],
    "SET_TRACK_OBJ": [("obj", "B"), ("track", "h")],
    "STOP_L_TRACK": [],
    "STOP_L_TRACK_OBJ": [("obj", "B")],
    "RESTORE_L_TRACK": [],
    "RESTORE_L_TRACK_OBJ": [("obj", "B")],
    "IF": [("cond", "C"), ("else", "J")],
    "AND_IF": [("cond", "C"), ("else", "J")],
    "OR_IF": [("cond", "C"), ("then", "J")],
    "SWIF": [("cond", "C"), ("else", "J")],
    "SNIF": [("cond", "C"), ("else", "J")],
    "ONEIF": [("cond", "C"), ("else", "J")],
    "NEVERIF": [("cond", "C"), ("to", "J")],
    "OFFSET": [("to", "J")],
    "ELSE": [("to", "J")],
    "SWITCH": [("func", "F")],
    "CASE": [("next", "J"), ("test", "T")],
    "OR_CASE": [("then", "J"), ("test", "T")],
    "BREAK": [("to", "J")],
    "DEFAULT": [], "END_SWITCH": [],
    "BODY": [("body", "B")],
    "BODY_OBJ": [("obj", "B"), ("body", "B")],
    "ANIM_OBJ": [("obj", "B"), ("anim", "H")],
    "ANIM": [("anim", "H")],
    "ANIM_SET": [("anim", "H")],
    "MESSAGE_OBJ": [("obj", "B"), ("text", "h")],
    "ADD_MESSAGE_OBJ": [("obj", "B"), ("text", "h")],
    "MESSAGE": [("text", "h")],
    "ADD_MESSAGE": [("text", "h")],
    "MESSAGE_ZOE": [("text", "h")],
    "SET_VAR_CUBE": [("var", "B"), ("value", "B")],
    "ADD_VAR_CUBE": [("var", "B"), ("value", "B")],
    "SUB_VAR_CUBE": [("var", "B"), ("value", "B")],
    "SET_VAR_GAME": [("var", "B"), ("value", "h")],
    "ADD_VAR_GAME": [("var", "B"), ("value", "h")],
    "SUB_VAR_GAME": [("var", "B"), ("value", "h")],
    "SET_USED_INVENTORY": [("item", "B")],
    "GIVE_GOLD_PIECES": [("count", "h")],
    "ADD_GOLD_PIECES": [("count", "h")],
    "USE_ONE_LITTLE_KEY": [], "INC_CHAPTER": [],
    "STATE_INVENTORY": [("item", "B"), ("body", "B")],
    "FOUND_OBJECT": [("item", "B")],
    "SET_DOOR_LEFT": [("dist", "h")],
    "SET_DOOR_RIGHT": [("dist", "h")],
    "SET_DOOR_UP": [("dist", "h")],
    "SET_DOOR_DOWN": [("dist", "h")],
    "GIVE_BONUS": [("keep", "B")],
    "CHANGE_CUBE": [("cube", "B")],
    "ADD_FUEL": [("fuel", "B")],
    "SUB_FUEL": [("fuel", "B")],
    "SET_HOLO_POS": [("pos", "B")],
    "CLR_HOLO_POS": [("pos", "B")],
    "SET_GRM": [("zone", "B"), ("on", "B")],
    "SET_CHANGE_CUBE": [("zone", "B"), ("on", "B")],
    "INC_CLOVER_BOX": [],
    "OBJ_COL": [("on", "B")],
    "INVISIBLE": [("on", "B")],
    "BRICK_COL": [("mode", "B")],
    "POS_POINT": [("point", "B")],
    "PLAY_ACF": [("name", "Z")],
    "ADD_CHOICE": [("text", "h")],
    "ASK_CHOICE": [("text", "h")],
    "ASK_CHOICE_OBJ": [("obj", "B"), ("text", "h")],
    "FULL_POINT": [],
    "BETA": [("beta", "h")],
    "BULLE": [("on", "B")],
    "GAME_OVER": [], "THE_END": [], "BRUTAL_EXIT": [],
    "PLAY_MUSIC": [("music", "B")],
    "ECHELLE": [("zone", "B"), ("on", "B")],
    "SET_HIT_ZONE": [("zone", "B"), ("force", "B")],
    "SET_ARMURE": [("armure", "b")],
    "SET_ARMURE_OBJ": [("obj", "B"), ("armure", "b")],
    "SAVE_COMPORTEMENT": [],
    "SAVE_COMPORTEMENT_OBJ": [("obj", "B")],
    "RESTORE_COMPORTEMENT": [],
    "RESTORE_COMPORTEMENT_OBJ": [("obj", "B")],
    "SAMPLE": [("sample", "h")],
    "SAMPLE_RND": [("sample", "h")],
    "SAMPLE_ALWAYS": [("sample", "h")],
    "SAMPLE_STOP": [("sample", "h")],
    "REPEAT_SAMPLE": [("sample", "h"), ("repeat", "B")],
    "BACKGROUND": [("on", "B")],
    "SET_RAIL": [("zone", "B"), ("on", "B")],
    "INVERSE_BETA": [], "NO_BODY": [], "POPCORN": [],
    "FLOW_POINT": [("point", "B"), ("flow", "B")],
    "FLOW_OBJ": [("obj", "B"), ("flow", "B")],
    "SET_ANIM_DIAL": [("anim", "H")],
    "PCX": [("pcx", "B"), ("effect", "B")],
    "PCX_MESS_OBJ": [("pcx", "B"), ("effect", "B"), ("obj", "B"), ("text", "h")],
    "PALETTE": [("palette", "B")],
    "FADE_TO_PAL": [("palette", "B")],
    "SET_CAMERA": [("zone", "B"), ("on", "B")],
    "CAMERA_CENTER": [("quarter", "B")],
    "MEMO_ARDOISE": [("num", "B")],
    "SAVE_HERO": [], "RESTORE_HERO": [],
    "SHADOW_OBJ": [("obj", "B"), ("on", "B")],
    "TRACK_TO_VAR_GAME": [("var", "B")],
    "VAR_GAME_TO_TRACK": [("var", "B")],
    "ACTION": [],
    "SET_FRAME": [("frame", "B")],
    "SET_SPRITE": [("sprite", "h")],
    "SET_FRAME_3DS": [("frame", "B")],
    "IMPACT_OBJ": [("obj", "B"), ("impact", "H"), ("dy", "h")],
    "IMPACT_POINT": [("point", "B"), ("impact", "H")],
    "NO_CHOC": [("on", "B")],
    "CINEMA_MODE": [("on", "B")],
    "ANIM_TEXTURE": [("on", "B")],
    "END_MESSAGE": [],
    "END_MESSAGE_OBJ": [("obj", "B")],
    "INIT_BUGGY": [("mode", "B")],
    "NEW_SAMPLE": [("sample", "h"), ("decalage", "h"), ("volume", "B"), ("frequence", "h")],
    "PARM_SAMPLE": [("decalage", "h"), ("volume", "B"), ("frequence", "h")],
    "POS_OBJ_AROUND": [("obj", "B"), ("around", "B")],
    "ESCALATOR": [("zone", "B"), ("on", "B")],
    "ECLAIR": [("duration", "B")],
    "PLUIE": [("duration", "B")],
}

# Conditions (LF_*): (parameter, compare width). DoFuncLife leaves TypeAnswer at
# RET_S8 unless the case sets RET_U8 or RET_S16; DoTest then reads one test byte
# (LT_*) and a value of that width. No condition returns RET_STRING.
FUNC = {
    "COL": (None, 1), "CHAPTER": (None, 1), "LIFE_POINT": (None, 2), "HIT_BY": (None, 1),
    "HIT_OBJ_BY": ("obj", 1), "ACTION": (None, 1), "LIFE_POINT_OBJ": ("obj", 2),
    "COL_OBJ": ("obj", 1), "DISTANCE": ("obj", 2), "DISTANCE_3D": ("obj", 2),
    "CONE_VIEW": ("obj", 2), "ZONE": (None, 1), "NB_GOLD_PIECES": (None, 2),
    "NB_LITTLE_KEYS": (None, 1), "COMPORTEMENT_HERO": (None, 1), "MAGIC_LEVEL": (None, 1),
    "MAGIC_POINT": (None, 1), "CHOICE": (None, 2), "FUEL": (None, 2), "L_TRACK": (None, 1),
    "ZONE_OBJ": ("obj", 1), "VAR_CUBE": ("var", 1), "VAR_GAME": ("var", 2),
    "USE_INVENTORY": ("item", 1), "L_TRACK_OBJ": ("obj", 1), "BODY": (None, 1),
    "BODY_OBJ": ("obj", 1), "ANIM": (None, 2), "ANIM_OBJ": ("obj", 2), "CARRY_BY": (None, 1),
    "CARRY_OBJ_BY": ("obj", 1), "CDROM": (None, 1), "ECHELLE": ("zone", 1), "RND": ("max", 1),
    "RAIL": ("zone", 1), "BETA": (None, 2), "BETA_OBJ": ("obj", 2), "ANGLE": ("obj", 2),
    "ANGLE_OBJ": ("obj", 2), "REAL_ANGLE": ("obj", 2), "DISTANCE_MESSAGE": ("obj", 2),
    "DEMO": (None, 1), "COL_DECORS": (None, 1), "COL_DECORS_OBJ": ("obj", 1),
    "PROCESSOR": (None, 1), "OBJECT_DISPLAYED": ("obj", 1),
}
# The compare value is signed for RET_S8 and RET_S16, unsigned for RET_U8.
FUNC_U8 = {"L_TRACK", "VAR_CUBE", "L_TRACK_OBJ", "RND", "COL_DECORS"}

# Track (TM_*): sizes agree between DoTrack and SearchOffsetTrack. The WAIT_NB_*
# timers carry 4 bytes of runtime state after their count; LOOP carries its
# initial count, the live counter, then the target.
TRACK = {
    "END": [], "NOP": [], "BODY": [("body", "B")], "ANIM": [("anim", "H")],
    "GOTO_POINT": [("point", "B")], "WAIT_ANIM": [],
    "LOOP": [("count", "B"), ("counter", "B"), ("to", "G")],
    "ANGLE": [("beta", "h")], "POS_POINT": [("point", "B")], "LABEL": [("label", "B")],
    "GOTO": [("to", "G")], "STOP": [], "GOTO_SYM_POINT": [("point", "B")],
    "WAIT_NB_ANIM": [("count", "B"), ("counter", "B")], "SAMPLE": [("sample", "h")],
    "GOTO_POINT_3D": [("point", "B")], "SPEED": [("speed", "h")],
    "BACKGROUND": [("on", "B")],
    "WAIT_NB_SECOND": [("seconds", "B"), ("timer", "I")], "NO_BODY": [],
    "BETA": [("beta", "h")], "OPEN_LEFT": [("dist", "h")], "OPEN_RIGHT": [("dist", "h")],
    "OPEN_UP": [("dist", "h")], "OPEN_DOWN": [("dist", "h")], "CLOSE": [], "WAIT_DOOR": [],
    "SAMPLE_RND": [("sample", "h")], "SAMPLE_ALWAYS": [("sample", "h")],
    "SAMPLE_STOP": [("sample", "h")], "PLAY_ACF": [("name", "Z")],
    "REPEAT_SAMPLE": [("repeat", "h")], "SIMPLE_SAMPLE": [("sample", "h")],
    "FACE_TWINSEN": [("state", "h")], "ANGLE_RND": [("beta", "h"), ("state", "h")],
    "REM": [], "WAIT_NB_DIZIEME": [("tenths", "B"), ("timer", "I")], "DO": [],
    "SPRITE": [("sprite", "h")],
    "WAIT_NB_SECOND_RND": [("seconds", "B"), ("timer", "I")], "AFF_TIMER": [],
    "SET_FRAME": [("frame", "B")], "SET_FRAME_3DS": [("frame", "B")],
    "SET_START_3DS": [("frame", "B")], "SET_END_3DS": [("frame", "B")],
    "START_ANIM_3DS": [("fps", "B")], "STOP_ANIM_3DS": [], "WAIT_ANIM_3DS": [],
    "WAIT_FRAME_3DS": [("frame", "B")],
    "WAIT_NB_DIZIEME_RND": [("tenths", "B"), ("timer", "I")],
    "DECALAGE": [("decalage", "h")], "FREQUENCE": [("frequence", "h")],
    "VOLUME": [("volume", "B")],
}

SIZE = {"B": 1, "b": 1, "h": 2, "H": 2, "I": 4, "J": 2, "G": 2}
FMT = {"B": "<B", "b": "<b", "h": "<h", "H": "<H", "I": "<I", "J": "<h", "G": "<h"}


def load_defines():
    path = os.path.join(REPO, "SOURCES", "COMMON.H")
    defs = {}
    with open(path, encoding="latin-1") as f:
        for line in f:
            m = re.search(r"#define\s+((?:LM|LF|LT|TM|MOVE)_\w+)\s+(\d+)\b", line)
            if m:
                defs[m.group(1)] = int(m.group(2))
    return defs


DEFS = load_defines()


def table(prefix, names):
    out = {}
    for name in names:
        key = prefix + name
        if key not in DEFS:
            sys.exit(f"{key} is not defined in SOURCES/COMMON.H")
        out[DEFS[key]] = name
    return out


LIFE_BY_NUM = table("LM_", LIFE)
FUNC_BY_NUM = table("LF_", FUNC)
TRACK_BY_NUM = table("TM_", TRACK)
TEST_BY_NUM = {v: k[3:] for k, v in DEFS.items() if k.startswith("LT_")}
TEST_SYM = {"EQUAL": "==", "SUP": ">", "LESS": "<", "SUP_EQUAL": ">=", "LESS_EQUAL": "<=",
            "DIFFERENT": "!="}
# AdjustDirObject (GERELIFE.CPP) reads Info3 for these moves.
MOVE_WITH_ARG = {DEFS[m] for m in ("MOVE_FOLLOW", "MOVE_SAME_XZ", "MOVE_SAME_XZ_BETA",
                                   "MOVE_CIRCLE", "MOVE_CIRCLE2")}
MOVE_BY_NUM = {v: k[5:] for k, v in DEFS.items() if k.startswith("MOVE_")}


class DecodeError(Exception):
    pass


def need(b, pc, n):
    if pc + n > len(b):
        raise DecodeError(f"operand runs past the end at {pc}")


def read_cond(b, pc, with_test):
    """DoFuncLife, then DoTest if with_test. Returns (text, width, pc)."""
    need(b, pc, 1)
    f = b[pc]
    pc += 1
    if f not in FUNC_BY_NUM:
        raise DecodeError(f"unknown condition {f} at {pc - 1}")
    name = FUNC_BY_NUM[f]
    param, width = FUNC[name]
    text = name
    if param:
        need(b, pc, 1)
        text += f"({b[pc]})"
        pc += 1
    if not with_test:
        return text, width, pc
    test, pc = read_test(b, pc, width, name in FUNC_U8)
    return f"{text} {test}", width, pc


def read_test(b, pc, width, unsigned):
    need(b, pc, 1 + width)
    t = b[pc]
    fmt = "<B" if width == 1 and unsigned else ("<b" if width == 1 else "<h")
    value = struct.unpack_from(fmt, b, pc + 1)[0]
    sym = TEST_SYM.get(TEST_BY_NUM.get(t, ""), f"?test{t}")
    return f"{sym} {value}", pc + 1 + width


def decode(b, kind):
    """Decode one script linearly. Returns [(offset, name, [(field, value, type)])]."""
    by_num, spec = (LIFE_BY_NUM, LIFE) if kind == "life" else (TRACK_BY_NUM, TRACK)
    pc, out, switch_width = 0, [], None
    while pc < len(b):
        start = pc
        op = b[pc]
        pc += 1
        if op not in by_num:
            raise DecodeError(f"unknown {kind} opcode {op} at {start}")
        name = by_num[op]
        args = []
        for field, t in spec[name]:
            if t in SIZE:
                need(b, pc, SIZE[t])
                args.append((field, struct.unpack_from(FMT[t], b, pc)[0], t))
                pc += SIZE[t]
            elif t == "Z":
                end = b.find(0, pc)
                if end < 0:
                    raise DecodeError(f"unterminated string at {pc}")
                args.append((field, b[pc:end].decode("latin-1"), t))
                pc = end + 1
            elif t == "C":
                text, _, pc = read_cond(b, pc, True)
                args.append((field, text, t))
            elif t == "F":
                text, switch_width, pc = read_cond(b, pc, False)
                args.append((field, text, t))
            elif t == "T":
                # ExeSwitch is one slot per object, not a stack: after a nested switch
                # ends, the outer CASEs compare at the inner switch's width.
                if switch_width is None:
                    raise DecodeError(f"{name} before any SWITCH at {start}")
                text, pc = read_test(b, pc, switch_width, False)
                args.append((field, text, t))
            elif t == "M":
                need(b, pc, 1)
                move = b[pc]
                pc += 1
                args.append((field, move, t))
                if move in MOVE_WITH_ARG:
                    need(b, pc, 1)
                    args.append(("info3", b[pc], "B"))
                    pc += 1
        out.append((start, name, args))
    return out


def check_jumps(ins, size):
    # Every retail script ends with END and has no other: a field read one byte
    # short leaves its zero high byte to decode as END, so this catches it.
    for s, name, _ in ins[:-1]:
        if name == "END":
            raise DecodeError(f"END before the end of the script at {s}")
    if not ins or ins[-1][1] != "END":
        raise DecodeError("script does not end with END")
    starts = {s for s, _, _ in ins} | {size}
    for s, name, args in ins:
        for field, value, t in args:
            if t in "JG" and value not in starts:
                raise DecodeError(f"{name} at {s} jumps to {value}, not an instruction start")


def scene_scripts(sc):
    """Yield (object, track bytes, life bytes) in LoadScene order (DISKFUNC.CPP)."""
    p = 6 + 1 + 4 + 5 * 2 * 4 + 4 + 1  # info world, cube count, light, ambiance, jingle
    p += 6  # hero start
    t, = struct.unpack_from("<H", sc, p)
    track = sc[p + 2:p + 2 + t]
    p += 2 + t
    n, = struct.unpack_from("<H", sc, p)
    yield 0, track, sc[p + 2:p + 2 + n]
    p += 2 + n
    nobj, = struct.unpack_from("<H", sc, p)
    p += 2
    anim_3ds = 1 << 18  # ANIM_3DS, COMMON.H
    for obj in range(1, nobj):
        flags, = struct.unpack_from("<I", sc, p)
        # Flags, file3d, body, anim, sprite, x y z, hitforce, optionflags, beta, srot,
        # move, info..info3, nbbonus, coulobj
        p += 4 + 2 + 1 + 2 + 2 + 6 + 1 + 2 + 2 + 2 + 1 + 8 + 2 + 1
        if flags & anim_3ds:
            p += 4 + 2
        p += 1 + 1  # armure, life points
        t, = struct.unpack_from("<H", sc, p)
        track = sc[p + 2:p + 2 + t]
        p += 2 + t
        n, = struct.unpack_from("<H", sc, p)
        yield obj, track, sc[p + 2:p + 2 + n]
        p += 2 + n


def decode_cube(sc):
    """Decode every script of one cube. Returns ({obj: (track_ins, life_ins)}, errors)."""
    objs, errors = {}, []
    for obj, track, life in scene_scripts(sc):
        pair = []
        for kind, b in (("track", track), ("life", life)):
            try:
                ins = decode(b, kind)
                check_jumps(ins, len(b))
                pair.append(ins)
            except DecodeError as e:
                pair.append(None)
                errors.append((obj, kind, len(b), str(e)))
        objs[obj] = tuple(pair)
    errors += [(o, "life", 0, e) for o, e in cross_check(objs)]
    return objs, errors


def cross_check(objs):
    """Offsets one script hands to another must land on an instruction start there."""
    starts = {o: tuple({s for s, _, _ in ins} if ins else None for ins in pair)
              for o, pair in objs.items()}
    errors = []
    for obj, (_, life) in objs.items():
        for s, name, args in life or []:
            if name == "SET_TRACK":
                target, kind, value = obj, 0, args[0][1]
            elif name in ("SET_TRACK_OBJ", "SET_COMPORTEMENT_OBJ"):
                target, value = args[0][1], args[1][1]
                kind = 0 if name == "SET_TRACK_OBJ" else 1
            else:
                continue
            if value == -1:
                continue
            if target not in starts:
                errors.append((obj, f"{name} at {s} names object {target}, which does not exist"))
            elif starts[target][kind] is not None and value not in starts[target][kind]:
                errors.append((obj, f"{name} at {s} points at {value}, not an instruction "
                                    f"start in object {target}"))
    return errors


def track_label(objs, obj, offset):
    track = objs.get(obj, (None, None))[0] or []
    for s, name, args in track:
        if s == offset and name == "LABEL":
            return f"  ; label {args[0][1]}"
    return ""


def cubes(path):
    ents, data = hqr_inspect.entries(path)
    for i, off, size, csize, method in ents:
        if i == 0 or size is None:
            continue
        yield i - 1, hqr_inspect.decompress_entry(data, off, size, csize, method)


def fmt_arg(field, value, t, labels):
    if t in "JG" and value in labels:
        return f"{field} {labels[value]}"
    if t == "M":
        return f"{field}={MOVE_BY_NUM.get(value, value)}"
    if t in "CFT":
        return str(value)
    if t == "Z":
        return f'{field}="{value}"'
    return f"{field}={value}"


def listing(ins, b, out, note=None):
    targets = sorted({v for _, _, a in ins for _, v, t in a if t in "JG"})
    labels = {v: f"L_{v:04d}" for v in targets}
    for s, name, args in ins:
        if s in labels:
            out.append(f"{labels[s]}:")
        text = " ".join(fmt_arg(f, v, t, labels) for f, v, t in args)
        extra = note(name, args) if note else ""
        out.append(f"  {s:5d}  {name} {text}".rstrip() + extra)
    if len(b) in labels:
        out.append(f"{labels[len(b)]}:  ; end")


def cmd_cube(path, num):
    for cube, sc in cubes(path):
        if cube != num:
            continue
        objs, errors = decode_cube(sc)
        out = [f"; cube {cube} (SCENE.HQR entry {cube + 1})"]
        for obj, track, life in scene_scripts(sc):

            def note(name, args, obj=obj):
                if name == "SET_TRACK":
                    return track_label(objs, obj, args[0][1])
                if name == "SET_TRACK_OBJ":
                    return track_label(objs, args[0][1], args[1][1])
                return ""

            out.append("")
            out.append(f"== object {obj}  track {len(track)} bytes  life {len(life)} bytes")
            for kind, b, ins in (("track", track, objs[obj][0]), ("life", life, objs[obj][1])):
                out.append(f"-- {kind}")
                if ins is None:
                    out.append("  ; did not decode, see --check")
                else:
                    listing(ins, b, out, note if kind == "life" else None)
        for obj, kind, _, err in errors:
            out.append(f"; object {obj} {kind}: {err}")
        print("\n".join(out))
        return 1 if errors else 0
    sys.exit(f"no cube {num} in {path}")


def walk_all(path):
    """Yield (cube, obj, kind, instructions) for every script that decodes."""
    for cube, sc in cubes(path):
        objs, _ = decode_cube(sc)
        for obj, (track, life) in objs.items():
            for kind, ins in (("track", track), ("life", life)):
                if ins is not None:
                    yield cube, obj, kind, ins


def cmd_check(path):
    counts = {"track": [0, 0], "life": [0, 0]}
    nscripts, bad_cubes, ncubes = 0, 0, 0
    for cube, sc in cubes(path):
        ncubes += 1
        objs, errors = decode_cube(sc)
        nscripts += len(objs)
        for obj, kind, size, err in errors:
            print(f"FAIL cube {cube} object {obj} {kind}: {err}")
        bad_cubes += bool(errors)
        for track, life in objs.values():
            for kind, ins in (("track", track), ("life", life)):
                if ins is not None:
                    counts[kind][0] += 1
                    counts[kind][1] += len(ins)
    for kind, (ok, n) in counts.items():
        print(f"{kind}: {ok}/{nscripts} scripts decoded, {n} instructions")
    print(f"cubes: {ncubes - bad_cubes}/{ncubes} clean")
    return 1 if bad_cubes else 0


def cmd_find(path, names):
    want = set()
    for n in names:
        n = n.upper()
        if not n.startswith(("LM_", "TM_")):
            sys.exit(f"--find takes LM_ or TM_ names, got {n}")
        if n[3:] not in (LIFE if n.startswith("LM_") else TRACK):
            sys.exit(f"{n} is not a known opcode")
        want.add(("life" if n.startswith("LM_") else "track", n[3:]))
    for cube, obj, kind, ins in walk_all(path):
        for s, name, args in ins:
            if (kind, name) in want:
                text = " ".join(fmt_arg(f, v, t, {}) for f, v, t in args)
                print(f"cube {cube} object {obj} {kind} @{s}: {name} {text}".rstrip())
    return 0


TYPE_TEXT = {"B": "u8", "b": "s8", "h": "s16", "H": "u16", "I": "u32", "J": "s16 Life offset",
             "G": "s16 Track offset", "Z": "string", "C": "condition", "F": "condition, no test",
             "T": "test", "M": "move"}


def cmd_table():
    rows = []
    for title, prefix, spec in (("Life", "LM_", LIFE), ("Track", "TM_", TRACK)):
        rows += [f"### {title} opcodes", "", "| Code | Opcode | Operands |", "|---|---|---|"]
        for name, fields in sorted(spec.items(), key=lambda kv: DEFS[prefix + kv[0]]):
            ops = ", ".join(f"`{f}` {TYPE_TEXT[t]}" for f, t in fields) or "none"
            rows.append(f"| {DEFS[prefix + name]} | `{prefix}{name}` | {ops} |")
        rows.append("")
    rows += ["### Conditions", "", "| Code | Condition | Parameter | Compared as |", "|---|---|---|---|"]
    for name, (param, width) in sorted(FUNC.items(), key=lambda kv: DEFS["LF_" + kv[0]]):
        kind = "s16" if width == 2 else ("u8" if name in FUNC_U8 else "s8")
        rows.append(f"| {DEFS['LF_' + name]} | `LF_{name}` | {f'`{param}` u8' if param else 'none'} | {kind} |")
    print("\n".join(rows))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("hqr", nargs="?", help="path to SCENE.HQR")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--table", action="store_true", help="print the operand tables as Markdown")
    g.add_argument("--cube", type=int, help="list one cube's scripts")
    g.add_argument("--check", action="store_true", help="decode and verify every script")
    g.add_argument("--find", nargs="+", metavar="OP", help="list uses of LM_/TM_ opcodes")
    a = ap.parse_args()
    if a.table:
        return cmd_table()
    if not a.hqr:
        ap.error("SCENE.HQR is required")
    if a.check:
        return cmd_check(a.hqr)
    if a.find:
        return cmd_find(a.hqr, a.find)
    return cmd_cube(a.hqr, a.cube)


if __name__ == "__main__":
    sys.exit(main())
