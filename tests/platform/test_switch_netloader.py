#!/usr/bin/env python3
"""Exercise the launcher's real nxlink without hardware, inside the pinned SDK.

A Docker shim executes the launcher's client command in this container instead
of nesting Docker. The protocol peer and client use loopback; stdout stays a pipe.
Build the NRO first. This does not test Docker port forwarding or console runtime.
"""

import os
from pathlib import Path
import select
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest
import zlib


ROOT = Path(__file__).resolve().parents[2]


def docker_shim():
    args = sys.argv[1:]
    if args == ["info"]:
        return
    if not args or args[0] != "run":
        raise RuntimeError(f"Unexpected Docker invocation: {args}")
    image = (ROOT / "docker/switch-image.txt").read_text().strip()
    command = args[args.index(image) + 1:]
    os.chdir(ROOT / "out/switch")
    os.execvp(command[0], command)


def read_exact(connection, size):
    data = bytearray()
    while len(data) < size:
        block = connection.recv(size - len(data))
        if not block:
            raise RuntimeError("Unexpected end of nxlink upload")
        data.extend(block)
    return bytes(data)


def read_u32(connection):
    return struct.unpack("<I", read_exact(connection, 4))[0]


class NetloaderTest(unittest.TestCase):
    def test_logs_arrive_while_connection_is_open(self):
        nro = (ROOT / "out/switch/switch/lba2cc/lba2cc.nro").read_bytes()
        (ROOT / ".tmp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / ".tmp") as temporary, socket.socket() as listener:
            os.symlink(Path(__file__).resolve(), Path(temporary) / "docker")
            environment = dict(os.environ, PATH=temporary + os.pathsep + os.environ["PATH"])
            listener.bind(("127.0.0.1", 28280))
            listener.listen(1)
            listener.settimeout(10)
            process = subprocess.Popen(
                ["bash", str(ROOT / "scripts/dev/run-switch.sh"), "127.0.0.1"],
                env=environment, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            )
            try:
                with listener.accept()[0] as upload:
                    upload.settimeout(10)
                    self.assertEqual(read_exact(upload, read_u32(upload)), b"lba2cc/lba2cc.nro")
                    self.assertEqual(read_u32(upload), len(nro))
                    upload.sendall(struct.pack("<I", 0))
                    inflater = zlib.decompressobj()
                    received = bytearray()
                    while not inflater.eof:
                        received.extend(inflater.decompress(read_exact(upload, read_u32(upload))))
                    self.assertEqual(received, nro)
                    upload.sendall(struct.pack("<I", 0))
                    self.assertEqual(read_u32(upload), 0)

                deadline = time.monotonic() + 5
                while True:
                    try:
                        stream = socket.create_connection(("127.0.0.1", 28771), timeout=1)
                        break
                    except ConnectionRefusedError:
                        if time.monotonic() >= deadline:
                            raise
                        time.sleep(0.01)

                with stream:
                    output = bytearray()
                    for marker in (b"live nxlink log\n", b"partial nxlink log"):
                        stream.sendall(marker)
                        deadline = time.monotonic() + 5
                        while marker not in output:
                            remaining = deadline - time.monotonic()
                            if remaining <= 0 or not select.select([process.stdout], [], [], remaining)[0]:
                                break
                            block = os.read(process.stdout.fileno(), 4096)
                            if not block:
                                break
                            output.extend(block)
                        self.assertIsNone(process.poll(), "nxlink exited before the log connection closed")
                        self.assertIn(marker, output, "nxlink buffered logs while the game remained connected")
                remainder, _ = process.communicate(timeout=5)
                self.assertEqual(process.returncode, 0, (output + remainder).decode(errors="replace"))
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.communicate(timeout=5)
                if process.stdout:
                    process.stdout.close()


if __name__ == "__main__":
    if Path(sys.argv[0]).name == "docker":
        docker_shim()
    else:
        unittest.main()
