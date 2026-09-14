---
type: Subsystem
title: Memory and resource loading
description: One allocation carved into eleven fixed regions, a screen-sized scratch region that several owners borrow in turn, and a resource loader that decompresses in place, so every destination it writes must hold the entry plus a 512-byte margin that no call site states.
status: draft
subsystem: memory
as_of: dc38d6bc
generated: { by: claude-code/claude-opus-5, at: 2026-09-14T14:00:00Z }
relates_to:
  - /subsystems/save.md
sources:
  - id: mem-cpp
    resource: ../../../SOURCES/MEM.CPP
    title: MEM.CPP, the region table, Mem_ConfigureScreenBuffers, InitMainBuffer and the isolate flag
  - id: hqfile-cpp
    resource: ../../../LIB386/SYSTEM/HQFILE.CPP
    title: HQFILE.CPP, HQF_LoadClose and HQF_ResSize
  - id: hqrload-cpp
    resource: ../../../LIB386/SYSTEM/HQRLOAD.CPP
    title: HQRLOAD.CPP, Load_HQR and LoadMalloc_HQR
  - id: hqrress-cpp
    resource: ../../../LIB386/SYSTEM/HQRRESS.CPP
    title: HQRRESS.CPP, the HQR_Get cache
  - id: hqrmload-cpp
    resource: ../../../LIB386/SYSTEM/HQRMLOAD.CPP
    title: HQRMLOAD.CPP, HQRM_Load and its margin comment
  - id: grille-cpp
    resource: ../../../SOURCES/GRILLE.CPP
    title: GRILLE.CPP, InitBufferCube and the brick flag table inside Screen
  - id: hologlob-cpp
    resource: ../../../SOURCES/HOLOGLOB.CPP
    title: HOLOGLOB.CPP, InitHoloMalloc and HoloMalloc over ScreenAux
  - id: config-file-cpp
    resource: ../../../SOURCES/CONFIG_FILE.CPP
    title: CONFIG_FILE.CPP, the config read into ScreenAux
  - id: diskfunc-cpp
    resource: ../../../SOURCES/DISKFUNC.CPP
    title: DISKFUNC.CPP, LoadSceneCubeXY reading a scene into ScreenAux
  - id: screen-cpp
    resource: ../../../LIB386/SVGA/SCREEN.CPP
    title: SCREEN.CPP, CreateScreenMemory and the separately allocated Log
  - id: res-switch-cpp
    resource: ../../../SOURCES/RES_SWITCH.CPP
    title: RES_SWITCH.CPP, the runtime resolution switch rebuilding the block
  - id: save-bounds-cpp
    resource: ../../../SOURCES/SAVEGAME_LOAD_BOUNDS.CPP
    title: SAVEGAME_LOAD_BOUNDS.CPP, the save reader's own staging check
  - id: bug-hunting
    resource: ../../BUG_HUNTING.md
    title: Bug hunting runbook, "Un-merge the arena"
  - id: probe
    resource: ../../../scripts/dev/probe_hqr_margins.py
    title: probe_hqr_margins.py, fixed destinations against the data they receive
---

The engine's memory is mostly not dynamic. A handful of large buffers are sized once from the render resolution and the game data, handed out of one allocation, and reused by whichever system needs them next; the resources that fill them come through one loader with one sizing rule. This concept holds the contracts and the traps of that arrangement. The loader's rule has [its own concept](/quirks/a-compressed-resource-decompresses-in-place.md), because it is the part that has been rediscovered.

# Principles

- **One allocation, fixed regions.** `InitMainBuffer` sums the region table, rounds each size up to 16, makes one allocation and slices it in table order. Every size is decided before that call: the screen buffers from the render resolution, the cube buffers from the background data. A resolution change rebuilds the whole block rather than resizing a region.[^mem-cpp] [^res-switch-cpp]
- **The loader decompresses in place.** A compressed resource is read past the end of its own decompressed size and expanded back towards the start of the destination, so the destination needs a 512-byte margin that the call site never mentions.[^hqfile-cpp]
- **The margin lives at the allocation, not at the load.** Where the engine gets it right, the `+ RECOVER_AREA` is in the size of the buffer (a region table row, a `#define`, an allocation), far from the `Load_HQR` that depends on it. Reading a call site cannot tell you whether it is safe.

# Contracts

| Contract | Statement |
|---|---|
| Region order | `PtrZBuffer`, `ScreenAux`, `BufSpeak`, `Screen`, `BufferMaskBrick`, `BufMap`, `TabBlock`, `ListFlowDots`, `PtrXplPalette`, `BufferSmack`, `ImageStaging`. Each region's overrun lands in the next one in this order, so `TabBlock` takes a spill from `BufMap`. `BufCube` and `BufferBrick` are not regions; they are slices of `PtrZBuffer`.[^mem-cpp] |
| Screen buffers | The Z-buffer is two bytes a pixel. `ScreenAux`, `BufSpeak` and `Screen` are one byte a pixel plus `RECOVER_AREA`. `ScreenAux` is floored at 640x480 plus the margin, because the holomap allocates against that budget, and `Screen` is floored so the brick flag table at a fixed offset inside it always fits. `Log` is not a region: it is its own allocation, one byte a pixel plus 512.[^mem-cpp] [^grille-cpp] [^screen-cpp] |
| The loader's margin | A destination passed to `Load_HQR` holds `SizeFile + RECOVER_AREA` bytes for a compressed entry and `SizeFile` for a stored one. `LoadMalloc_HQR` allocates the margin itself. `HQR_Get` sizes its cache at the largest entry plus the margin and places each new entry at the end of the used space, so a spill lands in free space or in the tail. A buffer sized at runtime uses `HQF_ResSize` plus the margin.[^hqrload-cpp] [^hqrress-cpp] |
| `ScreenAux` is scratch | No system owns it. The holomap bump-allocates its globe, textures and plan resources through it, the config reader parses `lba2.cfg` in it, and the buggy's scene lookup reads a scene into it. Whatever used it last owns its contents, and nothing may keep a pointer into it across a use by someone else.[^hologlob-cpp] [^config-file-cpp] [^diskfunc-cpp] |

# Seams

| Seam | Direction | Contract |
|---|---|---|
| Every `Load_HQR` caller | write into caller memory | The destination holds the entry plus the margin for a compressed entry. Owned by [the in-place decompression quirk](/quirks/a-compressed-resource-decompresses-in-place.md), which also records the destinations that deliberately have no margin. |
| The savegame reader | write into `Screen` | Stages its own decompression rather than calling the loader, with the compressed bytes at `SizeFile + RECOVER_AREA` past the output rather than the loader's offset, and checks the layout with `SaveLoadValidateCompressedStaging` before expanding, which `test_savegame_load_bounds` pins. Same margin, different offset, see [savegames](/subsystems/save.md).[^save-bounds-cpp] |
| The image loaders | write into `ImageStaging` | Load the 640x480 bitmap into staging, then copy it with the real row stride. The region is exactly 640x480 plus the margin, and a compressed full-screen image fills it to the byte, so it has no slack at all. |
| The runtime resolution switch | rebuild | Recomputes the screen sizes and calls `InitMainBuffer` again, so every pointer into the block is invalid across a switch.[^res-switch-cpp] |
| `LBA2_DBG_ISOLATE` | debug only | Gives each region its own allocation with 256 bytes of slack, so a spill between regions becomes a sanitizer report. Off by default, and the shipped layout is the merged one.[^mem-cpp] [^bug-hunting] |
| `scripts/dev/probe_hqr_margins.py` | read | The oracle for the loader's rule: the worst entry each fixed-size destination can receive, against its capacity, and the margin each compressed entry needs to decompress in place, each with a control that must fail. Needs retail data, so it runs locally.[^probe] |

# What it does not tell you

- **That an overrun is visible.** With the regions merged, a spill from one region into the next never leaves the allocation, and no sanitizer reports it.[^bug-hunting] Isolating the regions does not make every overrun visible either. A stack destination's overrun can reach past the function's own locals into memory ASan treats as addressable: in the frame description ASan compiles into the binary, the island plan's camera buffer sat at 32 to 112 and the compressed read covered 547 to 580, beyond both of the frame's locals, with no report. Only a Release build crashed.
- **That a header maximum bounds the data.** The background header's declared maximum map size is smaller than the largest map in at least one retail release, which is why the map and block buffers are sized from the entries themselves.[^grille-cpp]
- **That a destination without a margin is a bug.** Several are sized for entries that are stored in every release on hand, and the code says so beside three of them. They break if the data changes, not if the code does. [The quirk](/quirks/a-compressed-resource-decompresses-in-place.md) lists them.
- **That a margin comment is right.** `HQRM_Load` says its allocator already adds 500 bytes. `HQM_Alloc` adds nothing, and the margin is 512 anyway. Nothing calls `HQRM_Load`, so the comment is only a trap for whoever revives it. `LoadUsedBrick` uses 500 as well, where the bricks need at most 51.[^hqrmload-cpp]
- **That the sizes hold below 640x480.** `Log` and the screen buffers shrink with the resolution while the resources do not. The demo build's bumper loads a full-screen image straight into `Log`, which fits at 640x480 and above only. The catalog's sub-640 modes are an open question beyond this concept.
- **That nothing else writes past a buffer.** This concept covers the loader's margin and the region layout. A plain out-of-bounds index into a region, of which the holomap had two, is not a sizing problem and nothing here would catch it.

[^mem-cpp]: [SOURCES/MEM.CPP](../../../SOURCES/MEM.CPP), the `ListMem` table, `Mem_ConfigureScreenBuffers` and `InitMainBuffer` with its `LBA2_DBG_ISOLATE` branch.
[^hqfile-cpp]: [LIB386/SYSTEM/HQFILE.CPP](../../../LIB386/SYSTEM/HQFILE.CPP), `HQF_LoadClose`.
[^res-switch-cpp]: [SOURCES/RES_SWITCH.CPP](../../../SOURCES/RES_SWITCH.CPP), the switch that calls `Mem_ConfigureScreenBuffers` and `InitMainBuffer`.
[^grille-cpp]: [SOURCES/GRILLE.CPP](../../../SOURCES/GRILLE.CPP), `InitBufferCube`, the floor under `Screen` for `OFFSET_BUFFER_FLAG`, and the map and block buffers sized from the data.
[^screen-cpp]: [LIB386/SVGA/SCREEN.CPP](../../../LIB386/SVGA/SCREEN.CPP), `CreateScreenMemory`.
[^hqrload-cpp]: [LIB386/SYSTEM/HQRLOAD.CPP](../../../LIB386/SYSTEM/HQRLOAD.CPP), `Load_HQR` and `LoadMalloc_HQR`.
[^hqrress-cpp]: [LIB386/SYSTEM/HQRRESS.CPP](../../../LIB386/SYSTEM/HQRRESS.CPP), `HQR_Get`.
[^hologlob-cpp]: [SOURCES/HOLOGLOB.CPP](../../../SOURCES/HOLOGLOB.CPP), `InitHoloMalloc` and `HoloMalloc`.
[^config-file-cpp]: [SOURCES/CONFIG_FILE.CPP](../../../SOURCES/CONFIG_FILE.CPP), the `DefFileBufferInit` call over `ScreenAux`.
[^diskfunc-cpp]: [SOURCES/DISKFUNC.CPP](../../../SOURCES/DISKFUNC.CPP), `LoadSceneCubeXY`.
[^save-bounds-cpp]: [SOURCES/SAVEGAME_LOAD_BOUNDS.CPP](../../../SOURCES/SAVEGAME_LOAD_BOUNDS.CPP), `SaveLoadValidateCompressedStaging`.
[^bug-hunting]: [docs/BUG_HUNTING.md](../../BUG_HUNTING.md), "The rig", step 3.
[^probe]: [scripts/dev/probe_hqr_margins.py](../../../scripts/dev/probe_hqr_margins.py).
[^hqrmload-cpp]: [LIB386/SYSTEM/HQRMLOAD.CPP](../../../LIB386/SYSTEM/HQRMLOAD.CPP), `HQRM_Load`.
