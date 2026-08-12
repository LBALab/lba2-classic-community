#!/usr/bin/env python3
"""Extract a playable game-data folder out of a CD, including the Red Book themes.

The engine already mounts a bin+cue rip and plays everything off it, so for that
shape this script is optional. It exists for the cases the engine cannot serve:

  * A physical disc. Data and jingles play straight off a mounted CD, but the six
    themes are Red Book audio and reading audio from a drive is a deliberate
    non-goal for the engine (see docs/DISC_IMAGE_SOURCE.md). `--from-drive` reads
    them here instead, where a dependency-free best effort is allowed to be one.
  * A container the engine mounts data-only: an MDX (encrypted track table), NRG,
    CCD. Convert or rip the audio separately, then use `--tracks-from`.
  * Wanting loose files on disk for modding or inspection.

What only this repo can tell you is the naming, and it depends on the disc. What
holds everywhere is positional: a disc's audio tracks, in order, are the themes
it did not ship as files, in ListJingle order. The Brazilian Activision disc
presses seven as tracks 2..8, both European pressings press one; all are named
that way. See themes_for_tracks.

CD_TRACK_NAMES below is the exception, and only for one artifact: the widely
circulated TWINSEN.mdx, which carries six audio tracks starting at TADPCM2. It
disagrees with the TrackCDUS table in the executable on its own image, which
expects seven starting at TADPCM1, so that rip has most likely lost a track and
renumbered. The table names its six for what they actually hold. Kept in step
with LIB386/SYSTEM/CDTRACKS.CPP by tests/cdtracks; see the header there before
changing it.

Usage:
  disc_extract.py SOURCE OUTDIR [--from-drive [DEV]] [--tracks-from DIR]
                                [--force] [--dry-run]

  SOURCE  a .cue, a disc image, a directory holding either, a mounted disc, or a
          directory of already-ripped track WAVs.

Examples:
  disc_extract.py TWINSEN.cue ~/lba2-data
  disc_extract.py /media/cdrom/TWINSEN ~/lba2-data --from-drive /dev/sr0
  disc_extract.py G:\\TWINSEN C:\\lba2-data --from-drive G
  disc_extract.py /media/cdrom ~/lba2-data --tracks-from ~/ripped
  disc_extract.py ~/ripped ~/lba2-data           # audio only, into an existing tree

Then: lba2cc --game-dir ~/lba2-data

Python 3 stdlib only. Idempotent: a file already there at the expected size is
left alone unless you pass --force. `--selftest` checks the cue rules against
fixtures and needs no disc.
"""
import argparse
import array
import os
import re
import shutil
import struct
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

# Every theme the game asks for, in SOURCES/MUSIC.CPP's ListJingle order. The
# order is load-bearing, not cosmetic: the audio tracks of a disc map onto the
# themes it did not ship as files, in this order, and JADPCM01 sits between
# TADPCM5 and TADPCM6 here rather than with the other jingles. Sorting the names
# instead swaps those two on any disc that presses both. Mirrors THEME_NAMES in
# LIB386/SYSTEM/CDTRACKS.CPP; tests/cdtracks reads both.
ALL_THEMES = (["TADPCM%d" % n for n in range(1, 6)] + ["JADPCM01", "TADPCM6"] +
              ["JADPCM%02d" % n for n in range(2, 19)] + ["LOGADPCM"])

RAW_SECTOR = 2352  # a CD-DA sector: 588 stereo frames of 16-bit PCM
CDDA_RATE = 44100
CDDA_CHANNELS = 2
CDDA_WIDTH = 2

# A cue's FILE type says how to read what it names. BINARY and MOTOROLA are raw
# sectors, MOTOROLA declaring the big-endian order Red Book uses on the disc
# itself; WAVE, MP3 and AIFF are decodable audio files. LIB386/SYSTEM/CUE.CPP
# draws the same line, and the two must agree or the engine and this script will
# read the same cue differently.
RAW_FILE_TYPES = ("BINARY", "MOTOROLA")

# What the engine resolves for a music request: NAME.WAV, then .ogg/.OGG. A file
# copied under any other extension is one nothing will ever load.
ENGINE_AUDIO_EXTS = (".wav", ".ogg")

IMAGE_EXTS = (".bin", ".iso", ".img", ".gog", ".dot", ".mdf", ".raw")
CUE_EXTS = (".cue", ".dat", ".toc")


# --- cue sheet ---------------------------------------------------------------
class CueTrack(object):
    def __init__(self, number, mode, source, source_type="BINARY"):
        self.number = number
        self.mode = mode  # AUDIO, MODE1/2352, ...
        self.source = source  # the FILE this track lives in
        # BINARY means raw sectors, whether that file is the whole disc or just
        # this track. WAVE/MP3/AIFF mean a decodable audio file.
        self.source_type = source_type
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
    source_type = "BINARY"
    current = None
    with open(path, "r", errors="replace") as fh:
        for lineno, line in enumerate(fh, 1):
            line = line.strip()
            if not line or line.upper().startswith("REM"):
                continue
            head = line.split(None, 1)[0].upper()
            if head == "FILE":
                m = re.match(r'FILE\s+"([^"]+)"\s*(\S+)?|FILE\s+(\S+)\s*(\S+)?', line, re.I)
                if m:
                    source = m.group(1) or m.group(3)
                    source_type = (m.group(2) or m.group(4) or "BINARY").upper()
            elif head == "TRACK":
                parts = line.split()
                if len(parts) >= 3:
                    current = CueTrack(int(parts[1]), parts[2].upper(), source, source_type)
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


def themes_without_files(outdir, stems):
    """The themes this tree has no file for, in ListJingle order."""
    have = set(stems)
    target = music_dir_in(outdir)
    if os.path.isdir(target):
        for name in os.listdir(target):
            have.add(os.path.splitext(name)[0].upper())
    return [t for t in ALL_THEMES if t.upper() not in have]


def themes_for_tracks(spans, outdir, plan):
    """Name every audio track, in one pass. Its number names nothing on its own.

    The rule that holds across every pressing is positional. A disc's audio
    tracks, in order, are the themes it did not ship as files, in ListJingle
    order. The Brazilian Activision disc presses seven as tracks 2..8, the US
    Activision disc presses six as tracks 2..7 with a different numbering, and
    both European pressings press one; only the position is common to all.

    It engages when the counts agree, which is what makes the correspondence
    unambiguous. The US disc is missing seven and presses six, since TADPCM1 is
    not on it at all, so it falls through to the table that was measured on it.

    Named all at once, and this is not a stylistic choice: every track written
    adds a file, so asking per track would shorten the missing list as it went
    and slide every later track onto the wrong theme."""
    missing = themes_without_files(outdir, plan.stems)
    if len(missing) == len(spans):
        return missing, "position"
    return [CD_TRACK_NAMES.get(number) for number, _, _ in spans], "the US track table"


def themes_without_a_file(outdir, stems):
    """Every theme with no file in the output, in ListJingle order.

    All of them, not just the ones the US table happens to name. A rip that lost
    an audio track leaves a theme with nowhere to come from, and saying which one
    is the difference between a silent theme and a diagnosis: the widely
    circulated TWINSEN.mdx is short its first audio track, so extracting it
    yields no TADPCM1 and nothing used to mention it."""
    return themes_without_files(outdir, stems)


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


def open_wav(dest):
    wf = wave.open(dest, "wb")
    wf.setnchannels(CDDA_CHANNELS)
    wf.setsampwidth(CDDA_WIDTH)
    wf.setframerate(CDDA_RATE)
    return wf


def write_wav_from_drive(drive, dest, first_sector, sectors, plan):
    """Rip a track off the disc in the chunks the transport allows.

    A failed chunk is retried before giving up, and the count comes back so a
    disc that is struggling says so rather than producing quiet damage."""
    payload = sectors * RAW_SECTOR
    if plan.keep_existing(dest, payload + 44):
        return 0
    plan.note(dest, payload + 44,
              "CD track from LBA %d (%d sectors)" % (first_sector, sectors))
    if plan.dry_run:
        return 0
    retries = 0
    swap = None
    with open_wav(dest) as wf:
        done = 0
        while done < sectors:
            count = min(MAX_SECTORS_PER_READ, sectors - done)
            for attempt in range(3):
                try:
                    pcm = drive.read_audio(first_sector + done, count)
                    if swap is None:
                        swap = needs_byte_swap(pcm)
                        if swap:
                            print("            (big-endian audio, byte-swapping it)")
                    wf.writeframes(byte_swap(pcm) if swap else pcm)
                    break
                except DriveError:
                    retries += 1
                    if attempt == 2:
                        raise
            done += count
    return retries


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
    with open(src, "rb") as fh, open_wav(dest) as wf:
        fh.seek(first_sector * RAW_SECTOR)
        # Decide the byte order once, from the head of the track, then hold it:
        # a decision taken per chunk could flip on a quiet passage.
        probe = fh.read(min(payload, RAW_SECTOR * 64))
        swap = needs_byte_swap(probe)
        if swap:
            print("            (big-endian audio, byte-swapping it)")
        fh.seek(first_sector * RAW_SECTOR)
        left = payload
        while left > 0:
            chunk = fh.read(min(left, RAW_SECTOR * 512))
            if not chunk:
                break
            wf.writeframes(byte_swap(chunk) if swap else chunk)
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


def holds_marker(directory):
    """Whether a directory is game data, by the marker discovery uses."""
    try:
        return any(n.upper() == MARKER for n in os.listdir(directory))
    except OSError:
        return False


def copy_data_tree(srcdir, outdir, plan):
    """Copy an already-readable game-data tree, which is what a mounted disc is."""
    for root, dirs, files in os.walk(srcdir):
        dirs.sort()
        rel = os.path.relpath(root, srcdir)
        target = outdir if rel == "." else os.path.join(outdir, rel)
        for name in sorted(files):
            src = os.path.join(root, name)
            dest = os.path.join(target, name)
            try:
                size = os.path.getsize(src)
            except OSError:
                continue
            if plan.keep_existing(dest, size):
                continue
            plan.note(dest, size, os.path.join(rel, name) if rel != "." else name)
            if plan.dry_run:
                continue
            os.makedirs(target, exist_ok=True)
            shutil.copyfile(src, dest)


def copy_ripped(sources, outdir, plan, force_name=None):
    """Name a ripper's output the way the engine asks for it.

    `force_name` overrides the track-number lookup, for a caller that has already
    worked out which theme this is."""
    target = music_dir_in(outdir)
    for number, path in sorted(sources):
        name = force_name if force_name else CD_TRACK_NAMES.get(number)
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


# --- reading a drive ---------------------------------------------------------
# Audio sectors carry no error correction, so a scratched disc gives clicks that
# cdparanoia would re-read and interpolate away and this will not. That is the
# trade for having no dependency; --from-drive reports its retry count so a bad
# read is visible, and a damaged disc is a reason to rip with cdparanoia and come
# back through --tracks-from.
MAX_SECTORS_PER_READ = 27  # 27 x 2352 = 63504, just under the 64 KB ATAPI ceiling
LEADOUT_TRACK = 0xAA
MSF_OFFSET = 150  # MSF counts from 00:02:00, so LBA 0 is at frame 150


class DriveError(Exception):
    pass


class TocTrack(object):
    def __init__(self, number, lba, is_data):
        self.number = number
        self.lba = lba
        self.is_data = is_data

    def __repr__(self):
        return "TocTrack(%d, %d, %s)" % (self.number, self.lba, self.is_data)


def parse_toc_buffer(buf):
    """Parse a Windows CDROM_TOC into TocTracks, lead-out included.

    Header is Length[2] big-endian, FirstTrack, LastTrack; then 8-byte entries of
    Reserved, Adr/Ctrl, TrackNumber, Reserved, then a 4-byte address whose last
    three bytes are M, S, F."""
    if len(buf) < 4:
        raise DriveError("table of contents too short (%d bytes)" % len(buf))
    tracks = []
    for o in range(4, len(buf) - 7, 8):
        number = buf[o + 2]
        if number == 0:
            break
        ctrl = buf[o + 1] & 0x0F
        m, s, f = buf[o + 5], buf[o + 6], buf[o + 7]
        lba = (m * 60 + s) * 75 + f - MSF_OFFSET
        tracks.append(TocTrack(number, lba, bool(ctrl & 0x04)))
        if number == LEADOUT_TRACK:
            break
    if not tracks:
        raise DriveError("table of contents lists no tracks")
    return tracks


def audio_tracks_from_toc(toc):
    """[(track number, first sector, sector count)] for the audio a disc carries.

    Each track runs to the start of the next entry, and the lead-out is what
    bounds the last one, which is why it has to be in the table."""
    spans = []
    for i, t in enumerate(toc):
        if t.is_data or t.number == LEADOUT_TRACK or i + 1 >= len(toc):
            continue
        count = toc[i + 1].lba - t.lba
        if count > 0:
            spans.append((t.number, t.lba, count))
    return spans


class WindowsDrive(object):
    """CD access through DeviceIoControl. Verified against a real mixed-mode disc."""

    IOCTL_CDROM_READ_TOC = 0x24000
    IOCTL_CDROM_RAW_READ = 0x2403E
    TRACK_MODE_CDDA = 2

    def __init__(self, device):
        import ctypes
        from ctypes import wintypes

        self.ctypes = ctypes
        self.wintypes = wintypes
        letter = device.strip().rstrip(":").replace("\\", "").replace(".", "")
        self.path = "\\\\.\\%s:" % letter[-1:].upper()
        self.name = self.path

    def _open(self):
        ctypes, wintypes = self.ctypes, self.wintypes
        create = ctypes.windll.kernel32.CreateFileW
        create.restype = wintypes.HANDLE  # must not truncate the handle on win64
        create.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                           ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD,
                           wintypes.HANDLE]
        invalid = ctypes.c_void_p(-1).value
        for access in (0x80000000, 0):  # GENERIC_READ, then none: many IOCTLs allow it
            handle = create(self.path, access, 0x1 | 0x2, None, 3, 0, None)
            if handle is not None and handle != invalid:
                return handle
        raise DriveError("cannot open %s (error %d)"
                         % (self.path, self.ctypes.get_last_error()))

    def _ioctl(self, code, in_buf, out_size):
        ctypes, wintypes = self.ctypes, self.wintypes
        handle = self._open()
        try:
            control = ctypes.windll.kernel32.DeviceIoControl
            control.argtypes = [wintypes.HANDLE, wintypes.DWORD, ctypes.c_void_p,
                                wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD,
                                ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
            out = ctypes.create_string_buffer(out_size)
            returned = wintypes.DWORD(0)
            ok = control(handle, code,
                         in_buf, len(in_buf) if in_buf else 0,
                         out, out_size, ctypes.byref(returned), None)
            if not ok:
                raise DriveError("ioctl 0x%X on %s failed (error %d)"
                                 % (code, self.path, ctypes.GetLastError()))
            return out.raw[:returned.value]
        finally:
            ctypes.windll.kernel32.CloseHandle(handle)

    def read_toc(self):
        return parse_toc_buffer(self._ioctl(self.IOCTL_CDROM_READ_TOC, None, 2048))

    def read_audio(self, lba, sectors):
        import struct as _struct
        # RAW_READ_INFO. DiskOffset is a byte offset of lba * 2048 even though
        # CDDA hands back 2352-byte sectors, which is this IOCTL's own quirk.
        info = _struct.pack("<qII", lba * 2048, sectors, self.TRACK_MODE_CDDA)
        data = self._ioctl(self.IOCTL_CDROM_RAW_READ, info, sectors * RAW_SECTOR)
        if len(data) != sectors * RAW_SECTOR:
            raise DriveError("short read at LBA %d: %d bytes for %d sectors"
                             % (lba, len(data), sectors))
        return data


class LinuxDrive(object):
    """CD access through the cdrom ioctls.

    The struct layouts are taken from <linux/cdrom.h> and let ctypes do the
    alignment, but nothing here has been run against a device: this box has no
    optical drive. If it fails, rip with cdparanoia and use --tracks-from, which
    is the tested path."""

    CDROMREADTOCHDR = 0x5305
    CDROMREADTOCENTRY = 0x5306
    CDROMREADAUDIO = 0x530E
    CDROM_LBA = 1

    def __init__(self, device):
        import ctypes
        import fcntl

        self.ctypes = ctypes
        self.fcntl = fcntl
        self.name = device
        self.path = device

        class Addr(ctypes.Union):
            _fields_ = [("msf", ctypes.c_uint8 * 3), ("lba", ctypes.c_int)]

        class TocHeader(ctypes.Structure):
            _fields_ = [("trk0", ctypes.c_uint8), ("trk1", ctypes.c_uint8)]

        class TocEntry(ctypes.Structure):
            _fields_ = [("track", ctypes.c_uint8),
                        ("adr", ctypes.c_uint8, 4),
                        ("ctrl", ctypes.c_uint8, 4),
                        ("format", ctypes.c_uint8),
                        ("addr", Addr),
                        ("datamode", ctypes.c_uint8)]

        class ReadAudio(ctypes.Structure):
            _fields_ = [("addr", Addr),
                        ("addr_format", ctypes.c_uint8),
                        ("nframes", ctypes.c_int),
                        ("buf", ctypes.POINTER(ctypes.c_uint8))]

        self.TocHeader, self.TocEntry, self.ReadAudio = TocHeader, TocEntry, ReadAudio

    def _fd(self):
        try:  # O_NONBLOCK so an empty or spinning-up drive fails rather than hangs
            return os.open(self.path, os.O_RDONLY | os.O_NONBLOCK)
        except OSError as exc:
            raise DriveError("cannot open %s: %s" % (self.path, exc))

    def read_toc(self):
        fd = self._fd()
        try:
            header = self.TocHeader()
            self.fcntl.ioctl(fd, self.CDROMREADTOCHDR, header)
            tracks = []
            numbers = list(range(header.trk0, header.trk1 + 1)) + [LEADOUT_TRACK]
            for number in numbers:
                entry = self.TocEntry()
                entry.track = number
                entry.format = self.CDROM_LBA
                self.fcntl.ioctl(fd, self.CDROMREADTOCENTRY, entry)
                tracks.append(TocTrack(number, entry.addr.lba, bool(entry.ctrl & 0x04)))
            return tracks
        except OSError as exc:
            raise DriveError("reading the table of contents of %s: %s" % (self.path, exc))
        finally:
            os.close(fd)

    def read_audio(self, lba, sectors):
        ctypes = self.ctypes
        fd = self._fd()
        try:
            buf = (ctypes.c_uint8 * (sectors * RAW_SECTOR))()
            request = self.ReadAudio()
            request.addr.lba = lba
            request.addr_format = self.CDROM_LBA
            request.nframes = sectors
            request.buf = ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8))
            self.fcntl.ioctl(fd, self.CDROMREADAUDIO, request)
            return bytes(bytearray(buf))
        except OSError as exc:
            raise DriveError("reading audio at LBA %d from %s: %s" % (lba, self.path, exc))
        finally:
            os.close(fd)


# On a mixed-mode disc the data track's run-out bleeds into the front of the first
# audio track, and a drive hands it back as audio: five sectors of full-scale noise
# before the music on the US disc. A cue can say "start later" and ours does; a TOC
# cannot, so a drive rip has to find the join itself.
#
# What separates them is not loudness but distribution. Data read as audio is
# uniform noise, so its mean sample magnitude sits near half of full scale, where the
# music that follows measures 0.005 and even a brickwalled master stays under about
# 0.35 because music has crest factor and noise does not. The gate is set at 0.40 to
# sit clear of both, the skip is capped at one transport read, and a trim is printed
# rather than done quietly, because the one signal this cannot tell from data is a
# sustained full-scale tone (a sine averages 0.637) and that deserves to be visible.
RUNOUT_GATE = 0.40  # a first sector this loud on average is data, not music
RUNOUT_TAIL = 0.05  # keep skipping while the noise fades out
RUNOUT_MAX_SECTORS = MAX_SECTORS_PER_READ  # one transport read, ~360 ms, is plenty


def mean_magnitude(sector, big_endian=False):
    """Mean absolute sample value of one CD-DA sector, as a fraction of full scale."""
    fmt = "%s%dh" % (">" if big_endian else "<", len(sector) // 2)
    samples = struct.unpack(fmt, sector)
    if not samples:
        return 0.0
    return sum(abs(v) for v in samples) / float(len(samples) * 32768)


# Red Book stores samples MSB-first and a ripper is meant to swap them into the
# little-endian order every player expects. Some do not: the European MODE2 rip
# on the Internet Archive carries raw disc order, and played as it stands it is
# full-scale noise. A cue has no field for this, so it is measured, by the same
# statistic that finds run-out: bytes in the wrong order are uniform noise
# averaging half of full scale, and music averages a few per cent.
def needs_byte_swap(pcm):
    """Whether this raw CD audio reads as noise one way round and music the other."""
    if len(pcm) < 4:
        return False
    le = mean_magnitude(pcm)
    be = mean_magnitude(pcm, big_endian=True)
    if le < 0.25:
        return False  # already quiet enough to be music, or too quiet to judge
    return be * 2 < le


def byte_swap(pcm):
    a = array.array("h")
    a.frombytes(pcm[:len(pcm) // 2 * 2])
    a.byteswap()
    return a.tobytes() + pcm[len(pcm) // 2 * 2:]


def runout_sectors(head):
    """How many leading sectors of `head` are data run-out rather than music."""
    if not head or mean_magnitude(head[0]) < RUNOUT_GATE:
        return 0
    skipped = 0
    for sector in head[:RUNOUT_MAX_SECTORS]:
        if mean_magnitude(sector) < RUNOUT_TAIL:
            break
        skipped += 1
    return skipped


def trim_leading_runout(drive, first_lba, sectors):
    """(first sector, count) with any data run-out trimmed off the head."""
    probe = min(RUNOUT_MAX_SECTORS, sectors)
    if probe <= 0:
        return first_lba, sectors
    data = drive.read_audio(first_lba, probe)
    head = [data[i * RAW_SECTOR:(i + 1) * RAW_SECTOR] for i in range(probe)]
    skip = runout_sectors(head)
    return first_lba + skip, sectors - skip


def default_device():
    """The drive to use when --from-drive names none."""
    if sys.platform == "win32":
        import ctypes
        mask = ctypes.windll.kernel32.GetLogicalDrives()
        for i in range(26):
            if not mask & (1 << i):
                continue
            root = "%s:\\" % chr(ord("A") + i)
            if ctypes.windll.kernel32.GetDriveTypeW(root) == 5:  # DRIVE_CDROM
                return chr(ord("A") + i)
        raise DriveError("no CD drive found")
    for path in ("/dev/cdrom", "/dev/sr0", "/dev/sr1"):
        if os.path.exists(path):
            return path
    raise DriveError("no CD drive found (looked for /dev/cdrom and /dev/sr0)")


def open_drive(device):
    if not device:
        device = default_device()
    if sys.platform == "win32":
        return WindowsDrive(device)
    if sys.platform.startswith("linux"):
        return LinuxDrive(device)
    raise DriveError(
        "reading a drive is not implemented on %s. Rip the audio with a tool your "
        "platform has, then pass --tracks-from." % sys.platform)


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

# Every FILE type a cue can name, since how each is read differs and the engine
# has to draw the same line. MOTOROLA is the trap: raw sectors like BINARY, but
# big-endian, so treating it as a decodable file copies noise under a name
# nothing loads.
FILE_TYPES_CUE = """FILE "data.bin" BINARY
  TRACK 01 MODE1/2352
    INDEX 01 00:00:00
FILE "t2.bin" MOTOROLA
  TRACK 02 AUDIO
    INDEX 01 00:00:00
FILE "t3.wav" WAVE
  TRACK 03 AUDIO
    INDEX 01 00:00:00
FILE "t4.ogg" MP3
  TRACK 04 AUDIO
    INDEX 01 00:00:00
FILE "t5.aiff" AIFF
  TRACK 05 AUDIO
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

# The US disc's own CDROM_TOC, read off the drive with IOCTL_CDROM_READ_TOC. One
# data track and six audio, lead-out at 292864. Kept verbatim so the parse is
# pinned against a real disc rather than a table written to match the parser.
RETAIL_TOC_HEX = (
    "00420107"
    "0014010000000200"
    "0010020000 2E083F"
    "0010030000 31363F"
    "0010040000 35061A"
    "0010050000 38271A"
    "0010060000 3C151A"
    "0010070000 3D0F1A"
    "0010AA0000 410640"
).replace(" ", "")


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

    # FILE types. Raw-versus-decodable is what picks how a track is read, and
    # MOTOROLA has to land with BINARY or its audio comes out as noise.
    types = parse_cue(cue_of(FILE_TYPES_CUE))
    check("file types parsed", [t.source_type for t in types],
          ["BINARY", "MOTOROLA", "WAVE", "MP3", "AIFF"])
    check("raw types", [t.source_type in RAW_FILE_TYPES for t in types],
          [True, True, False, False, False])
    # Where each audio track can be read from. The trap is a track inside an
    # image that cannot carry audio: deciding on the FILE type alone sends the
    # image itself down the raw-sector path and cuts its filesystem out as PCM.
    cooked = parse_cue(cue_of(RETAIL_CUE))          # audio tracks inside one BINARY
    check("in-image track needs a usable image",
          [audio_source_kind(t, cooked, False) for t in cooked if t.mode == "AUDIO"],
          [None, None])
    check("in-image track with a usable image",
          [audio_source_kind(t, cooked, True) for t in cooked if t.mode == "AUDIO"],
          ["image", "image"])
    mixed = parse_cue(cue_of(FILE_TYPES_CUE))       # MOTOROLA, WAVE, MP3, AIFF
    check("separate files by type",
          [audio_source_kind(t, mixed, True) for t in mixed if t.mode == "AUDIO"],
          ["raw", "file", "file", "file"])
    # A separate file does not care whether the image is usable: it is not the image.
    check("separate files ignore the image",
          [audio_source_kind(t, mixed, False) for t in mixed if t.mode == "AUDIO"],
          ["raw", "file", "file", "file"])

    check("engine can decode wav and ogg",
          [e in ENGINE_AUDIO_EXTS for e in (".wav", ".ogg", ".aiff", ".bin")],
          [True, True, False, False])

    check("track number from cdparanoia", track_number_of("track02.cdda.wav"), 2)
    check("track number from a space", track_number_of("Track 7.wav"), 7)
    check("track number from none", track_number_of("purple.wav"), None)

    # The disc's own table of contents, against the addresses the corrected cue
    # records. Every track sits 150 frames above its offset in a BIN that dropped
    # the data-track run-out, which is what makes the two views agree.
    toc = parse_toc_buffer(bytearray.fromhex(RETAIL_TOC_HEX))
    check("toc track count", len(toc), 8)
    check("toc data track", (toc[0].number, toc[0].lba, toc[0].is_data), (1, 0, True))
    check("toc audio numbering", [t.number for t in toc[1:-1]], [2, 3, 4, 5, 6, 7])
    check("toc all audio", [t.is_data for t in toc[1:]], [False] * 7)
    check("toc leadout", (toc[-1].number, toc[-1].lba), (LEADOUT_TRACK, 292864))
    check("toc track 2", toc[1].lba, 207368 + MSF_OFFSET - 5)  # the 5 run-out sectors
    check("toc track 3", toc[2].lba, 224313 + MSF_OFFSET)
    check("toc spans", audio_tracks_from_toc(toc),
          [(2, 207513, 16950), (3, 224463, 14363), (4, 238826, 15975),
           (5, 254801, 16650), (6, 271451, 4050), (7, 275501, 17363)])
    # Every audio track on this disc must be one the naming table knows, or a
    # theme would come off the disc with nowhere to go.
    check("toc tracks are all named",
          [n for n, _, _ in audio_tracks_from_toc(toc) if n not in CD_TRACK_NAMES], [])

    # The run-out trim, on sectors built to look like what it has to tell apart.
    # A plain LCG so the "noise" is the same on every run and every machine.
    def noise_sector():
        out = bytearray()
        seed = 12345
        for _ in range(RAW_SECTOR // 2):
            seed = (1103515245 * seed + 12345) & 0x7FFFFFFF
            out += struct.pack("<h", (seed >> 8) % 65536 - 32768)
        return bytes(out)

    def level_sector(amplitude):
        return struct.pack("<h", amplitude) * (RAW_SECTOR // 2)

    def wave_sector(amplitude):
        """A triangle wave: a real waveform, with the crest factor music has."""
        out = bytearray()
        v, step = 0, max(1, amplitude // 64)
        for _ in range(RAW_SECTOR // 2):
            v += step
            if v > amplitude or v < -amplitude:
                step = -step
            out += struct.pack("<h", v)
        return bytes(out)

    silence = level_sector(0)
    quiet = level_sector(160)  # about what the music measures
    noise = noise_sector()

    check("noise looks like data", round(mean_magnitude(noise), 1), 0.5)
    check("silence measures zero", mean_magnitude(silence), 0.0)
    check("trim skips the run-out", runout_sectors([noise, noise, silence, quiet]), 2)
    check("trim leaves clean audio alone", runout_sectors([quiet] * 4), 0)
    check("trim leaves silence alone", runout_sectors([silence] * 4), 0)
    check("trim needs the gate to open", runout_sectors([quiet, noise, noise]), 0)
    check("trim on nothing", runout_sectors([]), 0)
    check("trim is capped", runout_sectors([noise] * 100), RUNOUT_MAX_SECTORS)

    # Byte order. Music the right way round must be left alone, since that is the
    # path every working disc takes; the same music swapped has to be caught.
    music = wave_sector(2500) * 4
    check("correct order left alone", needs_byte_swap(music), False)
    check("swapped music detected", needs_byte_swap(byte_swap(music)), True)
    check("swap settles", needs_byte_swap(byte_swap(byte_swap(music))), False)
    check("loud music is still music", needs_byte_swap(wave_sector(30000) * 4), False)
    check("silence decides nothing", needs_byte_swap(silence), False)
    check("byte_swap round trips", byte_swap(byte_swap(music)), music)

    # Naming a disc that pressed one theme. Every other theme is a file; the one
    # that is not is the track. Reading its number instead would say TADPCM2.
    plan = Plan(False, True)
    # ListJingle order is the whole point: JADPCM01 comes before TADPCM6.
    check("theme order", ALL_THEMES[:7],
          ["TADPCM1", "TADPCM2", "TADPCM3", "TADPCM4", "TADPCM5", "JADPCM01", "TADPCM6"])

    def spans_of(*numbers):
        return [(n, 0, 1) for n in numbers]

    # A European pressing: one track, one theme with no file.
    plan.stems = set(t for t in ALL_THEMES if t != "TADPCM6")
    check("europe names its one track",
          themes_for_tracks(spans_of(2), "/nonexistent", plan), (["TADPCM6"], "position"))

    # The Brazilian disc: seven pressed, seven missing, so positional throughout.
    br = set(ALL_THEMES) - {"TADPCM1", "TADPCM2", "TADPCM3", "TADPCM4", "TADPCM5",
                            "JADPCM01", "TADPCM6"}
    plan.stems = br
    check("brazil maps tracks 2..8",
          themes_for_tracks(spans_of(2, 3, 4, 5, 6, 7, 8), "/nonexistent", plan),
          (["TADPCM1", "TADPCM2", "TADPCM3", "TADPCM4", "TADPCM5", "JADPCM01", "TADPCM6"],
           "position"))

    # The US disc presses six of the same seven, so the counts disagree and the
    # table takes over. Positional would have named track 2 TADPCM1.
    plan.stems = br
    check("us falls back to the table",
          themes_for_tracks(spans_of(2, 3, 4, 5, 6, 7), "/nonexistent", plan),
          (["TADPCM2", "TADPCM3", "TADPCM4", "TADPCM5", "JADPCM01", "TADPCM6"],
           "the US track table"))

    for line in failures:
        print("FAIL %s" % line)
    print("%d check(s), %d failed" % (checks[0], len(failures)))
    return 1 if failures else 0


# --- music sources -----------------------------------------------------------
def audio_track_extent(track, tracks, file_sectors):
    """(first sector, sector count) of one audio track inside its own FILE.

    A cue's addresses are relative to the file carrying the track, so a track
    runs to the next one in that same file and the last runs to its end. A
    pregap belongs to the track that follows it, as Red Book defines it."""
    end = file_sectors
    for other in tracks:
        if other.source != track.source or other.first_index is None:
            continue
        if track.start < other.first_index < end:
            end = other.first_index
    return track.start, max(0, end - track.start)


def audio_source_kind(track, tracks, image_usable):
    """Where one cue audio track can be read from, or None when it cannot be.

    The question is which file the track names, not which type the cue gave it.
    A track inside the image is readable only when the image can carry audio at
    all: a cooked 2048-byte image holds no audio sectors, and a cue naming some
    other file is not describing this one. Deciding on the type alone treats the
    image itself as a raw per-track file and cuts the filesystem out of it as
    PCM, which is silent and sounds like noise."""
    if track.source == tracks[0].source:
        return "image" if image_usable else None
    if track.source_type in RAW_FILE_TYPES:
        return "raw"
    return "file"


def music_from_cue(image, cue, raw, outdir, plan):
    tracks = parse_cue(cue)
    print("Cue:        %s (%d tracks)" % (cue, len(tracks)))
    base = os.path.dirname(os.path.abspath(cue))
    audio = [t for t in tracks if t.mode == "AUDIO" and t.start is not None]
    if not audio:
        print("Music:      the cue lists no audio tracks")
        return

    # A cue's INDEX addresses belong to the file it names. Applying them to a
    # different one lands mid-sector and plays as full-scale noise, so "probably
    # the same disc" is not enough to read sectors out of the mounted image.
    image_usable = raw and cue_names(cue, tracks, image)
    unusable_why = None
    if not image_usable:
        unusable_why = ("the image is cooked (2048), so it carries no CD audio" if not raw
                        else "%s describes %r, not this image"
                             % (os.path.basename(cue), tracks[0].source))

    names, rule = themes_for_tracks([(t.number, t.start, 0) for t in audio], outdir, plan)
    print("Music:      %d audio track(s), named by %s" % (len(audio), rule))
    target = music_dir_in(outdir)
    if not plan.dry_run:
        os.makedirs(target, exist_ok=True)

    for track, name in zip(audio, names):
        if name is None:
            print("  track %02d: cannot tell which theme this is" % track.number)
            continue
        path = os.path.join(base, track.source) if track.source else None
        kind = audio_source_kind(track, tracks, image_usable)

        if kind is None:
            print("  track %02d: %s" % (track.number, unusable_why))
            continue
        if kind == "image":
            # Audio sectors inside the mounted image.
            first, count = audio_track_extent(
                track, tracks, os.path.getsize(image) // RAW_SECTOR)
            write_wav_from_sectors(image, os.path.join(target, name + ".WAV"),
                                   first, count, plan)
        elif kind == "raw":
            # Its own raw-sector file: a rip that split every track, which is what
            # DiscImageCreator and EAC produce. MOTOROLA lands here too, and the
            # byte-order check turns it the right way round on the way out.
            if not path or not os.path.isfile(path):
                print("  track %02d: %r is not beside the cue" % (track.number, track.source))
                continue
            first, count = audio_track_extent(
                track, tracks, os.path.getsize(path) // RAW_SECTOR)
            write_wav_from_sectors(path, os.path.join(target, name + ".WAV"),
                                   first, count, plan)
        else:
            # A decodable audio file the cue names (GOG ships its one theme as
            # LBA2.OGG). Copied under the theme's name, keeping its own
            # extension so the engine's OGG fallback still finds it.
            if not path or not os.path.isfile(path):
                print("  track %02d: %r is not beside the cue" % (track.number, track.source))
                continue
            if os.path.splitext(path)[1].lower() not in ENGINE_AUDIO_EXTS:
                print("  track %02d: %s is a %s file, which the engine cannot decode"
                      % (track.number, track.source, track.source_type))
                continue
            copy_ripped([(track.number, path)], outdir, plan, force_name=name)


def music_from_drive(drive, outdir, plan):
    """Read the disc's own table of contents and rip each audio track it lists."""
    try:
        toc = drive.read_toc()
    except DriveError as exc:
        raise SystemExit("%s\nRip the audio with cdparanoia or ImgBurn and pass "
                         "--tracks-from instead." % exc)

    spans = audio_tracks_from_toc(toc)
    data = [t for t in toc if t.is_data]
    print("Drive:      %s" % drive.name)
    print("TOC:        %d track(s), %d data, %d audio, lead-out %d"
          % (len(toc) - 1, len(data), len(spans), toc[-1].lba))
    if not spans:
        print("Music:      this disc has no audio tracks")
        return

    print("Music:      from the disc's own table of contents")
    target = music_dir_in(outdir)
    if not plan.dry_run:
        os.makedirs(target, exist_ok=True)

    retries = 0
    names, _ = themes_for_tracks(spans, outdir, plan)
    for (number, first, count), name in zip(spans, names):
        if name is None:
            print("  track %02d is audio, but no theme is mapped to it" % number)
            continue
        try:
            start, count = trim_leading_runout(drive, first, count)
            if start != first:
                print("  track %02d starts with %d sector(s) of data run-out, trimmed"
                      % (number, start - first))
            first = start
            retries += write_wav_from_drive(
                drive, os.path.join(target, name + ".WAV"), first, count, plan)
        except DriveError as exc:
            print("  track %02d failed: %s" % (number, exc))
    if retries:
        print("            %d chunk(s) needed a retry, so check those themes by ear"
              % retries)


# --- driver ------------------------------------------------------------------
def resolve_source(source):
    """(image, cue, ripped WAVs, plain data directory), each None or empty if absent."""
    if os.path.isdir(source):
        image = pick_image(source)
        if image is not None:
            return image, pick_cue(image), [], None
        # A mounted disc, or any install: the files are already readable, so the
        # data side is a copy and only the audio needs a source.
        if holds_marker(source):
            return None, None, [], source
        return None, None, ripped_wavs_in(source), None
    ext = os.path.splitext(source)[1].lower()
    if ext in CUE_EXTS:
        tracks = parse_cue(source)
        if not tracks or not tracks[0].source:
            raise SystemExit("%s names no FILE" % source)
        image = os.path.join(os.path.dirname(os.path.abspath(source)), tracks[0].source)
        if not os.path.isfile(image):
            raise SystemExit("%s names %r, which is not beside it"
                             % (source, tracks[0].source))
        return image, source, [], None
    if ext in IMAGE_EXTS or looks_like_image(source):
        return source, pick_cue(source), [], None
    raise SystemExit("%s is not a cue, an image, or a directory holding either" % source)


def main():
    ap = argparse.ArgumentParser(
        description="Extract a playable game-data folder from a CD, themes included.")
    ap.add_argument("source", nargs="?",
                    help="a .cue, a disc image, a directory holding either, a mounted "
                         "disc, or a directory of ripped track WAVs")
    ap.add_argument("outdir", nargs="?", help="where to write the game data")
    ap.add_argument("--tracks-from", metavar="DIR",
                    help="take the CD audio from a folder of ripped WAVs instead of "
                         "from the image (for containers whose audio is unreachable)")
    ap.add_argument("--from-drive", metavar="DEVICE", nargs="?", const="",
                    help="read the CD audio off a drive (/dev/sr0, or a Windows drive "
                         "letter); omit the value to use the first one found")
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

    image, cue, ripped, datadir = resolve_source(a.source)
    if a.tracks_from:
        ripped = ripped_wavs_in(a.tracks_from)
        if not ripped:
            raise SystemExit("no track WAVs in %s" % a.tracks_from)

    drive = None
    if a.from_drive is not None:
        try:
            drive = open_drive(a.from_drive)
        except DriveError as exc:
            raise SystemExit("%s" % exc)

    plan = Plan(a.force, a.dry_run)
    if not a.dry_run:
        os.makedirs(a.outdir, exist_ok=True)

    if image is None and datadir is None and not ripped and drive is None:
        raise SystemExit("%s holds no disc image, no game data and no ripped WAVs"
                         % a.source)

    raw = False
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
    elif datadir is not None:
        print("Data dir:   %s" % datadir)
        print("Data:")
        copy_data_tree(datadir, a.outdir, plan)

    # One audio source, in the order of how much it is trusted: a drive is the
    # disc itself, a rip the user made is next, and a cue is an assertion about a
    # file. Whichever wins, the naming is the same.
    if drive is not None:
        music_from_drive(drive, a.outdir, plan)
    elif ripped:
        print("Music:      from %s" % (a.tracks_from or a.source))
        copy_ripped(ripped, a.outdir, plan)
    elif image is not None and cue:
        music_from_cue(image, cue, raw, a.outdir, plan)
    elif image is not None:
        print("Music:      no cue beside the image, so the themes stay silent")

    # What matters is whether each theme ended up with a file, not how it got
    # there: on GOG the same music sits inside the data track, so counting only
    # the CD tracks we cut would report every theme missing from a folder that
    # has them all.
    missing = themes_without_a_file(a.outdir, plan.stems)
    if missing:
        print("No file for: %s" % ", ".join(missing))
        print("             That music has nowhere to come from. If the disc should "
              "have pressed it, the rip is short a track.")
    print("%d file(s) written, %d already there, %.1f MB"
          % (plan.written, plan.skipped, plan.bytes / 1048576.0))
    if not a.dry_run and (image is not None or datadir is not None):
        print("Play it with: lba2cc --game-dir %s" % os.path.abspath(a.outdir))


if __name__ == "__main__":
    sys.exit(main() or 0)
