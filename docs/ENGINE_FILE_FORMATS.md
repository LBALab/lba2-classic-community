# Adeline engine file formats

> **Status: DRAFT / living document (v0).** Part of the engine-axis family with
> [ENGINE_GAME_SEAM.md](ENGINE_GAME_SEAM.md) (which code is reusable) and
> [ENGINE_GAME_INTERFACE.md](ENGINE_GAME_INTERFACE.md) (the script-VM membrane). This one
> documents the **on-disk formats** the engine reads, at *engine* altitude — these are
> Adeline-library formats shared across titles, not "LBA2 file formats." Truth hierarchy:
> **code > this document > external sources.**

These formats are the data contract of `LIB386` (the Adeline SDK). The same reader code in
this repo parses three shipped games — LBA1 (1994), Time Commando (1996), LBA2 (1997) — which
is the evidence that `LIB386` is a versioned SDK rather than LBA2-specific code. Where a format
evolved between titles, the section says so; the consolidated timeline is in
[ENGINE_GAME_SEAM.md § version / dual-path seams](ENGINE_GAME_SEAM.md#version--dual-path-seams-the-sdk-timeline).

## Two layers: container + payloads

Almost every asset is a **payload format wrapped in the HQR container**. Read order is always:

```
HQR archive  →  entry (offset table)  →  decompress (stored / LZSS / LZMIT)  →  payload
```

So HQR + the LZ codecs are the *meta-formats*; everything else is a payload you get after
decompressing an entry.

| Format | Owner (LIB386) | Container | Seen in | Evolution |
|---|---|---|---|---|
| HQR archive | `SYSTEM` (`HQFILE`) | — (is the container) | LBA1, TC, LBA2 | stable |
| LZSS / LZMIT | `SYSTEM` (`LZ`) | inside HQR entry | LBA1=LZSS; TC/LBA2=+LZMIT | LZMIT added 1994→96 |
| Palette | `SVGA` / `SYSTEM` | HQR entry | all | stable (768 B VGA) |
| 3D body | `3D` + `OBJECT` | HQR entry (`BODY`) | all | 16-bit (LBA1/TC) → 32-bit (LBA2) |
| Animation | `ANIM` | HQR entry (`ANIM`) | all | header stable, keyframe grew |
| Sprite | `SVGA` | HQR entry (`SPRITES`) | all | stable |
| Texture page | `pol_work` | HQR entry (`RESS`) | TC, LBA2 | added by 1996 (none in LBA1) |
| Sound sample | `AIL` | HQR entry (`SAMPLES`) | all | VOC (LBA1) → RIFF/WAV (TC/LBA2) |
| Music | `AIL` | HQR entry (`MIDI_*`) | all | XMIDI, stable |
| XCF/ACF cinematic | `DEC_XCF` (dormant) | own file / HQR | TC (LBA2 uses Smacker) | XCF → Smacker by 1997 |
| Scene / grid / island | `SYSTEM`+`3DEXT` | HQR / `.ILE`/`.OBL` | all | compatible model, repackaged |

Tooling for all of the below lives in `scripts/dev/`: `hqr_inspect.py` (HQR + LZ),
`acf_inspect.py` / `acf_decode.py` (XCF), `iso_bin.py` (read a raw CD-image / `.gog`/`.bin`).

---

## HQR — resource archive

The container for nearly everything. Source of truth: `LIB386/SYSTEM/HQFILE.CPP`,
`LIB386/H/SYSTEM/HQR.H`.

```
file:
  u32  offsetTable[ N ]     # N = offsetTable[0] / 4  (first offset doubles as the count)
                            #   each entry = byte offset of that entry's header, 0 = empty slot
entry (at offsetTable[i]):
  u32  SizeFile             # decompressed size
  u32  CompressedSizeFile   # stored size of the payload that follows
  s16  CompressMethod       # 0 = stored, 1 = LZSS, 2 = LZMIT
  u8   data[ CompressedSizeFile ]
```

Entries are addressed by **fixed index** — the engine hard-codes which slot holds what (e.g.
the ACF list, a palette, a font). Drop-in identical across LBA1/TC/LBA2; verified by reading
every retail archive of all three with `hqr_inspect.py`
([LBA1_PORTING_SURFACE.md § verified](LBA1_PORTING_SURFACE.md#verified-against-lba1-retail-data-2026-06-04)).

## LZSS / LZMIT — compression

Both are the same `ExpandLZ` scheme (`LIB386/SYSTEM/LZ.CPP`), differing only in the minimum
match length. Decompression:

```
while output not full:
  flag = next byte
  for each of 8 bits (LSB first):
    if bit == 1:  copy 1 literal byte
    else:         read 2 bytes lo,hi:
                    length = (lo & 0x0F) + (CompressMethod + 1)
                    offset = (hi << 4) | (lo >> 4)
                    copy `length` bytes from `output[-(offset+1)]`
```

`CompressMethod` (the HQR field) sets the match base: **1 = LZSS** (min length 2), **2 = LZMIT**
(min length 3). Method 0 is stored (no compression). **LZMIT appears in TC and LBA2 but never
in LBA1** — it is a post-1994 SDK addition, so the LBA2-era reader is a strict superset.

## Palette

768 bytes = 256 × RGB triplets, 8 bits per channel (0–255, already VGA-scaled — *not* the
6-bit 0–63 DOS form). Stable across all three games. A bare palette entry is exactly 768 bytes;
many image payloads carry one inline.

## 3D body / model

The 3D mesh format read by `LIB386/3D` + `LIB386/OBJECT` (`AFF_OBJ`). Header front:

```
LBA1 / Time Commando (16-bit):           LBA2 (32-bit):
  u16 Info                                 u32 Info        (e.g. 272)
  s16 bbox[6]  (Xmin,Xmax,Ymin,…)          u32 SizeHeader  (e.g. 96)
  … sequential poly / line / sphere blocks s32 bbox[6]
                                           … count+offset table → groups, points, normals,
                                             normFaces, polys, lines, spheres, textures
```

The widening from 16-bit fields + sequential blocks to 32-bit fields + a random-access
count/offset table (and added `textures` section) happened **between TC (1996) and LBA2
(1997)** — TC bodies still use the LBA1-style 16-bit header. Full byte-diff, the per-poly
record layouts, and the LBA1→LBA2 transcoder spec are in
[LBA1_PORTING_SURFACE.md § body](LBA1_PORTING_SURFACE.md#body-header-byte-diff). The `MASK_OBJECT_ANIMATED`
flag (`AFF_OBJ.H:129`, bit 8) marks a body that carries bone groups.

## Animation

Keyframe bone animation (`LIB386/ANIM`). Header (verified LBA1 + LBA2):

```
u16  NbKeyframes
u16  KeyframeSize       # bytes per keyframe (grew 21 → 24 LBA1→LBA2 as bone records widened)
u16  NbBones / loop     # frame timing + bone count fields
… per-keyframe bone transforms
```

The header shape is **stable across LBA1 and LBA2** (entry 1 of both is a 5-keyframe clip —
almost certainly the same Twinsen animation, three years apart); only the per-keyframe record
grew. Bodies and animations are bound at runtime by bone count.

## Sprite

2D sprite bank in `SPRITES.HQR`, drawn by `LIB386/SVGA` (`AffGraph` / `PtrAffGraph`). On-disk
header (verified LBA1 + LBA2):

```
u32  field0            # 8 in both games (render kind / flag)
u32  PayloadSize
u8   Width
u8   Height
u8   OffsetX
u8   OffsetY
u8   pixels[]          # palette indices
```

Same layout across titles (LBA1 13×13, LBA2 7×7 for entry 1). The sprite *system* (UI vs
world "extras" vs the brick atlas) is documented in [SPRITES.md](SPRITES.md); this is just the
bank entry's on-disk header. Raw (uncompressed) sprite variants live in LBA2's `SPRIRAW.HQR`.

## Texture page

256 × 256 × 8-bit texture atlas (65536-byte HQR entries), consumed by the textured fillers in
`LIB386/pol_work`. Present in Time Commando (`RESSOURC.HQR`, with `Texture: ON` in `TIMECO.DEF`)
and LBA2; **absent from LBA1**, which had no textured polygons. So hardware-texture support
entered the SDK by 1996, before LBA2. (TC pages share a small fixed prefix
`00 c3 01 d9 41 44 46 68`; treat the page as 256×256 indexed pixels for rendering.)

## Sound sample

SFX samples, parsed by the `AIL` backend (`LIB386/AIL/SDL/SAMPLE.CPP`). The on-disk encoding
**changed across the SDK's life**, and the parser handles both:

- **VOC** — Creative Voice File (`"Creative Voice File\x1A"` magic). Used by **LBA1 (1994)**.
- **RIFF / WAV** — PCM (tag 1) or IMA-ADPCM (tag 17). Used by **Time Commando (1996)** and
  **LBA2 (1997)**.

`SAMPLE.CPP` parsing *both* VOC and WAV is the dual-path seam for this transition, not dead
code. Runtime audio architecture (the Miles→SDL `AIL` contract) is in [AUDIO.md](AUDIO.md).

## Music

XMIDI — IFF-wrapped MIDI (`FORM…XDIR` / `XMID` magic), in `MIDI_*.HQR`. Stable across LBA1 and
TC (and LBA2's media layer); this is why the TimeCo reimplementation ships an `xmidi.c`. The
GPLv2 decoder situation for the media wall is noted in the architecture corpus.

## XCF / ACF cinematic

Adeline's pre-Smacker cinematic codec, decoded by `SOURCES/DEC_XCF.{ASM,CPP}` (**dormant** in
the LBA2 binary — see the [dormant register](ENGINE_GAME_SEAM.md#dormant-register-present-in-tree-not-in-the-shipping-binary)).
Internal signature `XCF File Format` / `Adeline Software`. Container is a flat chunk list
(`tag[8] + u32 size + data`); frames are an **8×8-tile codec with 64 six-bit opcodes** (raw,
colour fills, 1–4-bit packed palette tiles, split/cross/bank tiles, run blocks, and full
motion compensation with residual passes). Audio is 22050 Hz 8-bit. Used by **Time Commando**;
LBA2 replaced it with **Smacker** (`SOURCES/PLAYACF.CPP` → `LIB386/libsmacker`). Full chunk +
opcode spec and a working decoder walkthrough: **[TIME_COMMANDO.md](TIME_COMMANDO.md)**.

## Scene / grid / island

The world/cube data. Two packagings of one model:

- **Isometric interiors** — brick grids + block libraries. LBA1 splits these across
  `LBA_GRI` / `LBA_BLL` / `LBA_BRK` HQRs; LBA2 uses `LBA_BKG.HQR` + `SCENE.HQR`.
- **3D exteriors** — heightmap islands in `.ILE` (island geometry) + `.OBL` (objects),
  loaded by `SOURCES/3DEXT/LOADISLE`.

Scene blobs in `SCENE.HQR` also carry the inline **Life/Track scripts** — the engine/game
membrane (see [ENGINE_GAME_INTERFACE.md](ENGINE_GAME_INTERFACE.md)). The 223-scene index and
interior/exterior split is in [SCENES.md](SCENES.md); the LBA1↔LBA2 grid/block compatibility
assessment is in [LBA1_PORTING_SURFACE.md](LBA1_PORTING_SURFACE.md).

---

## Cross-game evolution at a glance

The reference exists partly to make the SDK timeline legible: most formats are **stable**
(HQR, palette, sprite, anim header, XMIDI), and the churn is concentrated in compression,
audio encoding, 3D coordinate width, and cinematics — the same handful of dual-path seams the
per-module map flags. Dating them needs all three games; Time Commando (1996) is the midpoint
that pins each change:

| Change | LBA1 1994 | TC 1996 | LBA2 1997 |
|---|---|---|---|
| Compression | LZSS | **+ LZMIT** | + LZMIT |
| Sample audio | VOC | **WAV** | WAV |
| 3D body coords | 16-bit | 16-bit | **32-bit** |
| Textures | none | **64K pages** | textured polys |
| Cinematics | (FLA) | **XCF** | Smacker |

See [ENGINE_GAME_SEAM.md](ENGINE_GAME_SEAM.md) for how these map onto live vs dormant modules.
