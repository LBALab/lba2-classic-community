# The engine/game interface — the Life/Track script VM

> **Status: DRAFT / living document (v0).** Companion to [ENGINE_GAME_SEAM.md](ENGINE_GAME_SEAM.md)
> and [ARCHITECTURE.md](ARCHITECTURE.md). This documents the membrane between the reusable
> engine and the game: the bytecode VM that runs the game's behaviour. Truth hierarchy:
> code > this document.

## Summary

The engine/game membrane is a **per-object bytecode VM** with two cooperating script types:

- **Life** — behaviour / AI. *Decides.* (`SOURCES/GERELIFE.CPP`, `DoLife`)
- **Track** — movement / pathing. *Executes.* (`SOURCES/GERETRAK.CPP`, `DoTrack`)

Both are hand-rolled `switch`-on-opcode interpreters over a byte cursor. Scripts are
compiled byte-streams embedded **inline in the per-cube scene file**, pointed to directly
from each object's struct. The VM's "syscalls" are direct C calls and direct reads/writes
of engine globals — **there is no isolation layer**. The same VM exists in LBA1; LBA2 is a
strict superset.

## 1. Two script types, dispatched per object per frame

Both run inside the main loop `MainLoop()` (`SOURCES/PERSO.CPP:437`), in its per-object
loop (`for i in 0..NbObjets`, `PERSO.CPP:1548`). Order per object:

1. `DoDir(i)` — movement/rotation physics
2. **Track**: `if (OffsetTrack != -1) DoTrack(i);` (`PERSO.CPP:1593`)
3. `PtrDoAnim(i)` — advance animation
4. **Life**: `if (OffsetLife != -1) DoLife(i);` (`PERSO.CPP:1637`)

So **Track runs before Life** each frame; both halt when their program counter is `-1`.
This is the same membrane LIFECYCLES.md shows as main-loop steps 3 and 6.

- **Life** runs opcodes in a `while` loop *until it hits a yield/terminator*
  (`LM_RETURN`, `LM_END_COMPORTEMENT`, `LM_SUICIDE`, …). It is decision logic: evaluate
  conditions, set state, fire dialogue, change cubes, assign Track programs.
- **Track** is a **coroutine**: opcodes like `TM_GOTO_POINT`, `TM_WAIT_ANIM`,
  `TM_WAIT_NB_SECOND` rewind the PC and break to yield until next frame; `OffsetTrack`
  persists across frames.

Life hands a movement program to an object via `LM_SET_TRACK` / `LM_VAR_GAME_TO_TRACK`.
**Life decides, Track executes.**

## 2. Three-table design and opcode counts

Conditionals are not single opcodes — they are composed from three separate tables
(`SOURCES/COMMON.H`):

| Table | Kind | Count | Evaluated by |
|---|---|---|---|
| `LM_*` | Life **statements** | 155 defined (0–154), ~142 handler cases | the `switch` in `DoLife` |
| `LF_*` | Life **queries/expressions** | 46 | `DoFuncLife` → result in global `Value` + `TypeAnswer` |
| `LT_*` | **comparison operators** | 6 | `DoTest` |
| `TM_*` | Track **statements** | 53 defined, ~50 handler cases | the `switch` in `DoTrack` |

A conditional = one `LF_*` query + one `LT_*` operator + an immediate operand. (Markers
like `LM_ENDIF`/`LM_DEFAULT`/`LM_NOP`/`LM_REM` have no runtime case — the compiler resolves
block structure into absolute offset jumps, leaving these as compile-time-only.)

`LM_*` categories: control flow / conditionals; object lifecycle (`LM_KILL_OBJ`,
`LM_SUICIDE`, life-points, hit, position, collision flags, doors); behaviour
(`LM_COMPORTEMENT_HERO`, save/restore comportement); Track binding; animation/body/sprite;
sound; scene/cube/world (`LM_CHANGE_CUBE`, camera, zone toggles, palette, rain, cinema,
`LM_PLAY_ACF`); variables; dialogue/choice/UI; inventory/economy/progression; effects
(`LM_IMPACT_*`, `LM_FLOW_*`); game flow (`LM_GAME_OVER`, `LM_THE_END`).

`TM_*` categories: movement/facing; coroutine waits; flow control (`TM_LABEL`/`TM_GOTO`/
`TM_LOOP`); animation/body/3DS-sprite suite; doors; sound. Life and Track share a
*vocabulary* (`*_SAMPLE`, `*_ANIM`, `*_BODY`) but use **separate numeric tables and
handlers** (`LM_SAMPLE`=122 vs `TM_SAMPLE`=14).

## 3. Dispatch mechanism

A classic **threaded-bytecode interpreter** over a global PC pointer `U8 *PtrPrg`
(`GERELIFE.CPP:8`). No function-pointer table:

```c
while (flag != -1) {
    ptrmacro = PtrPrg;
    switch (*PtrPrg++) {   // read opcode byte, advance PC
```

Operands are read by post-incrementing the cursor. Jumps reseat it relative to the script
base: `PtrPrg = ptrobj->PtrLife + *(S16*)PtrPrg`. Conditionals are **two-stage**
(`GERELIFE.CPP:1066`): `DoFuncLife` evaluates an `LF_*` query into global `Value`; `DoTest`
applies the `LT_*` operator against an inline immediate; false jumps to the else/endif
offset, true falls through. Several opcodes are **self-modifying** — `LM_SWIF`/`LM_SNIF`
(do-once-until-condition-flips) and `LM_ONEIF`/`LM_NEVERIF` (run-once-ever) rewrite their
own opcode byte in place (`*ptrmacro = LM_SNIF`). Track uses the same pattern on a
**per-object** cursor (`OffsetTrack`, written back into the struct each yield).

**What `EXTFUNC`/`PTRFUNC` are *not*:** they are not the script dispatch. `PTRFUNC.H` is
C function-pointer **typedefs** for engine-internal indirection / a build seam
(`PtrDoAnim`, `PtrGetScreenBrick`, …); `EXTFUNC.CPP`/`FUNC.CPP` are engine helper and
`LBA_EDITOR` tooling functions, callable from C but **not addressable as script opcodes**.
Scripts reach engine code only through the hardcoded `case` bodies in
`DoLife`/`DoTrack`/`DoFuncLife`.

## 4. The VM syscall surface (engine API exposed to scripts)

Each opcode body calls engine C functions directly. Categories:

- **Animation/body**: `InitAnim`, `InitBody`, `SetAnim`, `InitSprite`, `ObjectSetFrame`, `SetComportement`.
- **Audio**: `HQ_3D_MixSample`, `HQ_MixSample`, `HQ_StopOneSample`, `IsSamplePlaying`, `PlayMusic`.
- **Dialogue/UI**: `Dial`, `GameAskChoice`, `InitIncrustDisp`, `MenuInventory`, `DoFoundObj`, `EffectPcx`.
- **Scene/world**: cube change via `NewCube`+`FlagChgCube`, `CheckZoneSce`, `CameraCenter`, `InitGrille`, palette/fade, `PlayAcf`, `DoImpact`, `CreateParticleFlow`.
- **Geometry/queries**: `Distance2D/3D`, `GetAngle2D` (behind `LF_DISTANCE*`/`LF_ANGLE*`/`LF_CONE_VIEW`), `PosObjetAroundAnother`, `HitObj`, buggy take/leave.
- **Direct global state**: a large unmediated surface — `MagicLevel`, `MagicPoint`, gold/Zlitos, keys, `Comportement`, `FlagTheEnd`, `CinemaMode`, `FlagRain`, `ListZone[]`, `TabInv[]`, camera globals.

**No sandbox.** The VM has unmediated access to engine globals and arrays, and indices like
`ListObjet[*PtrPrg++]` are unchecked.

## 5. Script storage and script-visible variables

**Storage:** scripts are not a dedicated resource — they are embedded inline in the
per-cube scene blob. `LoadScene(numscene)` (`SOURCES/DISKFUNC.CPP:50`) loads the scene from
`scene.hqr` into `PtrScene`, then walks the blob with a byte cursor, reading for the hero
and each object a length-prefixed Track stream then a Life stream:

```c
sizetoload = GET_S16; ptrobj->PtrTrack = PtrSce; PtrSce += sizetoload;  // DISKFUNC.CPP:135
sizetoload = GET_S16; ptrobj->PtrLife  = PtrSce; PtrSce += sizetoload;  // DISKFUNC.CPP:139
```

So `PtrLife`/`PtrTrack` (`DEFINES.H:412`) point **directly into the scene buffer**, and
`OffsetLife`/`OffsetTrack` are the per-object PCs (`-1` = halted). The same blob supplies
`ListBrickTrack` (the named waypoints `TM_GOTO_POINT`/`LM_POS_POINT` index), zones, and
patches.

**Script-visible variables** (`GLOBAL.CPP:262`):

- `ListVarCube[80]` (`U8`) — **cube-scoped**, zeroed by `ClearVarsCube()` (`OBJECT.CPP:964`)
  inside `ClearScene()` on every cube transition. Intra-cube puzzle scratch.
  (`LF_VAR_CUBE`, `LM_SET/ADD/SUB_VAR_CUBE`.)
- `ListVarGame[256]` (`S16`) — **game-scoped, persistent** (part of the save file). Many
  slots are named flags (`FLAG_HOLOMAP`=0, `FLAG_CLOVER`=251, `FLAG_CHAPTER`=253, …,
  `COMMON.H:228`) and some alias engine systems (setting `FLAG_MONEY` mirrors into
  `NbGoldPieces`/`NbZlitosPieces`, `GERELIFE.CPP:1320`). (`LF_VAR_GAME`,
  `LM_SET/ADD/SUB_VAR_GAME`.)

## 6. Title-agnosticism and the LBA1 porting path

**LBA1 uses the same VM.** `lba1-classic/SOURCES/GERELIFE.C` and `GERETRAK.C` are direct
ancestors: identical `DoLife`/`DoTrack`/`DoFuncLife`/`DoTest` architecture, the same global
`PtrPrg` cursor, the same `Value`+`TypeAnswer` query/test mechanism, the same control-flow
family (`LM_IF`/`LM_OR_IF`/`LM_SWIF`/`LM_SNIF`/`LM_ONEIF`/`LM_NEVERIF`/`LM_OFFSET`).

LBA2 is a **strict superset**:

| | LBA1 | LBA2 |
|---|---|---|
| Life cases | ~99 | ~142 |
| `LF_*` queries | ~30 | 46 |
| Track cases | ~38 | ~50 |

- **Shared core:** control flow, conditionals, `LM_BODY`/`LM_ANIM`/`LM_KILL_OBJ`/
  `LM_SUICIDE`/`LM_COMPORTEMENT_HERO`/`LM_SET_TRACK`/`LM_MESSAGE`/`LM_SET_DIR`, the whole
  Track movement/wait/sample family.
- **LBA1-only / renamed:** `LM_LABEL`, `LM_INIT_PINGOUIN`, `LM_SET_LIFE(_OBJ)`,
  `LM_SAY_MESSAGE`; and the key rename `LM_SET_FLAG_CUBE/GAME` → `LM_SET_VAR_CUBE/GAME`
  (LBA1's single-byte flag model became LBA2's `S16` game vars with add/sub).
- **LBA2-only:** the 3DS-sprite Track suite, `LM_IMPACT_*`/`LM_FLOW_*`, `LM_CINEMA_MODE`,
  `LM_SWITCH`/`LM_CASE`, holomap/ardoise/buggy/Zlitos opcodes, `LM_ANIM_TEXTURE`, the
  door/rail/escalator zone toggles — each added LBA2 system bolted on opcodes.

**The conclusion that matters for the LBA1 goal:** the **VM is fully title-agnostic** — a
generic per-object bytecode interpreter with no LBA2-specific assumptions in its core loop,
so it is a good reuse target. And a cross-repo opcode diff (see
[LBA1_PORTING_SURFACE.md](LBA1_PORTING_SURFACE.md) §8) shows the gap is smaller than a
"not numerically stable" first read suggested: the `LM_*` command tables are **numerically
identical for opcodes 0–56**. Divergence starts ~57 — a few same-slot renames
(`LM_SET_FLAG_CUBE`→`LM_SET_VAR_CUBE`, with LBA1's byte flags widened to LBA2's `S16` vars,
so operand widths must be watched) and a handful of true collisions
(`LM_ZOOM`(57)→`LM_SHADOW_OBJ`, `LM_PLAY_FLA`(64)→`LM_PLAY_ACF`,
`LM_PLAY_MIDI`(65)→`LM_ECLAIR`, `LM_INIT_PINGOUIN`(71)→`LM_MEMO_ARDOISE`), several of them
the FLA/MIDI media opcodes. So LBA1 scripts run on the LBA2 VM after an **opcode-remap
table** (rewrite the collided/renamed opcodes, redirect FLA/MIDI), plus matching the
scene-file container — **not** a full transpiler. *The architecture ports trivially; the
bytecode needs a remap table, not a rewrite.*

## Relation to other docs, and uncertainties

- [IMPACT_SCRIPTS.md](IMPACT_SCRIPTS.md) documents a **separate third** bytecode system
  (IMPACT/FLOW point-in-space effects); Life reaches it via `LM_IMPACT_*`/`LM_FLOW_*`.
  [LBA_EDITOR.md](LBA_EDITOR.md) covers `LBA_EDITOR` authoring tooling (confirms
  EXTFUNC/IMPACT/FLOW are tooling, not script dispatch). Neither covers the Life/Track VM —
  that is the gap this doc fills. A `docs/SCRIPTS.md` operand spec is the planned next step
  (Life/Track tooling, issue #181).
- **Not yet verified:** exact per-opcode operand encodings (cursor pattern and categories
  confirmed, not an exhaustive operand table); the on-disk scene container beyond the
  object/zone/track-point walk. The LBA1↔LBA2 opcode-number alignment (0–56 identical,
  collisions at 57+) *was* diffed — see LBA1_PORTING_SURFACE.md §8 — but the operand
  widths/order of each shared opcode still want a spot-check.

## Key files

`SOURCES/GERELIFE.CPP` (Life VM), `SOURCES/GERETRAK.CPP` (Track VM), `SOURCES/COMMON.H`
(`LM_*`/`LF_*`/`LT_*`/`TM_*` constants), `SOURCES/PERSO.CPP:1533` (per-frame dispatch),
`SOURCES/DISKFUNC.CPP:50` (`LoadScene`), `SOURCES/GLOBAL.CPP:262` + `COMMON.H:463` (var
arrays), `SOURCES/DEFINES.H:412` (`PtrTrack`/`PtrLife`). LBA1: `lba1-classic/SOURCES/GERELIFE.C`,
`GERETRAK.C`.
