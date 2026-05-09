# ABI boundaries

LBA2's retail data files (HQR resources, save games) were authored against the original 1997 32-bit DOS ABI. Modern builds run on 64-bit, where some C struct types grow because pointer-sized fields go from 4 to 8 bytes. Reading retail data through grown structs misaligns every record after the first pointer-sized field — this is what caused [issue #65](https://github.com/LBALab/lba2-classic-community/issues/65) (endgame credits segfault) and motivated [PR #63](https://github.com/LBALab/lba2-classic-community/pull/63) (legacy save-game compatibility).

Truth hierarchy: code > this document > external sources.

This doc is scoped to **on-disk** struct layout. For 32→64-bit pointer hazards in **runtime** renderer math (`U32` + pointer arithmetic that loses 32-bit address-space wrap), see [PLATFORM.md §1 — Renderer-side wraparound](PLATFORM.md#renderer-side-wraparound). For the asan + gdb workflow used to catch these, see [CRASH_INVESTIGATION.md](CRASH_INVESTIGATION.md).

## The rule

> A struct whose layout is dictated by a retail file or a legacy save format must never assume `sizeof(T)` matches the on-disk record size.

Direct casts of file buffers to fat runtime structs are always wrong on 64-bit. The smell when this bites: two offsets that should differ read as identical, or all trailing fields read as `0`.

### Three patterns, each with a clear scope

**Pattern (1) — Paired on-disk type.** Define a sister `T_DISK` (or `T_WIRE32`) struct with explicit-width fields, no pointers, no embedded fat types, and pin its size with a static-size assert. Read into the disk type, then copy fields into the runtime type. Use this when the wire format is frozen and known — retail HQR records, or the 32-bit-era legacy save layout.

Existing examples:
- [`S_CRED_OBJ_2_DISK`](../SOURCES/CREDITS.H) — credits HQR records, used in [`SOURCES/CREDITS.CPP`](../SOURCES/CREDITS.CPP) (#65).
- `T_OBJ_3D_WIRE32` + `SavegameObj3dFromWire32` — embedded `T_OBJ_3D` blob in legacy saves, used in [`SOURCES/SAVEGAME.CPP`](../SOURCES/SAVEGAME.CPP) (#63).

**Pattern (2) — Field-by-field serialization.** Read or write each field at a known wire width (`LbaWriteByte`, `LbaWriteWord`, `LbaWriteLong`); the in-memory struct is irrelevant to the format. Use this when you control the writer — new save versions, any new format you're authoring. The format becomes a wire protocol, decoupled from any C struct.

Existing example: most of [`SaveContexte` / `LoadContexte`](../SOURCES/SAVEGAME.CPP) — see comments at lines 825–837 explicitly skipping `T_OBJ_3D` and pointer fields. Issue #64 extends this pattern into a fully canonical save format.

**Pattern (3) — Tolerant read with stride retry.** When the writer's ABI is fundamentally unknown at read time — typically player-authored saves spanning eras (32-bit DOS retail, 32-bit modern, 64-bit modern) — pick a candidate stride, read at it, validate a discriminating field per record, rewind and retry the alternate stride on mismatch. Only fail if every candidate fails.

Existing example: `LoadContexteReadObjectsAtStride` + `SaveLoadGuessObjectWireStride` in [`SOURCES/SAVEGAME.CPP`](../SOURCES/SAVEGAME.CPP) (#63). Validates `IndexFile3D` per object as the discriminator.

This is not a fallback for sloppy parsing — it's the right answer when player saves authored by older binaries must remain loadable. New formats should use pattern (2) and avoid the need entirely.

### Bounded reads (orthogonal safety layer)

Independent of which of the three patterns you use, untrusted on-disk input must not be able to walk off the buffer. PR #63 introduced `SaveLoadSetReadLimit` + cursor-aware `LbaRead*` macros that return `SAVELOAD_CTX_ERR` on overrun instead of segfaulting, plus `MAX_*` range checks on every count field. Apply the same discipline to any new file-load site that consumes count-prefixed arrays.

## Catalogue of fat types

These types contain pointer-sized fields and are larger on 64-bit than on 32-bit:

| Type | Defined in | Why it's fat |
|------|------------|---------------|
| `T_OBJ_3D` | [`LIB386/H/OBJECT/AFF_OBJ.H`](../LIB386/H/OBJECT/AFF_OBJ.H) | 3× `T_PTR_NUM` + 2× `void*` + 2× `PTR_U32` = 7 pointer-sized fields. 32-bit: 376 B; 64-bit: 404 B (+28). |
| `T_PTR_NUM` (union) | `AFF_OBJ.H:22` | `union { void* Ptr; S32 Num; }` — sized to the larger member. |
| Any struct embedding the above by value | — | Inherits the size delta. |

### Embedders of `T_OBJ_3D` (audited)

| Struct | File | File-backed? | Status |
|--------|------|--------------|--------|
| `S_CRED_OBJ_2` | [`SOURCES/CREDITS.H`](../SOURCES/CREDITS.H) | Yes — `lba2.hqr` index 0 | Fixed (#65). On-disk variant `S_CRED_OBJ_2_DISK` exists; runtime parser uses it. |
| `T_OBJET` | [`SOURCES/DEFINES.H:387`](../SOURCES/DEFINES.H) | No — runtime only; save uses field-by-field | Safe. |
| `T_OBJET` (3DEXT MOUNFRAC) | [`SOURCES/3DEXT/DEFINES.H:26`](../SOURCES/3DEXT/DEFINES.H) | No — gated `#ifdef MOUNFRAC` | Safe. |

If you add a new struct that embeds a fat type and intend to read it from disk, add a `T_DISK` paired type and a `static_assert`-equivalent. If you only need it at runtime, no action required.

## Compile-time guards

C++98 doesn't have `static_assert` as a keyword, so use the typedef-array idiom:

```c
typedef char ABI_assert_T_size[(sizeof(T) == EXPECTED_BYTES) ? 1 : -1];
typedef char ABI_assert_T_offset[(offsetof(T, Field) == EXPECTED_OFFSET) ? 1 : -1];
```

On a violation the build fails with `array size is negative`. Existing examples in [`SOURCES/CREDITS.CPP`](../SOURCES/CREDITS.CPP) (top of file) lock `S_CRED_INFOS_2`, `S_CRED_OBJ_2_DISK`, and the `OffBody`/`OffAnim` offsets.

`offsetof` requires `#include <cstddef>`.

## What's in scope vs out of scope

| In scope | Out of scope |
|---|---|
| Reading retail HQR data into typed structs | Pure runtime structs that never touch disk |
| Reading legacy `.lba` saves authored by 32-bit binaries | Format design for *new* save versions — see [issue #64](https://github.com/LBALab/lba2-classic-community/issues/64) |
| Cross-platform persistence of any binary blob | Text/JSON formats |

## Reviewing a new file-load site

Checklist when adding a `Load_HQR` / `LoadMalloc_HQR` / `fread` call site:

1. **Are you reading or writing?** If writing a new format, default to pattern (2). Don't author new formats that need pattern (1) or (3).
2. **What's the wire layout source?**
   - Retail HQR / frozen legacy → pattern (1).
   - You control it (new save version) → pattern (2).
   - Player files written by binaries you don't control, possibly in different ABIs → pattern (3), with validation.
3. **What type is the buffer cast to?** Does it contain `T_PTR_NUM`, `void*`, `PTR_U32`, function pointers, or embed `T_OBJ_3D`?
   - If yes and pattern (1): define a paired `T_DISK` / `T_WIRE32` type, lock its size with the typedef-array assert, cast the file buffer to it, copy fields into the runtime type. Never advance pointers with `sizeof(T)` — use `sizeof(T_DISK)`.
   - If no (all fixed-width fields): still add a size assert as a contract.
4. **Bounded reads.** Use `SaveLoadSetReadLimit` + `LbaRead*` (or equivalent) for any count-prefixed array. Range-check counts against `MAX_*` constants before allocating.

## Related work

- [Issue #65](https://github.com/LBALab/lba2-classic-community/issues/65) / [PR #66](https://github.com/LBALab/lba2-classic-community/pull/66) — endgame credits segfault. Worked example of pattern (1) on retail HQR data.
- [Issue #62](https://github.com/LBALab/lba2-classic-community/issues/62) / [PR #63](https://github.com/LBALab/lba2-classic-community/pull/63) — legacy save load hardening. Demonstrates patterns (1) (`T_OBJ_3D_WIRE32`), (2) (most of `SaveContexte`), and (3) (`LoadContexteReadObjectsAtStride`) coexisting in one read path, plus the bounded-reads safety layer (`SaveLoadSetReadLimit`).
- [Issue #64](https://github.com/LBALab/lba2-classic-community/issues/64) — canonical portable save format. The long-term direction: define `NUM_VERSION 37+` purely via pattern (2) so new saves never need pattern (3). Legacy loaders for older saves stay in place.
