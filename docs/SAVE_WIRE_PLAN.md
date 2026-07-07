# Save wire-format plan (bit-exact retail serialization)

Status: **Phase 0 research, review-gated.** This document is the deliverable of
Phase 0. No tests or production code have been written. Three decisions at the
end need a human answer before Phase 1 (RED tests) begins.

Goal: make the save serializer emit and consume bytes that are identical to what
the retail 1997 (32-bit) engine wrote, independent of host word size. Concretely:
finish the writer-side `...ToWire32` conversion so a 64-bit build emits the same
bytes a 32-bit build does, then retire the ABI stride heuristic from the canonical
path. Driven test-first (red then green then refactor).

Related docs: [SAVEGAME.md](SAVEGAME.md) (field map, version 36, lifecycle),
[ABI.md](ABI.md) (the "never `sizeof(T)`-as-stride for fat structs" rule this work
enforces), [TESTING.md](TESTING.md).

## TL;DR findings

- **Only three writer sites need conversion**, exactly the three named in the
  task: the per-object `T_OBJ_3D` tail, the `T_EXTRA` array, and each
  `S_PART_FLOW`. Every other raw `LbaWrite(&x, sizeof(x))` in `SaveContexte`
  writes a pure-scalar POD with identical 32/64 layout (`Coord`, `BoundAngle`,
  `ExeSwitch`, `S_ONE_DOT`, `T_INCRUST_DISP`, `S_BUGGY`). They are safe to leave
  as-is; they still deserve cheap size locks.
- **The read side is already done.** `SavegameObj3dFromWire32` /
  `...ExtraFromWire32` / `...FlowFromWire32` decode fixed 32-bit wire structs.
  The writer converters are their missing inverses.
- **Independent layout derivation matches the wire structs on sizes, and
  contradicts the code on the per-object stride constant.** Derived from the real
  native structs (not from the `_WIRE32` mirrors): wire `T_OBJ_3D` = 136 B,
  `T_EXTRA` = 68 B, `S_PART_FLOW` = 60 B. The per-object stride is
  **140 (scalar prefix) + 136 (wire Obj) = 276 B**, not the `278` the heuristic
  hardcodes. This is already known and empirically confirmed in the repo
  ([SAVEGAME.md](SAVEGAME.md) "Empirical note (probe)" and
  `scripts/save_probe.py`, which walk 276/304). The `142`/`278` constants in
  `SAVEGAME_LOAD_BOUNDS.CPP` are wrong-but-harmless (used only for a bounds
  pre-check and a sniff hint; the real read is field-by-field). The
  static_assert lock must anchor on 276 / 140 / 136, never 278 / 142.
- **Compressed round-trip is achievable and directly testable.** The engine ships
  a real compressor (`Compress_LZSS`, the original binary-tree LZSS), so if the
  re-serialized payload is byte-identical and `Compress_LZSS` is faithful to
  retail's encoder, `compress(our_payload)` reproduces the original compressed
  bytes. The bundled corpus is entirely compressed (`0xA4`), so it is the direct
  oracle for that hypothesis.
- **The repo already ships a 50-save retail corpus, git-tracked.** This
  contradicts the task's "retail saves are local, git-ignored only" guardrail.
  See decision 3.

## End-to-end section map

Write order is `SaveGame` (envelope) wrapping `SaveContexte` (payload); read order
is `LoadGame` wrapping `LoadContexte`. Offsets past the player name are not
absolute (the name is variable-length). "Oracle" is the ground-truth source for
that section's layout.

Legend: B = bytes, W = `LbaWriteWord` (S16), L = `LbaWriteLong` (S32/U32),
raw = `LbaWrite(&x, sizeof x)` memcpy.

### Envelope (`SaveGame`, `SOURCES/SAVEGAME.CPP:586`+)

| # | Field | On-disk | Writer / reader | Oracle |
|---|-------|---------|-----------------|--------|
| E1 | `NumVersion` | 1 B | `SaveGame:586` / `SaveLoadDecompressIfNeeded` | LBA2SD flag byte (`0x24`/`0xA4`) |
| E2 | `NumCube` (scene/cube) | 4 B L | `SaveGame:591` | source; see LBA2SD reconciliation below |
| E3 | `PlayerName` | strlen+1, NUL-term ASCII | `SaveGame:594` / `SaveLoadReadPlayerName` | LBA2SD NUL-name |
| E4 | `sizefile` (only if `0xA4`) | 4 B L, LE | `SaveGame:598,634` | LBA2SD LE `original_size` |
| E5 | Screenshot | 160x120 = 19200 B | `SaveGame:603` | LBA2R2I / I2R |
| .. | `SaveContexte` payload | (below) | `SaveContexte` | corpus |
| E6 | `ValidePos` | 4 B L | `SaveGame:615` | corpus |
| E7 | `LastValidePos` | 4 B L | `SaveGame:616` | corpus |
| E8 | `ValideCube`, `SizeOfBufferValidePos`, `BufferValidePos` (only if `!ValidePos`) | 4+4+var | `SaveGame:619-621` | corpus |

When compressed, everything from E5 (screenshot) through E8 is one LZSS blob;
`sizefile` (E4) is its decompressed length. The screenshot is inside the
compressed region.

### Payload (`SaveContexte`, `SOURCES/SAVEGAME.CPP:1089`+)

| # | Field | On-disk | Notes | Oracle |
|---|-------|---------|-------|--------|
| P1 | `ListVarGame` | 256 x S16 = 512 B | quest/story flags | corpus |
| P2 | `ListVarCube` | 80 x U8 = 80 B | per-cube vars | corpus |
| P3 | `Comportement` | 1 B | | corpus |
| P4 | packed gold/zlitos | 4 B L | `(NbZlitosPieces<<16) \| (NbGoldPieces & 0xFFFF)` | corpus |
| P5 | `MagicLevel`,`MagicPoint`,`NbLittleKeys` | 3 x 1 B | | corpus |
| P6 | `NbCloverBox` | 2 B W | | corpus |
| P7 | `SceneStartX/Y/Z` | 3 x 4 B L | | corpus |
| P8 | `StartXCube/Y/Z` | 3 x 4 B L | | corpus |
| P9 | `Weapon` | 1 B | | corpus |
| P10 | `savetimerrefhr` | 4 B L | game timestamp | corpus |
| P11 | `NumObjFollow` | 1 B | | corpus |
| P12 | `SaveComportementHero`,`SaveBodyHero` | 2 x 1 B | | corpus |
| P13 | `TabArrow[0..304].FlagHolo` | (50+255) x U8 = 305 B | holomap flags | corpus |
| P14 | `TabInv[40]` | 40 x {S32 PtMagie, S32 FlagInv, U16 IdObj3D} = 400 B | inventory | corpus |
| P15 | `Checksum` | 4 B L | start of "incompatibilite" region | corpus |
| P16 | `LastMyFire/MyJoy/Input/JoyFlag` | 4 x 4 B L | | corpus |
| P17 | `Bulle`,`ActionNormal` | 2 x 1 B | | corpus |
| P18 | `InventoryAction`,`MagicBall` | 2 x 4 B L | | corpus |
| P19 | `MagicBallType`,`MagicBallCount` | 2 x 1 B | | corpus |
| P20 | `MagicBallFlags` | 4 B L | | corpus |
| P21 | `FlagClimbing` | 1 B | | corpus |
| P22 | `StartYFalling` | 4 B L | | corpus |
| P23 | `CameraZone` | 1 B | | corpus |
| P24 | `InvSelect`,`ExtraConque` | 2 x 4 B L | | corpus |
| P25 | `PingouinActif` | 1 B | | corpus |
| P26 | `PtrZoneClimb` | 4 B L | truncated live pointer, preserve-bytes | corpus |
| P27 | `ListDart[3]` | 3 x {7 x S32} = 84 B | field-by-field | corpus |
| P28 | `NbObjets` | 4 B L | | corpus |
| **P29** | `ListObjet[NbObjets]` | **NbObjets x 276 B** | 140 B scalar prefix + 136 B wire `Obj`; **writer TODO** | corpus |
| P30 | `NbPatches` | 4 B L | | corpus |
| P31 | patch blob | 4 B size + variable | per `ListPatches[].Size` (1/2/4/raw) into `PtrScene` | corpus + scene |
| **P32** | `NbExtras` (1 B) + `ListExtra[]` | 1 + NbExtras x 68 B | **writer TODO** (`T_EXTRA` raw today) | corpus |
| P33 | `NbZones` (4 B) + `ListZone[]` | 4 + NbZones x {4 x S32} | field-by-field | corpus |
| P34 | `NbIncrust` (1 B) + `ListIncrustDisp[]` | 1 + NbIncrust x 16 B | `T_INCRUST_DISP` raw, POD-safe | corpus |
| **P35** | `NbFlows` (1 B) + per-flow | 1 + [60 B flow + 1 B NbDots + NbDots x 40 B] | **flow writer TODO**; dots (`S_ONE_DOT`) POD-safe | corpus |
| P36 | camera block | `VueDistance`,`AlphaCam`,`BetaCam`,`GammaCam`,`AddBetaCam` = 5 x 4 B L | | corpus |
| P37 | `VueOffsetX/Y/Z` | 3 x 4 B L | | corpus |
| P38 | cinema block | `CinemaMode` (1 B) + `TimerCinema`,`LastYCinema`,`DebCycleCinema`,`DureeCycleCinema` (4 x 4 B L) | | corpus |
| P39 | `ClipWindowYMin/YMax` | 2 x 4 B L | after `RestoreClipWindow()` | corpus |
| P40 | `AnimateTexture` | 1 B | | corpus |
| P41 | `ParmSampleDecalage/Frequence/Volume` | 3 x 4 B L | | corpus |
| P42 | `NumBuggy` | 1 B | | corpus |
| P43 | `ListBuggy[2]` | 2 x 112 B = 224 B | `S_BUGGY` raw, POD-safe | corpus |
| P44 | `ListArdoise[5]` | 5 x S8 = 5 B | | corpus |
| P45 | `CurrentArdoise`,`NbArdoise` | 2 x 1 B | | corpus |
| P46 | `VueCamera` | 4 B L | | corpus |

The three **writer TODO** rows (P29 Obj tail, P32 extras, P35 flows) are the
entire production change. Everything else already writes host-independent bytes.

## ABI audit of every non-WIRE32 aggregate write

Each raw `LbaWrite(&x, sizeof x)` in `SaveContexte`, checked for hidden pointer
width or alignment dependence. "32==64" means identical layout on both ABIs.

| Struct (raw write) | Where | 32-bit size | Pointers? | 32==64? | Verdict |
|--------------------|-------|-------------|-----------|---------|---------|
| `Coord` (union) | obj prefix `:1214` | 12 B (3 x S32) | no | yes | POD-safe, keep raw; add size lock |
| `BoundAngle` (`BOUND_MOVE`) | obj prefix `:1244` | 20 B (5 x S32/U32, `pack(4)`) | no | yes | POD-safe, keep raw; add size lock |
| `ExeSwitch` (`T_EXE_SWITCH`) | obj prefix `:1262` | 4 B (U8,U8,S16) | no | yes | POD-safe, keep raw; add size lock |
| `Obj` (`T_OBJ_3D` - `CurrentFrame`) | obj tail `:1272` | 136 B (`pack(1)`) | **7** | **no** (164 B on 64-bit) | **needs `...ToWire32`** |
| `T_EXTRA` | extras `:1321` | 68 B (natural align, no pad) | **1** (`PtrBody`) | **no** (80 B on 64-bit) | **needs `...ToWire32`** |
| `S_PART_FLOW` | flows `:1369` | 60 B | **1** (`PtrListDot`) | **no** (64 B on 64-bit) | **needs `...ToWire32`** |
| `S_ONE_DOT` | flow dots `:1377` | 40 B (10 x S32) | no | yes | POD-safe, keep raw; add size lock |
| `T_INCRUST_DISP` | incrust `:1352` | 16 B (6 x S16 + U32) | no | yes | POD-safe, keep raw; add size lock |
| `S_BUGGY` | buggy `:1433` | 112 B (all S32) | no | yes | POD-safe, keep raw; add size lock |

The 7 pointer-width fields in `T_OBJ_3D` are `Body`, `NextBody`, `Anim`
(`T_PTR_NUM` union with `void*`), `Texture`, `NextTexture` (`void*`),
`LastOfsFrame`, `NextOfsFrame` (`PTR_U32` = `U32*`). Each grows 4 to 8 bytes on
64-bit, so 7 x 4 = 28 extra bytes per object (136 to 164), which is the
documented "28 bytes per object" stride delta. `LoopOfsFrame` is a plain `U32`
despite the name and does not grow.

The POD-safe structs are safe today but a future field addition that includes a
pointer would silently reintroduce the bug. Cheap insurance: `static_assert` each
of their 32-bit sizes so such a change breaks the build instead of a user's save.

## Independent retail layout tables (the non-circular anchor)

Derived from the real native struct definitions, not from the `_WIRE32` mirrors.
This is the "retail layout spec"; the `_WIRE32` structs are the thing under test
and are diffed against it below.

### `T_OBJ_3D` wire (from `LIB386/H/OBJECT/AFF_OBJ.H`, `pack(1)`, minus `CurrentFrame`)

All 34 fields are 4 B. Offsets are contiguous (packed). Total = **136 B**.

| off | field | native type | wire | note |
|----:|-------|-------------|------|------|
| 0..20 | X,Y,Z,Alpha,Beta,Gamma | S32 | S32 | |
| 24 | Body.Num | `T_PTR_NUM` | S32 | union, `.Num` view |
| 28 | NextBody.Num | `T_PTR_NUM` | S32 | |
| 32 | Anim.Num | `T_PTR_NUM` | S32 | |
| 36 | Texture | `void*` | U32 | pointer, discarded on read (refilled) |
| 40 | NextTexture | `void*` | U32 | discarded |
| 44 | LastOfsIsPtr | U32 | U32 | |
| 48 | LastFrame | S32 | S32 | |
| 52 | LastOfsFrame | `PTR_U32` | U32 | pointer, discarded |
| 56 | LastTimer | U32 | U32 | |
| 60 | LastNbGroups | U32 | U32 | |
| 64 | NextFrame | S32 | S32 | |
| 68 | NextOfsFrame | `PTR_U32` | U32 | pointer, discarded |
| 72 | NextTimer | U32 | U32 | |
| 76 | NextNbGroups | U32 | U32 | |
| 80 | LoopFrame | S32 | S32 | |
| 84 | LoopOfsFrame | U32 | U32 | plain U32, not a pointer |
| 88 | NbFrames | U32 | U32 | |
| 92..112 | LastAnimStepX/Y/Z/Alpha/Beta/Gamma | S32 | S32 | |
| 116 | Interpolator | U32 | U32 | |
| 120 | Time | U32 | U32 | |
| 124 | Status | U32 | U32 | |
| 128 | Master | U32 | U32 | |
| 132 | NbGroups | U32 | U32 | |

Diff vs `T_OBJ_3D_WIRE32`: **match** (36..40, 52, 68 are the discarded pointer
slots the existing struct already models as `U32`). Existing struct is correct.

### `T_EXTRA` wire (from `SOURCES/COMMON.H:672`, natural align, no `pack`)

32-bit natural alignment produces **zero internal padding** (every field lands on
its natural boundary), so the `pack(1)` mirror is byte-identical. Total = **68 B**.

| off | field | native type | note |
|----:|-------|-------------|------|
| 0..8 | PosX,PosY,PosZ | S32 | |
| 12 | U (union MOVE/Org) | 12 B | 3 x S32 either arm |
| 24 | Info | S32 | |
| 28 | PtrBody | `U8*` | **pointer**, discarded on read |
| 32..38 | Sprite,Vx,Vy,Vz | S16 | |
| 40 | Flags | U32 | |
| 44 | Timer | U32 | |
| 48..54 | Body,Beta,TimeOut,Divers | S16 | |
| 56..59 | Poids,HitForce,Owner,NewForce | U8 | |
| 60 | Impact | S32 | non-`LBA_EDITOR` arm |
| 64 | Scale | S32 | |

On 64-bit, `PtrBody` forces 4 B pad before it (24->28 Info, then align pointer to
32) + 4 B pointer growth + 4 B tail pad = 80 B. Diff vs `T_EXTRA_WIRE32`:
**match**.

### `S_PART_FLOW` wire (from `SOURCES/FLOW.H:50`, natural align)

14 x 4 B scalars then a trailing `S_ONE_DOT*`. 32-bit = **60 B**, no padding.

| off | field | native type | note |
|----:|-------|-------------|------|
| 0..12 | Flag,Owner,NumPoint,NbDot | S32 | |
| 16..24 | OrgX,OrgY,OrgZ | S32 | |
| 28..36 | XMin,YMin,ZMin | S32 | |
| 40..48 | XMax,YMax,ZMax | S32 | |
| 52 | FlowTimerStart | U32 | |
| 56 | PtrListDot | `S_ONE_DOT*` | **pointer**, discarded (restored externally) |

On 64-bit `PtrListDot` is 8 B (align 8 at offset 56, no pad) = 64 B. Diff vs
`S_PART_FLOW_WIRE32`: **match**.

### Per-object stride (P29): 276, not 278

The object scalar prefix is written field-by-field with explicit widths
(`SaveContexte:1201-1272`). Summed: **140 B**. Wire `Obj` tail: **136 B**. Total
per-object stride = **276 B** (32-bit), 304 B (64-bit native).

The heuristic hardcodes `SAVEGAME_PER_OBJECT_PREFIX 142u` +
`SAVEGAME_OBJ_SUFFIX_32 136u` = 278 (`SAVEGAME_LOAD_BOUNDS.CPP:49-50`). The
prefix should be **140**. Why the code tolerates the error: `LoadContexte` reads
the prefix field-by-field, so its cursor tracks the true 276 wire regardless; the
278 constant is used only for a `SaveLoadRoom` upper-bound pre-check (an
over-estimate is harmless) and as the `NbPatches` sniff stride (a wrong sniff
just defers to the validate-and-retry path). The 50-save corpus loading cleanly
via 276-consuming reads is itself proof that retail wrote 276: a 278 stride would
drift 2 bytes per object and fail `IndexFile3D` validation within a dozen
objects. Already documented as the "Empirical note (probe)" in
[SAVEGAME.md](SAVEGAME.md); `scripts/save_probe.py` walks 276/304.

**Lock target: 276 / 140 / 136. Never 278 / 142.** Phase 1's corpus round-trip
proves it byte-exact; Phase 3 corrects or retires the 142/278 constants.

## Container, compression, and screenshot oracles

### Envelope vs LBA2SD

LBA2SD describes the header as flag byte, then a "vestigial system-hour byte and
three zero bytes", then the NUL name, then the LE size. Reconciled against the
source: the 4 bytes LBA2SD reads as "system-hour + three zeros" are `NumCube`
(E2), a small `S32` scene index whose little-endian encoding is
`[low byte, 0, 0, 0]` for any cube < 256. Source wins; the field is `NumCube`,
not a timestamp. The flag byte (E1), NUL name (E3), and LE `sizefile` (E4) agree
with LBA2SD directly.

### LZSS codec

Params (from `SOURCES/LZSS.H`): `INDEX_BIT_COUNT 12` (window 4096),
`LENGTH_BIT_COUNT 4`, `BREAK_EVEN 1`, phrases 2..17 bytes. Decoder
(`ExpandLZ`, `LIB386/SYSTEM/LZ.CPP`) is called with `MinBloc=2`, so decoded match
length = `(token & 0x0f) + 2` and window index = `(src[1]<<4) | (src[0]>>4)`.
This matches LBA2SD's stated 12-bit index / 4-bit length. Encoder is
`Compress_LZSS` (`SOURCES/LZSS.CPP:211`), the original Adeline binary-tree LZSS
("ASSEZ FORTEMENT MODIFIE LE 21/06/93"), deterministic per call (globals
re-initialized in `InitTree` and the fill loop).

**Compressed byte-exactness hinges on encoder fidelity.** If `Compress_LZSS`
reproduces retail's exact token stream, then for any payload our writer produces
byte-identically, `compress(payload)` equals the original compressed bytes, and a
full-file compressed round-trip is byte-exact. If the encoder differs from
retail's (e.g. a different greedy tie-break), compressed saves will not
round-trip at the file level even with a correct payload. The all-`0xA4` corpus
is the direct oracle. See decision 4.

### Semantic layer: the LBA file-info wiki

The [LBA2:Savegame wiki](https://lbafileinfo.kaziq.net/index.php/LBA2:Savegame)
is a **semantic** oracle, not a layout one. It names what fields mean (quest and
story flags in `ListVarGame`, holomap room bits in `TabArrow`, inventory slots,
behaviour mode) and is the right reference for interpreting decoded values in a
human-readable way. It is not trustworthy for bytes: it carries documented drift
(field offsets off by a few bytes past the variable player name, the packed
gold/zlitos `S32` modelled as two `U16`, a `ScenePosZ`/`ScenePosY` typo, the
`Weapon` offset off by 3). Where wiki and source disagree, source wins.

Use for this task: validate that the section map's field *meanings* are right and
name `ListVarGame` slots when we surface them; do not use it to derive or check
byte offsets (that is the corpus + struct-source job). This split is already the
convention in [SAVEGAME.md](SAVEGAME.md), which annotates every wiki label as
"semantics only". The plan's byte layout comes from the frozen struct source and
the corpus; the wiki rides alongside as the meaning layer.

### Screenshot vs LBA2R2I / I2R

E5 is a raw 160x120 = 19200 B palette-indexed buffer (from `BufSpeak+150000`,
via `RemapPicture` which remaps indices to `PtrPalNormal` when the live palette
differs). The 256-entry palette is the fixed game palette and is not stored in
the save. LBA2R2I/I2R use 160x120 + a fixed 256-entry palette, so the byte
comparison is on the index buffer directly.

## Equivalence-lock design

Two layers, both copyright-safe (no retail data), both in public CI:

1. **Static size + offset locks.** `static_assert` (C++11 in the test TU, or a
   `typedef`-array trick in the C++98 engine TU) on:
   - `sizeof(T_OBJ_3D_WIRE32) == 136`, `sizeof(T_EXTRA_WIRE32) == 68`,
     `sizeof(S_PART_FLOW_WIRE32) == 60`.
   - `offsetof` for every field in the three tables above, so a field reorder or
     width change breaks the build.
   - The POD-safe structs: `sizeof(Coord)==12`, `BOUND_MOVE==20`,
     `T_EXE_SWITCH==4`, `S_ONE_DOT==40`, `T_INCRUST_DISP==16`, `S_BUGGY==112`.
   - The stride identity: `140 + sizeof(T_OBJ_3D_WIRE32) == 276`, and replace the
     `142` constant with `140`.
   The numbers are justified by the independent tables above, not by asserting a
   struct equals itself.

2. **Golden byte-vectors.** A small set of hand-authored 32-bit wire byte
   vectors (constructed from the layout tables, never extracted from a
   copyrighted save) that `...FromWire32` must decode to known field values and
   `...ToWire32` must reproduce byte-for-byte. This is what makes `FromWire32` a
   trustworthy round-trip partner and breaks the write/read-mirror-the-same-bug
   circularity.

## Decisions (resolved 2026-07-07)

1. **Version numbering: no bump.** Stay at v36; the canonical writer emits true
   retail-v36 bytes. Consequence: the version byte cannot distinguish retail-v36
   (32-bit wire) from port-64-v36 (64-bit native), so the reader is
   **canonical-first**: always read 32-bit retail wire; only on validation
   failure fall back to the legacy native read. The predictive stride sniff
   (`SaveLoadGuessObjectWireStride`) is no longer needed on the canonical path and
   can be deleted; what remains is a deterministic canonical read plus a
   legacy fallback.
2. **Port-64 legacy load: quarantine, warn, migrate.** The native 64-bit read
   path is deprecated but retained so existing port-64 saves still load. On
   detecting one (canonical read fails validation, native read succeeds), the
   engine warns and migrates (re-saves in canonical retail-wire format). This
   path must have test coverage (detection, warning, and migration).
   **Low-stakes / minimal / removable:** the port is pre-v1, so port-64 saves
   exist only as local dev/tester artifacts of the current incomplete-writer
   state. There is no external installed base and no external compatibility
   burden. The legacy path is a best-effort courtesy for local saves, kept
   deliberately minimal and clearly marked for eventual removal; its test is a
   focused detect-warn-migrate check, not an exhaustive compatibility matrix.
3. **Corpus: use the committed 50 as the CI oracle**; wire the larger MagicBall
   187-save set as an optional git-ignored, env-var-gated local job.
4. **Compressed contract: full-file byte-identity.** Compressed saves must
   round-trip byte-for-byte at the file level, which requires `Compress_LZSS` to
   reproduce retail's exact encoding. Any divergence is an encoder-fidelity bug to
   fix, not to work around. The all-`0xA4` corpus is the oracle.

The original decision write-ups (trade-offs) are retained below for reference.

### Decision 1: version numbering

Retail is `NUM_VERSION 36`. Community 64-bit builds may already emit "v36" with a
64-bit payload (an aberration this work removes on the write side).

- (a) Canonical writer emits true retail-v36 bytes; treat any existing port-64
  "v36" saves as a migration corpus. Simplest on-disk story (one v36, always
  retail-shaped); risk is that port-64 v36 saves already in the wild are
  indistinguishable by version byte from retail v36, so the reader cannot tell
  them apart by version alone (must rely on the existing stride sniff for legacy).
- (b) Bump canonical to v37; keep v36 readable under a legacy branch. Clean
  version dispatch (v37 = always retail-wire, heuristic-free; v36 = legacy path).
  [SAVEGAME.md](SAVEGAME.md) already names v37 as "the redesign target". Cost: a
  version bump and a documented compatibility matrix.

Recommendation to consider: (b), because it gives the reader an unambiguous
signal to run the heuristic-free canonical path vs the legacy path, which is what
makes "retire the heuristic from the canonical path" clean. Your call.

### Decision 2: port-64 legacy load

Saves the 64-bit port already wrote (native 64-bit payload, 304 B/object, 80 B
extras, 64 B flows):

- Keep the `stride64_host` / native-read path alive under a version or detection
  guard, quarantined off the canonical path, with `SAVEGAME_LOAD_BOUNDS` intact
  for these legacy inputs only.
- Or declare port-64 saves unsupported and delete the heuristic entirely.

This is coupled to decision 1: under 1(b) the legacy path is gated by
`version == 36`; under 1(a) it stays keyed on the sniff. How much port-64 save
data exists in the wild is the deciding input.

### Decision 3: corpus provenance and handling

The task says retail `.LBA` files stay local and git-ignored, with CI's gated job
reading a private corpus via an env var. **The repo already contradicts this**:
`tests/savegame/corpus/saves/steam_classic_2023/` has **50 git-tracked** retail
saves (all `0xA4` compressed), with an opt-in `!saves/steam_classic_2023/` in
`.gitignore` and a written license rationale (runtime state, no retail assets,
two PII names anonymized in place). Options:

- Accept the existing precedent: use the committed 50-save corpus as the
  public-CI round-trip oracle. Fastest path, but revisits the "no retail saves in
  repo" guardrail and should be confirmed against project policy / AGENTS.md
  "never add game assets" (the existing README argues saves are not assets).
- Keep the existing 50 as-is but add any new corpus (e.g. the 187-save
  MagicBall set) only under a git-ignored dir gated by an env var, matching the
  task's model for the augmented corpus.

Recommendation to consider: treat the committed 50 as the CI oracle (precedent
already set and reviewed), and wire the larger MagicBall corpus as an optional,
git-ignored, env-var-gated local job. Your call on whether the committed corpus
stays.

### Decision 4 (surfaced): compressed round-trip contract

Since the corpus is entirely compressed, "byte-identical round-trip" for it is
only reachable if `Compress_LZSS` reproduces retail's exact encoding. Define the
contract:

- (a) Assert full-file byte-identity for compressed saves too, treating any
  divergence as an encoder-fidelity bug to fix. Strongest guarantee; risks
  turning an encoder mismatch into a blocking failure.
- (b) Assert byte-identity on the decompressed payload (what `...ToWire32`
  actually governs), plus a separate compress/decompress identity test. Isolates
  the serializer contract from encoder fidelity.

Phase 1 will empirically report which holds on the corpus (re-compress one save,
compare). Recommendation: start with (b) as the hard contract and report (a) as
an observed property, promoting it to a hard assert only if the corpus shows the
encoder is byte-faithful.

## Existing infrastructure to reuse

- `tests/savegame/` host tests register via `register_host_test(...)`
  (`tests/CMakeLists.txt:14`); `test_load_bounds.cpp` and
  `test_save_name_validation.cpp` are the pattern to follow. Runs in `make test`,
  no retail data.
- `tests/savegame/corpus/` has the committed 50-save corpus, `build_manifest.py`
  (stride sniff classifier), `run_harness.py` (`--save-load-test` driver),
  `baseline_harness.py` (world-state goldens), and `.gitignore` opt-in policy.
  The corpus round-trip test slots in alongside these.
- `tools/save_decompress.cpp` (built at `build/tools/save_decompress`) and
  `scripts/save_probe.py` already decode the envelope + LZSS offline; the probe
  hardcodes the empirical 276/304 stride and is the reference walker.
- `SOURCES/CREDITS_PARSE.*` + `tests/credits/` are a precedent for a fixed-width
  disk mirror of a pointer-bearing struct (`BOUND_MOVE` serialized to fixed
  bytes) with a host test, mirroring what the three `...ToWire32` converters do.
- `docs/ABI.md` is the governing convention (paired `T_DISK` / field-by-field, no
  `sizeof(T)`-as-stride).

## Proposed phasing (post-approval)

Detail is deferred until the four decisions land; outline only.

- **Phase 1 (RED).** Layered failing suite, each naming its oracle: envelope
  encode/decode (LBA2SD), LZSS compress/decompress identity + params 12/4
  (LBA2SD), screenshot section (R2I), payload struct locks + golden byte-vectors
  (4a, must pass first), payload round-trip via the not-yet-existing `...ToWire32`
  (4b, red until Phase 2), and the gated full-corpus round-trip. First empirical
  task: confirm 276 byte-exact on the corpus and confirm/deny compressed
  byte-fidelity (decision 4). Commit red, stop for review (TDD gate).
- **Phase 2 (GREEN).** Implement the three `...ToWire32` converters as exact
  inverses of the `...FromWire32` readers; swap the three raw writes to route
  through them; add nothing no red test demands.
- **Phase 3 (REFACTOR).** Apply the version decision (dispatch the reader so the
  canonical path is heuristic-free); retain or quarantine `stride64_host` per
  decision 2; land the static_assert locks (with 140/276, not 142/278); update
  [SAVEGAME.md](SAVEGAME.md), this doc's outcomes, and `CHANGELOG.md` via PR
  title. Small commits: converters / writer swap / version dispatch / locks.

## Risks and open questions

- **Encoder fidelity (decision 4)** is the largest unknown for full-file
  compressed byte-exactness. Resolvable empirically in Phase 1.
- **Historic struct cross-check.** The tables above are derived from the current
  repo's native structs. The task asks to also diff against the historic 1997
  struct source (`../lba2-classic` sibling or this repo's pre-port history). The
  corpus round-trip is the stronger check (it tests against real retail bytes),
  but a struct-source diff is a cheap independent confirmation worth doing in
  Phase 1 before locking numbers.
- **`PtrZoneClimb` (P26)** and the discarded pointer slots are preserve-or-zero
  by design; the writer must emit a defined value (0) so byte-comparison is
  stable. The read side already discards them; the write side must not leak a
  live 64-bit pointer's low word into the byte stream if we want cross-host
  determinism (retail emitted a live 32-bit value here, so exact match to a
  specific retail file is not expected for this field, only self-consistent
  round-trip). Confirm the round-trip contract treats P26 as preserve-bytes.
- **Oracle availability.** LBA2SD / LBA2R2I / I2R are external LBALab
  repositories, not vendored here. Phase 1 either vendors the specific
  test vectors (copyright-safe: format constants and synthetic buffers only) or
  documents them as external references the synthetic tests encode by hand.
