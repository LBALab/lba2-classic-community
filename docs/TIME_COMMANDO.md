# Time Commando and the Adeline libraries

A field guide to *Time Commando* (Adeline Software International, 1996) as seen
through this repo's LBA2 tooling. Time Commando shipped a year before LBA2 and
shares the same engine lineage, so most of its on-disk formats are the **same
Adeline libraries** we already have source for here. Reading its data is a clean
way to see how those libraries worked before LBA2 layered its own game on top.

Nothing here is committed by default — the explorer scripts live in
`scripts/dev/` and the extracted samples under `.tc-scratch/` (gitignored / untracked).

## What's shared, what's not

| Layer | Time Commando (1996) | LBA2 (1997) | Our code |
|---|---|---|---|
| Resource container | `HQR` | `HQR` | `LIB386/SYSTEM/HQFILE.CPP`, identical |
| Compression | stored / LZSS / LZMIT | same | `LIB386/SYSTEM/LZ.CPP` `ExpandLZ` |
| Cinematics | **XCF** (Adeline tile codec) | **Smacker** (libsmacker) | `SOURCES/DEC_XCF.*` (XCF) vs `SOURCES/PLAYACF.CPP` (Smacker) |
| 3D / world | Adeline 3D libs | Adeline 3D libs (`LIB386/3D`) | shared lineage |

The headline finding: `SOURCES/PLAYACF.CPP` does **not** decode Time Commando's
`.ACF` files. By LBA2 the cinematic pipeline had been swapped to Smacker
(`smk_open_memory`), and "PlayAcf" is just a legacy name. The *original* Adeline
cinematic decoder is still in the tree though — `SOURCES/DEC_XCF.{ASM,CPP}` and
`SOURCES/DEC.{ASM,CPP}` — unused by the shipping LBA2 binary, but exactly what
plays Time Commando's videos.

## How the disc is packaged (GOG)

The GOG release ships the CD as a raw image plus a cue sheet wearing a `.DAT`
extension:

- `GAME.GOG` — raw `MODE1/2352` image (2352 bytes/sector: 12 sync + 4 header +
  2048 user data + 288 EDC/ECC). 260383 sectors.
- `GAME.DAT` — a CUE: one `MODE1/2352` data track (the ISO9660 filesystem) plus
  two `AUDIO` tracks (Red Book CD music).
- `RESSOURC.HQR`, `TIMECO.EXE`, etc. are also dropped loose in the install dir.

`scripts/dev/iso_bin.py` reads the ISO9660 fs straight out of the 2352-byte
sectors (logical block `lba` → byte offset `lba*2352 + 16`, take 2048 bytes), so
there's no need to convert to a plain `.iso` first. (Same reader works on any
Adeline CD dump — LBA1 `LBA.DOT`, LBA2 `LBA2.GOG`, TC `GAME.GOG`.)

```
python3 scripts/dev/iso_bin.py GAME.GOG --tree
python3 scripts/dev/iso_bin.py GAME.GOG --extract /STAGE00/RUN0/SCENE.HQR out.hqr
```

Disc layout (volume `TIMECOMM`):

```
/RESSOURC.HQR              shared resources (palettes, fonts, texture pages)
/CREDITS.HQR
/SEQUENCE/*.ACF            cutscenes; BIG*=hi-res, LOW*=lo-res variants
/STAGE00../STAGE0A/        12 time-period levels
   RUN0/ RUN1/             "runs" through the level (forward / back)
      SCENE.HQR            level data (models, collision, scripts)
      SCENE.ACF            the pre-rendered scrolling background animation
```

That `SCENE.HQR` + `SCENE.ACF` pairing is the core of the game: a real-time 3D
character is composited over a pre-rendered animated background. The background
leaves a hole for the playfield — see `Recouvre` below.

## HQR — the resource container

Byte-identical to LBA's. `scripts/dev/hqr_inspect.py` (a port of `HQFILE.CPP` +
`LZ.CPP`) reads every Time Commando HQR with no changes:

- File starts with a `U32` offset table; `table[0]/4` = slot count; each slot is
  a byte offset to that entry's header (0 = empty).
- Entry header: `{ u32 SizeFile; u32 CompressedSizeFile; s16 CompressMethod }`,
  then data. Method `0` = stored, `1` = LZSS, `2` = LZMIT.
- LZSS/LZMIT are the same `ExpandLZ` scheme: a flag byte whose 8 bits select
  literal-vs-match; matches are `(length = low nibble + method + 1, offset =
  (hi<<4)|(lo>>4))` back-references. Decompressing all of `RESSOURC.HQR` (114
  entries), `SCENE.HQR`, and `CREDITS.HQR` yields exact sizes, zero failures.

## XCF — the cinematic codec (`.ACF` files)

Internal signature, literally in the bytes: `XCF File Format` / `Adeline
Software`. The container is a flat list of chunks; each is

```
char tag[8]       // ASCII, space-padded
u32  size         // little-endian payload length
u8   data[size]
```

contiguous (next tag immediately follows the payload). `scripts/dev/acf_inspect.py`
walks them. Tags seen:

| tag | meaning |
|---|---|
| `FrameLen` | `u32 frameCount`, `u32 biggestFrame`, then a per-frame duration byte each |
| `Format  ` | the `FORMAT` struct below |
| `Palette ` | 768-byte VGA palette (256 × RGB, 0–255) |
| `KeyFrame` | intra-coded frame |
| `DltFrame` | delta (inter-coded) frame |
| `Camera  ` | 32 bytes: `x,y,z, target_x,y,z, roll, fov` (the scripted rail camera) |
| `Recouvre` | overlay-mask region — where the live 3D playfield is punched in |
| `SoundBuf` | audio pre-roll buffer |
| `SoundFrm` | per-frame audio |
| `SoundEnd` / `End` | terminators |
| `NulChunk` | padding to align the next frame chunk |

`FORMAT` (`SOURCES/DEC_XCF.H`): `struct_size, DeltaX(width), DeltaY(height),
FrameSize, KeySize, KeyRate, PlayRate, SamplingRate, SampleType, SampleFlags,
Compressor (0=ACF, 1=XCF)`. Observed: 320×200 or 320×240, 15 fps. Audio is
22050 Hz, 8-bit unsigned, stereo (`SampleType=2`); `2940 B/frame × 15 fps =
44100 B/s = 22050 × 2ch`.

### The frame codec (the interesting part)

A frame is a grid of **8×8 tiles**. Each tile gets a **6-bit opcode** (0–63)
that selects one of 64 decode routines. The frame payload splits into three
regions:

```
payload[0:4]                = colour_offset (u32)
payload[4 : 4+(H/8)*30]     = opcode stream   (6 bits/tile, 4 packed per 3 bytes)
payload[4+(H/8)*30 : co]    = "aligned"   stream
payload[colour_offset:]     = "unaligned" stream
```

`(H/8)*30` because each tile-row is `W/8 = 40` tiles × 6 bits = 240 bits = 30
bytes. `DEC.H` derives the same number: `Dec_CodeLen = (H/8)*(320/8)*6/8 + 4`.
The two data streams interleave per opcode — `aligned` carries bitmasks and
packed palette indices, `unaligned` carries literal colour bytes and motion
vectors. Opcodes are read 4 at a time: `op = (read3 | 0xff000000)`, take
`op & 63`, `op >>= 6`; the forced top byte makes `op == -1` after exactly four
tiles, signalling the next 3-byte read.

The 64 opcodes (from the original `TabFunc` jump table in `DEC_XCF.ASM`) fall
into families. Most motion/fill ops come in four flavours: plain, then `+update4
/ +update8 / +update16` residual passes that repaint a few pixels on top.

| opcodes | routine | what it does |
|---|---|---|
| 0 | `Raw` | 64 literal palette bytes straight into the tile |
| 1–4 | `ZeroMotion` | copy tile from previous frame, same position |
| 5–8 | `SMotion8` | short motion, one signed 4+4 nibble vector, 8×8 |
| 9–12 | `Motion8` | absolute offset into previous frame, 8×8 |
| 13–16 | `SMotion4` | short motion, four 4×4 sub-blocks |
| 17–20 | `Motion4` | absolute motion, four 4×4 sub-blocks |
| 21–24 | `SingleFill` | one colour over the whole tile |
| 25–28 | `FourFill` | one colour per 4×4 quadrant |
| 29–32 | `Bit1..Bit4` | 1/2/3/4-bit packed indices into a small per-tile palette |
| 33–35 | `Split1..3` | same idea, but per 4×4 sub-tile |
| 36 | `Cross` | 4 base colours + a 4×4 correspondence map |
| 37 | `Prime` | one prime colour + a bitmask of literal exceptions |
| 38–39 | `Bank1/Bank2` | 4-bit / 5-bit indices relative to one or two colour banks |
| 40–43 | `Block` | run-fill in horizontal / vertical / two diagonal scan orders |
| 44–47 | `BlockBank1` | same runs, nibble-packed within a bank |
| 48–51 | `ROMotion8` | relative-offset motion (signed 16-bit), 8×8 |
| 52–55 | `RCMotion8` | relative motion with separate dx/dy bytes, 8×8 |
| 56–63 | `ROMotion4` / `RCMotion4` | the 4×4 versions |

`update4` reads four `(x,y)` positions (3 packed bytes, 6 bits each) from the
unaligned stream and four colours from the aligned stream; `update16` reads an
8×8 bitmask and literal pixels. These let a motion-copied tile carry a small
residual without a full re-encode — a hand-rolled equivalent of motion
compensation + sparse residual, in 1996, on a 486.

`scripts/dev/acf_decode.py` is a Python port of `DEC_XCF.CPP` (cross-checked
against `../timeco/src/acf.c`). It decodes keyframes and delta frames with full
double-buffering:

```
python3 scripts/dev/acf_decode.py SCENE.ACF --frame 0  --out frame0.png
python3 scripts/dev/acf_decode.py SCENE.ACF --all out_dir/      # every frame
```

Verified by rendering: scene keyframes (the Adeline boxing-ring intro, a fiery
cavern), and an Activision-logo *delta* frame 45 — reconstructed cleanly from
the keyframe through 44 motion-compensated deltas, no drift.

### Recouvre — the playfield hole

`Recouvre` ("covers/overlay" in French) marks the region the pre-rendered
background leaves empty so the engine can composite the real-time 3D character.
In a decoded scene frame it shows up as a black, white-rimmed cut-out. This is
the architectural heart of the game: **animated pre-rendered backdrop + live 3D
actor masked into a window**, with a scripted `Camera` rail driving both.

## What this teaches about the Adeline libraries

- **One resource layer, reused for years.** HQR + the `ExpandLZ` LZSS/LZMIT pair
  carried from Time Commando through LBA1/LBA2 unchanged. Learn it once.
- **Codec built for the hardware, not the format.** XCF's split aligned/unaligned
  streams, 6-bit opcodes packed 4-per-3-bytes, and bank-relative nibble indices
  are all about cheap 486-era reads and small palettes, not elegance. The 64-way
  jump table is a per-tile cost model: pick the cheapest representation for each
  8×8 block.
- **The same studio iterated the pipeline.** Time Commando = pre-rendered XCF
  backdrops + 3D actor. LBA2 kept the 3D and HQR libraries but moved cutscenes
  to licensed Smacker — which is why `DEC_XCF` sits unused in this tree while
  `PLAYACF.CPP` calls libsmacker.

## Tools (this repo, `scripts/dev/`)

| script | purpose |
|---|---|
| `iso_bin.py` | read ISO9660 out of a raw `MODE1/2352` CD image (`.gog`/`.bin`/`.dot`) |
| `hqr_inspect.py` | list / decompress HQR entries (LZSS / LZMIT / stored) |
| `acf_inspect.py` | walk the XCF/ACF chunk container |
| `acf_decode.py` | decode XCF key/delta frames to PNG (port of `DEC_XCF.CPP`) |

## References

- Original Adeline source in this repo: `SOURCES/DEC.{ASM,CPP}`,
  `SOURCES/DEC_XCF.{ASM,CPP,H}`, `LIB386/SYSTEM/HQFILE.CPP`, `LIB386/SYSTEM/LZ.CPP`.
- TimeCo reimplementation (LBALab): `../timeco/src/acf.c` — clean C port of the
  decoder.
- Defence-Force "Time Commando" articles + `ACF2PCX.cpp` reverse-engineering
  notes (linked from `../timeco/src/acf.c`).
