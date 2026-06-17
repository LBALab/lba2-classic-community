#!/usr/bin/env python3
"""Feasibility spike 2: run LBA1 Life-script bytecode on the LBA2 VM (docs/LBA1_PORT_PLAN.md §6).

Source of truth for LBA1 opcode numbers is ../lba1-classic/SOURCES/COMMON.H; the LBA2 numbers
are this repo's SOURCES/COMMON.H. twin-e is only a name legend, not consulted here.

Two parts:
  1. Parse both COMMON.H opcode tables (LM_/LF_/LT_/TM_) and emit the LBA1->LBA2 remap with a
     divergence report (identical slot / semantic rename / true collision / title-only).
  2. The flag-width linchpin. From the engines' own GERELIFE:
       LBA1: LF_FLAG_GAME -> Value = ListFlagGame[i] (UBYTE), TypeAnswer = RET_BYTE  (1-byte cmp)
             LM_SET_FLAG_GAME writes 1 byte.
       LBA2: LF_VAR_GAME   -> Value = ListVarGame[i]  (S16),   TypeAnswer = RET_S16   (2-byte cmp)
             LM_SET_VAR_GAME writes 2 bytes.
     IF jumps are absolute 2-byte offsets (PtrPrg = PtrLife + *(WORD*)PtrPrg), so widening the
     value operands shifts every jump. Transcode a `set flag -> if flag==v -> branch` LBA1
     script to LBA2 (remap opcodes + widen operands + recompute jumps) and run both on a mini
     Life-VM; assert identical branching. This is the quest mechanism in miniature.

Usage: lba1_script_remap.py [--lba1-common PATH] [--lba2-common PATH]
"""
import argparse
import os
import re
import struct
import sys

PREFIXES = ("LM_", "LF_", "LT_", "TM_")


def parse_defines(path):
    out = {p: {} for p in PREFIXES}
    rx = re.compile(r"#define\s+((?:LM_|LF_|LT_|TM_)\w+)\s+(\d+)\b")
    with open(path, encoding="latin-1") as f:
        for line in f:
            m = rx.search(line)
            if m:
                name, num = m.group(1), int(m.group(2))
                out[name[:3]][name] = num
    return out


def by_num(table):
    return {v: k for k, v in table.items()}


def norm(name):
    # FLAG<->VAR is the systematic LBA1->LBA2 rename (flags became S16 vars).
    return name.replace("_FLAG_", "_VAR_")


def opcode_diff(l1, l2, group, lo=0, hi=56):
    """Slot-by-slot compare for one opcode group; classify each LBA1 slot."""
    n1, n2 = by_num(l1[group]), by_num(l2[group])
    rows, counts = [], {"identical": 0, "rename": 0, "collision": 0, "lba1_only": 0}
    for num in sorted(n1):
        a = n1[num]
        b = n2.get(num)
        if b is None:
            kind = "lba1_only"
        elif a == b:
            kind = "identical"
        elif norm(a) == b:
            kind = "rename"
        else:
            kind = "collision"
        counts[kind] += 1
        rows.append((num, a, b, kind))
    in_window = [r for r in rows if lo <= r[0] <= hi]
    win_clean = sum(1 for r in in_window if r[3] in ("identical", "rename"))
    return rows, counts, (len(in_window), win_clean)


def build_remap(l1_lm, l2_lm):
    """LBA1 LM_ number -> LBA2 LM_ number, matched by name then by FLAG->VAR rename."""
    remap, unmapped = {}, []
    for name, num in l1_lm.items():
        if name in l2_lm:
            remap[num] = l2_lm[name]
        elif norm(name) in l2_lm:
            remap[num] = l2_lm[norm(name)]
        else:
            unmapped.append((num, name))
    return remap, unmapped


# ---------------------------------------------------------- mini Life-VM + transcoder
# Opcode "roles" used by the demo, resolved to each game's real numbers from COMMON.H.
# Bytecode layouts (vw = value width: LBA1=1, LBA2=2):
#   END    : [op]
#   SET    : [op][idx:1][val:vw]
#   IF     : [op][cond:1][idx:1][cmp:1][cmpval:vw][jump:2 absolute]
#   OFFSET : [op][jump:2 absolute]

def roles_for(table_lm, table_lf, table_lt, set_name):
    return {
        "END": table_lm["LM_END"], "SET": table_lm[set_name], "IF": table_lm["LM_IF"],
        "OFFSET": table_lm["LM_OFFSET"],
        "_cond": table_lf["LF_VAR_GAME"] if "LF_VAR_GAME" in table_lf else table_lf["LF_FLAG_GAME"],
        "_eq": table_lt["LT_EQUAL"],
    }


def assemble(prog, roles, vw):
    """prog: list of tuples. Labels resolved to absolute offsets.
       ("SET", idx, val) ("IF", idx, val, else_label) ("OFFSET", target_label)
       ("LABEL", name) ("END",)"""
    # pass 1: offsets
    off, labels = 0, {}
    sizes = []
    for ins in prog:
        if ins[0] == "LABEL":
            labels[ins[1]] = off; sizes.append(0); continue
        sizes.append({"SET": 2 + vw, "IF": 4 + vw + 2, "OFFSET": 3, "END": 1}[ins[0]])
        off += sizes[-1]
    end_off = off
    # pass 2: emit
    code = bytearray()
    for ins in prog:
        k = ins[0]
        if k == "LABEL":
            continue
        if k == "SET":
            code += bytes([roles["SET"], ins[1]]) + int(ins[2]).to_bytes(vw, "little", signed=True)
        elif k == "IF":
            tgt = labels[ins[3]]
            code += bytes([roles["IF"], roles["_cond"], ins[1], roles["_eq"]])
            code += int(ins[2]).to_bytes(vw, "little", signed=True)
            code += struct.pack("<H", tgt)
        elif k == "OFFSET":
            code += bytes([roles["OFFSET"]]) + struct.pack("<H", labels[ins[1]])
        elif k == "END":
            code += bytes([roles["END"]])
    return bytes(code), end_off


def transcode(src, src_roles, dst_roles, src_vw, dst_vw):
    """Decode LBA1 bytecode, re-emit as LBA2: remap opcodes, widen value operands, and
    recompute the absolute jump offsets (two-pass with an old->new offset map)."""
    n2r = {v: k for k, v in src_roles.items() if not k.startswith("_")}
    # pass 1: decode to instruction list with source offsets
    insns, pc = [], 0
    while pc < len(src):
        src_off = pc
        role = n2r[src[pc]]; pc += 1
        if role == "END":
            insns.append((src_off, "END", None)); break
        if role == "SET":
            idx = src[pc]; val = int.from_bytes(src[pc + 1:pc + 1 + src_vw], "little", signed=True)
            pc += 1 + src_vw
            insns.append((src_off, "SET", (idx, val)))
        elif role == "IF":
            cond, idx, cmp = src[pc], src[pc + 1], src[pc + 2]
            cmpval = int.from_bytes(src[pc + 3:pc + 3 + src_vw], "little", signed=True)
            jump = struct.unpack_from("<H", src, pc + 3 + src_vw)[0]
            pc += 3 + src_vw + 2
            insns.append((src_off, "IF", (cond, idx, cmp, cmpval, jump)))
        elif role == "OFFSET":
            jump = struct.unpack_from("<H", src, pc)[0]; pc += 2
            insns.append((src_off, "OFFSET", (jump,)))
    # pass 2: emit + record old->new offset map and jump-field positions to patch
    dst = bytearray(); offmap = {}; patches = []
    for src_off, role, args in insns:
        offmap[src_off] = len(dst)
        if role == "END":
            dst += bytes([dst_roles["END"]])
        elif role == "SET":
            idx, val = args
            dst += bytes([dst_roles["SET"], idx]) + int(val).to_bytes(dst_vw, "little", signed=True)
        elif role == "IF":
            cond, idx, cmp, cmpval, jump = args
            # remap condition + comparator opcodes too (they are LF_/LT_ numbers)
            dst += bytes([dst_roles["IF"], dst_roles["_cond"], idx, dst_roles["_eq"]])
            dst += int(cmpval).to_bytes(dst_vw, "little", signed=True)
            patches.append((len(dst), jump)); dst += b"\x00\x00"
        elif role == "OFFSET":
            jump = args[0]
            dst += bytes([dst_roles["OFFSET"]])
            patches.append((len(dst), jump)); dst += b"\x00\x00"
    offmap[len(src)] = len(dst)               # jumps to end-of-script
    # pass 3: patch jumps through the offset map
    for pos, old_target in patches:
        struct.pack_into("<H", dst, pos, offmap[old_target])
    return bytes(dst)


def run_vm(code, roles, vw):
    n2r = {v: k for k, v in roles.items() if not k.startswith("_")}
    store, pc, trace = {}, 0, []
    while True:
        role = n2r[code[pc]]; pc += 1
        if role == "END":
            break
        if role == "SET":
            idx = code[pc]; val = int.from_bytes(code[pc + 1:pc + 1 + vw], "little", signed=True)
            pc += 1 + vw; store[idx] = val; trace.append(f"set v{idx}={val}")
        elif role == "IF":
            _cond, idx, _cmp = code[pc], code[pc + 1], code[pc + 2]
            cmpval = int.from_bytes(code[pc + 3:pc + 3 + vw], "little", signed=True)
            jump = struct.unpack_from("<H", code, pc + 3 + vw)[0]
            pc += 3 + vw + 2
            taken = store.get(idx, 0) == cmpval
            trace.append(f"if v{idx}({store.get(idx,0)})=={cmpval}? {taken}")
            if not taken:
                pc = jump
        elif role == "OFFSET":
            pc = struct.unpack_from("<H", code, pc)[0]
    return store, trace


def line(t, ok, extra=""):
    return f"  [{'PASS' if ok else 'FAIL'}] {t}" + (f"  ({extra})" if extra else "")


def main():
    hacking = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lba1-common", default=os.path.join(hacking, "lba1-classic", "SOURCES", "COMMON.H"))
    ap.add_argument("--lba2-common", default=os.path.join(hacking, "lba2-classic-community", "SOURCES", "COMMON.H"))
    a = ap.parse_args()
    all_ok = True

    l1 = parse_defines(a.lba1_common)
    l2 = parse_defines(a.lba2_common)
    print("=" * 74)
    print(f"LBA1 opcodes: LM_={len(l1['LM_'])} LF_={len(l1['LF_'])} LT_={len(l1['LT_'])} TM_={len(l1['TM_'])}"
          f"   LBA2: LM_={len(l2['LM_'])} LF_={len(l2['LF_'])} TM_={len(l2['TM_'])}")

    # --- 1. opcode remap + divergence report (Life commands) ---
    rows, counts, (winn, winclean) = opcode_diff(l1, l2, "LM_", 0, 56)
    win_collisions = [(n, a, b) for n, a, b, k in rows if 0 <= n <= 56 and k == "collision"]
    print("-" * 74)
    print("Life (LM_) opcode alignment vs LBA2 (over all 101 LBA1 slots):")
    print(f"  identical={counts['identical']} rename={counts['rename']} "
          f"collision={counts['collision']} lba1_only={counts['lba1_only']}")
    print("  FINDING: slots 0..56 are NOT all identical. LBA1_PORTING_SURFACE.md overstates this: "
          f"{winclean}/{winn} clean in 0..56;")
    print("    collisions inside 0..56: "
          + ", ".join(f"{n}({a}->{b})" for n, a, b in win_collisions))
    print("    renames (FLAG->VAR, same slot): "
          + ", ".join(f"{n}:{a}->{b}" for n, a, b, k in rows if k == "rename"))
    n1lm, n2lm = by_num(l1["LM_"]), by_num(l2["LM_"])
    past = [(num, n1lm[num], n2lm.get(num)) for num in sorted(n1lm)
            if num > 56 and n2lm.get(num) and n1lm[num] != n2lm[num] and norm(n1lm[num]) != n2lm[num]]
    print(f"    first collisions >56 (the media wall): {past[:5]}")
    remap, unmapped = build_remap(l1["LM_"], l2["LM_"])
    print(line(f"by-name remap covers most LBA1 ops ({len(remap)}/{len(l1['LM_'])})",
               len(remap) >= 60, f"{len(remap)}/{len(l1['LM_'])}")); all_ok &= (len(remap) >= 60)
    print(f"    unmapped (need manual alias e.g. LABEL->COMPORTEMENT, or a handler e.g. media): "
          f"{[n for _, n in unmapped]}")

    # --- 2. flag-width transcode + dual-VM proof ---
    print("-" * 74)
    print("Flag-width linchpin: set flag -> if flag==v -> branch, LBA1 byte vs LBA2 S16")
    l1_roles = roles_for(l1["LM_"], l1["LF_"], l1["LT_"], "LM_SET_FLAG_GAME")
    l2_roles = roles_for(l2["LM_"], l2["LF_"], l2["LT_"], "LM_SET_VAR_GAME")
    print(f"  LBA1 nums: SET={l1_roles['SET']} IF={l1_roles['IF']} cond(FLAG_GAME)={l1_roles['_cond']} "
          f"eq={l1_roles['_eq']} OFFSET={l1_roles['OFFSET']}")
    print(f"  LBA2 nums: SET={l2_roles['SET']} IF={l2_roles['IF']} cond(VAR_GAME)={l2_roles['_cond']} "
          f"eq={l2_roles['_eq']} OFFSET={l2_roles['OFFSET']}")

    for trigger, expect in ((1, 42), (0, 99)):
        prog = [("SET", 5, trigger),
                ("IF", 5, 1, "else"),     # if flag5 == 1
                ("SET", 6, 42),
                ("OFFSET", "end"),
                ("LABEL", "else"),
                ("SET", 6, 99),
                ("LABEL", "end"),
                ("END",)]
        lba1_code, _ = assemble(prog, l1_roles, 1)
        lba2_code = transcode(lba1_code, l1_roles, l2_roles, 1, 2)
        s1, t1 = run_vm(lba1_code, l1_roles, 1)
        s2, t2 = run_vm(lba2_code, l2_roles, 2)
        ok = s1.get(6) == s2.get(6) == expect
        print(line(f"flag5={trigger}: LBA1({len(lba1_code)}B) v6={s1.get(6)}  "
                   f"LBA2({len(lba2_code)}B) v6={s2.get(6)}  expect {expect}", ok))
        all_ok &= ok
    # show the widening + jump recompute is real, not a no-op
    p = [("SET", 5, 1), ("IF", 5, 1, "e"), ("SET", 6, 42), ("OFFSET", "end"),
         ("LABEL", "e"), ("SET", 6, 99), ("LABEL", "end"), ("END",)]
    c1, _ = assemble(p, l1_roles, 1)
    c2 = transcode(c1, l1_roles, l2_roles, 1, 2)
    if_jump1 = struct.unpack_from("<H", c1, c1.index(bytes([l1_roles["IF"]])) + 5)[0]
    print(line("transcode widened bytecode and recomputed jumps (not identity)",
               len(c2) > len(c1), f"LBA1={len(c1)}B LBA2={len(c2)}B, IF jump {if_jump1} -> recomputed"))

    print("=" * 74)
    print(f"SPIKE 2 {'PASS' if all_ok else 'FAIL'}: the quest's set/test/branch transcodes across "
          "the byte->S16 flag-width seam and branches identically on a mini-VM, and a by-name "
          "opcode remap covers most LBA1 ops. FINDING: the LM_ tables diverge MORE than "
          "LBA1_PORTING_SURFACE.md claims (collisions inside 0..56, not just >56), so a real "
          "by-name remap + per-op aliases/handlers is required, not a 0..56 identity assumption. "
          "Remaining: full operand-table transcoder + handlers for the LBA1-only/media ops.")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
