#!/usr/bin/env python3
"""Print the package full name Windows gives the app in tools/uwp/AppxManifest.xml.

Name_Version_Architecture__PublisherId. The publisher ID is the first 8 bytes of the
SHA-256 of the Publisher string in UTF-16LE, read as 13 five-bit digits (a zero bit
appended) in Crockford's base32 alphabet, lower case.

Usage: package-name.py [MANIFEST]
"""

import hashlib
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

NS = {"m": "http://schemas.microsoft.com/appx/manifest/foundation/windows10"}
ALPHABET = "0123456789abcdefghjkmnpqrstvwxyz"


def publisher_id(publisher: str) -> str:
    digest = hashlib.sha256(publisher.encode("utf-16-le")).digest()[:8]
    value = int.from_bytes(digest, "big") << 1
    return "".join(ALPHABET[(value >> shift) & 31] for shift in range(60, -1, -5))


def main() -> int:
    manifest = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("AppxManifest.xml")
    identity = ET.parse(manifest).getroot().find("m:Identity", NS)
    if identity is None:
        print(f"{manifest}: no Identity element", file=sys.stderr)
        return 1
    attrs = identity.attrib
    print(f"{attrs['Name']}_{attrs['Version']}_{attrs['ProcessorArchitecture']}__{publisher_id(attrs['Publisher'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
