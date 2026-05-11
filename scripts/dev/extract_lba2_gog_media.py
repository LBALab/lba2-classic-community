#!/usr/bin/env python3
"""Extract the missing media files (VIDEO.HQR, VOX, music WAVs) from a
GOG DRM-free LBA2 install's `LBA2.GOG` BIN image into the install
directory, so the modern engine can load them at filesystem level.

See issue #119 for why this exists. In short: GOG ships the 1997 retail
disc as a raw Mode-1 BIN image (`LBA2.GOG`). HQRs are duplicated at
filesystem level so gameplay works, but FMVs, voices, and music live
only inside the BIN. This script extracts them once.

After running, point the engine at the install dir:

    ./lba2 --game-dir /path/to/LBA2-GOG

Usage:
    python3 scripts/dev/extract_lba2_gog_media.py <gog-install-dir>

Idempotent: skips files that already exist at the destination with the
expected size. Pass `--force` to overwrite.
"""
import argparse
import os
import struct
import sys

SECTOR = 2352
DATA_OFFSET = 16
DATA_SIZE = 2048


def read_sector(f, lba):
    f.seek(lba * SECTOR + DATA_OFFSET)
    return f.read(DATA_SIZE)


def parse_dir(buf):
    out = []
    i = 0
    while i < len(buf):
        rec_len = buf[i] if i < len(buf) else 0
        if rec_len == 0:
            i = (i // DATA_SIZE + 1) * DATA_SIZE
            if i >= len(buf):
                break
            continue
        lba = struct.unpack_from("<I", buf, i + 2)[0]
        size = struct.unpack_from("<I", buf, i + 10)[0]
        flags = buf[i + 25]
        nl = buf[i + 32]
        name = buf[i + 33 : i + 33 + nl].decode("latin-1", errors="replace").split(";")[0]
        if name not in ("\x00", "\x01"):
            out.append((name, lba, size, bool(flags & 0x02)))
        i += rec_len
    return out


def read_data(f, lba, size):
    chunks = []
    rem = size
    s = lba
    while rem > 0:
        d = read_sector(f, s)
        chunks.append(d[: min(DATA_SIZE, rem)])
        rem -= DATA_SIZE
        s += 1
    return b"".join(chunks)


def find(f, lba, size, parts):
    raw = read_data(f, lba, size)
    for name, clba, csize, isdir in parse_dir(raw):
        if name.upper() == parts[0].upper():
            if len(parts) == 1:
                return (clba, csize, isdir)
            if isdir:
                return find(f, clba, csize, parts[1:])
    return None


def list_dir(f, lba, size):
    return parse_dir(read_data(f, lba, size))


def extract_file(f, lba, size, dest, force):
    if os.path.exists(dest) and os.path.getsize(dest) == size and not force:
        return False
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    data = read_data(f, lba, size)
    with open(dest, "wb") as o:
        o.write(data)
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("install_dir", help="GOG install dir containing LBA2.GOG")
    ap.add_argument("--force", action="store_true", help="re-extract even if dest exists at expected size")
    args = ap.parse_args()

    bin_path = os.path.join(args.install_dir, "LBA2.GOG")
    if not os.path.isfile(bin_path):
        sys.exit(f"error: {bin_path} not found — is this a GOG install dir?")

    with open(bin_path, "rb") as f:
        pvd = read_sector(f, 16)
        if pvd[1:6] != b"CD001":
            sys.exit("error: LBA2.GOG is not an ISO9660 image")
        root_lba = struct.unpack_from("<I", pvd, 156 + 2)[0]
        root_size = struct.unpack_from("<I", pvd, 156 + 10)[0]

        lba2_dir = find(f, root_lba, root_size, ["LBA2"])
        if not lba2_dir:
            sys.exit("error: /LBA2/ directory not found inside image")
        lba2_lba, lba2_size, _ = lba2_dir

        # Plan: VIDEO/VIDEO.HQR (1 file), VOX/* (39 files), MUSIC/* (24 files)
        plan = []
        for subdir in ("VIDEO", "VOX", "MUSIC"):
            res = find(f, lba2_lba, lba2_size, [subdir])
            if not res:
                print(f"warning: /LBA2/{subdir}/ not found, skipping")
                continue
            sub_lba, sub_size, _ = res
            for name, lba, size, isdir in list_dir(f, sub_lba, sub_size):
                if isdir:
                    continue
                # Destination: install_dir/<subdir>/<name> (e.g. .../VOX/EN_000.VOX)
                # except MUSIC → Music to match modern Classic layout
                top = "Music" if subdir == "MUSIC" else subdir
                dest = os.path.join(args.install_dir, top, name)
                plan.append((name, lba, size, dest))

        total_bytes = sum(size for _, _, size, _ in plan)
        print(f"Planning to extract {len(plan)} files, {total_bytes / 1024 / 1024:.1f} MB total")

        extracted = 0
        skipped = 0
        for name, lba, size, dest in plan:
            wrote = extract_file(f, lba, size, dest, args.force)
            if wrote:
                extracted += 1
                print(f"  extracted  {size:>11}  {dest}")
            else:
                skipped += 1

        print()
        print(f"Done. Extracted {extracted}, skipped {skipped} (already present).")
        if extracted:
            print(f"\nYou can now run the engine against {args.install_dir}.")


if __name__ == "__main__":
    main()
