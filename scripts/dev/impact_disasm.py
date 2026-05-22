#!/usr/bin/env python3
"""Disassemble Adeline IMPACT scripts (the compiled RESS_IMPACT=47 blob in ress.hqr).

IMPACT is the effects-scripting system (SOURCES/IMPACT.CPP): each impact is a compiled
command stream run by DoImpact(num,x,y,z,owner) to spawn particles/thrown objects/sounds at
a hit point. Blob layout: a U32 offset table (count = first/4), each slot a byte offset to
impact N's stream. Each stream is commands until IMP_END(0). Operand byte layouts are taken
from DoImpact's runtime reads (the shipped/COMPILATOR form: S16 body indices).

Usage:
  impact_disasm.py <impact.bin>          disassemble all impacts
  impact_disasm.py <impact.bin> --check  round-trip: reassemble and verify byte-identical
(decompress RESS_IMPACT=47 from ress.hqr first via hqr_inspect.py --entry 47 --dump)
"""
import struct, sys

# command -> (name, [(field, type)]) ; types: h=s16, i=s32, B=u8
CMD = {
    0:  ("END", []),
    2:  ("SAMPLE", [("sample","h"),("freq","h"),("decalage","h"),("vol","B")]),
    3:  ("FLOW", [("flow","B")]),
    4:  ("THROW", [("sprite","h"),("alpha","h"),("beta","h"),("speed","h"),("poids","h"),("force","B")]),
    5:  ("THROW_POF", [("pof","h"),("alpha","h"),("beta","h"),("speed","h"),("poids","h"),("scale","i")]),
    6:  ("THROW_OBJ", [("body","h"),("alpha","h"),("beta","h"),("speed","h"),("poids","h"),("rot","h"),("force","B")]),
    7:  ("SPRITE", [("deb","h"),("fin","h"),("tempo","i"),("scale","i"),("transp","B"),("force","B")]),
    8:  ("FLOW_SPRITE", [("flow","B"),("deb","h"),("fin","h"),("tempo","i"),("scale","i"),("transp","B"),("force","B")]),
    9:  ("FLOW_OBJ", [("flow","B"),("body","h"),("force","B")]),
    10: ("FLOW_POF", [("flow","B"),("pof","h"),("scale_deb","i"),("scale_fin","i"),("tempo","i"),("speedrot","h")]),
}
SZ = {"h":2,"i":4,"B":1}
UNP = {"h":"<h","i":"<i","B":"B"}

def disasm(d, start, end):
    p = start; out = []
    while p < end:
        c = d[p]; p += 1
        if c not in CMD:
            out.append(f"    ??? 0x{c:02x} (stop)"); break
        name, fields = CMD[c]
        if c == 0:
            out.append("    END"); break
        vals = []
        for fn, ty in fields:
            v = struct.unpack_from(UNP[ty], d, p)[0]; p += SZ[ty]
            vals.append(f"{fn}={v}")
        out.append(f"    {name} " + " ".join(vals))
    return out, p

def parse(d, start, end):
    """Parse a stream into [(code, [values])] -- the structured form a compiler emits from."""
    p = start; cmds = []
    while p < end:
        c = d[p]; p += 1
        if c == 0:
            cmds.append((0, [])); break
        vals = []
        for fn, ty in CMD[c][1]:
            vals.append(struct.unpack_from(UNP[ty], d, p)[0]); p += SZ[ty]
        cmds.append((c, vals))
    return cmds

def assemble(cmds):
    """Compile structured commands back to bytecode (inverse of parse)."""
    out = bytearray()
    for c, vals in cmds:
        out.append(c)
        for (fn, ty), v in zip(CMD[c][1], vals):
            out += struct.pack(UNP[ty], v)
    return bytes(out)

def check_roundtrip(d):
    """Reassemble the whole blob and verify byte-identical -- proves the codec is complete."""
    n = struct.unpack_from("<I", d, 0)[0] // 4
    offs = [struct.unpack_from("<I", d, i*4)[0] for i in range(n)]
    streams = []
    for i in range(n):
        s = offs[i]; e = offs[i+1] if i+1 < n else len(d)
        streams.append(b"" if (s >= len(d) or s == e) else assemble(parse(d, s, e)))
    cur = n * 4; offs2 = []
    for st in streams:
        offs2.append(cur); cur += len(st)
    rebuilt = b"".join(struct.pack("<I", o) for o in offs2) + b"".join(streams)
    ok = rebuilt == d
    print(f"round-trip: {'BYTE-IDENTICAL' if ok else 'DIFFERS'} ({len(d)} bytes)")
    return ok

def main():
    d = open(sys.argv[1], "rb").read()
    if "--check" in sys.argv[2:]:
        sys.exit(0 if check_roundtrip(d) else 1)
    n = struct.unpack_from("<I", d, 0)[0] // 4
    offs = [struct.unpack_from("<I", d, i*4)[0] for i in range(n)]
    print(f"{len(d)} bytes, {n} impact slots")
    cmd_hist = {}
    for i in range(n):
        start = offs[i]
        end = offs[i+1] if i+1 < n else len(d)
        if start >= len(d) or start == end:
            print(f"impact {i}: (empty)"); continue
        lines, _ = disasm(d, start, end)
        for l in lines:
            nm = l.strip().split(" ")[0]
            cmd_hist[nm] = cmd_hist.get(nm, 0) + 1
        print(f"impact {i}: ({end-start} bytes)")
        for l in lines:
            print(l)
    print("\n=== command frequency across all impacts ===")
    for k, v in sorted(cmd_hist.items(), key=lambda x:-x[1]):
        print(f"  {k}: {v}")

if __name__ == "__main__":
    main()
