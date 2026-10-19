"""Poll UI samples must not shift the offline reader's next record."""

import contextlib
import importlib.util
import io
from pathlib import Path
import struct
import tempfile

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "dump_recording", root / "scripts/dev/dump_recording.py"
)
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)

stream = bytearray(b"LBA2REC 17\n\n")
for poll in range(70):
    if poll == 64:
        stream += struct.pack("<BIII", reader.REC_SYNC, reader.SYNC_MAGIC, poll, 1)
    stream += bytes([0x1F])
    stream += struct.pack("<BHB", 1, 82, poll & 1)
    stream += struct.pack("<i", 82)
    stream += struct.pack("<hhIiii", 0, 0, 0, poll, 0, 0)
    stream += struct.pack("<IB", 16, 0x11 if poll & 1 else 0)
    stream += bytes([0])  # Unchanged UI state has no payload.
stream += struct.pack("<BIIQ", 0x80, 1, 1000, 123)

with tempfile.TemporaryDirectory(dir=Path.cwd()) as folder:
    recording = Path(folder) / "ui.rec"
    recording.write_bytes(stream)
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        parsed = reader.parse(str(recording))
    assert not output.getvalue(), output.getvalue()
    assert parsed[1] == 140, parsed[1]
    assert parsed[2] == [(1, 1000, 123)], parsed[2]
    assert parsed[6] == 1, parsed[6]
    assert len(parsed[8]) == 140, len(parsed[8])

    for legacy in ("legacy-v10.rec", "legacy-v13.rec"):
        parsed = reader.parse(str(root / "tests/automation/recordings" / legacy))
        assert parsed[1] > 0, legacy
