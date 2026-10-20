---
type: Quirk
title: A landed 3D extra is placed by the body header's padding, not its YMin
description: When a thrown 3D object lands, GereExtras hands ReajustPosExtra a pointer to byte 6 of the body header, the S16 Dummy, where the sprite path beside it points at the sprite's Y-min; the 1997 DOS executable does the same, Dummy is 0 in every shipped body and the dart's YMin is 0 too, so the object's origin is put on the floor and nothing differs in play.
status: draft
scope: "a 3D extra (a dart) landing on the floor, interior or exterior; the other body path is unreachable with the original callers"
equivalence: untested
asm_origin: "SOURCES/EXTRA.CPP:GereExtras"
generated: { by: claude-code/claude-opus-5-5, at: 2026-10-20T20:00:00Z }
owner: /subsystems/movement.md
sources:
  - id: extra
    resource: ../../../SOURCES/EXTRA.CPP
    title: EXTRA.CPP, the two landing calls in GereExtras, ReajustPosExtra, ExtraBonus
  - id: extfunc
    resource: ../../../SOURCES/EXTFUNC.CPP
    title: EXTFUNC.CPP, ReajustPosExtraExt, the exterior landing
  - id: header
    resource: ../../../LIB386/H/OBJECT/AFF_OBJ.H
    title: AFF_OBJ.H, T_BODY_HEADER
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, the same two calls and PtrBody as U8 *
---

# Scope

A 3D extra that stops on the floor. In practice that is the dart: `GereExtras` calls the landing routine for a dart that falls onto the floor, and in the `EXTRA_STOP_COL` branch, where the code also has a body path. Only `ExtraBonus` sets `EXTRA_STOP_COL`, and it sets `Body` to -1, so that body path is never taken.[^extra] Interiors land through `ReajustPosExtra`, exteriors through `ReajustPosExtraExt`; both subtract the value the pointer gives from the floor height.[^extra][^extfunc]

# Evidence

`untested`, checked against the original rather than a test, on two counts:

- **The initial import** has both calls in the same form, and `PtrBody` is `U8 *` there too.[^original]
- **The 1997 DOS executable**, `LBA2.DOS` from the GOG release (Watcom LE, build string "Jul 03 1997"), read with a disassembler. The landing routine is called through a function pointer at `0xc49e8`, which holds `ReajustPosExtraExt` (`0x1a3dc`); that routine reads the pointer as an `S16`. In `GereExtras` (`0x1ece0`) the dart call at `0x1fc0a` passes `PtrBody + 6`, and the call at `0x1fdc8` passes the sprite table entry plus 8 for a sprite and `PtrBody + 2 + 4` for a body. The extra's field offsets the code uses (`PtrBody` +0x1c, `Sprite` +0x20, `Flags` +0x28, `Timer` +0x2c, `Body` +0x30) match the 1997 `T_EXTRA`.

The data was read once, not by a tool in the tree: `Dummy` is 0 in all 99 bodies of `OBJFIX.HQR` in the GOG DOS release and all 70 in the demo, and the dart (entry 61) has `YMin` 0.

# Behaviour

Both call sites build the pointer as `(S16 *)(PtrBody + 2)` and then add 2 elements. `PtrBody` is a `U8 *`, so the pointer lands on byte 6 of `T_BODY_HEADER`, the `S16 Dummy` after `SizeHeader`. `YMin` is an `S32` at byte 16.[^header] The sprite path beside it uses the same two steps on the `S16` sprite table and lands on that sprite's Y-min.[^extra]

So a landed body is placed at the floor height minus `Dummy`, which is 0: its origin sits on the floor. For the dart that is also floor minus `YMin`, because the dart's `YMin` is 0.

The same "skip one `S16`, then two" lands on Y-min in LBA1's body header, whose bounds are `S16` fields straight after a 2-byte info word. That would explain the offset. It is an inference; nothing in the source says so.

# Why it is load bearing

- **It reads as a bug, and the fix changes nothing that ships.** Pointing the call at `YMin` gives the same position for the dart, and the other body path never runs. A change here is a change for mods, not for the game.
- **It would matter for a replacement dart.** A dart body whose origin is not at its lowest point would sink into the floor, or float, by that distance. With the original reading it sinks by `-YMin`; the fix would sit it on the floor.
- **A pointer type changed during the port would move it silently.** If `PtrBody` became an `S16 *` or a typed header pointer, `+ 2` would no longer mean byte 2, and a reader would get `XMin` or a pointer past the header without any compile error.

# What it does not tell you

- **What the authors meant.** Nothing in the source comments the offset.
- **Whether any other thrown body ever reaches the landing code.** Life scripts and animations throw other bodies through `ThrowExtraObj`, but those start without `EXTRA_STOP_COL` and are not darts, so by reading they explode or vanish rather than land. That was read, not played.
- **The 2022 Classic release.** Not checked.

[^extra]: [SOURCES/EXTRA.CPP](../../../SOURCES/EXTRA.CPP), `GereExtras`, the `EXTRA_DART` case and the `EXTRA_STOP_COL` block; `ReajustPosExtra`; `ExtraBonus`.
[^extfunc]: [SOURCES/EXTFUNC.CPP](../../../SOURCES/EXTFUNC.CPP), `ReajustPosExtraExt`.
[^header]: [LIB386/H/OBJECT/AFF_OBJ.H](../../../LIB386/H/OBJECT/AFF_OBJ.H), `T_BODY_HEADER`.
[^original]: Commit 333929ab, the initial import, [SOURCES/EXTRA.CPP](../../../SOURCES/EXTRA.CPP) and [SOURCES/COMMON.H](../../../SOURCES/COMMON.H), `T_EXTRA`.
