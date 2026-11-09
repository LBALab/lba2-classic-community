#!/usr/bin/env python3
"""Pack a folder into an unsigned .appx, without the Windows SDK.

    pack_appx.py FOLDER OUT.appx

FOLDER holds AppxManifest.xml and the app's files. The package is a ZIP64
file whose entries are stored, not compressed, so the block map needs only
one SHA-256 per 64 KiB block. Sign the result with osslsigncode, using a certificate whose
subject matches the manifest's Publisher.
"""

import base64
import hashlib
import os
import struct
import sys
import zlib
from xml.sax.saxutils import quoteattr

BLOCK = 64 * 1024
CONTENT_TYPES = {
    "exe": "application/x-msdownload",
    "dll": "application/x-msdownload",
    "png": "image/png",
}
DOS_TIME, DOS_DATE = 0, (2026 - 1980) << 9 | 1 << 5 | 1  # 2026-01-01 00:00
ZIP64 = 45  # version needed for ZIP64 records


class Zip64Writer:
    """Stored entries with ZIP64 central records, as makeappx writes them."""

    def __init__(self, path):
        self.f = open(path, "wb")
        self.entries = []

    def add(self, name, data, deflate=False):
        """Returns the size of the entry's local file header."""
        raw = name.encode("utf-8")
        offset = self.f.tell()
        crc = zlib.crc32(data) & 0xFFFFFFFF
        stored = data
        if deflate:
            packer = zlib.compressobj(9, zlib.DEFLATED, -15)
            stored = packer.compress(data) + packer.flush()
        method = 8 if deflate else 0
        header = struct.pack("<IHHHHHIIIHH", 0x04034B50, ZIP64, 0, method, DOS_TIME, DOS_DATE, crc,
                             len(stored), len(data), len(raw), 0) + raw
        self.f.write(header)
        self.f.write(stored)
        self.entries.append((raw, crc, len(stored), len(data), method, offset))
        return len(header)

    def close(self):
        cd_start = self.f.tell()
        for raw, crc, csize, size, method, offset in self.entries:
            extra = struct.pack("<HHQQQ", 0x0001, 24, size, csize, offset)
            self.f.write(struct.pack("<IHHHHHHIIIHHHHHII", 0x02014B50, ZIP64, ZIP64, 0, method, DOS_TIME, DOS_DATE,
                                     crc, 0xFFFFFFFF, 0xFFFFFFFF, len(raw), len(extra), 0, 0, 0, 0,
                                     0xFFFFFFFF) + raw + extra)
        cd_size = self.f.tell() - cd_start
        eocd64 = self.f.tell()
        n = len(self.entries)
        self.f.write(struct.pack("<IQHHIIQQQQ", 0x06064B50, 44, ZIP64, ZIP64, 0, 0, n, n, cd_size, cd_start))
        self.f.write(struct.pack("<IIQI", 0x07064B50, 0, eocd64, 1))
        self.f.write(struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, 0xFFFF, 0xFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0))
        self.f.close()


def payload(folder):
    names = []
    for root, _, files in os.walk(folder):
        for f in files:
            path = os.path.relpath(os.path.join(root, f), folder).replace(os.sep, "/")
            if path != "AppxManifest.xml" and not f.startswith("."):
                names.append(path)
    # The manifest comes after the payload, as makeappx orders it.
    return sorted(names) + ["AppxManifest.xml"]


def main():
    folder, out = sys.argv[1], sys.argv[2]
    names = payload(folder)
    extensions = sorted({n.rsplit(".", 1)[-1].lower() for n in names[:-1] if "." in n})
    unknown = [e for e in extensions if e not in CONTENT_TYPES]
    if unknown:
        sys.exit("no content type for: " + ", ".join(unknown))

    blockmap = [
        '<?xml version="1.0" encoding="UTF-8" standalone="no"?>',
        '<BlockMap xmlns="http://schemas.microsoft.com/appx/2010/blockmap"'
        ' HashMethod="http://www.w3.org/2001/04/xmlenc#sha256">',
    ]
    z = Zip64Writer(out)
    for name in names:
        with open(os.path.join(folder, name), "rb") as f:
            data = f.read()
        lfh = z.add(name, data)
        blockmap.append('<File Name=%s Size="%d" LfhSize="%d">' % (quoteattr(name.replace("/", "\\")), len(data), lfh))
        for i in range(0, max(len(data), 1), BLOCK):
            digest = base64.b64encode(hashlib.sha256(data[i : i + BLOCK]).digest()).decode()
            blockmap.append('<Block Hash="%s"/>' % digest)
        blockmap.append("</File>")
    blockmap.append("</BlockMap>")
    z.add("AppxBlockMap.xml", "".join(blockmap).encode("utf-8"))

    types = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">']
    for e in extensions:
        types.append('<Default Extension="%s" ContentType="%s"/>' % (e, CONTENT_TYPES[e]))
    types.append('<Override PartName="/AppxManifest.xml" ContentType="application/vnd.ms-appx.manifest+xml"/>')
    types.append('<Override PartName="/AppxBlockMap.xml" ContentType="application/vnd.ms-appx.blockmap+xml"/>')
    types.append("</Types>")
    # Deflated: osslsigncode rewrites this entry compressed but keeps the
    # original method, so a stored one comes out corrupt (osslsigncode 2.15).
    z.add("[Content_Types].xml", "".join(types).encode("utf-8"), deflate=True)
    z.close()


if __name__ == "__main__":
    main()
