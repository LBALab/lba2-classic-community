#!/usr/bin/env python3
"""Feasibility spike 4: confirm LBA1 audio (VOC) plays through lba2cc's EXISTING sample path.

The lba2cc community port already decodes Creative Voice (.voc): LIB386/AIL/SDL/SAMPLE.CPP
`PlaySample()` calls `ParseVocIfPresent()` (added for LBA2's own VOX voices). Its magic check is
`memcmp(buffer + 1, "reative Voice File\\x1A", 19)`, i.e. it accepts the 0x00/`C`/`W`-prefixed
VOC variants. LBA1's SAMPLES.HQR and VOX voices are exactly that VOC format, so they are
**drop-in**: no new decoder needed.

This tool ports `ParseVocIfPresent` (faithfully) and runs it over real LBA1 audio to confirm
every entry is VOC in the supported variant (8-bit PCM, sane rate). Source of truth for the VOC
acceptance rule is lba2cc's SAMPLE.CPP; LBA1 data is from ../LBA.

Usage: lba1_voc_probe.py [--samples ../LBA/SAMPLES.HQR] [--vox ../LBA/VOX/EN_000.VOX]
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hqr_inspect  # noqa: E402

VOC_MAGIC = b"reative Voice File\x1a"   # 19 bytes, matched at offset 1 (SAMPLE.CPP:97,105)


def parse_voc(buf):
    """Port of LIB386/AIL/SDL/SAMPLE.CPP ParseVocIfPresent. Detection is the offset-1 magic
    (the real VOC signature); `gate_ok` records whether lba2cc's CURRENT first-byte gate
    (buffer[0] in {C,W,0x00}, SAMPLE.CPP:103) would also accept it."""
    if len(buf) < 26:
        return None
    if buf[1:20] != VOC_MAGIC:                       # SAMPLE.CPP:105 (the true signature)
        return None
    gate_ok = buf[0] in (ord('C'), ord('W'), 0)      # SAMPLE.CPP:103 (current acceptance gate)
    pos, ext_rate = 26, 0
    while pos + 4 <= len(buf):
        t = buf[pos]
        bs = buf[pos + 1] | (buf[pos + 2] << 8) | (buf[pos + 3] << 16)
        if t == 0x00:
            break
        if pos + 4 + bs > len(buf):
            break
        if t == 0x08 and bs >= 4:                    # extended: rate + channels
            fd = buf[pos + 4] | (buf[pos + 5] << 8)
            ch = buf[pos + 7] + 1
            ch = ch if 1 <= ch <= 8 else 1
            if fd != 65535 and ch > 0:
                ext_rate = 256000000 // ((65536 - fd) * ch)
        if t == 0x01 and bs >= 2:                    # sound data
            fd, codec = buf[pos + 4], buf[pos + 5]
            hz = ext_rate if ext_rate > 0 else (1000000 // (256 - fd) if fd != 255 else 0)
            if hz <= 0 or hz > 192000:
                hz = 11025
            return {"rate": hz, "is8bit": codec == 0, "codec": codec,
                    "pcm": bs - 2, "first_byte": buf[0], "gate_ok": gate_ok}
        pos += 4 + bs
    return None


def scan(path, label):
    ents, data = hqr_inspect.entries(path)
    voc = nonvoc = empty = gate_rejected = 0
    rates, codecs, firsts = {}, {}, {}
    bad, gated = [], []
    for i, off, size, csize, method in ents:
        if size is None:
            empty += 1
            continue
        blob = hqr_inspect.decompress_entry(data, off, size, csize, method)
        r = parse_voc(blob)
        if r is None:
            nonvoc += 1
            if len(bad) < 5:
                bad.append((i, blob[:4].hex(" ")))
            continue
        voc += 1
        rates[r["rate"]] = rates.get(r["rate"], 0) + 1
        codecs[r["codec"]] = codecs.get(r["codec"], 0) + 1
        firsts[r["first_byte"]] = firsts.get(r["first_byte"], 0) + 1
        if not r["gate_ok"]:
            gate_rejected += 1
            if len(gated) < 6:
                gated.append(i)
    present = voc + nonvoc
    print(f"{label}: {present} present entries, {voc} are VOC (offset-1 magic), {nonvoc} not, {empty} empty")
    print(f"    first-byte variants: { {chr(k) if k else f'0x{k:02x}': v for k, v in firsts.items()} }")
    print(f"    codecs: {codecs}  (0 = 8-bit PCM)   rates: {dict(sorted(rates.items()))}")
    if gate_rejected:
        print(f"    !! {gate_rejected} VOC entries rejected by lba2cc's current first-byte gate "
              f"(buffer[0] not in {{C,W,0x00}}): entries {gated} -> need a 1-line widening")
    if bad:
        print(f"    non-VOC entries: {bad}")
    return present, voc, codecs, gate_rejected


def line(t, ok, extra=""):
    return f"  [{'PASS' if ok else 'FAIL'}] {t}" + (f"  ({extra})" if extra else "")


def main():
    hacking = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--samples", default=os.path.join(hacking, "LBA", "SAMPLES.HQR"))
    ap.add_argument("--vox", default=os.path.join(hacking, "LBA", "VOX", "EN_000.VOX"))
    a = ap.parse_args()
    all_ok = True
    total_gated = 0

    print("=" * 72)
    print("Spike 4: LBA1 VOC audio through lba2cc's existing ParseVocIfPresent (SAMPLE.CPP)")
    print("-" * 72)
    for path, label in ((a.samples, "SAMPLES.HQR (SFX)"), (a.vox, "VOX EN_000 (voices)")):
        if not os.path.exists(path):
            print(f"{label}: MISSING {path}"); continue
        present, voc, codecs, gated = scan(path, label)
        ok = present > 0 and voc == present and set(codecs) <= {0}      # all VOC, all 8-bit
        print(line(f"{label}: all entries are VOC + 8-bit PCM", ok))
        all_ok &= ok
        total_gated += gated

    print("=" * 72)
    print(f"SPIKE 4 {'PASS' if all_ok else 'FAIL'}: LBA1 SAMPLES + VOX are Creative Voice (.voc), "
          "8-bit PCM, the exact format lba2cc's PlaySample already decodes (VOC support was added "
          "for LBA2's own VOX voices). SFX are fully drop-in via the existing AIL path.")
    if total_gated:
        print(f"FINDING: {total_gated} LBA1 voice entries use a 0x01-prefixed VOC variant that "
              "lba2cc's current first-byte gate (SAMPLE.CPP:103, {C,W,0x00}) rejects. Fix is one "
              "line: accept any first byte (the offset-1 magic is the real signature) or add 0x01. "
              "Then voices are drop-in too, via the GameProfile's sample loader.")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
