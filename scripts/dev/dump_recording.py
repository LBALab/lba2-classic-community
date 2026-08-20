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
    scripts/dev/dump_recording.py session.rec analog     # polls carrying mouse or stick
    scripts/dev/dump_recording.py session.rec clock      # what the game clock did, per tick
    scripts/dev/dump_recording.py session.rec tele       # the values the digest mixes
    scripts/dev/dump_recording.py session.rec tele-changing   # only the ones that ever move
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

import os
import struct
import sys

# The keyframe fields, in the order RECORD.CPP writes them (s_kfNames). From version 12
# the record says how many it carries, so this list only supplies the *names*; a file
# with more fields than are named here reads them and reports them by index. Versions 9
# to 11 wrote no count and always the first 23 of these.
KF_LEGACY_FIELDS = 23
KF = [
    "cube", "cubemode", "hero.x", "hero.y", "hero.z", "hero.beta",
    "hero.anim", "hero.gen_anim", "hero.body", "hero.move", "hero.life", "hero.zone",
    "comportement", "weapon",
    "cam.beta", "cam.alpha", "cam.addbeta", "cam.dist",
    "cinema", "dial.obj", "choice", "choices", "fade", "blackpal",
]

# The leading fixed values of Control_StateDigest, in the order CONTROL.CPP mixes them.
# This list stops at NbLittleKeys deliberately: the per-actor loop runs next, and only
# after it come the modal fields and the two variable tables. So everything past index 37
# sits at an offset that depends on how many actors the scene holds, and cannot be named
# from a fixed list. tele_layout() derives the rest from the file.
TELE_FIXED = [
    "Island", "NumCube", "CubeMode", "NbObjets", "NbZones",
    "hero->Obj.X", "hero->Obj.Y", "hero->Obj.Z",
    "hero->Obj.Alpha", "hero->Obj.Beta", "hero->Obj.Gamma",
    "hero->LifePoint", "Comportement", "Weapon",
    "hero->Obj.Body.Num", "hero->Obj.Anim.Num", "hero->GenAnim",
    "hero->Obj.LastFrame", "hero->Move", "hero->ZoneSce",
    "hero->Flags", "hero->WorkFlags",
    "BetaCam", "AlphaCam", "GammaCam", "AddBetaCam",
    "VueDistance", "VueOffsetX", "VueOffsetY", "VueOffsetZ",
    "FollowCamera", "VueCamera", "CameraZone", "CinemaMode",
    "MagicLevel", "MagicPoint", "NbGoldPieces", "NbLittleKeys",
]
TELE_ACTOR = ["obj.X", "obj.Y", "obj.Z", "obj.Beta", "obj.Life",
              "obj.Body", "obj.Anim", "obj.Move", "obj.Flags"]
TELE_MODAL = ["CinemaMode", "NumObjDial", "GameChoice", "GameNbChoices",
              "FlagFade", "FlagBlackPal"]
MAX_VARS_GAME, MAX_VARS_CUBE = 256, 80

# What each digest version adds, from the `s_digestVersion >= N` blocks in
# Control_StateDigest. A recording made before the header carried `numeric.digest`
# is version 1. An unlisted version is one this reader does not know, and it says so
# rather than naming from the nearest table it has.
TELE_VERSIONS = {
    1: {"actor": [], "probe": []},
    2: {"actor": ["obj.Sample", "obj.LastFrame", "obj.LastTimer", "obj.NextTimer"],
        # sim.carry is hashed, rng.draws is report-only, but both take a slot in the
        # stream, which is all that matters for reading positions back out.
        "probe": ["sim.carry", "rng.draws"]},
}


def tele_version(header):
    """The digest version the header declares. Absent means 1: the field was added after
    the format was, so an older recording carries no line and holds the original set."""
    at = header.find("numeric.digest=")
    if at < 0:
        return 1
    try:
        return int(header[at + len("numeric.digest="):].split("\n", 1)[0].strip())
    except ValueError:
        return -1


def tele_layout(header, vals):
    """Names for one tick's telemetry, or None when the layout cannot be established.

    Two things have to line up. The digest version says which fields exist, and it comes
    from `numeric.digest` in the header (absent means 1, from before the line existed).
    The actor block's length then depends on NbObjets, which is itself in the stream at
    index 3, so the layout solves rather than being assumed.

    The solve is the check: if the arithmetic does not come out exactly, this is not the
    field set it claims and the honest answer is to report by index. A version this
    reader does not know is refused for the same reason -- naming from the nearest table
    to hand is how six fields got the wrong names in the first place."""
    spec = TELE_VERSIONS.get(tele_version(header))
    if spec is None:
        return None

    actor = TELE_ACTOR + spec["actor"]
    tail = len(TELE_MODAL) + len(spec["probe"]) + MAX_VARS_GAME + MAX_VARS_CUBE
    n = len(vals)
    if n <= len(TELE_FIXED) + tail:
        return None
    nb = vals[3]  # NbObjets, mixed fourth
    body = n - len(TELE_FIXED) - tail
    if nb <= 0 or body <= 0 or body != nb * len(actor):
        return None

    names = list(TELE_FIXED)
    for i in range(nb):
        names.extend("%s[%d]" % (f, i) for f in actor)
    names.extend(TELE_MODAL)
    names.extend(spec["probe"])
    names.extend("var.game[%d]" % i for i in range(MAX_VARS_GAME))
    names.extend("var.cube[%d]" % i for i in range(MAX_VARS_CUBE))
    return names


def tele_name(names, k):
    if names is not None and k < len(names):
        return names[k]
    return TELE_FIXED[k] if k < len(TELE_FIXED) else "value[%d]" % k


REC_KEY = 0x50   # keyframe: u32 tick, u16 count (v12+), then count * s32
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

    # "LBA2REC <ver>" is the first line, and the keyframe's shape depends on it.
    try:
        version = int(header.split("\n", 1)[0].split()[1])
    except (IndexError, ValueError):
        version = 0
    p, n = split + 2, len(data)

    polls = 0
    # One entry per poll that carried mouse or stick data. Empty for a keyboard-only
    # session, which every session is until the two devices can be driven headlessly.
    analog = []
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
            return header, polls, ticks, keys, cmds, teles, syncs, snaps, analog
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
                if version >= 12:
                    count = struct.unpack_from("<H", data, p)[0]
                    p += 2
                else:
                    count = KF_LEGACY_FIELDS
                vals = list(struct.unpack_from("<%di" % count, data, p))
                p += 4 * count
                keys.append((tick, vals))
            elif flags == REC_TELE:
                tick, count = struct.unpack_from("<IH", data, p)
                p += 6
                vals = list(struct.unpack_from("<%di" % count, data, p))
                p += 4 * count
                teles.append((tick, vals))
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
                    analog.append((polls,) + struct.unpack_from("<hhIiii", data, p))
                    p += 20
                if flags & 0x08:      # clock delta
                    p += 4
        except (IndexError, struct.error):
            print("truncated at byte %d" % start)
            break

    return header, polls, ticks, keys, cmds, teles, syncs, snaps, analog


def write_saves(path, snaps):
    """The savegames, back out as files the engine can be pointed at."""
    stem = path[:-4] if path.endswith(".rec") else path
    if not snaps:
        print("no savegames in this recording")
        return
    for which in ("start", "end"):
        if which not in snaps:
            continue
        out = "%s.%s.lba" % (stem, which)
        if os.path.exists(out):
            print("%s exists; not overwriting it" % out)
            continue
        # Closed rather than left to the interpreter: an unclosed handle is a savegame
        # that stops early, which is the one failure this whole format is built to avoid.
        with open(out, "wb") as fh:
            fh.write(snaps[which])
        print("wrote %s (%d bytes)" % (out, len(snaps[which])))


def show_clock(ticks):
    """What the game clock did, per tick.

    A pinned step should give one delta and nothing else. Anything else is the clock
    moving in a way the session did not ask for, and a *negative* delta is the game
    clock going backwards: the savegame carries TimerRefHR (SOURCES/SAVEGAME.CPP), so
    every load reinstalls that save's baseline. On a session that reloads often this is
    the dominant term, and it is invisible in a summary that only reports a maximum."""
    if len(ticks) < 2:
        print("fewer than two ticks; nothing to compare")
        return
    ticks.sort()
    deltas = {}
    jumps = []
    for i in range(1, len(ticks)):
        d = ticks[i][1] - ticks[i - 1][1]
        deltas[d] = deltas.get(d, 0) + 1
        if d != 16:
            jumps.append((ticks[i][0], d, ticks[i - 1][1], ticks[i][1]))

    span = ticks[-1][1] - ticks[0][1]
    print("ticks %d, clock ref %d -> %d" % (len(ticks), ticks[0][1], ticks[-1][1]))
    print("span %d ms (%.1f min); at 16 ms a tick it would be %d ms (%.1f min)"
          % (span, span / 60000.0,
             16 * (len(ticks) - 1), 16 * (len(ticks) - 1) / 60000.0))
    print("deltas that are not 16 ms: %d of %d" % (len(jumps), len(ticks) - 1))

    back = [j for j in jumps if j[1] < 0]
    if back:
        # Every backward jump landing on one value is a restore rather than drift, and
        # saying which value turns "the clock is noisy" into "something reinstalls this".
        lands = {}
        for _, _, _, to in back:
            lands[to] = lands.get(to, 0) + 1
        worst = min(back, key=lambda j: j[1])
        print("  BACKWARDS: %d, worst %+d ms at tick %d (%d -> %d)"
              % (len(back), worst[1], worst[0], worst[2], worst[3]))
        common = sorted(lands.items(), key=lambda kv: -kv[1])[:3]
        print("  they land on: %s"
              % ", ".join("%d (x%d)" % (v, n) for v, n in common))

    print("delta histogram:")
    for d, n in sorted(deltas.items(), key=lambda kv: -kv[1])[:14]:
        print("  %+8d ms x %d" % (d, n))


def show_analog(analog, polls):
    """A summary before the rows, because the interesting thing about analog is usually
    a value that never changes rather than one that does. The format writes this block
    only on polls that carry something (RECORD.CPP), so an axis that is never zero
    defeats that and puts 20 bytes on every poll."""
    if not analog:
        print("no analog samples")
        return
    fields = [("rsx", 1), ("rsy", 2), ("padfirst", 3), ("mdx", 4), ("mdy", 5), ("click", 6)]
    print("analog blocks: %d of %d polls (%.1f%%), %d bytes"
          % (len(analog), polls, 100.0 * len(analog) / polls if polls else 0,
             20 * len(analog)))
    for name, i in fields:
        vals = [a[i] for a in analog]
        nz = [v for v in vals if v]
        if not nz:
            print("  %-9s always zero" % name)
            continue
        distinct = set(nz)
        if len(distinct) == 1 and len(nz) == len(vals):
            # The case worth shouting about: a constant on every sample is a stuck
            # input, and it is what makes the block unskippable.
            print("  %-9s CONSTANT %d on every sample -- this alone defeats the "
                  "per-poll skip" % (name, nz[0]))
        else:
            print("  %-9s nonzero on %d, |max| %d, %d distinct"
                  % (name, len(nz), max(abs(v) for v in nz), len(distinct)))


def show_tele(teles, changing_only, header=""):
    """The values the digest mixes, per tick. Nothing else can read these: the recorder
    reports a divergence for at most three ticks and eight values, and before this the
    offline reader counted them without printing any."""
    if not teles:
        print("no verbose telemetry in this recording "
              "(record with --record-telemetry, or `rec start verbose`)")
        return
    teles.sort()
    n = len(teles[0][1])
    names = tele_layout(header, teles[0][1])
    print("%d ticks, %d values a tick" % (len(teles), n))
    if names is None:
        # Say so rather than let indexed output pass for named output.
        print("layout not established for this file; reporting past index %d by index"
              % (len(TELE_FIXED) - 1))
    else:
        print("layout: digest v%d, %d fixed, %d actors x %d, %d modal+probe, %d vars"
              % (tele_version(header), len(TELE_FIXED), teles[0][1][3],
                 len(TELE_ACTOR) + len(TELE_VERSIONS[tele_version(header)]["actor"]),
                 len(TELE_MODAL) + len(TELE_VERSIONS[tele_version(header)]["probe"]),
                 MAX_VARS_GAME + MAX_VARS_CUBE))

    if changing_only:
        # Which values ever move. A field that is constant across a whole session is
        # either genuinely static or stuck, and the two look identical in one tick.
        first = teles[0][1]
        moved = set()
        for _, vals in teles:
            for k in range(min(n, len(vals))):
                if vals[k] != first[k]:
                    moved.add(k)
        print("values that change at least once: %d of %d" % (len(moved), n))
        print("constant for the whole session: %d" % (n - len(moved)))
        for k in sorted(moved)[:40]:
            lo = min(v[1][k] for v in teles if k < len(v[1]))
            hi = max(v[1][k] for v in teles if k < len(v[1]))
            print("  %-24s %d .. %d" % (tele_name(names, k), lo, hi))
        return

    prev = None
    for tick, vals in teles:
        if prev is None:
            print("tele %5d: %s" % (tick, "  ".join(
                "%s=%d" % (tele_name(names, i), vals[i]) for i in range(min(24, len(vals))))))
        else:
            shared = min(len(prev), len(vals))
            moved = ["%s %d->%d" % (tele_name(names, i), prev[i], vals[i])
                     for i in range(shared) if prev[i] != vals[i]]
            if moved:
                print("tele %5d: %s" % (tick, "  ".join(moved)))
        prev = vals


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    path = argv[1]
    what = argv[2] if len(argv) > 2 else "all"
    header, polls, ticks, keys, cmds, teles, syncs, snaps, analog = parse(path)

    if what == "saves":
        write_saves(path, snaps)
        return

    if what == "all":
        print(header)
    print("polls=%d ticks=%d keyframes=%d telemetry=%d cmds=%d syncs=%d analog=%d"
          % (polls, len(ticks), len(keys), len(teles), len(cmds), syncs, len(analog)))
    # No end snapshot means the session did not stop cleanly, which is worth saying
    # rather than leaving to be noticed.
    print("savegames: start %s, end %s"
          % tuple("%d bytes" % len(snaps[k]) if k in snaps else "none"
                  for k in ("start", "end")))
    if teles:
        print("verbose telemetry: %d values a tick" % len(teles[0][1]))

    if what in ("all", "cmds"):
        for tick, line in cmds:
            print("  cmd @tick %d: %s" % (tick, line))

    if what in ("all", "keys"):
        def kfname(i):
            # A file may carry a field this list does not name yet, and reporting it by
            # index beats dropping it or shifting every name after it along.
            return KF[i] if i < len(KF) else "field[%d]" % i

        prev = None
        for tick, vals in keys:
            if prev is None:
                print("kf %5d: %s" % (tick, "  ".join(
                    "%s=%d" % (kfname(i), vals[i]) for i in range(len(vals)))))
            else:
                shared = min(len(prev), len(vals))
                moved = ["%s %d->%d" % (kfname(i), prev[i], vals[i])
                         for i in range(shared) if prev[i] != vals[i]]
                print("kf %5d: %s" % (tick, "  ".join(moved) if moved else "(no change)"))
            prev = vals

    if what == "clock":
        show_clock(ticks)

    if what in ("tele", "tele-changing"):
        show_tele(teles, what == "tele-changing", header)

    if what == "analog":
        show_analog(analog, polls)
        print()
        for poll, rsx, rsy, pad, mdx, mdy, click in analog:
            print("poll %d rsx %d rsy %d padfirst %d mdx %d mdy %d click %d"
                  % (poll, rsx, rsy, pad, mdx, mdy, click))

    if what == "ticks":
        for tick, ref, h in ticks:
            print("%d %d %016x" % (tick, ref, h))


if __name__ == "__main__":
    main(sys.argv)
