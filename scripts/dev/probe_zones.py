#!/usr/bin/env python3
"""Characterise camera-zone activation by teleporting the hero around a cube.

The boxes are static scene data — containment is arithmetic and needs no engine.
What needs measuring is the *latch*: which camera zone carries ZONE_ACTIVE, when it
flips, and whether that depends on how the hero arrived. `zonelist` reports both,
so a teleport plus a read is one full observation.
"""
import re
import sys
import time

sys.path.insert(0, "scripts/dev")
from lba2ctl import Control

ZONE = re.compile(
    r"\[\s*(\d+)\]\s+(\S+)\s+Num=(-?\d+)\s+"
    r"box=\((-?\d+),(-?\d+),(-?\d+)\)-\((-?\d+),(-?\d+),(-?\d+)\)\s+"
    r"info=([-\d,]+)\s+flags=0x([0-9a-f]+)\s*(.*)$")

ZONE_TEST_BRICK = 2  # in Info5; see COMMON.H
HEAD = re.compile(r"hero @ \((-?\d+),(-?\d+),(-?\d+)\) ZoneSce=(-?\d+)")


class Zone:
    def __init__(self, m):
        g = m.groups()
        self.idx, self.type, self.num = int(g[0]), g[1], int(g[2])
        self.box = tuple(int(v) for v in g[3:9])
        self.info = [int(v) for v in g[9].split(",")]
        self.flags = int(g[10], 16)
        self.tail = g[11]
        self.inside = "<-- HERO" in g[11]
        self.active = "active" in g[11]

    @property
    def needs_brick(self):
        """A cube gate that waits for the hero to collide with the door.

        GereZoneChangeCube tests this before doing anything, so a gate carrying it
        never fires on containment alone, which is all a teleport can produce. It
        reads identically to a gate that is simply switched off, and the two want
        opposite fixes.
        """
        return len(self.info) > 5 and bool(self.info[5] & ZONE_TEST_BRICK)

    def __repr__(self):
        return f"[{self.idx}]{self.type}"


def read_zones(c):
    lines = c.send("zonelist")
    head = HEAD.search(lines[0])
    hero = tuple(int(v) for v in head.groups()[:3]) if head else None
    zonesce = int(head.group(4)) if head else None
    zones = [Zone(m) for m in (ZONE.search(ln) for ln in lines[1:]) if m]
    return hero, zonesce, zones


def goto(c, x, y, z, settle=0.15):
    """Teleport, then let a simulation tick actually happen before reading.

    Commands run one per presented frame, and presents outnumber sim ticks by a
    long way — CheckZoneSce, which does all the zone entering and leaving, runs in
    the object loop on stepped frames only. Reading straight after the teleport
    returns the zone state from before it, which looks exactly like a latch that
    never moves.
    """
    c.send(f"teleport {x} {y} {z}")
    if settle:
        time.sleep(settle)
    return read_zones(c)


if __name__ == "__main__":
    with Control(4444, timeout=60) as c:
        c.send("fixedtimestep 40")
        hero, zonesce, zones = read_zones(c)
        cams = [z for z in zones if z.type == "camera"]
        print(f"hero @ {hero}  ZoneSce={zonesce}")
        print(f"{len(zones)} zones, {len(cams)} camera:")
        for z in cams:
            print(f"   [{z.idx}] box={z.box} flags=0x{z.flags:02x} "
                  f"{'INSIDE' if z.inside else '      '} "
                  f"{'ACTIVE' if z.active else ''}")
