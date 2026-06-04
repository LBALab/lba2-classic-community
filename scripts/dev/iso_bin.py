#!/usr/bin/env python3
"""Read the ISO9660 filesystem out of a raw Mode1/2352 CD image, on the fly.

These "CD dumps" ship under many extensions — GOG's `.GOG`, plain `.BIN`, `.DOT`,
`.IMG` — but the format is the same: a raw image whose data track is MODE1/2352
(2352 bytes/sector = 12 sync + 4 header + 2048 user data + 288 EDC/ECC), paired with
a CUE sheet (often a `.DAT`/`.CUE`) that lists the data track plus any CD-DA audio
tracks. This reader cares only about the single MODE1/2352 *data* track (track 1,
starting at image offset 0); audio tracks live elsewhere in the image and are the
CUE's business, not ours.

We map an ISO logical block (LBA) to a byte offset of `lba*2352 + 16` and read 2048
bytes, so the ISO9660 filesystem is walked in place — no need to materialize a 600 MB
plain `.iso`. Verified against the Adeline CD dumps for LBA1 (`LBA.DOT`), Time
Commando (`GAME.GOG`), and LBA2 (`LBA2.GOG`).

This also doubles as the working spec for an in-engine ISO9660-from-image reader that
could let the engine load media directly from a GOG package without an extraction step
(see issue #119). For bulk extraction of LBA2's missing media into an install dir, use
`extract_lba2_gog_media.py`.

Usage:
  iso_bin.py <image> [--ls PATH] [--tree] [--extract ISOPATH OUTFILE]

Examples:
  iso_bin.py LBA2.GOG --tree | grep -i '\\.HQR$'
  iso_bin.py GAME.GOG --extract /STAGE00/RUN0/SCENE.HQR out.hqr
"""
import struct, sys, argparse

SECTOR = 2352
USER = 2048
HDR = 16  # 12 sync + 4 header before the 2048 user bytes (Mode1/2352)

class Image:
    def __init__(self, path):
        self.f = open(path, "rb")
    def read_lba(self, lba, count=1):
        out = bytearray()
        for i in range(count):
            self.f.seek((lba + i) * SECTOR + HDR)
            out += self.f.read(USER)
        return bytes(out)

def parse_dir_records(data):
    """Yield (name, lba, size, is_dir) from a directory extent's bytes."""
    i = 0
    while i < len(data):
        rec_len = data[i]
        if rec_len == 0:
            # advance to next logical sector boundary
            nxt = (i // USER + 1) * USER
            if nxt <= i:
                break
            i = nxt
            continue
        ext_attr = data[i + 1]
        lba = struct.unpack_from("<I", data, i + 2)[0]
        size = struct.unpack_from("<I", data, i + 10)[0]
        flags = data[i + 25]
        len_fi = data[i + 32]
        fi = data[i + 33 : i + 33 + len_fi]
        is_dir = bool(flags & 0x02)
        if len_fi == 1 and fi in (b"\x00", b"\x01"):
            name = "." if fi == b"\x00" else ".."
        else:
            name = fi.decode("latin-1").split(";")[0]
        yield name, lba, size, is_dir
        i += rec_len

def get_pvd(img):
    # volume descriptors start at LBA 16
    for lba in range(16, 32):
        d = img.read_lba(lba)
        if d[1:6] != b"CD001":
            raise SystemExit("not an ISO9660 image (no CD001 at LBA %d)" % lba)
        if d[0] == 1:  # Primary Volume Descriptor
            return d
        if d[0] == 255:
            break
    raise SystemExit("no Primary Volume Descriptor found")

def root_record(pvd):
    rec = pvd[156 : 156 + 34]
    lba = struct.unpack_from("<I", rec, 2)[0]
    size = struct.unpack_from("<I", rec, 10)[0]
    return lba, size

def list_dir(img, lba, size):
    nsec = (size + USER - 1) // USER
    data = img.read_lba(lba, nsec)
    return [r for r in parse_dir_records(data) if r[0] not in (".", "..")]

def walk(img, lba, size, prefix=""):
    for name, clba, csize, is_dir in list_dir(img, lba, size):
        path = prefix + "/" + name
        yield path, clba, csize, is_dir
        if is_dir:
            yield from walk(img, clba, csize, path)

def find(img, root_lba, root_size, target):
    target = target.strip("/").upper()
    lba, size = root_lba, root_size
    if not target:
        return lba, size, True
    parts = target.split("/")
    for j, part in enumerate(parts):
        match = None
        for name, clba, csize, is_dir in list_dir(img, lba, size):
            if name.upper() == part:
                match = (clba, csize, is_dir); break
        if match is None:
            raise SystemExit("path not found: %s" % target)
        lba, size, is_dir = match
        if j < len(parts) - 1 and not is_dir:
            raise SystemExit("not a directory: %s" % part)
    return lba, size, is_dir

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--ls", default="/", help="list a directory")
    ap.add_argument("--tree", action="store_true", help="recursive listing from root")
    ap.add_argument("--extract", nargs=2, metavar=("ISOPATH", "OUT"))
    a = ap.parse_args()
    img = Image(a.image)
    pvd = get_pvd(img)
    rlba, rsize = root_record(pvd)
    volid = pvd[40:72].decode("latin-1").strip()
    print(f"# volume: {volid!r}  root LBA={rlba} size={rsize}")

    if a.extract:
        lba, size, is_dir = find(img, rlba, rsize, a.extract[0])
        if is_dir:
            raise SystemExit("is a directory")
        nsec = (size + USER - 1) // USER
        data = img.read_lba(lba, nsec)[:size]
        with open(a.extract[1], "wb") as fh:
            fh.write(data)
        print(f"wrote {a.extract[1]} ({size} bytes, LBA {lba})")
        return

    if a.tree:
        for path, lba, size, is_dir in walk(img, rlba, rsize):
            tag = "DIR " if is_dir else "    "
            print(f"{tag}{size:>10}  LBA{lba:>8}  {path}")
        return

    lba, size, is_dir = find(img, rlba, rsize, a.ls)
    if not is_dir:
        print(f"(file) {a.ls}  size={size} LBA={lba}")
        return
    for name, clba, csize, d in sorted(list_dir(img, lba, size)):
        tag = "DIR " if d else "    "
        print(f"{tag}{csize:>10}  LBA{clba:>8}  {name}")

if __name__ == "__main__":
    main()
