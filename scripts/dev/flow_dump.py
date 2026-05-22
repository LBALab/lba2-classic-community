#!/usr/bin/env python3
"""Decode Adeline FLOW particle definitions (the RESS_FLOW=45 blob in ress.hqr).

FLOW defines a particle emitter: emission angles + spread, a bounding box, per-dot speed/
weight/delay/count, and colour. IMPACT scripts reference these by index (FLOW/FLOW_SPRITE/
FLOW_OBJ/FLOW_POF "flow=N"); the engine spawns the flow via CreateParticleFlow / the extra
particle system. The blob is a flat array of T_FLOW (SOURCES/FLOW.H), loaded raw by
HQ Load_HQR into TabPartFlow -- no offset table.

T_FLOW (36 bytes, no padding): Alpha Beta OuvertureAlpha OuvertureBeta XMin YMin ZMin
XMax YMax ZMax Delay Speed Weight NbDot Bank (15x s16), Coul Range (2x u8), Flags (u32).

Usage: flow_dump.py <flow.bin>   (decompress RESS_FLOW=45 via hqr_inspect.py --entry 45 --dump)
"""
import struct, sys

FIELDS = ["Alpha","Beta","OuvAlpha","OuvBeta","XMin","YMin","ZMin","XMax","YMax","ZMax",
          "Delay","Speed","Weight","NbDot","Bank"]
REC = struct.Struct("<15hBBI")  # 36 bytes

def main():
    d = open(sys.argv[1], "rb").read()
    assert len(d) % REC.size == 0, f"{len(d)} not a multiple of {REC.size}"
    n = len(d) // REC.size
    print(f"{len(d)} bytes, {n} flows ({REC.size} bytes each)")
    used = {}
    for i in range(n):
        v = REC.unpack_from(d, i * REC.size)
        s16s, coul, rng, flags = v[:15], v[15], v[16], v[17]
        f = dict(zip(FIELDS, s16s))
        # compact: emission, spread, dots, speed/weight, colour, flags
        print(f"flow {i:3}: a={f['Alpha']:>5} b={f['Beta']:>5} ouv=({f['OuvAlpha']},{f['OuvBeta']}) "
              f"dots={f['NbDot']:>3} spd={f['Speed']:>4} wgt={f['Weight']:>4} dly={f['Delay']:>4} "
              f"bank={f['Bank']} coul={coul} range={rng} flags=0x{flags:x}")
    # round-trip check
    rebuilt = b"".join(REC.pack(*REC.unpack_from(d, i*REC.size)) for i in range(n))
    print(f"\nround-trip: {'BYTE-IDENTICAL' if rebuilt == d else 'DIFFERS'}")

if __name__ == "__main__":
    main()
