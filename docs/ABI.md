# ABI Boundaries

LBA2's retail data files (HQR resources, save games) were authored against the original 1997 32-bit DOS ABI. Modern builds run on 64-bit, where some C struct types grow because pointer-sized fields go from 4 to 8 bytes. Reading retail data through grown structs misaligns every record after the first pointer-sized field — this is what caused [issue #65](https://github.com/LBALab/lba2-classic-community/issues/65) (endgame credits segfault) and motivated [PR #63](https://github.com/LBALab/lba2-classic-community/pull/63) (legacy save-game compatibility).

Truth hierarchy: **code > this document > external sources**.

## The rule

> **A struct whose layout is dictated by a retail file or a legacy save format must never assume `sizeof(T)` matches the on-disk record size.**

Two acceptable patterns:

1. **Paired on-disk type.** Define a sister `T_DISK` struct with explicit-width fields (no pointers, no embedded "fat" types) and a `static_assert`-equivalent that pins its size. Read into the disk type, then copy fields into the runtime type. Worked example: [`S_CRED_OBJ_2_DISK`](../SOURCES/CREDITS.H), used in [`SOURCES/CREDITS.CPP`](../SOURCES/CREDITS.CPP).
2. **Field-by-field serialization.** Read or write each field at a known wire width (`LbaWriteByte`, `LbaWriteWord`, `LbaWriteLong`). The in-memory struct is irrelevant to the format. Worked example: [`SaveContexte` / `LoadContexte`](../SOURCES/SAVEGAME.CPP) — see the comments at lines 825–837 explicitly skipping `T_OBJ_3D` and pointer fields.

Direct casts of file buffers to "fat" runtime structs are **always** wrong on 64-bit. The smell when this bites: two offsets that should differ read as identical, or all trailing fields read as `0`.

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

1. What type is the buffer cast to?
2. Does the type contain `T_PTR_NUM`, `void*`, `PTR_U32`, function pointers, or embed `T_OBJ_3D`?
3. If yes:
   - Define a paired `T_DISK` type with explicit-width fields and lock its size with the typedef-array assert above.
   - Cast the file buffer to `T_DISK*`, then copy the fields you need into the runtime `T*`.
   - Do **not** advance pointers using `sizeof(T)` — use `sizeof(T_DISK)`.
4. If no (all fields are fixed-width integers/chars): still consider adding the size assert as a contract.

## Related work

- [Issue #65](https://github.com/LBALab/lba2-classic-community/issues/65) / [PR #66](https://github.com/LBALab/lba2-classic-community/pull/66) — endgame credits segfault, the worked example for pattern (1).
- [PR #63](https://github.com/LBALab/lba2-classic-community/pull/63) — legacy save compatibility, the worked example for pattern (2).
- [Issue #64](https://github.com/LBALab/lba2-classic-community/issues/64) — canonical portable save format, the long-term direction for serialization. This document is intended as scaffolding that #64 can extend with the canonical wire schema.
