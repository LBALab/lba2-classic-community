#!/usr/bin/env python3
"""Identify which LBA2 release a game directory or disc image holds.

docs/VERSIONS.md records that nothing in the engine detects the release: the
`Version` key in `lba2.cfg` is written by an installer and believed on sight, so
a hand-assembled install loses its publisher identity silently and the engine
then looks for the wrong CD volume label, the wrong voice folder and the wrong
CD track table. This script is the detector that document says does not exist,
kept outside the engine until the rules have been checked against enough
releases to be worth compiling in.

Two independent questions get answered, because they are independent facts:

  lineage    Which of the two masters this data is: lba2, or twinsen for
             Twinsen's Odyssey. The engine only ever distinguishes those two.
             Decided by RESS.HQR, whose contents differ between the Activision
             and the Electronic Arts masters and nowhere else that matters.
  packaging  Who shipped the copy: a 1997 disc, a DOS install off one, GOG's
             DOSBox package, GOG or Steam's 2point21 re-release, the demo. This
             is wrapper evidence only (steam_api.dll, distrib.cfg, goggame-*),
             since the payload is identical across all of them.

The evidence for both is printed, not just the verdict, so a disagreement is
visible rather than averaged away.

Usage:
  fingerprint_distro.py PATH [PATH...]      a game dir, a disc image, or a cue
  fingerprint_distro.py --scan DIR          every immediate child of DIR
  fingerprint_distro.py --json PATH         machine-readable, one object per path

Python 3 stdlib only. Reads at most a few hundred KB per target (RESS.HQR and
the config file); nothing is written and nothing is mounted.
"""
import argparse
import hashlib
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import iso_bin

# DistribVersion values, DEFINES.H:200.
UNKNOWN_VERSION = 0
ACTIVISION_VERSION = 1
ACTIVISION_SUD_VERSION = 2
EA_VERSION = 3
VIRGIN_VERSION = 4
VIRGIN_ASIA_VERSION = 5

DISTRIB_NAMES = {
    UNKNOWN_VERSION: "unknown",
    ACTIVISION_VERSION: "activision",
    ACTIVISION_SUD_VERSION: "activision_sud",
    EA_VERSION: "ea",
    VIRGIN_VERSION: "virgin",
    VIRGIN_ASIA_VERSION: "virgin_asia",
}

# The engine reads two masters, not six publishers. {UNKNOWN, EA} take the LBA2
# identity, disc label LBA2 and lba2/vox; everything else takes Twinsen's
# Odyssey, TWINSEN and twinsen/vox. A lineage answer names the product for that
# reason: it stays true for a Virgin pressing nobody has sampled, where naming
# the publisher would be a guess about a company that never touched this data.
MASTER_NAMES = {
    UNKNOWN_VERSION: "lba2",
    EA_VERSION: "lba2",
    ACTIVISION_VERSION: "twinsen",
    ACTIVISION_SUD_VERSION: "twinsen",
    VIRGIN_VERSION: "twinsen",
    VIRGIN_ASIA_VERSION: "twinsen",
}


def master_name(ver):
    return MASTER_NAMES.get(ver, "?")


# RESS.HQR splits the corpus in two, and the split lines up exactly with the
# publisher. Three of its fifty entries differ between the two masters, and none
# of them is branding: 1 RESS_FONT_GPM, 44 RESS_FILE3D, 47 RESS_IMPACT. So what
# is really being measured is which data master this copy was built from. That
# the two map one to one onto the publisher holds across every release checked,
# but it is a correlation, not a logo being read off the disc, and a pressing
# from a publisher not in the table below will land as unrecorded rather than as
# a wrong answer. That failure mode is deliberate: an unrecorded payload falls
# through to whatever the disc label or the config declares.
#
# Both Activision pressings (European and Brazilian) ship the same RESS while
# differing in SCENE.HQR and TEXT.HQR, which is what makes those two a locale
# signal instead of a publisher one. They are used for `pressing` below and must
# not be used for lineage.
#
# Sizes come first in the key because a stat() is what an in-engine version of
# this check would want to try before reading 570 KB.
RESS_LINEAGE = {
    (582445, "e60135c38b88547fd914245c0d2ab914"): ACTIVISION_VERSION,
    (582473, "7bf2437b5954d9423db86174eb974264"): EA_VERSION,
}
# The demo carries its own cut of the resources and no LBA2.HQR at all.
RESS_DEMO = (289888, "375eea42aff9d7761858ec14d5adbb49")

# (SCENE.HQR size, TEXT.HQR size) -> which pressing, for the distinctions RESS
# cannot make. Sizes alone are enough to separate the four known payloads.
PRESSINGS = {
    (542973, 443010): "activision-eu",
    (543198, 442911): "activision-br",
    (543251, 442979): "ea",  # also every GOG and Steam re-release
    (49017, 231241): "demo",
}

# Volume label the disc carries, which is also what PERSO.CPP:3250 goes looking
# for once DistribVersion is set. Agreement between this and RESS is the check
# that the lineage rule is really about the publisher.
LABEL_LINEAGE = {"TWINSEN": ACTIVISION_VERSION, "LBA2": EA_VERSION}

IMAGE_EXTS = (".iso", ".bin", ".img", ".gog", ".mdx", ".dot", ".nrg")


def index_dir(path):
    """Case-insensitive name -> real name map for one directory level."""
    try:
        return {name.lower(): name for name in os.listdir(path)}
    except OSError:
        return {}


def pick(idx, path, *names):
    """First of `names` that exists in `idx`, as a full path, else None."""
    for name in names:
        real = idx.get(name.lower())
        if real is not None:
            return os.path.join(path, real)
    return None


def md5_of(path):
    h = hashlib.md5()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_cfg(blob):
    """Pull the publisher header, Version and Demo keys out of an LBA2.CFG.

    The header comment is the installer's own claim about the release, e.g.
    `; L.B.A.2 Configuration file (Electronic Arts)`. It is worth reading
    because it survives in installs whose Version key has been lost.
    """
    text = blob.decode("latin-1", "replace")
    out = {"header_publisher": None, "version": None, "demo": None, "keys": 0}
    lines = text.splitlines()
    if lines:
        m = re.search(r"\(([^)]+)\)", lines[0])
        if m and lines[0].lstrip().startswith(";"):
            out["header_publisher"] = m.group(1).strip()
    for line in lines:
        m = re.match(r"\s*([A-Za-z_]+)\s*:\s*(.*?)\s*$", line)
        if not m:
            continue
        out["keys"] += 1
        key, val = m.group(1).lower(), m.group(2)
        if key == "version" and val:
            try:
                out["version"] = int(val)
            except ValueError:
                pass
        elif key == "demo" and val:
            out["demo"] = val
    return out


HEADER_LINEAGE = {
    "electronic arts": EA_VERSION,
    "activision": ACTIVISION_VERSION,
    "virgin": VIRGIN_VERSION,
}


class Target:
    """One thing to identify, with its evidence collected as it is found."""

    def __init__(self, label, kind):
        self.label = label
        self.kind = kind  # "dir" or "image"
        self.game = "unknown"
        self.packaging = "unknown"
        self.pressing = "-"
        self.markers = []
        self.signals = {}  # signal name -> (DistribVersion, detail)
        self.notes = []

    def signal(self, name, version, detail):
        self.signals[name] = (version, detail)

    MEASURED = ("ress", "volume_label")
    DECLARED = ("cfg_version", "cfg_header")

    def measured_master(self):
        """The master the copy itself shows, or (None, None).

        RESS.HQR first: it is the copy. The volume label is as good but only
        exists for a disc.
        """
        for name in self.MEASURED:
            ver = self.signals.get(name, (None,))[0]
            if ver is not None:
                return master_name(ver), name
        return None, None

    def resolve(self):
        """Settle on one DistribVersion, preferring measured over declared.

        The measurement is coarser than the answer. RESS and the volume label
        distinguish the two masters and nothing finer, so on the Twinsen's
        Odyssey side they can only ever say ACTIVISION_VERSION, whoever actually
        pressed it. A declaration that agrees about the master is therefore not
        competing with the measurement, it is naming the publisher the
        measurement cannot reach, and it is taken for that and nothing else. One
        that disagrees about the master is a genuine conflict and is ignored
        here; conflicts() reports it.
        """
        master, basis = self.measured_master()
        for name in self.DECLARED:
            ver = self.signals.get(name, (None,))[0]
            if ver is None:
                continue
            if master is None:
                return ver, name
            if master_name(ver) == master and ver != self.signals[basis][0]:
                return ver, name
        if basis is not None:
            return self.signals[basis][0], basis
        return UNKNOWN_VERSION, "default"

    def conflicts(self):
        """Signals that disagree, which is the whole reason to print evidence.

        Compared as masters, because that is the unit every signal can actually
        speak in. RESS can only ever answer ACTIVISION_VERSION or EA_VERSION, so
        holding it against a config declaring Virgin would report a disagreement
        between two statements that agree completely: both say Twinsen's
        Odyssey. That false positive would fire on precisely the pressings
        nobody has sampled, which is where the tool is supposed to degrade
        gracefully.

        What is lost is two publishers disagreeing on one master, which is a
        disagreement about who sold a copy rather than about what the copy is.

        A disagreement here is not noise to be averaged away: it means either
        the rules are wrong or the install has been assembled from two sources,
        and both are worth knowing before this logic moves into the engine.
        """
        claims = {}
        for name, (ver, _) in sorted(self.signals.items()):
            if ver is not None:
                claims.setdefault(master_name(ver), []).append(name)
        if len(claims) < 2:
            return []
        return ["%s" % ", ".join("%s says %s" % ("+".join(names), master)
                                 for master, names in sorted(claims.items()))]

    def to_dict(self):
        ver, basis = self.resolve()
        return {
            "target": self.label,
            "game": self.game,
            "packaging": self.packaging,
            "pressing": self.pressing,
            "distrib_version": ver,
            "distrib_name": DISTRIB_NAMES[ver],
            "master": master_name(ver),
            "distrib_basis": basis,
            "markers": self.markers,
            "signals": {k: {"version": v[0], "detail": v[1]} for k, v in self.signals.items()},
            "conflicts": self.conflicts(),
            "notes": self.notes,
        }


# --- Directory targets -------------------------------------------------------

# Ordered: the first rule that matches names the packaging. Each entry is
# (label, predicate over the case-insensitive index of the game root).
def classify_packaging(root_idx, data_idx, has_lba2_hqr):
    def has(*names):
        return any(n.lower() in root_idx for n in names)

    def glob(pattern):
        return any(re.match(pattern, n) for n in root_idx)

    if has("steam_api.dll", "steam_api64.dll") and "common" in root_idx:
        return "steam-classic"
    if has("distrib.cfg") or glob(r"goggame-\d+\.info$"):
        return "gog-classic" if "common" in root_idx else "gog"
    if has("lba2.gog") or glob(r"dosbox.*\.conf$"):
        return "gog-dos"
    if "common" in root_idx and has("tlba2c.exe", "tlba2.exe"):
        return "2point21-classic"
    if not has_lba2_hqr:
        return "demo"
    if has("lba2.exe", "lba2.dos", "setup.exe"):
        return "retail-install"
    if has("lba2cc.exe", "lba2cc"):
        return "port-tree"
    return "loose-data"


def probe_dir(path):
    t = Target(path, "dir")
    root_idx = index_dir(path)

    # The 2point21 re-releases keep the payload one level down in Common/.
    data = path
    data_idx = root_idx
    common = pick(root_idx, path, "Common")
    if common and os.path.isdir(common) and "ress.hqr" in index_dir(common):
        data, data_idx = common, index_dir(common)
        t.markers.append("Common/ payload layout")

    ress = pick(data_idx, data, "RESS.HQR")
    lba2_hqr = pick(data_idx, data, "LBA2.HQR")

    # RESS.HQR alone does not mean LBA2: LBA1 ships one too, and both demos
    # ship one without the LBA2.HQR that would otherwise separate them. The
    # island files and the background bank are what only LBA2 has.
    is_lba2 = (ress is not None
               and (any(n.endswith(".ile") for n in data_idx)
                    or "lba_bkg.hqr" in data_idx
                    or "anim3ds.hqr" in data_idx))
    if not is_lba2:
        if "file3d.hqr" in data_idx or any(n.endswith(".fla") for n in data_idx):
            t.game = "lba1"
        elif any(n.endswith(IMAGE_EXTS) for n in data_idx):
            t.game = "container"
            t.notes.append("holds a disc image; pass the image itself")
        t.packaging = "n/a"
        return t

    t.game = "lba2"
    size = os.path.getsize(ress)
    digest = md5_of(ress)
    if (size, digest) == RESS_DEMO:
        t.signal("ress", None, "demo resources (%d)" % size)
        t.notes.append("demo payload")
    else:
        ver = RESS_LINEAGE.get((size, digest))
        t.signal("ress", ver, "%d/%s%s" % (size, digest[:8], "" if ver else " (unrecorded)"))

    scene = pick(data_idx, data, "SCENE.HQR")
    text = pick(data_idx, data, "TEXT.HQR")
    if scene and text:
        key = (os.path.getsize(scene), os.path.getsize(text))
        t.pressing = PRESSINGS.get(key, "unrecorded %d/%d" % key)

    cfg_path = pick(data_idx, data, "LBA2.CFG") or pick(root_idx, path, "LBA2.CFG")
    if cfg_path:
        with open(cfg_path, "rb") as fh:
            cfg = parse_cfg(fh.read())
        t.markers.append("LBA2.CFG (%d keys)" % cfg["keys"])
        if cfg["version"] is not None:
            t.signal("cfg_version", cfg["version"], "Version: %d" % cfg["version"])
        if cfg["header_publisher"]:
            hv = HEADER_LINEAGE.get(cfg["header_publisher"].lower())
            t.signal("cfg_header", hv, "(%s)" % cfg["header_publisher"])
        if cfg["demo"]:
            t.notes.append("cfg declares Demo: %s" % cfg["demo"])
    else:
        t.notes.append("no LBA2.CFG in the tree, so nothing is declared")

    t.packaging = classify_packaging(root_idx, data_idx, lba2_hqr is not None)
    for name in ("steam_api.dll", "distrib.cfg", "LBA2.GOG", "TLBA2C.exe", "TLBA2.exe",
                 "LBA2.EXE", "LBA2.DOS", "INSTALL.INI", "manual.pdf"):
        if name.lower() in root_idx:
            t.markers.append(name)
    for name in sorted(root_idx):
        if re.match(r"goggame-\d+\.info$", name):
            t.markers.append(name)
    distrib = pick(root_idx, path, "distrib.cfg")
    if distrib:
        with open(distrib, "rb") as fh:
            claim = fh.read(64).decode("latin-1", "replace").strip()
        t.notes.append("distrib.cfg says %r" % claim)
    return t


# --- Disc image targets ------------------------------------------------------

def resolve_cue(path):
    """A cue points at the image that actually holds the filesystem."""
    with open(path, "rb") as fh:
        text = fh.read(4096).decode("latin-1", "replace")
    m = re.search(r'FILE\s+"([^"]+)"', text, re.I)
    if not m:
        return None
    cand = os.path.join(os.path.dirname(path), m.group(1))
    return cand if os.path.exists(cand) else None


def probe_image(path):
    t = Target(path, "image")
    if path.lower().endswith(".cue"):
        resolved = resolve_cue(path)
        if resolved is None:
            t.notes.append("cue does not resolve to an image beside it")
            return t
        t.notes.append("via %s" % os.path.basename(resolved))
        path = resolved
    try:
        img = iso_bin.open_image(path)
        if img is None:
            t.notes.append("no ISO9660 filesystem in it")
            return t
        pvd = iso_bin.get_pvd(img)
        root_lba, root_size = iso_bin.root_record(pvd)
    except SystemExit as exc:
        t.notes.append("not an ISO9660 image (%s)" % exc)
        return t
    except OSError as exc:
        t.notes.append("unreadable (%s)" % exc)
        return t

    label = pvd[40:72].decode("latin-1").strip()
    t.markers.append("volume %r, %s" % (label, img.mode))
    t.signal("volume_label", LABEL_LINEAGE.get(label.upper()), "volume %r" % label)

    # The game directory on a disc is named for the same axis as the label.
    entries = {n.upper(): (lba, sz, isdir)
               for n, lba, sz, isdir in iso_bin.list_dir(img, root_lba, root_size)}
    gamedir = next((n for n in ("TWINSEN", "LBA2") if n in entries and entries[n][2]), None)
    if gamedir is None:
        t.notes.append("no TWINSEN/ or LBA2/ directory at the root")
        return t
    t.game = "lba2"
    t.packaging = "disc-image"
    t.markers.append("/%s/" % gamedir)

    lba, size, _ = entries[gamedir]
    files = {n.upper(): (l, s) for n, l, s, isdir in iso_bin.list_dir(img, lba, size) if not isdir}
    if "RESS.HQR" in files:
        flba, fsize = files["RESS.HQR"]
        blob = img.read_lba(flba, (fsize + 2047) // 2048)[:fsize]
        digest = hashlib.md5(blob).hexdigest()
        ver = RESS_LINEAGE.get((fsize, digest))
        t.signal("ress", ver, "%d/%s%s" % (fsize, digest[:8], "" if ver else " (unrecorded)"))
    if "SCENE.HQR" in files and "TEXT.HQR" in files:
        # Sizes come straight out of the directory records, so this costs nothing.
        key = (files["SCENE.HQR"][1], files["TEXT.HQR"][1])
        t.pressing = PRESSINGS.get(key, "unrecorded %d/%d" % key)
    if "LBA2.CFG" in files:
        flba, fsize = files["LBA2.CFG"]
        cfg = parse_cfg(img.read_lba(flba, (fsize + 2047) // 2048)[:fsize])
        t.markers.append("LBA2.CFG (%d keys)" % cfg["keys"])
        if cfg["version"] is not None:
            t.signal("cfg_version", cfg["version"], "Version: %d" % cfg["version"])
        if cfg["header_publisher"]:
            hv = HEADER_LINEAGE.get(cfg["header_publisher"].lower())
            t.signal("cfg_header", hv, "(%s)" % cfg["header_publisher"])
    return t


def probe(path):
    if os.path.isdir(path):
        return probe_dir(path)
    if path.lower().endswith((".cue",) + IMAGE_EXTS):
        return probe_image(path)
    t = Target(path, "dir")
    t.notes.append("not a directory or a recognised image")
    return t


def collect(paths, scan):
    targets = []
    for p in paths:
        targets.append(p)
    for d in scan:
        for name in sorted(os.listdir(d)):
            full = os.path.join(d, name)
            if os.path.isdir(full):
                targets.append(full)
                # A folder holding a rip is identified through the rip.
                for inner in sorted(os.listdir(full)):
                    if inner.lower().endswith(IMAGE_EXTS):
                        targets.append(os.path.join(full, inner))
            elif name.lower().endswith(IMAGE_EXTS):
                targets.append(full)
    return targets


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("paths", nargs="*", help="game dirs, disc images or cue sheets")
    ap.add_argument("--scan", action="append", default=[],
                    help="identify every immediate child of this directory")
    ap.add_argument("--json", action="store_true", help="one JSON object per target")
    ap.add_argument("--verbose", "-v", action="store_true", help="markers and notes too")
    args = ap.parse_args()

    targets = collect(args.paths, args.scan)
    if not targets:
        ap.error("nothing to identify")

    results = [probe(p) for p in targets]

    if args.json:
        json.dump([r.to_dict() for r in results], sys.stdout, indent=2)
        sys.stdout.write("\n")
        return 0

    width = max(len(os.path.basename(r.label.rstrip("/"))) for r in results)
    width = min(max(width, 12), 44)
    head = "%-*s  %-9s %-16s %-11s %-14s %s"
    print(head % (width, "target", "game", "packaging", "lineage", "pressing", "from"))
    print("-" * (width + 62))
    for r in results:
        ver, basis = r.resolve()
        name = os.path.basename(r.label.rstrip("/"))[:width]
        lineage = master_name(ver) if r.game == "lba2" else "-"
        print(head % (width, name, r.game, r.packaging, lineage, r.pressing,
                      basis if r.game == "lba2" else ""))
        if args.verbose:
            for sig, (sver, detail) in sorted(r.signals.items()):
                print("    %-14s %-15s %s" % (sig, DISTRIB_NAMES.get(sver, "-"), detail))
            for m in r.markers:
                print("    marker         %s" % m)
            for n in r.notes:
                print("    note           %s" % n)
        for c in r.conflicts():
            print("    CONFLICT       %s" % c)
    return 0


if __name__ == "__main__":
    sys.exit(main())
