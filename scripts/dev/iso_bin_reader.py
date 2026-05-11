#!/usr/bin/env python3
"""Minimal ISO9660 reader for raw Mode-1 BIN images (2352-byte sectors).

Walks the ISO9660 filesystem inside a raw CD-ROM image (BIN format with
2352-byte sectors: 16-byte sync+header + 2048 bytes user data + 288
bytes ECC/EDC). Lists every file with size and path.

Used originally to map the contents of GOG's `LBA2.GOG` — the bit-exact
preservation of the 1997 retail CD's *data track* (the disc's CD-DA audio
track is not in the BIN; GOG ships it separately as `LBA2.OGG`). See
issue #119 for the broader context — this script is the working spec
for an in-engine ISO9660-from-BIN reader that would let the engine read
media files directly from the GOG package without an extraction step.

Usage:
    python3 scripts/dev/iso_bin_reader.py /path/to/image.bin

The output is a flat sorted listing: `<size>  /path/to/file`. Pipe to grep
to find specific resources, e.g.:

    python3 scripts/dev/iso_bin_reader.py LBA2.GOG | grep -i '\\.HQR$'
"""
import sys, struct

BIN = sys.argv[1]

SECTOR = 2352
DATA_OFFSET = 16   # sync(12) + header(4)
DATA_SIZE = 2048

def read_sector(f, lba):
    f.seek(lba * SECTOR + DATA_OFFSET)
    return f.read(DATA_SIZE)

def parse_dir(buf):
    """Parse one ISO9660 directory data block. Returns list of (name, lba, size, is_dir)."""
    out = []
    i = 0
    while i < len(buf):
        rec_len = buf[i] if i < len(buf) else 0
        if rec_len == 0:
            # padding to next 2048 boundary
            i = (i // DATA_SIZE + 1) * DATA_SIZE
            if i >= len(buf):
                break
            continue
        lba = struct.unpack_from("<I", buf, i+2)[0]
        size = struct.unpack_from("<I", buf, i+10)[0]
        flags = buf[i+25]
        name_len = buf[i+32]
        name = buf[i+33 : i+33+name_len].decode("latin-1", errors="replace")
        # strip ;1 version suffix
        if ";" in name:
            name = name.split(";", 1)[0]
        # skip . and ..
        if name not in ("\x00", "\x01"):
            out.append((name, lba, size, bool(flags & 0x02)))
        i += rec_len
    return out

def read_file_data(f, lba, size):
    chunks = []
    remaining = size
    s = lba
    while remaining > 0:
        d = read_sector(f, s)
        chunks.append(d[:min(DATA_SIZE, remaining)])
        remaining -= DATA_SIZE
        s += 1
    return b"".join(chunks)

def walk(f, lba, size, path=""):
    """Recursively walk directory at given extent."""
    raw = read_file_data(f, lba, size)
    entries = parse_dir(raw)
    files = []
    for name, child_lba, child_size, is_dir in entries:
        full = path + "/" + name
        if is_dir:
            files.extend(walk(f, child_lba, child_size, full))
        else:
            files.append((full, child_lba, child_size))
    return files

with open(BIN, "rb") as f:
    pvd = read_sector(f, 16)
    assert pvd[1:6] == b"CD001", "not ISO9660"
    # root directory record at offset 156 in PVD user data, length 34
    root_rec = pvd[156:156+34]
    root_lba = struct.unpack_from("<I", root_rec, 2)[0]
    root_size = struct.unpack_from("<I", root_rec, 10)[0]
    print(f"Volume ID: {pvd[40:72].decode('latin-1').rstrip()}")
    print(f"Root LBA={root_lba}, size={root_size}")
    print()
    files = walk(f, root_lba, root_size)
    files.sort(key=lambda x: x[0].lower())
    for name, lba, size in files:
        print(f"  {size:>11}  {name}")
    print()
    print(f"Total: {len(files)} files")
