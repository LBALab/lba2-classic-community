---
type: Quirk
title: A compressed resource decompresses in place, from past the end of its destination
description: HQF_LoadClose reads a compressed entry to dest + SizeFile - CompressedSizeFile + RECOVER_AREA and expands it back towards dest, so a destination must hold the entry plus 512 bytes; the original left that margin off the destinations it knew held stored entries, and three separate fixes have been this rule met from a different symptom.
status: draft
scope: "every Load_HQR into caller memory; compressed entries only, CompressMethod 1 or 2"
equivalence: untested
asm_origin: "LIB386/SYSTEM/HQFILE.CPP:HQF_LoadClose"
as_of: dc38d6bc
generated: { by: claude-code/claude-opus-5, at: 2026-09-14T14:00:00Z }
owner: /subsystems/memory.md
sources:
  - id: hqfile-cpp
    resource: ../../../LIB386/SYSTEM/HQFILE.CPP
    title: HQFILE.CPP, HQF_LoadClose
  - id: lz-h
    resource: ../../../LIB386/H/SYSTEM/LZ.H
    title: LZ.H, RECOVER_AREA "for overlapping ExpandLZ"
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, LIB386/SYSTEM/HQFILE.CPP with the same offset, and SOURCES/GRILLE.CPP with the two no-margin comments
  - id: grille-cpp
    resource: ../../../SOURCES/GRILLE.CPP
    title: GRILLE.CPP, InitBufferCube with its two "pas de RECOVER_AREA" comments, and LoadUsedBrick
  - id: loadisle-cpp
    resource: ../../../SOURCES/3DEXT/LOADISLE.CPP
    title: LOADISLE.CPP, IsleMapIndex allocated with "stored file no recover"
  - id: probe
    resource: ../../../scripts/dev/probe_hqr_margins.py
    title: probe_hqr_margins.py, the destination sweep and the margin each compressed entry needs
  - id: fix-grille
    resource: https://github.com/LBALab/lba2-classic-community/commit/ab589cbd
    title: "ab589cbd, fix(grille): size the map and block buffers from the data"
  - id: fix-holomalloc
    resource: https://github.com/LBALab/lba2-classic-community/commit/7077980f
    title: "7077980f, fix(holomap): give the holomap's loads the room the loader writes"
  - id: fix-holoplan
    resource: https://github.com/LBALab/lba2-classic-community/commit/1ae73d05
    title: "1ae73d05, fix(holomap): give the island plan's camera record the room the loader writes"
  - id: fix-palette
    resource: https://github.com/LBALab/lba2-classic-community/commit/dc38d6bc
    title: "dc38d6bc, fix(menu): give the PCX palette buffer the room the loader writes"
---

# Scope

Every load through `Load_HQR` into memory the caller sized, for an entry whose compression method is 1 (LZSS) or 2 (LZMIT). A stored entry is read straight into the destination and needs only its own size. `LoadMalloc_HQR` and the `HQR_Get` cache size their own buffers with the margin, so they are inside the rule rather than exposed to it.

# Evidence

`untested`, on the "checked against the original" row: the offset below is the same in the initial import's `HQF_LoadClose`, and so are the two comments that leave the margin off stored entries.[^original] `ExpandLZ` itself is under the ASM equivalence suite, but nothing tests the in-place layout or any destination's size.

The destination list is a data check, not a code test. `scripts/dev/probe_hqr_margins.py` takes every fixed-size destination, finds the worst entry it can receive, and compares, with the island plan's old 80-byte buffer as a control that must fail. On 2026-09-14 it passed every destination on the five releases with extracted data on hand (the working copy, Steam, GOG classic, GOG DOSBox and the demo) and caught the control on all five.[^probe]

The same script measures whether the margin itself is enough. For every compressed entry it walks the token stream and finds the smallest margin at which the output never overwrites an input byte not yet read, then decompresses the tightest entry in one buffer at that margin and one byte below, which must come out right and wrong respectively. The tightest entry in every release is a 26 KB sound in SAMPLES.HQR that barely compresses, needing 489 of the 512 bytes; the bricks need at most 51 of their 500.[^probe]

# Behaviour

`HQF_LoadClose` reads a compressed entry's bytes to `dest + SizeFile - CompressedSizeFile + RECOVER_AREA`, then calls `ExpandLZ` to write the decompressed entry from `dest` forwards. The input sits at the high end and the output grows towards it; the 512-byte `RECOVER_AREA` is the gap that keeps the writer from overtaking bytes it has not read yet, and how much of that gap an entry uses depends on how well it compressed. So the read touches up to `dest + SizeFile + RECOVER_AREA`, even though the entry is only `SizeFile` long.[^hqfile-cpp] [^lz-h]

The original sized its fixed destinations with this in mind, and left the margin off where it knew an entry was stored:

| Destination | Capacity | Entry | Why it fits |
|---|---|---|---|
| `BkgHeader`, `TabAllCube` | 28 bytes, 256 two-byte entries | LBA_BKG.HQR header and cube table | Stored, and the original says so at the load[^grille-cpp] |
| `IsleMapIndex` | 256 bytes | each island's map index | Stored, "stored file no recover" at the allocation[^loadisle-cpp] |
| `TabArrow` | 305 records | HOLOMAP.HQR arrow table | Stored |
| `sizeinfo`, `PtrSceneMem` | 16 and 4 bytes | RESS.HQR size info, SCENE.HQR maximum | Stored |

Every other fixed destination the sweep checks carries a margin, and several fill it to the byte: the sky textures, the island palettes, `ImageStaging` for a full-screen image, and the island plan's camera record.[^probe]

`LoadUsedBrick` repeats the layout by hand rather than calling the loader. It packs a cube's bricks one after another into `BufferBrick`, reading each compressed brick to `SizeFile - CompressedSizeFile + 500` past its slot, so every brick but the last spills into space the next one overwrites. Its switch handles stored and method 1 only; no brick in any release on hand uses method 2.[^grille-cpp]

# Why it is load bearing

- **The margin is the only thing between a load and its neighbour.** A destination one margin short does not fail to load; the read runs past it into whatever follows, a region of the main buffer or a caller's saved registers, and the damage shows up somewhere else.
- **The stored-only destinations depend on the data, not the code.** They are correct for every release on hand. A repack or a mod that recompresses one of those entries turns a working load into an overrun with no code change at all.
- **The layout is what makes one buffer enough.** Decompressing in place avoids a second buffer the size of the largest entry. Changing the loader to read into separate scratch would remove the margin rule everywhere, at the cost of that buffer and of diverging from the original; nobody has decided to do that, so the rule stands.

# How it has been met

The same rule, found three times from three symptoms, each fixed where it surfaced:

1. **Garbage in the block data after one cube.** The map and block buffers were sized from the background header's declared maxima, which one retail master exceeds by two bytes; the load spilled into the next region, and nothing reported it because the regions share one allocation.[^fix-grille]
2. **An overflow opening the holomap at a small resolution.** The holomap's bump allocator gave each load only its `SizeFile`, so every compressed resource ran past its own slot and the last one past `ScreenAux`.[^fix-holomalloc]
3. **The app closing when the holomap closes.** The island plan's 36-byte camera record was loaded into an 80-byte stack array; compressed for most islands, it overran by up to 468 bytes into the callers' saved registers. ASan with the regions isolated did not see it, a Release build crashed on macOS and on Android.[^fix-holoplan]

A fourth came from the sweep rather than a symptom. `PalettePcx` was declared `768 + 500`, so a compressed palette overruns it by 12 bytes however well it compresses. No shipped palette is compressed, so it was forced: a copy of SCREEN.HQR with palettes re-encoded as LZSS literals made ASan report the overflow, and on a macOS Release build the 12 bytes replaced `PtrPal`, which `EffectPcx` never reassigns, so the next dialog read the palette through a garbage pointer and crashed. The buffer now carries `RECOVER_AREA`.[^fix-palette]

# What it does not tell you

- **That a destination is safe because a comment says the entry is stored.** It is safe because the data on hand is stored. The sweep is the check, and it covers five releases with extracted files; the disc images were not scanned.
- **That 512 is enough for any data.** It is enough for every compressed entry on hand, with 23 bytes to spare on the tightest. An entry that compresses worse than anything shipped, from a repack or a mod, can need more, and the decompressor would then overwrite compressed bytes it has not read yet: a silently wrong entry, not an overrun.
- **That the margin is 500.** Two places use 500 where the constant is 512: `LoadUsedBrick` and the comment in the unused `HQRM_Load`. The bricks need at most 51 bytes. The palette buffer used 500 as well, until it was forced to crash.
- **That the sweep sees every destination.** Its table is maintained by hand from the call sites. A new `Load_HQR` into a fixed buffer is not covered until someone adds a row.
- **Anything about sizes below 640x480.** The destinations are checked against their declared capacities, and `Log` and the screen regions shrink with the resolution. The demo bumper's full-screen load into `Log` fits at 640x480 and above only.

[^original]: [The initial import](https://github.com/LBALab/lba2-classic-community/commit/333929ab), `HQF_LoadClose` in LIB386/SYSTEM/HQFILE.CPP and the two load comments in SOURCES/GRILLE.CPP.
[^hqfile-cpp]: [LIB386/SYSTEM/HQFILE.CPP](../../../LIB386/SYSTEM/HQFILE.CPP), `HQF_LoadClose`.
[^lz-h]: [LIB386/H/SYSTEM/LZ.H](../../../LIB386/H/SYSTEM/LZ.H), `RECOVER_AREA`.
[^grille-cpp]: [SOURCES/GRILLE.CPP](../../../SOURCES/GRILLE.CPP), `InitBufferCube` and `LoadUsedBrick`.
[^loadisle-cpp]: [SOURCES/3DEXT/LOADISLE.CPP](../../../SOURCES/3DEXT/LOADISLE.CPP), the `IsleMapIndex` allocation.
[^probe]: [scripts/dev/probe_hqr_margins.py](../../../scripts/dev/probe_hqr_margins.py), run against the five data directories.
[^fix-grille]: [ab589cbd](https://github.com/LBALab/lba2-classic-community/commit/ab589cbd), fix(grille): size the map and block buffers from the data.
[^fix-holomalloc]: [7077980f](https://github.com/LBALab/lba2-classic-community/commit/7077980f), fix(holomap): give the holomap's loads the room the loader writes.
[^fix-holoplan]: [1ae73d05](https://github.com/LBALab/lba2-classic-community/commit/1ae73d05), fix(holomap): give the island plan's camera record the room the loader writes.
[^fix-palette]: [dc38d6bc](https://github.com/LBALab/lba2-classic-community/commit/dc38d6bc), fix(menu): give the PCX palette buffer the room the loader writes.
