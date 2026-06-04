#!/usr/bin/env python3
"""Decode Adeline ACF/XCF cinematic frames (Time Commando, 1996).

Port of the original Adeline tile codec — see SOURCES/DEC_XCF.{ASM,CPP} in this
repo and the TimeCo reimplementation (../timeco/src/acf.c). The frame is a grid
of 8x8 tiles; each tile carries a 6-bit opcode (0..63) selecting a decode method
that pulls from two byte streams:

  payload[0:4]              = colour_offset (u32)  -> start of 'unaligned' stream
  payload[4 : 4+(H/8)*30]   = opcodes (6 bits each, 4 per 3 bytes)
  payload[4+(H/8)*30 : co]  = 'aligned' stream
  payload[colour_offset:]   = 'unaligned' stream

Opcodes mix intra coding (raw, colour fills, 1..4-bit packed palette tiles,
split tiles, bank tiles, run blocks) with inter coding (zero/short/absolute/
relative motion, 8x8 and 4x4) plus optional residual 'update' passes.

Usage:
  acf_decode.py <file.acf> [--frame N] [--out OUT.png] [--all DIR]
"""
import struct, sys, argparse, importlib.util
from PIL import Image

_spec = importlib.util.spec_from_file_location("acf", __file__.replace("acf_decode.py","acf_inspect.py"))
_acf = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(_acf)

DIAG1 = [0,1,320,640,321,2,3,322,641,960,1280,961,642,323,4,5,324,643,962,1281,1600,1920,1601,1282,963,644,325,6,7,
         326,645,964,1283,1602,1921,2240,2241,1922,1603,1284,965,646,327,647,966,1285,1604,1923,2242,2243,1924,1605,1286,
         967,1287,1606,1925,2244,2245,1926,1607,1927,2246,2247]
DIAG2 = [7,6,327,647,326,5,4,325,646,967,1287,966,645,324,3,2,323,644,965,1286,1607,1927,1606,1285,964,643,322,1,0,
         321,642,963,1284,1605,1926,2247,2246,1925,1604,1283,962,641,320,640,961,1282,1603,1924,2245,2244,1923,1602,1281,
         960,1280,1601,1922,2243,2242,1921,1600,1920,2241,2240]
SPLIT = [0, 4, 320*4, 320*4+4]

def sext4(v):  # sign-extend low 4 bits
    v &= 15
    return v-16 if v >= 8 else v

class Frame:
    def __init__(self, W, H):
        self.W, self.H = W, H
        self.cur = bytearray(W*H)
        self.prev = bytearray(W*H)
    def swap(self):
        self.cur, self.prev = self.prev, self.cur

    def decode(self, payload):
        W, H = self.W, self.H
        cur, prev = self.cur, bytes(self.prev)
        pad = payload + b"\x00"*64
        colour_offset = struct.unpack_from("<I", pad, 0)[0]
        ai = 4 + (H//8)*30          # aligned stream index
        ui = colour_offset          # unaligned stream index
        opi = 4                     # opcode index
        self.ct = 0                 # current_tile base (into cur)
        self.pt = 0                 # previous_tile base (into prev)

        def u8a():
            nonlocal ai; v = pad[ai]; ai += 1; return v
        def u8u():
            nonlocal ui; v = pad[ui]; ui += 1; return v
        def sp(x, y, c):
            idx = self.ct + x + y*W
            if 0 <= idx < len(cur): cur[idx] = c
        def rdprev(i):
            return prev[i] if 0 <= i < len(prev) else 0
        def bcopy(dst, src, n):
            for y in range(n):
                d = dst + y*W; s = src + y*W
                for k in range(n):
                    di = d+k
                    if 0 <= di < len(cur): cur[di] = rdprev(s+k)

        # ---- residual update passes ----
        def update4():
            nonlocal ui, ai
            value = struct.unpack_from("<I", pad, ui)[0]; ui += 3
            for _ in range(4):
                sp(value & 7, (value >> 3) & 7, pad[ai]); ai += 1
                value >>= 6
        def update8(): update4(); update4()
        def update16():
            nonlocal ui, ai
            for y in range(8):
                mask = pad[ai]; ai += 1
                for x in range(8):
                    if mask & 1: sp(x, y, pad[ui]); ui += 1
                    mask >>= 1

        # ---- motion ----
        def zero_motion(): bcopy(self.ct, self.pt, 8)
        def short_motion8():
            nonlocal ui
            v = pad[ui]; ui += 1
            bcopy(self.ct, self.pt + 4 + W*4 + sext4(v) + sext4(v>>4)*W, 8)
        def short_motion4():
            nonlocal ai
            base = self.pt + 2 + W*2
            for q in (0, 4, W*4, W*4+4):
                v = pad[ai]; ai += 1
                bcopy(self.ct+q, base + sext4(v) + sext4(v>>4)*W + q, 4)
        def motion8():
            nonlocal ui
            off = struct.unpack_from("<H", pad, ui)[0]; ui += 2
            bcopy(self.ct, off, 8)
        def motion4():
            nonlocal ai
            for q in (0, 4, W*4, W*4+4):
                off = struct.unpack_from("<H", pad, ai)[0]; ai += 2
                bcopy(self.ct+q, off, 4)
        def ro_motion8():
            nonlocal ui
            off = struct.unpack_from("<h", pad, ui)[0]; ui += 2
            bcopy(self.ct, self.pt + off + 4 + W*4, 8)
        def ro_motion4():
            nonlocal ai
            for q in (0, 4, W*4, W*4+4):
                off = struct.unpack_from("<h", pad, ai)[0]; ai += 2
                bcopy(self.ct+q, self.pt + off + 2 + W*2 + q, 4)
        def rc_motion8():
            nonlocal ui
            a = struct.unpack_from("<b", pad, ui)[0]; b = struct.unpack_from("<b", pad, ui+1)[0]; ui += 2
            bcopy(self.ct, self.pt + a + b*W//2 + 4 + W*4, 8)
        def rc_motion4():
            nonlocal ai
            for q in (0, 4, W*4, W*4+4):
                a = struct.unpack_from("<b", pad, ai)[0]; b = struct.unpack_from("<b", pad, ai+1)[0]; ai += 2
                bcopy(self.ct+q, self.pt + a + b*W//2 + 2 + W*2 + q, 4)

        # ---- intra ----
        def single_fill():
            nonlocal ui
            c = pad[ui]; ui += 1
            for y in range(8):
                for x in range(8): sp(x, y, c)
        def four_fill():
            nonlocal ai
            tl, tr, bl, br = pad[ai], pad[ai+1], pad[ai+2], pad[ai+3]; ai += 4
            for y in range(4):
                for x in range(4):
                    sp(x, y, tl); sp(x+4, y, tr); sp(x, y+4, bl); sp(x+4, y+4, br)
        def bit1():
            nonlocal ai, ui
            for y in range(8):
                a = pad[ai]; ai += 1
                for x in range(8): sp(x, y, pad[ui + (a & 1)]); a >>= 1
            ui += 2
        def bit2():
            nonlocal ai
            cols = ai; ai += 4
            for y in range(8):
                a = struct.unpack_from("<H", pad, ai)[0]; ai += 2
                for x in range(8): sp(x, y, pad[cols + (a & 3)]); a >>= 2
        def bit3():
            nonlocal ai, ui
            for y in range(8):
                a = struct.unpack_from("<I", pad, ai)[0]; ai += 3
                for x in range(8): sp(x, y, pad[ui + (a & 7)]); a >>= 3
            ui += 8
        def bit4():
            nonlocal ai, ui
            for y in range(8):
                a = struct.unpack_from("<I", pad, ai)[0]; ai += 4
                for x in range(8): sp(x, y, pad[ui + (a & 15)]); a >>= 4
            ui += 16
        def split1():
            nonlocal ai
            for off in SPLIT:
                a = struct.unpack_from("<H", pad, ai)[0]; ai += 2
                for y in range(4):
                    for x in range(4):
                        i = self.ct + x + y*W + off
                        if 0 <= i < len(cur): cur[i] = pad[ai + (a & 1)]
                        a >>= 1
                ai += 2
        def split2():
            nonlocal ai
            for off in SPLIT:
                a = struct.unpack_from("<I", pad, ai)[0]; ai += 4
                for y in range(4):
                    for x in range(4):
                        i = self.ct + x + y*W + off
                        if 0 <= i < len(cur): cur[i] = pad[ai + (a & 3)]
                        a >>= 2
                ai += 4
        def split3():
            nonlocal ai, ui
            for off in SPLIT:
                a = 0
                for y in range(4):
                    if not (y & 1):
                        a = struct.unpack_from("<I", pad, ai)[0]; ai += 3
                    for x in range(4):
                        i = self.ct + x + y*W + off
                        if 0 <= i < len(cur): cur[i] = pad[ui + (a & 7)]
                        a >>= 3
                ui += 8
        def cross():
            nonlocal ai
            value = struct.unpack_from("<I", pad, ai)[0]; ai += 4
            for off in SPLIT:
                d = self.ct + off; s = ai
                def w(o, c):
                    i = d + o
                    if 0 <= i < len(cur): cur[i] = pad[s + c]
                w(0, value & 1); w(1, 0); w(2, 0); w(3, ((value & 2) >> 1)*3)
                w(320, 1); w(321, (value & 4) >> 2); w(322, ((value & 8) >> 3)*3); w(323, 3)
                w(640, 1); w(641, 1+((value & 16) >> 4)); w(642, 2+((value & 32) >> 5)); w(643, 3)
                w(960, 1+((value & 64) >> 6)); w(961, 2); w(962, 2); w(963, 2+((value & 128) >> 7))
                ai += 4; value >>= 8
        def prime():
            nonlocal ai, ui
            pc = pad[ui]; ui += 1
            for y in range(8):
                a = pad[ai]; ai += 1
                for x in range(8):
                    if a & 1: sp(x, y, pad[ui]); ui += 1
                    else: sp(x, y, pc)
                    a >>= 1
        def raw():
            nonlocal ai
            for y in range(8):
                for x in range(8): sp(x, y, pad[ai+x])
                ai += 8
        def one_bank():
            nonlocal ai, ui
            bank = pad[ui]; ui += 1
            for y in range(8):
                for x in range(8):
                    if x & 1: sp(x, y, (bank + (pad[ai] >> 4)) & 0xFF); ai += 1
                    else: sp(x, y, (bank + (pad[ai] & 15)) & 0xFF)
        def two_banks():
            nonlocal ai, ui
            b = pad[ui]; ui += 1
            bank = [(b & 0x0f) << 4, b & 0xf0]
            for y in range(8):
                part1 = struct.unpack_from("<I", pad, ai)[0]
                part2 = struct.unpack_from("<I", pad, ai+4)[0]; ai += 5
                for x in range(8):
                    sp(x, y, (bank[(part1 & 16) >> 4] + (part1 & 15)) & 0xFF)
                    part1 = (part1 >> 5) | ((part2 << 27) & 0xFFFFFFFF)
                    part2 >>= 5
        def block_run(order):  # order: 'h','v',diag list
            nonlocal ai, ui
            last = 0
            if order == 'h':
                for y in range(8):
                    a = pad[ai]; ai += 1
                    for x in range(8):
                        if a & 1: last = pad[ui]; ui += 1
                        a >>= 1; sp(x, y, last)
            elif order == 'v':
                for x in range(8):
                    a = pad[ai]; ai += 1
                    for y in range(8):
                        if a & 1: last = pad[ui]; ui += 1
                        a >>= 1; sp(x, y, last)
            else:
                offs = order; k = 0
                for y in range(8):
                    a = pad[ai]; ai += 1
                    for x in range(8):
                        if a & 1: last = pad[ui]; ui += 1
                        a >>= 1
                        i = self.ct + offs[k]; k += 1
                        if 0 <= i < len(cur): cur[i] = last
        def block_bank(order):
            nonlocal ai, ui
            last = 0; bank = (pad[ui] << 4) & 0xFF; flag = 1
            def nibble():
                nonlocal ui, flag, last
                if flag: last = pad[ui] >> 4; flag = 0; ui += 1
                else: last = pad[ui] & 15; flag += 1
            base_ui = ui  # bank byte already read at ui; advance past it lazily like C
            # C reads bank from *unaligned_stream (no advance) then nibbles advance; trailing fixup
            ui += 0
            def emit(setter):
                nonlocal flag
                if order == 'h':
                    for y in range(8):
                        a = pad[ai_box[0]]; ai_box[0] += 1
                        for x in range(8):
                            if a & 1: nibble()
                            a >>= 1; setter(x, y, (bank + last) & 0xFF)
                elif order == 'v':
                    for x in range(8):
                        a = pad[ai_box[0]]; ai_box[0] += 1
                        for y in range(8):
                            if a & 1: nibble()
                            a >>= 1; setter(x, y, (bank + last) & 0xFF)
            # handled inline below instead
        # NOTE: block_bank variants handled explicitly in dispatch to keep ai/ui correct

        ai_box = [ai]
        def sync_in():  # pull module ai into ai_box
            ai_box[0] = ai
        def bb(order):
            nonlocal ai, ui
            last = 0; bank = (pad[ui] << 4) & 0xFF; flag = [1]
            start_ui = ui  # bank read but not advanced in C
            ui += 1  # we consumed the bank byte; C advances at end via "if(flag) unaligned++" + nibble advances
            # emulate C precisely: bank = *un (no advance); nibble: if flag: last=*un>>4; flag=0; un++  else last=*un&15; flag++
            ui = start_ui  # reset; replicate C pointer exactly
            def nib():
                nonlocal ui, last
                if flag[0]: last = pad[ui] >> 4; flag[0] = 0; ui += 1
                else: last = pad[ui] & 15; flag[0] += 1
            # bank uses pad[start_ui]; nibbles read from start_ui (same byte!) — matches C aliasing
            seq = []
            if order in ('h','v'):
                outer = range(8)
                for o in outer:
                    a = pad[ai]; ai += 1
                    for ii in range(8):
                        if a & 1: nib()
                        a >>= 1
                        if order == 'h': sp(ii, o, (bank+last)&0xFF)
                        else: sp(o, ii, (bank+last)&0xFF)
            else:
                offs = order; k = 0
                for y in range(8):
                    a = pad[ai]; ai += 1
                    for x in range(8):
                        if a & 1: nib()
                        a >>= 1
                        i = self.ct + offs[k]; k += 1
                        if 0 <= i < len(cur): cur[i] = (bank+last)&0xFF
            if flag[0]: ui += 1

        # dispatch table for the simple residual-bearing ops
        DISP = {
            0: raw,
            1: zero_motion, 2: lambda: (zero_motion(), update4()), 3: lambda: (zero_motion(), update8()), 4: lambda: (zero_motion(), update16()),
            5: short_motion8, 6: lambda:(short_motion8(),update4()), 7: lambda:(short_motion8(),update8()), 8: lambda:(short_motion8(),update16()),
            9: motion8, 10: lambda:(motion8(),update4()), 11: lambda:(motion8(),update8()), 12: lambda:(motion8(),update16()),
            13: short_motion4, 14: lambda:(short_motion4(),update4()), 15: lambda:(short_motion4(),update8()), 16: lambda:(short_motion4(),update16()),
            17: motion4, 18: lambda:(motion4(),update4()), 19: lambda:(motion4(),update8()), 20: lambda:(motion4(),update16()),
            21: single_fill, 22: lambda:(single_fill(),update4()), 23: lambda:(single_fill(),update8()), 24: lambda:(single_fill(),update16()),
            25: four_fill, 26: lambda:(four_fill(),update4()), 27: lambda:(four_fill(),update8()), 28: lambda:(four_fill(),update16()),
            29: bit1, 30: bit2, 31: bit3, 32: bit4,
            33: split1, 34: split2, 35: split3,
            36: cross, 37: prime,
            38: one_bank, 39: two_banks,
            40: lambda: block_run('h'), 41: lambda: block_run('v'), 42: lambda: block_run(DIAG1), 43: lambda: block_run(DIAG2),
            44: lambda: bb('h'), 45: lambda: bb('v'), 46: lambda: bb(DIAG1), 47: lambda: bb(DIAG2),
            48: ro_motion8, 49: lambda:(ro_motion8(),update4()), 50: lambda:(ro_motion8(),update8()), 51: lambda:(ro_motion8(),update16()),
            52: rc_motion8, 53: lambda:(rc_motion8(),update4()), 54: lambda:(rc_motion8(),update8()), 55: lambda:(rc_motion8(),update16()),
            56: ro_motion4, 57: lambda:(ro_motion4(),update4()), 58: lambda:(ro_motion4(),update8()), 59: lambda:(ro_motion4(),update16()),
            60: rc_motion4, 61: lambda:(rc_motion4(),update4()), 62: lambda:(rc_motion4(),update8()), 63: lambda:(rc_motion4(),update16()),
        }

        opcode = -1
        for ty in range(H//8):
            for tx in range(W//8):
                if opcode == -1:
                    raw32 = pad[opi] | (pad[opi+1]<<8) | (pad[opi+2]<<16) | 0xff000000
                    opcode = raw32 - 0x100000000  # signed
                    opi += 3
                DISP[opcode & 63]()
                opcode >>= 6
                self.ct += 8; self.pt += 8
            self.ct += W*7; self.pt += W*7
        return ai, ui

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--frame", type=int, default=0, help="frame index to render")
    ap.add_argument("--out", default=None)
    ap.add_argument("--all", default=None, help="render all frames to DIR")
    a = ap.parse_args()
    data = open(a.path, "rb").read()
    chunks = list(_acf.walk(data))
    W = H = None; pal = None
    fr = None; idx = 0
    for off, tag, size, p, ok in chunks:
        t = tag.strip()
        if t == "Format":
            W = struct.unpack_from("<I", data, p+4)[0]
            H = struct.unpack_from("<I", data, p+8)[0]
            comp = struct.unpack_from("<I", data, p+40)[0]
            print(f"# Format {W}x{H} compressor={comp} ({'XCF' if comp else 'ACF'})")
            fr = Frame(W, H)
        elif t == "Palette":
            pal = data[p:p+size]
        elif t in ("KeyFrame", "DltFrame"):
            payload = data[p:p+size]
            fr.decode(payload)
            if a.all:
                import os; os.makedirs(a.all, exist_ok=True)
                im = Image.frombytes("P", (W, H), bytes(fr.cur)); im.putpalette(pal or bytes(768))
                im.convert("RGB").save(f"{a.all}/frame_{idx:04d}.png")
            elif idx == a.frame:
                im = Image.frombytes("P", (W, H), bytes(fr.cur)); im.putpalette(pal or bytes(768))
                out = a.out or a.path.rsplit("/",1)[-1] + f".f{idx}.png"
                im.convert("RGB").save(out)
                print(f"# rendered frame {idx} ({t}) -> {out}")
                return
            fr.swap(); idx += 1
    if a.all:
        print(f"# rendered {idx} frames to {a.all}/")

if __name__ == "__main__":
    main()
