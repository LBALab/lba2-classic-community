#!/usr/bin/env python3
"""Vita FTP/companion client, run by run-vita.sh in the pinned SDK image."""

import argparse
import ftplib
import ipaddress
from pathlib import Path
import re
import socket
import sys


ROOT = Path(__file__).resolve().parents[2]


def deploy(address, mode):
    directory = ROOT / "out/vita"
    source = directory / ("lba2cc-vita.vpk" if mode == "vpksend" else "eboot.bin")
    if not source.is_file() or not source.stat().st_size:
        raise ValueError("Vita build missing. Run bash scripts/dev/build-vita.sh first.")
    title_id = (directory / "title-id.txt").read_text().strip()
    if not re.fullmatch(r"[A-Z0-9]{9}", title_id):
        raise ValueError("Invalid Vita title ID in out/vita/title-id.txt; rebuild before uploading.")
    target = "/ux0:/data/lba2cc-vita.vpk" if mode == "vpksend" else f"/ux0:/app/{title_id}/eboot.bin"
    print(f"Uploading {source.name} to {address}:1337{target}", flush=True)
    with ftplib.FTP() as ftp:
        ftp.connect(address, 1337, timeout=30)
        ftp.login()
        with source.open("rb") as binary:
            ftp.storbinary(f"STOR {target}", binary)
    if mode != "launch":
        print("Upload complete." if mode == "send" else "Upload complete. Install the VPK with VitaShell.")
        return
    with socket.create_connection((address, 1338), timeout=10) as connection:
        connection.sendall(f"launch {title_id}\n".encode("ascii"))
        response = bytearray()
        while True:
            block = connection.recv(1024)
            if not block:
                break
            response.extend(block)
            if len(response) > 4096:
                raise ValueError("Vita companion response exceeds 4096 bytes.")
    message = response.decode("utf-8", errors="replace").strip()
    if message != "Launched.":
        raise ValueError(f"Vita launch failed: {message or 'empty companion response'}")
    print(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("address", type=ipaddress.IPv4Address)
    parser.add_argument("mode", nargs="?", choices=("launch", "send", "vpksend"), default="launch")
    args = parser.parse_args()
    try:
        deploy(str(args.address), args.mode)
    except (OSError, ValueError, ftplib.Error) as error:
        print(f"Vita deployment failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
