#!/usr/bin/env python3
"""Extract a playable game-data folder out of a CD, including the Red Book themes.

The engine already mounts a bin+cue rip and plays everything off it, so for that
shape this script is optional. It exists for the cases the engine cannot serve:

  * A physical disc. Data and jingles play straight off a mounted CD, but the six
    themes are Red Book audio and reading audio from a drive is a deliberate
    non-goal (see docs/DISC_IMAGE_SOURCE.md). Rip the audio with any tool, point
    this at the result, and the themes become files the engine loads normally.
  * A container the engine mounts data-only: an MDX (encrypted track table), NRG,
    CCD. Convert or rip the audio separately, then use `--tracks-from`.
  * Wanting loose files on disk for modding or inspection.

What only this repo can tell you is the naming. Every ripper hands back
`track02.wav`; nothing else knows that CD track 6 is JADPCM01 rather than
TADPCM6, or that the ListJingle[1] theme is not pressed on the US disc at all.
That mapping is CD_TRACK_NAMES below, kept in step with LIB386/SYSTEM/CDTRACKS.CPP
by tests/cdtracks.

Usage:
  disc_extract.py SOURCE OUTDIR [--tracks-from DIR] [--force] [--dry-run]

  SOURCE  a .cue, a disc image, a directory holding either, or a directory of
          already-ripped track WAVs (audio only, for the mounted-disc flow).

Examples:
  disc_extract.py TWINSEN.cue ~/lba2-data
  disc_extract.py /media/cdrom ~/lba2-data --tracks-from ~/ripped
  disc_extract.py ~/ripped ~/lba2-data           # audio only, into an existing tree

Then: lba2cc --game-dir ~/lba2-data

Python 3 stdlib only. Idempotent: a file already there at the expected size is
left alone unless you pass --force. `--selftest` checks the cue rules against
fixtures and needs no disc.
"""
import argparse
import os
import re
import shutil
import sys
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import iso_bin

# CD track N holds ListJingle[N]. Track 1 is the data track, so the ListJingle[1]
# theme (TADPCM1) has no track and is not on the US disc. Mirrors CD_TRACK_NAMES
# in LIB386/SYSTEM/CDTRACKS.CPP; tests/cdtracks reads both so they cannot drift.
CD_TRACK_NAMES = {
    2: "TADPCM2",
    3: "TADPCM3",
    4: "TADPCM4",
    5: "TADPCM5",
    6: "JADPCM01",
    7: "TADPCM6",
}

# The marker that says a directory holds game data, same one discovery uses
# (FILE_VALID_RES_DIR in SOURCES/DIRECTORIES.CPP).
MARKER = "LBA2.HQR"

# The music subdirectory, in the four spellings Discover sweeps. The engine finds
# any of them; we reuse whichever the output tree already has.
MUSIC_DIR = "music"
MUSIC_SPELLINGS = ("music", "MUSIC", "Music")

RAW_SECTOR = 2352  # a CD-DA sector: 588 stereo frames of 16-bit LE PCM
CDDA_RATE = 44100
CDDA_CHANNELS = 2
CDDA_WIDTH = 2

IMAGE_EXTS = (".bin", ".iso", ".img", ".gog", ".dot", ".mdf", ".raw")
CUE_EXTS = (".cue", ".dat", ".toc")


# --- cue sheet ---------------------------------------------------------------
class CueTrack(object):
    def __init__(self, number, mode, source):
        self.number = number
        self.mode = mode  # AUDIO, MODE1/2352, ...
        self.source = source  # the FILE this track lives in
        self.indexes = {}  # index number -> LBA within that file

    @property
    def start(self):
        return self.indexes.get(1, self.indexes.get(0))

    @property
    def first_index(self):
        """Where the track's area begins, pregap included."""
        return self.indexes.get(0, self.indexes.get(1))


def msf_to_lba(text):
    m, s, f = (int(p) for p in text.split(":"))
    return (m * 60 + s) * 75 + f


def parse_cue(path):
    """Return the cue's tracks in file order. Raises ValueError on a bad INDEX."""
    tracks = []
    source = None
    current = None
    with open(path, "r", errors="replace") as fh:
        for lineno, line in enumerate(fh, 1):
            line = line.strip()
            if not line or line.upper().startswith("REM"):
                continue
            head = line.split(None, 1)[0].upper()
            if head == "FILE":
                m = re.match(r'FILE\s+"([^"]+)"|FILE\s+(\S+)', line, re.I)
                if m:
                    source = m.group(1) or m.group(2)
            elif head == "TRACK":
                parts = line.split()
                if len(parts) >= 3:
                    current = CueTrack(int(parts[1]), parts[2].upper(), source)
                    tracks.append(current)
            elif head == "INDEX" and current is not None:
                parts = line.split()
                if len(parts) >= 3:
                    try:
                        current.indexes[int(parts[1])] = msf_to_lba(parts[2])
                    except ValueError:
                        raise ValueError(
                            "%s:%d: malformed INDEX %r" % (path, lineno, parts[2]))
    return tracks


def audio_spans(tracks, image_sectors):
    """[(track number, first sector, sector count)] for audio inside one image.

    A track runs from its INDEX 01 to the start of the next track's area, so a
    pregap belongs to the track that follows it, as Red Book defines it. Only
    tracks sharing the data track's FILE are in here; a cue that names a separate
    file per track is handled as loose audio instead."""
    inside = [t for t in tracks if t.mode == "AUDIO" and t.source == tracks[0].source]
    spans = []
    for i, t in enumerate(inside):
        if t.start is None:
            continue
        nxt = inside[i + 1].first_index if i + 1 < len(inside) else None
        end = nxt if nxt is not None else image_sectors
        if end > t.start:
            spans.append((t.number, t.start, end - t.start))
    return spans


def cue_names(cue, tracks, image):
    """Whether the cue's first FILE really is the image we are reading."""
    if not tracks or not tracks[0].source:
        return False
    named = os.path.join(os.path.dirname(os.path.abspath(cue)), tracks[0].source)
    return os.path.abspath(named) == os.path.abspath(image)


def external_audio(tracks):
    """[(track number, filename)] for a cue that names one file per audio track.

    Only when there are at least two of them. A cue with a single external audio
    file is not describing a disc's track numbering: GOG's lists its one theme as
    TRACK 02 when the music is TADPCM6, so reading the number there renames the
    wrong theme. The engine draws the same line for the same reason."""
    if not tracks:
        return []
    loose = _loose_audio(tracks)
    return loose if len(loose) >= 2 else []


def _loose_audio(tracks):
    if not tracks:
        return []
    return [(t.number, t.source) for t in tracks
            if t.mode == "AUDIO" and t.source and t.source != tracks[0].source]


def single_external(tracks):
    """The filename of a cue's lone external audio track, if that is its shape."""
    loose = _loose_audio(tracks)
    return loose[0][1] if len(loose) == 1 else None


# --- locating things ---------------------------------------------------------
def looks_like_image(path):
    try:
        img = iso_bin.open_image(path)
    except OSError:
        return None
    if img is not None:
        img.f.close()
    return img is not None


def pick_image(directory):
    """The best image in a directory: raw beats cooked, then larger beats smaller.

    Same ranking the engine uses, so both land on the same file when a rip ships
    a .bin and a .iso of the same disc and only the .bin has the audio."""
    best = None
    for name in sorted(os.listdir(directory)):
        path = os.path.join(directory, name)
        if not os.path.isfile(path):
            continue
        if os.path.splitext(name)[1].lower() in CUE_EXTS:
            continue
        try:
            img = iso_bin.open_image(path)
        except OSError:
            continue
        if img is None:
            continue
        raw = img.is_raw
        img.f.close()
        rank = (1 if raw else 0, os.path.getsize(path))
        if best is None or rank > best[0]:
            best = (rank, path)
    return best[1] if best else None


def pick_cue(image_path):
    """The cue beside an image: its own stem first, then any single one there."""
    directory = os.path.dirname(os.path.abspath(image_path))
    stem = os.path.splitext(os.path.basename(image_path))[0]
    for ext in CUE_EXTS:
        candidate = os.path.join(directory, stem + ext)
        if os.path.isfile(candidate):
            return candidate
        candidate = os.path.join(directory, stem + ext.upper())
        if os.path.isfile(candidate):
            return candidate
    found = [os.path.join(directory, n) for n in sorted(os.listdir(directory))
             if os.path.splitext(n)[1].lower() in CUE_EXTS]
    return found[0] if len(found) == 1 else None


def find_asset_root(img, root_lba, root_size):
    """(iso path, lba, size) of the directory holding LBA2.HQR.

    CD masters nest the game under a volume directory (/TWINSEN on the US disc,
    /LBA2 on GOG's), so the marker is what finds it, not a fixed name."""
    for name, lba, size, is_dir in iso_bin.list_dir(img, root_lba, root_size):
        if not is_dir and name.upper() == MARKER:
            return "", root_lba, root_size
    for path, lba, size, is_dir in iso_bin.walk(img, root_lba, root_size):
        if is_dir:
            for name, _, _, child_is_dir in iso_bin.list_dir(img, lba, size):
                if not child_is_dir and name.upper() == MARKER:
                    return path, lba, size
    return None


def music_dir_in(outdir):
    """The music folder to write into: whichever spelling is already there."""
    for spelling in MUSIC_SPELLINGS:
        candidate = os.path.join(outdir, spelling)
        if os.path.isdir(candidate):
            return candidate
    return os.path.join(outdir, MUSIC_DIR)


def themes_without_a_file(outdir, stems):
    """CD track numbers whose theme has no file in the output, in track order."""
    have = set(stems)
    target = music_dir_in(outdir)
    if os.path.isdir(target):
        for name in os.listdir(target):
            have.add(os.path.splitext(name)[0].upper())
    return [n for n in sorted(CD_TRACK_NAMES) if CD_TRACK_NAMES[n].upper() not in have]


def track_number_of(filename):
    """The CD track number a ripped file names, or None.

    Rippers vary (track02.cdda.wav, Track 2.wav, 02.wav); the first run of digits
    is the track number in all of them."""
    m = re.search(r"\d+", os.path.basename(filename))
    return int(m.group(0)) if m else None


# --- writing -----------------------------------------------------------------
class Plan(object):
    def __init__(self, force, dry_run):
        self.force = force
        self.dry_run = dry_run
        self.written = 0
        self.skipped = 0
        self.bytes = 0
        self.stems = set()  # every basename the run put in place, minus extension

    def keep_existing(self, dest, expected_size):
        if self.force or not os.path.exists(dest):
            return False
        if expected_size is None or os.path.getsize(dest) == expected_size:
            self.skipped += 1
            self.stems.add(os.path.splitext(os.path.basename(dest))[0].upper())
            return True
        return False

    def note(self, dest, size, what):
        self.written += 1
        self.stems.add(os.path.splitext(os.path.basename(dest))[0].upper())
        self.bytes += size or 0
        print("  %-40s %10s  %s" % (os.path.basename(dest), size or "?", what))


def write_wav_from_sectors(src, dest, first_sector, sectors, plan):
    """Cut a CD-DA track out of a raw image. Sector bytes are already the PCM the
    WAV wants, so this is a header plus a copy."""
    payload = sectors * RAW_SECTOR
    if plan.keep_existing(dest, payload + 44):
        return
    plan.note(dest, payload + 44,
              "CD track from sector %d (%d sectors)" % (first_sector, sectors))
    if plan.dry_run:
        return
    with open(src, "rb") as fh, wave.open(dest, "wb") as wf:
        wf.setnchannels(CDDA_CHANNELS)
        wf.setsampwidth(CDDA_WIDTH)
        wf.setframerate(CDDA_RATE)
        fh.seek(first_sector * RAW_SECTOR)
        left = payload
        while left > 0:
            chunk = fh.read(min(left, RAW_SECTOR * 512))
            if not chunk:
                break
            wf.writeframes(chunk)
            left -= len(chunk)


def extract_tree(img, iso_root, lba, size, outdir, plan):
    """Write the asset root's subtree out, keeping the on-disc names."""
    for path, clba, csize, is_dir in iso_bin.walk(img, lba, size, iso_root):
        rel = path[len(iso_root):].lstrip("/")
        dest = os.path.join(outdir, *rel.split("/"))
        if is_dir:
            if not plan.dry_run:
                os.makedirs(dest, exist_ok=True)
            continue
        if plan.keep_existing(dest, csize):
            continue
        plan.note(dest, csize, rel)
        if plan.dry_run:
            continue
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        nsec = (csize + iso_bin.USER - 1) // iso_bin.USER
        with open(dest, "wb") as fh:
            done = 0  # sectors
            while done < nsec:
                batch = min(512, nsec - done)
                data = img.read_lba(clba + done, batch)
                fh.write(data[:csize - done * iso_bin.USER])
                done += batch


def copy_ripped(sources, outdir, plan):
    """Name a ripper's output the way the engine asks for it."""
    target = music_dir_in(outdir)
    for number, path in sorted(sources):
        name = CD_TRACK_NAMES.get(number)
        if name is None:
            print("  skipping %s: track %s carries no music on this disc"
                  % (os.path.basename(path), number))
            continue
        # Keep the source format in the name. The engine asks for NAME.WAV and
        # falls back to NAME.ogg, so a renamed OGG only loads under its own
        # extension.
        ext = os.path.splitext(path)[1]
        dest = os.path.join(target, name + (".WAV" if ext.lower() == ".wav" else ext))
        size = os.path.getsize(path)
        if plan.keep_existing(dest, size):
            continue
        plan.note(dest, size, "track %02d from %s" % (number, os.path.basename(path)))
        if plan.dry_run:
            continue
        os.makedirs(target, exist_ok=True)
        shutil.copyfile(path, dest)


def ripped_wavs_in(directory):
    """[(track number, path)] for the WAVs a ripper left in a folder."""
    out = []
    for name in sorted(os.listdir(directory)):
        path = os.path.join(directory, name)
        if not os.path.isfile(path) or not name.lower().endswith(".wav"):
            continue
        number = track_number_of(name)
        if number is not None:
            out.append((number, path))
    return out


# --- self-test ---------------------------------------------------------------
# The cue rules are where this can be wrong without anything failing: get a span
# or a track number off and every theme still plays, just not its own. `--selftest`
# pins them against fixture cues, no disc needed. The naming table has its own
# guard on the engine side, in tests/cdtracks.
RETAIL_CUE = """FILE "TWINSEN.bin" BINARY
  TRACK 01 MODE1/2352
    INDEX 01 00:00:00
  TRACK 02 AUDIO
    INDEX 01 46:04:68
  TRACK 03 AUDIO
    INDEX 00 49:50:60
    INDEX 01 49:50:63
"""

GOG_CUE = """FILE "LBA2.GOG" BINARY
  TRACK 01 MODE1/2352
    INDEX 01 00:00:00
FILE "LBA2.OGG" MP3
  TRACK 02 AUDIO
      INDEX 01 00:00:00
"""

PER_TRACK_CUE = """FILE "data.bin" BINARY
  TRACK 01 MODE1/2352
    INDEX 01 00:00:00
FILE "track02.wav" WAVE
  TRACK 02 AUDIO
    INDEX 01 00:00:00
FILE "track03.wav" WAVE
  TRACK 03 AUDIO
    INDEX 01 00:00:00
"""


def selftest():
    import tempfile

    failures = []
    checks = [0]

    def check(name, got, want):
        checks[0] += 1
        if got != want:
            failures.append("%s: got %r, want %r" % (name, got, want))

    def cue_of(text):
        fh = tempfile.NamedTemporaryFile("w", suffix=".cue", delete=False)
        fh.write(text)
        fh.close()
        return fh.name

    check("msf", msf_to_lba("46:04:68"), 207368)
    check("msf zero", msf_to_lba("00:00:00"), 0)

    retail = parse_cue(cue_of(RETAIL_CUE))
    check("retail tracks", len(retail), 3)
    # Track 2 ends where track 3's pregap starts, not where its INDEX 01 does:
    # an INDEX 00 belongs to the track that follows it.
    check("retail spans", audio_spans(retail, 300000),
          [(2, 207368, 224310 - 207368), (3, 224313, 300000 - 224313)])
    check("retail has no loose audio", external_audio(retail), [])

    gog = parse_cue(cue_of(GOG_CUE))
    # The trap: one external file whose cue number is not a CD track number.
    check("gog is not mapped by number", external_audio(gog), [])
    check("gog names its file", single_external(gog), "LBA2.OGG")
    check("gog has nothing inside the image", audio_spans(gog, 300000), [])

    per_track = parse_cue(cue_of(PER_TRACK_CUE))
    check("per-track cue maps by number", external_audio(per_track),
          [(2, "track02.wav"), (3, "track03.wav")])

    check("track number from cdparanoia", track_number_of("track02.cdda.wav"), 2)
    check("track number from a space", track_number_of("Track 7.wav"), 7)
    check("track number from none", track_number_of("purple.wav"), None)

    for line in failures:
        print("FAIL %s" % line)
    print("%d check(s), %d failed" % (checks[0], len(failures)))
    return 1 if failures else 0


# --- driver ------------------------------------------------------------------
def resolve_source(source):
    """(image path or None, cue path or None, ripped WAVs or [])."""
    if os.path.isdir(source):
        wavs = ripped_wavs_in(source)
        image = pick_image(source)
        if image is None:
            return None, None, wavs
        return image, pick_cue(image), []
    ext = os.path.splitext(source)[1].lower()
    if ext in CUE_EXTS:
        tracks = parse_cue(source)
        if not tracks or not tracks[0].source:
            raise SystemExit("%s names no FILE" % source)
        image = os.path.join(os.path.dirname(os.path.abspath(source)), tracks[0].source)
        if not os.path.isfile(image):
            raise SystemExit("%s names %r, which is not beside it"
                             % (source, tracks[0].source))
        return image, source, []
    if ext in IMAGE_EXTS or looks_like_image(source):
        return source, pick_cue(source), []
    raise SystemExit("%s is not a cue, an image, or a directory holding either" % source)


def main():
    ap = argparse.ArgumentParser(
        description="Extract a playable game-data folder from a CD, themes included.")
    ap.add_argument("source", nargs="?",
                    help="a .cue, a disc image, or a directory holding "
                         "either (or a directory of ripped track WAVs)")
    ap.add_argument("outdir", nargs="?", help="where to write the game data")
    ap.add_argument("--tracks-from", metavar="DIR",
                    help="take the CD audio from a folder of ripped WAVs instead of "
                         "from the image (for containers whose audio is unreachable)")
    ap.add_argument("--force", action="store_true",
                    help="overwrite files already at the destination")
    ap.add_argument("--dry-run", action="store_true", help="say what would be written")
    ap.add_argument("--selftest", action="store_true",
                    help="check the cue rules against fixtures; needs no disc")
    a = ap.parse_args()

    if a.selftest:
        return selftest()
    if not a.source or not a.outdir:
        ap.error("SOURCE and OUTDIR are both required")

    image, cue, ripped = resolve_source(a.source)
    if a.tracks_from:
        ripped = ripped_wavs_in(a.tracks_from)
        if not ripped:
            raise SystemExit("no track WAVs in %s" % a.tracks_from)

    plan = Plan(a.force, a.dry_run)
    if not a.dry_run:
        os.makedirs(a.outdir, exist_ok=True)

    if image is None and not ripped:
        raise SystemExit("%s holds no disc image and no ripped WAVs" % a.source)

    if image is not None:
        print("Image:      %s" % image)
        img = iso_bin.open_image(image)
        if img is None:
            raise SystemExit("%s is not an ISO9660 image" % image)
        pvd = iso_bin.get_pvd(img)
        rlba, rsize = iso_bin.root_record(pvd)
        found = find_asset_root(img, rlba, rsize)
        if found is None:
            raise SystemExit("no %s inside %s, so this is not LBA2 game data"
                             % (MARKER, image))
        iso_root, lba, size = found
        raw = img.is_raw
        print("Asset root: %s" % (iso_root or "/"))
        print("Data:")
        extract_tree(img, iso_root, lba, size, a.outdir, plan)
        img.f.close()

        if cue and not ripped:
            tracks = parse_cue(cue)
            print("Cue:        %s (%d tracks)" % (cue, len(tracks)))
            loose = external_audio(tracks)
            if loose:
                base = os.path.dirname(os.path.abspath(cue))
                print("Music:      one file per track, named by the cue")
                copy_ripped(
                    [(n, os.path.join(base, f)) for n, f in loose
                     if os.path.isfile(os.path.join(base, f))], a.outdir, plan)
            elif not cue_names(cue, tracks, image):
                # A cue's INDEX addresses belong to the file it names. Applying
                # them to a different one lands mid-sector and plays as
                # full-scale noise, so "probably the same disc" is not enough.
                print("Music:      %s describes %r, not this image"
                      % (os.path.basename(cue), tracks[0].source))
            elif not raw:
                print("Music:      image is cooked (2048), so it carries no CD audio")
            else:
                sectors = os.path.getsize(image) // RAW_SECTOR
                spans = audio_spans(tracks, sectors)
                if spans:
                    print("Music:      matched by CD track number")
                    target = music_dir_in(a.outdir)
                    if not a.dry_run:
                        os.makedirs(target, exist_ok=True)
                    for number, first, count in spans:
                        name = CD_TRACK_NAMES.get(number)
                        if name is None:
                            print("  track %02d carries no music on this disc" % number)
                            continue
                        write_wav_from_sectors(
                            image, os.path.join(target, name + ".WAV"),
                            first, count, plan)
                elif single_external(tracks):
                    print("Music:      one external audio file (%s), whose cue number "
                          "is not a CD track number" % single_external(tracks))
                    print("            The engine plays it as it stands; leave it "
                          "where it is rather than renaming it here.")
                else:
                    print("Music:      the cue lists no audio tracks in this image")
        elif not cue and not ripped:
            print("Music:      no cue beside the image, so the themes stay silent")

    if ripped:
        print("Music:      from %s" % (a.tracks_from or a.source))
        copy_ripped(ripped, a.outdir, plan)

    # What matters is whether each theme ended up with a file, not how it got
    # there: on GOG the same music sits inside the data track, so counting only
    # the CD tracks we cut would report every theme missing from a folder that
    # has them all.
    missing = themes_without_a_file(a.outdir, plan.stems)
    if missing:
        print("No file for: %s" % ", ".join(
            "%s (track %02d)" % (CD_TRACK_NAMES[n], n) for n in missing))
    print("%d file(s) written, %d already there, %.1f MB"
          % (plan.written, plan.skipped, plan.bytes / 1048576.0))
    if not a.dry_run and image is not None:
        print("Play it with: lba2cc --game-dir %s" % os.path.abspath(a.outdir))


if __name__ == "__main__":
    sys.exit(main() or 0)
