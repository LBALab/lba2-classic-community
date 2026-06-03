# LBA1 → lba2cc porting surface

> **Status: DRAFT / living document (v0).** Companion to [ENGINE_GAME_SEAM.md](ENGINE_GAME_SEAM.md),
> [ENGINE_GAME_INTERFACE.md](ENGINE_GAME_INTERFACE.md), and [ARCHITECTURE.md](ARCHITECTURE.md).
> A structure-level survey of what it would take to host **LBA1 content** on the **lba2cc
> engine**. Confidence is high on container/struct layout (parsers read on both sides),
> lower on exact body-record bytes and opcode operands (flagged below). Truth hierarchy:
> code > this document.

## Summary

LBA1 (`~/code/lba-hacking/lba1-classic`, 1994, C+ASM) and this engine (LBA2, 1997 + port)
are the same Adeline codebase one generation apart. That is *why* most content formats line
up: the divergence is concentrated in **I/O wrappers, the media stack, and rendering
capability** (and the capability gap runs the helpful direction — LBA2 is a superset). The
work to host LBA1 is **format transcoders (bodies, scenes) + a script opcode-remap +
replacing the FLA/MIDI media stack** — not re-architecting any subsystem.

## Per-subsystem verdict

| # | Subsystem | Verdict | Why |
|---|---|---|---|
| 1 | Resource container (HQR) | **Compatible as-is** | Same format; LBA2 natively reads LBA1's methods 0/1 |
| 2 | 3D body / model | **Needs transcoder** | Same model, different on-disk encoding; LBA2 superset (textures) |
| 3 | Animation | **Compatible as-is** | Same frame math/layout; rides on body group alignment |
| 4 | Scene / grid / cube | **Compatible model, packaging shim** | Same brick/block payloads; LBA1 3 HQRs → LBA2 merged container |
| 5 | Sprites / palettes | **Likely compatible, verify** | Same HQR container; sprite record header needs a byte-diff |
| 6 | Render capability | **Free win (LBA2 superset)** | Running less-capable content on a more-capable renderer |
| 7 | Audio + video | **Fundamentally different** | FLA video + MIDI music; LBA2 has neither decoder |
| 8 | Script VM (Life/Track) | **Opcode-remap shim** | Same VM; opcodes 0–56 identical, collisions at 57+ |

### 1. HQR container — compatible as-is

Same format: `[U32 total]`, a table of U32 offsets, each blob prefixed by
`{U32 SizeFile, U32 CompressedSizeFile, U16 CompressMethod}`. LBA1 (`LIB386/LIB_SYS/HQ_RESS.C`)
uses methods 0 (stored) / 1 (LZS); LBA2 (`LIB386/SYSTEM/HQFILE.CPP`, `HQR.H:16`: "0 stored,
1 LZSS, 2 LZMIT") reads `method <= 2`. The buffered LRU manager (`HQR_Init_Ressource`/
`HQR_Get`/`HQR_Del_Bloc`) is the same design on both sides. **LBA1 HQRs load on the LBA2
engine unchanged.** One thing to verify: the `CompressMethod + 1` reindex in
`HQF_LoadClose` suggests the LZ enum was renumbered — confirm LBA1 method-1 blobs decompress
byte-identical.

### 2. 3D body / model — needs a transcoder

Same conceptual model (header + groups + points + normals + polys/lines/spheres), different
encoding. LBA1 (`P_OBJET.ASM`): `Info` word + 12-byte ZV bbox + sequential blocks (polys,
lines, spheres); three entity types; flat/gouraud/dither materials; **no textures**. LBA2
(`AFF_OBJ.H` `T_BODY_HEADER`): an explicit offset/count table
(`OffGroupes/OffPoints/OffNormales/OffPolys/OffLines/OffSpheres/OffTextures`) plus a full
textured + z-buffered poly family and quad polys with UVs. LBA2 renders everything LBA1
bodies contain (flat/gouraud/dither/line/sphere is a subset), so the LBA1 record just needs
**transcoding** into `T_BODY_HEADER` + separated arrays. This is the dominant per-asset
model work, and it gates animation (group indices must line up afterward).

### 3. Animation — compatible as-is

The strongest match. Frame math is identical: LBA2 `FRAME.CPP:46`
`ofsFrame = (nbGroups*8 + 8)*frame + 8` equals LBA1 `P_ANIM.ASM` `SetAnimObjet`. Same header
`[U16 nbframes][U16 nbGroups]`, same 8-byte per-frame info, same `nbGroups × 8` per-group
keyframes (`{Type, Alpha, Beta, Gamma}`), same engine primitives (`SetAnimObjet`,
`SetInterAnimObjet`, `GetNbFramesAnim`). LBA1 anims should drive LBA2's anim engine
unchanged **provided the body they animate has the same group count/order** — i.e. anim
compatibility rides on the body transcoder (#2).

### 4. Scene / grid / cube — compatible model, packaging shim

Same brick-isometric world model and algorithms line-for-line: `BufCube/BufMap/TabBlock`,
`HEADER_BLOCK`, the used-block bitmap, `DecompColonne` column RLE, `LoadUsedBrick`'s
four-pass brick renumbering. The packaging differs: LBA1 keeps three HQRs (`LBA_GRI`,
`LBA_BLL`, `LBA_BRK`); LBA2 merges them under `T_BKG_HEADER` (index offsets into one
container) + per-scene `T_GRI_HEADER {My_Bll, My_Grm, UsedBlock[32]}`, and raised limits
(`NB_COLON` 28→256, `MAX_BRICK` 150→256) and added GRM overlay objects. A converter
re-indexes LBA1's three HQRs into LBA2's merged container and synthesizes the headers; the
brick/block payloads don't change. Medium, mechanical.

### 5. Sprites / palettes — likely compatible, verify

Both store sprites/inventory/font in HQR (`HQRPtrSpriteExtra`, `InventoryObj`), so the
wrapper is solved. Both are 256-color palette-indexed (768-byte VGA tables). Unknown: the
per-sprite record header (offset/width/height/clip) byte layout, LBA1 `INCRUST.C` vs LBA2
`INCRUST.CPP` — same subsystem name, needs a byte-diff. Probable small shim.

### 6. Render capability — free win (LBA2 is a superset)

The one place the asymmetry helps. LBA1: MCGA-first (`MCGA.C`), three body entity types,
flat/gouraud/dither, **no texture mapping, no z-buffer**, draw order via **bubble-sort
painter's** (`BUBSORT.C`). LBA2: SVGA, the full `pol_work` filler set (flat, gouraud,
textured, textured+z-buffer, +fog, sphere, clip), and the dependency-graph **sort tree**
(`SORT.CPP`). No capability *gap* to fill — LBA1 bodies simply won't exercise LBA2's texture
paths. The only consequence is cosmetic: LBA1 was authored for painter's order at 320×200,
so verify LBA1 scenes look right under LBA2's sort tree and resolution scaling (the exterior
draw-order quirk in the project notes could surface differently).

### 7. Audio + video — fundamentally different (the hard wall)

- **Video:** LBA1 = **FLA/FLI** (Autodesk-Animator-derived; `FLA.H`, `PLAYFLA.C`). LBA2 =
  **Smacker** (`libsmacker`, `SMACKER.CPP`, `PLAYACF.CPP`). LBA2 has **no FLA decoder**.
  LBA1 cutscenes need a new FLA player ported in, or an offline FLA→Smacker transcode.
- **Audio:** LBA1 = DLL-driver mixer (`MIXER.C`) + separate `LIB_MIDI` and `LIB_SAMP`; uses
  **MIDI music** (`LM_PLAY_MIDI`). LBA2 = the `AIL/` abstraction (SDL/Miles/null). Sound
  effects live in HQR on both sides (tractable); **music + cutscenes are not drop-in**.

This is the biggest single chunk of net-new engine work, or it is pushed into an offline
asset-transcode pipeline.

### 8. Script VM — opcode-remap shim

Same VM (see [ENGINE_GAME_INTERFACE.md](ENGINE_GAME_INTERFACE.md)): `*PtrPrg++` dispatch in
both `GERELIFE` and `GERETRAK`, same condition opcodes (`LF_*`), comparison ops (`LT_*`),
return widths. The command (`LM_*`) tables are **numerically identical for 0–56**.
Divergence begins ~57:

- **Renames at the same slot (semantically compatible):** `LM_SET_FLAG_CUBE`(31)→`LM_SET_VAR_CUBE`,
  `LM_SET_FLAG_GAME`(36)→`LM_SET_VAR_GAME` (and LBA1's byte "flags" became LBA2's `S16`
  vars — watch operand widths).
- **True collisions (different command, same byte):** `LM_ZOOM`(57)→`LM_SHADOW_OBJ`,
  `LM_PLAY_FLA`(64)→`LM_PLAY_ACF`, `LM_PLAY_MIDI`(65)→`LM_ECLAIR`,
  `LM_INIT_PINGOUIN`(71)→`LM_MEMO_ARDOISE`. LBA2 then extends well past LBA1.

So LBA1 scripts run on the LBA2 VM after an **opcode-remap table** — a contained shim, not a
transpiler. Notably, the collided opcodes are largely the FLA/MIDI media opcodes, so this
intersects the subsystem-7 media wall.

## The easy part vs the hard part

**Easy (shared lineage carries it):** HQR container (drop-in); animation (drop-in, rides on
body alignment); brick/block/column world payloads (drop-in; only the multi-HQR repackaging
is work); render capability (free — LBA2 is a strict superset); script VM model (same engine,
opcode-remap table).

**Hard (genuine divergence):**
- **FLA video + MIDI music** — LBA2 has no decoder for either. Largest net-new work, or an
  offline transcode pipeline. *Dominant effort.*
- **3D body transcoder** — biggest per-asset format conversion (sequential entity stream →
  offset-table `T_BODY_HEADER`); gates animation alignment.
- **Background repackaging** — three LBA1 HQRs → one LBA2 `T_BKG_HEADER` with synthesized
  per-scene `T_GRI_HEADER`.

## Dominant effort and biggest unknowns

**Dominant effort:** (1) the FLA/MIDI media wall (port a player or transcode offline), and
(2) the body/scene transcoders. Everything else is wrapper-level.

**Biggest unknowns — need hands-on verification (a structure survey can't settle these):**

1. **Exact LBA1 body poly-record byte layout** vs LBA2 `STRUC_POLY3` / `T_BODY_HEADER` —
   byte-diff one real body from each game. Gates models *and* anims.
2. **LZS vs LZSS decompressor equivalence** — the `CompressMethod + 1` reindex hints at an
   enum renumber; confirm LBA1 method-1 blobs decompress byte-identical.
3. **Shared-opcode operand encodings** — opcode numbers 0–56 align; confirm each reads the
   same operand widths/order in both `GERELIFE`/`GERETRAK`.
4. **Sprite record header** layout (LBA1 `INCRUST.C` vs LBA2 `INCRUST.CPP`).
5. **Behavioural fidelity under LBA2's sort tree + SVGA** for content authored against
   LBA1's MCGA painter's-order at 320×200.

**Net:** the engine is overwhelmingly capable of hosting LBA1 content because it is the same
codebase one generation on. The work is concentrated in **format transcoders + a script
opcode remap + replacing the FLA/MIDI media stack** — not in re-architecting any subsystem.
