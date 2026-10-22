#!/usr/bin/env python3
"""Check the built Vita package and real deployment client against loopback peers.

Run inside the pinned SDK after build-vita.sh. The Docker shim executes the
wrapper's Python client here; no console, external FTP service or nested Docker.
"""

from contextlib import contextmanager
import os
from pathlib import Path
import shlex
import selectors
import signal
import socket
import socketserver
import struct
import subprocess
import sys
import tempfile
import threading
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[2]


def docker_shim(args):
    if args == ["info"]:
        return
    image = (ROOT / "docker/vita-image.txt").read_text().strip()
    command = args[args.index(image) + 1:]
    os.chdir(ROOT)
    os.execvp(command[0], command)


class FTPHandler(socketserver.StreamRequestHandler):
    def handle(self):
        self.wfile.write(b"220 Vita fixture\r\n")
        passive = None
        try:
            while line := self.rfile.readline():
                command, _, argument = line.decode().strip().partition(" ")
                if command == "USER":
                    reply = b"331 Password required"
                elif command == "PASS":
                    reply = b"230 Logged in"
                elif command == "TYPE":
                    reply = b"200 Binary mode"
                elif command == "PASV":
                    passive = socket.socket()
                    passive.bind(("127.0.0.1", 0))
                    passive.listen(1)
                    passive.settimeout(5)
                    port = passive.getsockname()[1]
                    reply = f"227 Passive (127,0,0,1,{port // 256},{port % 256})".encode()
                elif command == "STOR":
                    self.server.paths.append(argument)
                    if self.server.reject_upload:
                        reply = b"550 Upload rejected"
                    else:
                        self.wfile.write(b"150 Opening data connection\r\n")
                        data = bytearray()
                        with passive.accept()[0] as connection:
                            connection.settimeout(5)
                            while block := connection.recv(65536):
                                data.extend(block)
                        self.server.uploads.append(bytes(data))
                        reply = b"226 Transfer complete"
                elif command == "QUIT":
                    self.wfile.write(b"221 Goodbye\r\n")
                    return
                else:
                    raise AssertionError(f"Unexpected FTP command: {command}")
                self.wfile.write(reply + b"\r\n")
        finally:
            if passive:
                passive.close()


class CompanionHandler(socketserver.StreamRequestHandler):
    def handle(self):
        self.server.commands.append(self.rfile.readline())
        self.wfile.write(self.server.response)


class Server(socketserver.TCPServer):
    allow_reuse_address = True


@contextmanager
def peers(reject_upload=False, response=b"Launched.\n"):
    with Server(("127.0.0.1", 1337), FTPHandler) as ftp, \
            Server(("127.0.0.1", 1338), CompanionHandler) as companion:
        ftp.paths, ftp.uploads, ftp.reject_upload = [], [], reject_upload
        companion.commands, companion.response = [], response
        threads = [threading.Thread(target=server.serve_forever) for server in (ftp, companion)]
        for thread in threads:
            thread.start()
        try:
            yield ftp, companion
        finally:
            ftp.shutdown()
            companion.shutdown()
            for thread in threads:
                thread.join()


def sfo_strings(data):
    magic, _, keys, values, count = struct.unpack_from("<4s4I", data)
    if magic != b"\0PSF":
        raise ValueError("Invalid SFO signature")
    result = {}
    for index in range(count):
        key, kind, length, _, value = struct.unpack_from("<HHIII", data, 20 + index * 16)
        if kind == 0x0204:
            end = data.index(b"\0", keys + key)
            result[data[keys + key:end].decode()] = data[values + value:values + value + length].rstrip(b"\0").decode()
    return result


class VitaTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / ".tmp").mkdir(exist_ok=True)
        cls.temporary = tempfile.TemporaryDirectory(dir=ROOT / ".tmp")
        shim = Path(cls.temporary.name) / "docker"
        shim.write_text("#!/bin/sh\nexec python3 " + shlex.quote(str(Path(__file__).resolve())) + ' --docker-shim "$@"\n')
        shim.chmod(0o755)
        cls.environment = dict(os.environ, PATH=cls.temporary.name + os.pathsep + os.environ["PATH"])
        cls.title_id = (ROOT / "out/vita/title-id.txt").read_text().strip()

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def run_client(self, *args):
        return subprocess.run(
            ["bash", str(ROOT / "scripts/dev/run-vita.sh"), *args],
            env=self.environment, capture_output=True, timeout=40,
        )

    def test_package_contents_and_identity(self):
        elf = (ROOT / "out/vita/debug/lba2cc.elf").read_bytes()
        diagnostic = b"debugNetInit\0" in elf
        with zipfile.ZipFile(ROOT / "out/vita/lba2cc-vita.vpk") as package:
            files = set(package.namelist())
            expected = {
                "eboot.bin", "sce_sys/param.sfo", "sce_sys/icon0.png",
                "sce_sys/livearea/contents/bg.png", "sce_sys/livearea/contents/startup.png",
                "sce_sys/livearea/contents/template.xml", "README.txt", "LICENSE",
            }
            if diagnostic:
                expected.add("DEBUGNET-NOTICE.txt")
                self.assertIn(b"Mozilla Public License", package.read("DEBUGNET-NOTICE.txt"))
            self.assertEqual(files, expected)
            self.assertEqual(package.read("eboot.bin"), (ROOT / "out/vita/eboot.bin").read_bytes())
            self.assertEqual(package.read("eboot.bin")[:4], b"SCE\0")
            metadata = sfo_strings(package.read("sce_sys/param.sfo"))
            self.assertEqual(metadata["TITLE_ID"], self.title_id)
            self.assertEqual(metadata["TITLE"], "LBA2 Classic Community")
            self.assertRegex(metadata["APP_VER"], r"^[0-9]{2}\.[0-9]{2}$")
            self.assertIn(b"ux0:/data/lba2/", package.read("README.txt"))
            for name, size in (("icon0.png", (128, 128)), ("livearea/contents/bg.png", (840, 500)),
                               ("livearea/contents/startup.png", (280, 158))):
                png = package.read("sce_sys/" + name)
                self.assertEqual(png[:8], b"\x89PNG\r\n\x1a\n")
                self.assertEqual(struct.unpack_from(">II", png, 16), size)
                self.assertEqual(png[24:26], b"\x08\x03", "LiveArea requires PNG-8 indexed palettes")
                self.assertLess(len(png), 420 * 1024)
        self.assertEqual(elf[:4], b"\x7fELF")

    def test_debugnet_receiver_preserves_output(self):
        process = subprocess.Popen(
            [sys.executable, str(ROOT / "scripts/dev/vita-log.py"), "--bind", "127.0.0.1"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        try:
            with selectors.DefaultSelector() as ready:
                ready.register(process.stderr, selectors.EVENT_READ)
                self.assertTrue(ready.select(timeout=5), "Receiver startup timed out")
            self.assertIn(b"Listening for Vita DEBUGNET", process.stderr.readline())
            message = "Vita boot fixture: \u00e9\n".encode()
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                sender.sendto(message, ("127.0.0.1", 18194))
            with selectors.DefaultSelector() as ready:
                ready.register(process.stdout, selectors.EVENT_READ)
                self.assertTrue(ready.select(timeout=5), "Receiver produced no output")
            self.assertEqual(os.read(process.stdout.fileno(), len(message)), message)
            process.send_signal(signal.SIGINT)
            _, error = process.communicate(timeout=5)
            self.assertEqual(process.returncode, 0, error.decode())
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()

    def test_upload_and_launch(self):
        with peers() as (ftp, companion):
            result = self.run_client("127.0.0.1")
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertEqual(ftp.paths, [f"/ux0:/app/{self.title_id}/eboot.bin"])
            self.assertEqual(ftp.uploads, [(ROOT / "out/vita/eboot.bin").read_bytes()])
            self.assertEqual(companion.commands, [f"launch {self.title_id}\n".encode()])

    def test_upload_only_modes_never_launch(self):
        for mode, path, file in (
            ("send", f"/ux0:/app/{self.title_id}/eboot.bin", "eboot.bin"),
            ("vpksend", "/ux0:/data/lba2cc-vita.vpk", "lba2cc-vita.vpk"),
        ):
            with self.subTest(mode=mode), peers() as (ftp, companion):
                result = self.run_client("127.0.0.1", mode)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual(ftp.paths, [path])
                self.assertEqual(ftp.uploads, [(ROOT / "out/vita" / file).read_bytes()])
                self.assertEqual(companion.commands, [])

    def test_upload_failure_prevents_launch(self):
        with peers(reject_upload=True) as (_, companion):
            result = self.run_client("127.0.0.1")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"550 Upload rejected", result.stderr)
            self.assertEqual(companion.commands, [])

    def test_launch_errors_are_reported(self):
        for response in (b"Error: cannot launch the app.\n", b""):
            with self.subTest(response=response), peers(response=response):
                result = self.run_client("127.0.0.1")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"Vita launch failed:", result.stderr)

    def test_invalid_arguments_do_not_upload(self):
        for args in ([], ["--bad"], ["999.1.1.1"], ["127.0.0.1", "bad-mode"]):
            with self.subTest(args=args):
                self.assertNotEqual(self.run_client(*args).returncode, 0)


if __name__ == "__main__":
    if sys.argv[1:2] == ["--docker-shim"]:
        docker_shim(sys.argv[2:])
    else:
        unittest.main()
