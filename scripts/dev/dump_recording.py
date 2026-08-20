#!/usr/bin/env python3
"""Read a .rec session recording without the engine.

The recorder's own reporting answers "did this replay reproduce". This answers "what is
in the file": the header, how many polls and ticks it holds, the console commands it
captured, and what the keyframes say the session did. Useful when the engine cannot be
run against a recording at all, and for reading a session as a story rather than as a
divergence.

    scripts/dev/dump_recording.py session.rec            # header + summary + keyframes
    scripts/dev/dump_recording.py session.rec keys       # keyframe deltas only
    scripts/dev/dump_recording.py session.rec cmds       # console commands only
    scripts/dev/dump_recording.py session.rec ticks      # tick, TimerRefHR, hash
    scripts/dev/dump_recording.py session.rec saves      # write out the savegames it carries

Keyframe output is a delta per line, so a session reads as what changed:

    kf  1600: cube 60->35  cubemode 1->0  hero.x 19483->4078  blackpal 1->0
    kf  1728: dial.obj 0->2

Format: SOURCES/RECORD.CPP is the authority. A text header of key=value lines, a blank
line, the savegame the session started from, then records with a flags byte each, and
the savegame it ended at. Little-endian throughout.

A recording is one file, so `saves` is how the savegames come back out of it without
running the engine. A chunk is [op][u32 len][payload][u32 len][magic]: the length twice
with the magic behind it, so a session killed mid-write leaves a chunk that fails to
close and is reported rather than written out as a savegame that stops early.
"""

import struct
import sys

# The keyframe fields, in the order RECORD.CPP writes them (s_kfNames).
KF = [
    "cube", "cubemode", "hero.x", "hero.y", "hero.z", "hero.beta",
    "hero.anim", "hero.body", "hero.move", "hero.life", "hero.zone",
    "comportement", "weapon",
    "cam.beta", "cam.alpha", "cam.addbeta", "cam.dist",
    "cinema", "dial.obj", "choice", "choices", "fade", "blackpal",
]

REC_KEY = 0x50   # keyframe: u32 tick, then one s32 per KF entry
REC_TELE = 0x51  # verbose telemetry: u32 tick, u16 count, then count * s32
REC_CMD = 0x40   # console command: u32 tick, u16 len, then len bytes
REC_SYNC = 0x60  # sync marker: u32 magic, u32 poll, u32 tick
SYNC_MAGIC = 0x53594E43  # "SYNC"
REC_SNAP_START = 0x70  # the savegame the session started from
REC_SNAP_END = 0x71    # the one it ended at, absent if it did not finish
SNAP_MAGIC = 0x534E4150  # "SNAP"
CHUNK_HEAD, CHUNK_TAIL = 5, 8


def read_chunk(data, at):
    """The payload at `at`, or None when the chunk does not close there.

    Returns (op, payload, next_offset). A chunk whose tail is missing or does not match
    its length is a torn write: everything from `at` on is unusable, and saying so is
    the point of the frame."""
    if at + CHUNK_HEAD > len(data):
        return None
    op = data[at]
    length = struct.unpack_from("<I", data, at + 1)[0]
    end = at + CHUNK_HEAD + length
    if end + CHUNK_TAIL > len(data):
        return None
    again, magic = struct.unpack_from("<II", data, end)
    if again != length or magic != SNAP_MAGIC:
        return None
    return op, data[at + CHUNK_HEAD:end], end + CHUNK_TAIL


def parse(path):
    data = open(path, "rb").read()
    split = data.find(b"\n\n")
    if split < 0:
        raise SystemExit("%s: no header (not a recording?)" % path)
    header = data[:split].decode("latin1")
    p, n = split + 2, len(data)

    polls = 0
    ticks, keys, cmds, teles = [], [], [], []
    syncs = 0
    snaps = {}

    # The start snapshot sits between the header and the first record. A recording made
    # before the format carried one starts straight into the stream, so this is a probe
    # rather than an expectation.
    if p < n and data[p] == REC_SNAP_START:
        got = read_chunk(data, p)
        if got is None:
            print("start snapshot at byte %d does not close; the rest is unreadable" % p)
            return header, polls, ticks, keys, cmds, teles, syncs, snaps
        snaps["start"] = got[1]
        p = got[2]

    while p < n:
        start = p
        try:
            flags = data[p]
            p += 1

            if flags == REC_SYNC:
                magic, _poll, _tick = struct.unpack_from("<III", data, p)
                p += 12
                if magic != SYNC_MAGIC:
                    print("bad sync marker at byte %d" % start)
                    break
                syncs += 1
            elif flags == REC_KEY:
                tick = struct.unpack_from("<I", data, p)[0]
                p += 4
                vals = list(struct.unpack_from("<%di" % len(KF), data, p))
                p += 4 * len(KF)
                keys.append((tick, vals))
            elif flags == REC_TELE:
                tick, count = struct.unpack_from("<IH", data, p)
                p += 6
                p += 4 * count
                teles.append((tick, count))
            elif flags == REC_CMD:
                tick, length = struct.unpack_from("<IH", data, p)
                p += 6
                cmds.append((tick, data[p:p + length].decode("latin1")))
                p += length
            elif flags in (REC_SNAP_START, REC_SNAP_END):
                got = read_chunk(data, start)
                if got is None:
                    print("snapshot at byte %d does not close; the session that wrote it "
                          "did not finish" % start)
                    break
                snaps["end" if flags == REC_SNAP_END else "start"] = got[1]
                p = got[2]
                if flags == REC_SNAP_END:
                    break  # the trailer: the record stream ends here
            elif flags & 0x80:
                # 0xC0 carries a 32-bit hash, 0x80 a 64-bit one.
                tick, ref = struct.unpack_from("<II", data, p)
                p += 8
                if flags == 0xC0:
                    h = struct.unpack_from("<I", data, p)[0]
                    p += 4
                else:
                    h = struct.unpack_from("<Q", data, p)[0]
                    p += 8
                ticks.append((tick, ref, h))
            elif flags > 0x0F:
                print("unknown record 0x%02x at byte %d" % (flags, start))
                break
            else:
                polls += 1
                if flags & 0x01:      # key table changes
                    p += 1 + data[p] * 3
                if flags & 0x02:      # Key
                    p += 4
                if flags & 0x04:      # analog block
                    p += 20
                if flags & 0x08:      # clock delta
                    p += 4
        except (IndexError, struct.error):
            print("truncated at byte %d" % start)
            break

    return header, polls, ticks, keys, cmds, teles, syncs, snaps


def write_saves(path, snaps):
    """The savegames, back out as files the engine can be pointed at."""
    stem = path[:-4] if path.endswith(".rec") else path
    if not snaps:
        print("no savegames in this recording")
        return
    for which in ("start", "end"):
        if which in snaps:
            out = "%s.%s.lba" % (stem, which)
            open(out, "wb").write(snaps[which])
            print("wrote %s (%d bytes)" % (out, len(snaps[which])))


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    path = argv[1]
    what = argv[2] if len(argv) > 2 else "all"
    header, polls, ticks, keys, cmds, teles, syncs, snaps = parse(path)

    if what == "saves":
        write_saves(path, snaps)
        return

    if what == "all":
        print(header)
    print("polls=%d ticks=%d keyframes=%d telemetry=%d cmds=%d syncs=%d"
          % (polls, len(ticks), len(keys), len(teles), len(cmds), syncs))
    # No end snapshot means the session did not stop cleanly, which is worth saying
    # rather than leaving to be noticed.
    print("savegames: start %s, end %s"
          % tuple("%d bytes" % len(snaps[k]) if k in snaps else "none"
                  for k in ("start", "end")))
    if teles:
        print("verbose telemetry: %d values a tick" % teles[0][1])

    if what in ("all", "cmds"):
        for tick, line in cmds:
            print("  cmd @tick %d: %s" % (tick, line))

    if what in ("all", "keys"):
        prev = None
        for tick, vals in keys:
            if prev is None:
                print("kf %5d: %s" % (tick, "  ".join(
                    "%s=%d" % (KF[i], vals[i]) for i in range(len(KF)))))
            else:
                moved = ["%s %d->%d" % (KF[i], prev[i], vals[i])
                         for i in range(len(KF)) if prev[i] != vals[i]]
                print("kf %5d: %s" % (tick, "  ".join(moved) if moved else "(no change)"))
            prev = vals

    if what == "ticks":
        for tick, ref, h in ticks:
            print("%d %d %016x" % (tick, ref, h))


if __name__ == "__main__":
    main(sys.argv)
