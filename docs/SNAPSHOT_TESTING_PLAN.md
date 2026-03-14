# Scene Snapshot Testing Framework — Plan & Progress

## TL;DR

A framework that captures the complete rendering state during gameplay (camera,
lights, objects, bodies, textures, palette, CLUTs) into a structured binary
file via **PrintScreen** key, then replays that state through **two separate
programs** — one linked against the original ASM library, one against the CPP
port — and compares the output framebuffers byte-for-byte.

---

## Architecture

### Two-Program Approach

Instead of a single test binary with renamed ASM symbols, we build **two
separate programs** that each load the same snapshot and render through their
respective pipeline:

- **`replay_snapshot_asm`** — links only `libasm_lib386.a` (all ASM files
  assembled with UASM)
- **`replay_snapshot_cpp`** — links only the existing CPP `lib386` libraries

No symbol renaming needed. Each binary only has one implementation. The
comparison script runs both on the same `.lba2snap` file and `cmp`s the raw
framebuffer output.

### Snapshot File Format (`.lba2snap`)

Structured binary with section headers:

| Section         | Contents                                              |
|-----------------|-------------------------------------------------------|
| `SHRD`          | Camera, projection, matrix, lights, screen, clip, fog |
| `PALT`          | PtrTruePal + Fill_Logical_Palette (256+256 bytes)     |
| `CLTG`          | PtrCLUTGouraud (256×256 = 65536 bytes)                |
| `CLTF`          | PtrCLUTFog (65536 bytes)                              |
| `TEXT`          | Texture page data (raw pixels)                        |
| `OBJS`          | Per-object: position, rotation, animation state       |
| `BODY`          | Deduplicated raw body blobs (T_BODY_HEADER + data)    |
| `TOLN`          | TabOffLine scanline offset table                      |
| `SORT`          | Object render order (from TreeInsert/BaseSort)        |
| `END!`          | End marker                                            |

---

## Phase 1: File Format & Core Implementation — DONE

### Files Created

| File | Purpose |
|------|---------|
| `LIB386/H/SNAPSHOT/SNAPSHOT.H` | Format structs, section IDs, API declarations |
| `LIB386/SNAPSHOT/SNAPSHOT.CPP` | Load, ApplyState, Free, ComputeBodySize |
| `LIB386/SNAPSHOT/CMakeLists.txt` | Build snapshot library module |
| `SOURCES/SNAPSHOT_CAPTURE.H` | Declares `Snapshot_CaptureScene()` |
| `SOURCES/SNAPSHOT_CAPTURE.CPP` | Capture implementation (game-side, accesses `ListObjet[]`) |

### Key Design Decisions

- **Capture in SOURCES/, Load in LIB386/:** Capture needs access to `T_OBJET`,
  `ListObjet[]`, `BufferTexture` (game-side). Load/Apply/Free only need
  LIB386 rendering globals.
- **Body data stored raw:** Byte-for-byte copy of in-memory T_BODY_HEADER +
  sub-arrays. No re-parsing needed on load.
- **Texture page stored raw:** RepMask=0xFFFF → 256×256 = 65536 bytes.
- **CLUTs stored raw:** PtrCLUTGouraud and PtrCLUTFog, 65536 bytes each.
- **Body deduplication:** Multiple objects sharing the same body store one copy.

---

## Phase 2: In-Game Capture Trigger — DONE

### Changes

| File | Change |
|------|--------|
| `SOURCES/GLOBAL.CPP` | Added `S32 SnapshotRequested = 0;` global |
| `SOURCES/C_EXTERN.H` | Added `extern S32 SnapshotRequested;` |
| `SOURCES/PERSO.CPP` | Added **Alt+F9** key check (after `#endif // debug tools`) |
| `SOURCES/OBJECT.CPP` | Added `#include "SNAPSHOT_CAPTURE.H"` and capture call in `AffScene()` |
| `SOURCES/CMakeLists.txt` | Added `SNAPSHOT_CAPTURE.CPP` to sources; linked `snapshot` library |
| `LIB386/CMakeLists.txt` | Added `add_subdirectory(SNAPSHOT)` |

### How It Works

1. During gameplay, press **Alt+F9**
2. `SnapshotRequested` flag is set to 1
3. At the start of `AffScene()` (before rendering), the flag is checked
4. `Snapshot_CaptureScene("snapshot_NNNN.lba2snap")` writes the file
5. Counter auto-increments: `snapshot_0000.lba2snap`, `snapshot_0001.lba2snap`, ...

**Note:** The trigger uses `Key` (raw SDL scancode) + `SDL_GetModState()` for modifier
detection, matching the pattern used by `CheckSavePcx()` (F9 screenshot). Alt+F9
avoids conflict with the existing F9 PCX screenshot feature.

---

## Phase 3: Replay Programs & Docker Tests — DONE (framework)

### Files Created

| File | Purpose |
|------|---------|
| `tests/SNAPSHOT/snapshot_replay.h` | Shared replay API header |
| `tests/SNAPSHOT/snapshot_replay.cpp` | Load snapshot → apply state → render → write FB |
| `tests/SNAPSHOT/replay_snapshot_main.cpp` | `main()` for both replay executables |
| `tests/SNAPSHOT/compare_snapshots.sh` | Runs both programs, `cmp` output, reports diff |
| `tests/SNAPSHOT/CMakeLists.txt` | Builds `libasm_lib386.a` + both replay programs |
| `tests/SNAPSHOT/fixtures/.gitkeep` | Directory for `.lba2snap` fixture files |
| `tests/CMakeLists.txt` | Added `add_subdirectory(SNAPSHOT)` |

### Replay Flow

```
replay_snapshot_{asm|cpp} <snapshot.lba2snap> <output.raw>
  1. Snapshot_Load(filename, &snap)
  2. Snapshot_ApplyState(&snap)          → sets all rendering globals
  3. allocate_buffers(width, height)     → Log = framebuffer, PtrZBuffer = zbuf
  4. For each object: ObjectDisplay() or BodyDisplay()
  5. Write framebuffer to output file
```

### Docker Integration

```bash
./run_tests_docker.sh test_snapshot_*    # Run all snapshot comparison tests
```

---

## Phase 4: TODO — First Real Snapshot

- [ ] Build game with `DEBUG_TOOLS` enabled
- [ ] Run game, navigate to an interesting scene, press PrintScreen
- [ ] Copy `snapshot_0000.lba2snap` to `tests/SNAPSHOT/fixtures/`
- [ ] Run `./run_tests_docker.sh test_snapshot_0000` — expect PASS (or fix CPP bugs)

---

## Phase 5: TODO — Enhancements

- [ ] **Animation frame data capture:** Currently animation keyframe blobs
  (pointed to by `LastOfsFrame`/`NextOfsFrame`) are not captured. Need to
  determine blob sizes from HQR resource entries and include them.
- [ ] **Sort order capture:** Currently objects are rendered in sequential
  order. Capturing the `TreeInsert`/`BaseSort` sort order would more
  faithfully reproduce the painter's algorithm.
- [ ] **Visual diff output:** When a mismatch is detected, dump both
  framebuffers as BMP files for visual inspection.
- [ ] **Multiple snapshot fixtures:** Build a test corpus covering various
  scenes (indoor, outdoor, cutscenes, menus, etc.)
- [ ] **CI integration:** Add snapshot tests to CI pipeline.

---

## Key Files Reference

| File | Role |
|------|------|
| [LIB386/H/SNAPSHOT/SNAPSHOT.H](../LIB386/H/SNAPSHOT/SNAPSHOT.H) | Format definitions |
| [LIB386/SNAPSHOT/SNAPSHOT.CPP](../LIB386/SNAPSHOT/SNAPSHOT.CPP) | Load / Apply / Free |
| [SOURCES/SNAPSHOT_CAPTURE.CPP](../SOURCES/SNAPSHOT_CAPTURE.CPP) | Game-side capture |
| [tests/SNAPSHOT/snapshot_replay.cpp](../tests/SNAPSHOT/snapshot_replay.cpp) | Test-side replay |
| [tests/SNAPSHOT/compare_snapshots.sh](../tests/SNAPSHOT/compare_snapshots.sh) | Comparison script |
| [tests/SNAPSHOT/CMakeLists.txt](../tests/SNAPSHOT/CMakeLists.txt) | Test build system |

---

## Globals Captured in Snapshot

**Camera:** CameraX/Y/Z, CameraAlpha/Beta/Gamma, CameraXr/Yr/Zr, CameraZrClip, NearClip  
**Projection:** TypeProj, XCentre/YCentre, FRatioX/Y, LFactorX/Y  
**Matrix:** MatriceWorld (TYPE_MAT, 48 bytes)  
**Light:** AlphaLight/BetaLight/GammaLight, NormalX/Y/ZLight, CameraX/Y/ZLight, LightNormalUnit, FactorLight  
**Screen:** ModeDesiredX/Y, ScreenPitch, TabOffLine[]  
**Clip:** ClipXMin/YMin/XMax/YMax  
**Fill:** Fill_Flag_ZBuffer/Fog/NZW, Fill_ZBuffer_Factor, Fill_Z_Fog_Near/Far  
**Texture:** RepMask, full texture page data  
**Palette:** PtrTruePal[256], Fill_Logical_Palette[256]  
**CLUTs:** PtrCLUTGouraud[65536], PtrCLUTFog[65536]  
**Per Object:** X/Y/Z, Alpha/Beta/Gamma, body data (raw), animation state (T_OBJ_3D fields + CurrentFrame[30])
