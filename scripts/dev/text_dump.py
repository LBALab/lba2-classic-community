#!/usr/bin/env python3
"""Dump a TEXT.HQR bank as (id, attribute, string) rows.

TEXT.HQR layout (see docs/TEXT.md, SOURCES/MESSAGE.CPP InitDial):
  - 6 languages x 15 banks x 2 entries = 180 HQR entries.
      entry (Language * 30) + (file * 2) + 0  ->  order bank, U16 id[MaxText]
      entry (Language * 30) + (file * 2) + 1  ->  text  bank, U16 offset[MaxText + 1] then blobs
  - The two banks are parallel arrays keyed by slot: slot n of the order bank names
    the id, slot n of the offset table locates the string.
  - Each blob is [U8 attribute][CP850 bytes][NUL]. Ids are in authoring order, not sorted.

Usage:
  text_dump.py <TEXT.HQR> [--lang N] [--bank sys|cre|gam|000..011] [--range LO:HI]
"""
import os, sys, struct, argparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hqr_inspect import entries, decompress_entry

LANGUAGES = ["English", "Francais", "Deutsch", "Espanol", "Italiano", "Portugues"]
BANKS = ["sys", "cre", "gam"] + ["%03d" % i for i in range(12)]
MAX_TEXT_LANG = 15


def _entry(path, index):
    ents, data = entries(path)
    if index >= len(ents):
        return b""
    _, off, size, csize, method = ents[index]
    if size is None:
        return b""
    return decompress_entry(data, off, size, csize, method)


def bank_rows(path, lang, file_index):
    """Return [(id, attribute, text)] for one language's copy of one bank."""
    base = lang * MAX_TEXT_LANG * 2 + file_index * 2
    order = _entry(path, base + 0)
    text = _entry(path, base + 1)
    count = len(order) // 2
    if not count or not text:
        return []
    ids = struct.unpack_from("<%dH" % count, order, 0)
    offsets = struct.unpack_from("<%dH" % (count + 1), text, 0)
    rows = []
    for slot in range(count):
        start, end = offsets[slot], offsets[slot + 1]
        if start >= len(text) or end > len(text) or end <= start:
            continue
        blob = text[start:end]
        rows.append((ids[slot], blob[0], blob[1:].split(b"\x00")[0].decode("cp850", "replace")))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--lang", type=int, default=0, help="0..5, order per TabLanguage[]")
    ap.add_argument("--bank", default="sys")
    ap.add_argument("--range", default=None, metavar="LO:HI", help="filter by text id")
    args = ap.parse_args()

    rows = bank_rows(args.path, args.lang, BANKS.index(args.bank))
    lo, hi = (-1, 1 << 30)
    if args.range:
        lo, hi = (int(v) for v in args.range.split(":"))

    print("# %s / %s: %d slots" % (LANGUAGES[args.lang], args.bank, len(rows)))
    for slot, (text_id, attr, body) in enumerate(rows):
        if lo <= text_id <= hi:
            print("%5d  slot=%4d attr=%3d  %r" % (text_id, slot, attr, body))


if __name__ == "__main__":
    main()
