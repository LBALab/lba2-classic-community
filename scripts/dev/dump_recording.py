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

Keyframe output is a delta per line, so a session reads as what changed:

    kf  1600: cube 60->35  cubemode 1->0  hero.x 19483->4078  blackpal 1->0
    kf  1728: dial.obj 0->2

Format: SOURCES/RECORD.CPP is the authority. A text header of key=value lines, a blank
line, then records with a flags byte each. Little-endian throughout.
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

    return header, polls, ticks, keys, cmds, teles, syncs


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    path = argv[1]
    what = argv[2] if len(argv) > 2 else "all"
    header, polls, ticks, keys, cmds, teles, syncs = parse(path)

    if what == "all":
        print(header)
    print("polls=%d ticks=%d keyframes=%d telemetry=%d cmds=%d syncs=%d"
          % (polls, len(ticks), len(keys), len(teles), len(cmds), syncs))
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
