# Life and Track scripts

Every object in a scene carries two bytecode scripts. **Track** (`TM_*`) is the
legs: it plays animations, walks to points and waits. **Life** (`LM_*`) is the
brain: it tests the game state with conditions (`LF_*`) and acts on it,
including switching the object's Track. `DoTrack` (`SOURCES/GERETRAK.CPP`) and
`DoLife` (`SOURCES/GERELIFE.CPP`) run them once per object per frame.

Adeline compiled the scripts with an external tool, `COMPILATOR`, which did not
ship. This page describes the compiled form, read off the interpreter, and
[`scripts/dev/scene_disasm.py`](../scripts/dev/scene_disasm.py) decodes it. It
is Phase 0 and the disassembler of Phase 1 in
[#181](https://github.com/LBALab/lba2-classic-community/issues/181); there is no
assembler yet.

```sh
scripts/dev/scene_disasm.py <data>/SCENE.HQR --cube 0             # one cube's scripts
scripts/dev/scene_disasm.py <data>/SCENE.HQR --check              # decode and verify all of them
scripts/dev/scene_disasm.py <data>/SCENE.HQR --find LM_PCX TM_LOOP
scripts/dev/scene_disasm.py <data>/SCENE.HQR --sweep              # what --check can catch
scripts/dev/scene_disasm.py --table                               # the tables below
```

`--game lba1` reads LBA1's scenes instead; see [LBA1](#lba1).

## Where the scripts are

`SCENE.HQR` entry `N + 1` is cube `N`; entry 0 holds the size of the largest
scene. `LoadScene` (`SOURCES/DISKFUNC.CPP`) reads an entry in this order, all
little-endian:

| Bytes | Field |
|---|---|
| 6 | `Island`, `CurrentCubeX`, `CurrentCubeY`, `ShadowLevel`, `ModeLabyrinthe`, `CubeMode` (s8 each) |
| 1 | a count `LoadScene` reads and does not use |
| 4 | `AlphaLight`, `BetaLight` (s16) |
| 40 | four ambient samples, five s16 each (sample, repeat, random, frequency, volume) |
| 4 | `SecondMin`, `SecondEcart` (s16) |
| 1 | `CubeJingle` |
| 6 | hero start X, Y, Z (s16) |
| 2 + n | hero Track: u16 length, then the script |
| 2 + n | hero Life: u16 length, then the script |
| 2 | `NbObjets`, counting the hero |

Each object after the hero then has 36 bytes of fields (`Flags` u32, 3D file
s16, `GenBody` s8, `GenAnim` s16, `Sprite` s16, X Y Z s16, `HitForce` s8,
`OptionFlags` s16, `Beta` s16, `SRot` s16, `Move` s8, `Info` to `Info3` s16,
`NbBonus` s16, `CoulObj` s8), 6 more when `Flags` has `ANIM_3DS` (1 << 18), then
`Armure` and `LifePoint` (s8), then its Track and its Life, each a u16 length
and the script. Zones, points and the rest of the scene follow the objects.

Life and Track come only from here: `LoadScene` is the one place that points an
object at its scripts. Two other compiled command streams live in `RESS.HQR`:

- **Impact scripts**, entry 47 (`RESS_IMPACT`): the effects a hit or explosion
  spawns, run by `DoImpact` (`SOURCES/IMPACT.CPP`). See
  [IMPACT_SCRIPTS.md](IMPACT_SCRIPTS.md) and `scripts/dev/impact_disasm.py`.
- **Animation actions**, in the object sheets of entry 44 (`RESS_FILE3D`): each
  sheet lists an object's bodies and animations, and each animation carries
  commands (`F_HIT`, `F_SAMPLE`, `F_THROW`, `F_IMPACT` and others) fired on given
  frames by `GereAnimAction` (`SOURCES/FICHE.CPP`). Nothing decodes these yet.

## Encoding

A script is a sequence of instructions: one opcode byte, then its operands, all
little-endian. Every shipped script ends with `END`, and `END` appears nowhere
else.

- **Jumps are absolute.** A Life jump is an s16 offset from the start of the
  object's own Life script (`PtrPrg = PtrLife + offset`); a Track jump
  (`TM_GOTO`, `TM_LOOP`) is an s16 offset into its own Track. `IF`, `AND_IF`,
  `SWIF`, `ONEIF` jump when the condition fails; `OR_IF` jumps when it holds.
- **Cross-object offsets** are byte offsets too. `SET_TRACK` takes an offset
  into the object's own Track; `SET_TRACK_OBJ` and `SET_COMPORTEMENT_OBJ` take
  an object number and an offset into that object's Track or Life. Every
  shipped `SET_TRACK` points at a `TM_LABEL` (9,035 of 9,035 in the GOG data).
- **A condition** is a function byte (`LF_*`), a u8 parameter for the functions
  that take one, then a test byte (`LT_*`: `==`, `>`, `<`, `>=`, `<=`, `!=`) and
  the value to compare with. The value is 1 byte or 2 depending on the function
  (the `TypeAnswer` `DoFuncLife` sets); see the table below.
- **`SWITCH`** evaluates a function without a test. Each `CASE` and `OR_CASE`
  carries a jump and a test whose value has the width of the switch's function.
  The switch state (`ExeSwitch`) is one slot per object, not a stack, so after a
  nested switch ends, the outer switch's remaining cases compare at the *inner*
  switch's width. The compiled bytes follow the same rule: cubes 196, 197 and
  200 have such cases, and 197 and 200 do not decode if a stack is assumed.
- **`SET_DIR` and `SET_DIR_OBJ`** take one more byte, the followed object or
  track point, when the move is `FOLLOW`, `SAME_XZ`, `SAME_XZ_BETA`, `CIRCLE` or
  `CIRCLE2` (`AdjustDirObject`).
- **Strings**: `LM_PLAY_ACF` and `TM_PLAY_ACF` take a zero-terminated name.
  Nothing else in Life or Track has a variable length.
- **Runtime state lives in the script.** `SWIF` rewrites itself to `SNIF` and back,
  and `ONEIF` to `NEVERIF`, so the shipped data holds only `SWIF` and `ONEIF`. The
  Track timers (`WAIT_NB_SECOND`, `WAIT_NB_DIZIEME` and their `_RND` forms) keep
  4 bytes of timer after their count, `WAIT_NB_ANIM` a counter, `LOOP` a live
  counter, `ANGLE_RND` and `FACE_TWINSEN` a state word. A script read back from
  memory can differ from the file there.
- `LM_COMPORTEMENT` (a behaviour's start, one operand byte the interpreter skips)
  does not occur in the shipped data, and `LM_REM`, `LM_NOP`, `LM_ENDIF`,
  `LM_SPY`, `LM_DEBUG`, `LM_DEBUG_OBJ` have no case in `DoLife` and do not occur
  either.

## Opcode tables

Generated by `scene_disasm.py --table`. Track sizes agree between `DoTrack` and
`SearchOffsetTrack`.

### Life opcodes

| Code | Opcode | Operands |
|---|---|---|
| 0 | `LM_END` | none |
| 2 | `LM_SNIF` | `cond` condition, `else` s16 Life offset |
| 3 | `LM_OFFSET` | `to` s16 Life offset |
| 4 | `LM_NEVERIF` | `cond` condition, `to` s16 Life offset |
| 10 | `LM_PALETTE` | `palette` u8 |
| 11 | `LM_RETURN` | none |
| 12 | `LM_IF` | `cond` condition, `else` s16 Life offset |
| 13 | `LM_SWIF` | `cond` condition, `else` s16 Life offset |
| 14 | `LM_ONEIF` | `cond` condition, `else` s16 Life offset |
| 15 | `LM_ELSE` | `to` s16 Life offset |
| 17 | `LM_BODY` | `body` u8 |
| 18 | `LM_BODY_OBJ` | `obj` u8, `body` u8 |
| 19 | `LM_ANIM` | `anim` u16 |
| 20 | `LM_ANIM_OBJ` | `obj` u8, `anim` u16 |
| 21 | `LM_SET_CAMERA` | `zone` u8, `on` u8 |
| 22 | `LM_CAMERA_CENTER` | `quarter` u8 |
| 23 | `LM_SET_TRACK` | `track` s16 |
| 24 | `LM_SET_TRACK_OBJ` | `obj` u8, `track` s16 |
| 25 | `LM_MESSAGE` | `text` s16 |
| 26 | `LM_FALLABLE` | `on` u8 |
| 27 | `LM_SET_DIR` | `move` move |
| 28 | `LM_SET_DIR_OBJ` | `obj` u8, `move` move |
| 29 | `LM_CAM_FOLLOW` | `obj` u8 |
| 30 | `LM_COMPORTEMENT_HERO` | `comportement` u8 |
| 31 | `LM_SET_VAR_CUBE` | `var` u8, `value` u8 |
| 32 | `LM_COMPORTEMENT` | `num` u8 |
| 33 | `LM_SET_COMPORTEMENT` | `life` s16 Life offset |
| 34 | `LM_SET_COMPORTEMENT_OBJ` | `obj` u8, `life` s16 |
| 35 | `LM_END_COMPORTEMENT` | none |
| 36 | `LM_SET_VAR_GAME` | `var` u8, `value` s16 |
| 37 | `LM_KILL_OBJ` | `obj` u8 |
| 38 | `LM_SUICIDE` | none |
| 39 | `LM_USE_ONE_LITTLE_KEY` | none |
| 40 | `LM_GIVE_GOLD_PIECES` | `count` s16 |
| 41 | `LM_END_LIFE` | none |
| 42 | `LM_STOP_L_TRACK` | none |
| 43 | `LM_RESTORE_L_TRACK` | none |
| 44 | `LM_MESSAGE_OBJ` | `obj` u8, `text` s16 |
| 45 | `LM_INC_CHAPTER` | none |
| 46 | `LM_FOUND_OBJECT` | `item` u8 |
| 47 | `LM_SET_DOOR_LEFT` | `dist` s16 |
| 48 | `LM_SET_DOOR_RIGHT` | `dist` s16 |
| 49 | `LM_SET_DOOR_UP` | `dist` s16 |
| 50 | `LM_SET_DOOR_DOWN` | `dist` s16 |
| 51 | `LM_GIVE_BONUS` | `keep` u8 |
| 52 | `LM_CHANGE_CUBE` | `cube` u8 |
| 53 | `LM_OBJ_COL` | `on` u8 |
| 54 | `LM_BRICK_COL` | `mode` u8 |
| 55 | `LM_OR_IF` | `cond` condition, `then` s16 Life offset |
| 56 | `LM_INVISIBLE` | `on` u8 |
| 57 | `LM_SHADOW_OBJ` | `obj` u8, `on` u8 |
| 58 | `LM_POS_POINT` | `point` u8 |
| 59 | `LM_SET_MAGIC_LEVEL` | `level` u8 |
| 60 | `LM_SUB_MAGIC_POINT` | `points` u8 |
| 61 | `LM_SET_LIFE_POINT_OBJ` | `obj` u8, `points` u8 |
| 62 | `LM_SUB_LIFE_POINT_OBJ` | `obj` u8, `points` u8 |
| 63 | `LM_HIT_OBJ` | `obj` u8, `force` u8 |
| 64 | `LM_PLAY_ACF` | `name` string |
| 65 | `LM_ECLAIR` | `duration` u8 |
| 66 | `LM_INC_CLOVER_BOX` | none |
| 67 | `LM_SET_USED_INVENTORY` | `item` u8 |
| 68 | `LM_ADD_CHOICE` | `text` s16 |
| 69 | `LM_ASK_CHOICE` | `text` s16 |
| 70 | `LM_INIT_BUGGY` | `mode` u8 |
| 71 | `LM_MEMO_ARDOISE` | `num` u8 |
| 72 | `LM_SET_HOLO_POS` | `pos` u8 |
| 73 | `LM_CLR_HOLO_POS` | `pos` u8 |
| 74 | `LM_ADD_FUEL` | `fuel` u8 |
| 75 | `LM_SUB_FUEL` | `fuel` u8 |
| 76 | `LM_SET_GRM` | `zone` u8, `on` u8 |
| 77 | `LM_SET_CHANGE_CUBE` | `zone` u8, `on` u8 |
| 78 | `LM_MESSAGE_ZOE` | `text` s16 |
| 79 | `LM_FULL_POINT` | none |
| 80 | `LM_BETA` | `beta` s16 |
| 81 | `LM_FADE_TO_PAL` | `palette` u8 |
| 82 | `LM_ACTION` | none |
| 83 | `LM_SET_FRAME` | `frame` u8 |
| 84 | `LM_SET_SPRITE` | `sprite` s16 |
| 85 | `LM_SET_FRAME_3DS` | `frame` u8 |
| 86 | `LM_IMPACT_OBJ` | `obj` u8, `impact` u16, `dy` s16 |
| 87 | `LM_IMPACT_POINT` | `point` u8, `impact` u16 |
| 88 | `LM_ADD_MESSAGE` | `text` s16 |
| 89 | `LM_BULLE` | `on` u8 |
| 90 | `LM_NO_CHOC` | `on` u8 |
| 91 | `LM_ASK_CHOICE_OBJ` | `obj` u8, `text` s16 |
| 92 | `LM_CINEMA_MODE` | `on` u8 |
| 93 | `LM_SAVE_HERO` | none |
| 94 | `LM_RESTORE_HERO` | none |
| 95 | `LM_ANIM_SET` | `anim` u16 |
| 96 | `LM_PLUIE` | `duration` u8 |
| 97 | `LM_GAME_OVER` | none |
| 98 | `LM_THE_END` | none |
| 99 | `LM_ESCALATOR` | `zone` u8, `on` u8 |
| 100 | `LM_PLAY_MUSIC` | `music` u8 |
| 101 | `LM_TRACK_TO_VAR_GAME` | `var` u8 |
| 102 | `LM_VAR_GAME_TO_TRACK` | `var` u8 |
| 103 | `LM_ANIM_TEXTURE` | `on` u8 |
| 104 | `LM_ADD_MESSAGE_OBJ` | `obj` u8, `text` s16 |
| 105 | `LM_BRUTAL_EXIT` | none |
| 107 | `LM_ECHELLE` | `zone` u8, `on` u8 |
| 108 | `LM_SET_ARMURE` | `armure` s8 |
| 109 | `LM_SET_ARMURE_OBJ` | `obj` u8, `armure` s8 |
| 110 | `LM_ADD_LIFE_POINT_OBJ` | `obj` u8, `points` u8 |
| 111 | `LM_STATE_INVENTORY` | `item` u8, `body` u8 |
| 112 | `LM_AND_IF` | `cond` condition, `else` s16 Life offset |
| 113 | `LM_SWITCH` | `func` condition, no test |
| 114 | `LM_OR_CASE` | `then` s16 Life offset, `test` test |
| 115 | `LM_CASE` | `next` s16 Life offset, `test` test |
| 116 | `LM_DEFAULT` | none |
| 117 | `LM_BREAK` | `to` s16 Life offset |
| 118 | `LM_END_SWITCH` | none |
| 119 | `LM_SET_HIT_ZONE` | `zone` u8, `force` u8 |
| 120 | `LM_SAVE_COMPORTEMENT` | none |
| 121 | `LM_RESTORE_COMPORTEMENT` | none |
| 122 | `LM_SAMPLE` | `sample` s16 |
| 123 | `LM_SAMPLE_RND` | `sample` s16 |
| 124 | `LM_SAMPLE_ALWAYS` | `sample` s16 |
| 125 | `LM_SAMPLE_STOP` | `sample` s16 |
| 126 | `LM_REPEAT_SAMPLE` | `sample` s16, `repeat` u8 |
| 127 | `LM_BACKGROUND` | `on` u8 |
| 128 | `LM_ADD_VAR_GAME` | `var` u8, `value` s16 |
| 129 | `LM_SUB_VAR_GAME` | `var` u8, `value` s16 |
| 130 | `LM_ADD_VAR_CUBE` | `var` u8, `value` u8 |
| 131 | `LM_SUB_VAR_CUBE` | `var` u8, `value` u8 |
| 133 | `LM_SET_RAIL` | `zone` u8, `on` u8 |
| 134 | `LM_INVERSE_BETA` | none |
| 135 | `LM_NO_BODY` | none |
| 136 | `LM_ADD_GOLD_PIECES` | `count` s16 |
| 137 | `LM_STOP_L_TRACK_OBJ` | `obj` u8 |
| 138 | `LM_RESTORE_L_TRACK_OBJ` | `obj` u8 |
| 139 | `LM_SAVE_COMPORTEMENT_OBJ` | `obj` u8 |
| 140 | `LM_RESTORE_COMPORTEMENT_OBJ` | `obj` u8 |
| 144 | `LM_POPCORN` | none |
| 145 | `LM_FLOW_POINT` | `point` u8, `flow` u8 |
| 146 | `LM_FLOW_OBJ` | `obj` u8, `flow` u8 |
| 147 | `LM_SET_ANIM_DIAL` | `anim` u16 |
| 148 | `LM_PCX` | `pcx` u8, `effect` u8 |
| 149 | `LM_END_MESSAGE` | none |
| 150 | `LM_END_MESSAGE_OBJ` | `obj` u8 |
| 151 | `LM_PARM_SAMPLE` | `decalage` s16, `volume` u8, `frequence` s16 |
| 152 | `LM_NEW_SAMPLE` | `sample` s16, `decalage` s16, `volume` u8, `frequence` s16 |
| 153 | `LM_POS_OBJ_AROUND` | `obj` u8, `around` u8 |
| 154 | `LM_PCX_MESS_OBJ` | `pcx` u8, `effect` u8, `obj` u8, `text` s16 |

### Track opcodes

| Code | Opcode | Operands |
|---|---|---|
| 0 | `TM_END` | none |
| 1 | `TM_NOP` | none |
| 2 | `TM_BODY` | `body` u8 |
| 3 | `TM_ANIM` | `anim` u16 |
| 4 | `TM_GOTO_POINT` | `point` u8 |
| 5 | `TM_WAIT_ANIM` | none |
| 6 | `TM_LOOP` | `count` u8, `counter` u8, `to` s16 Track offset |
| 7 | `TM_ANGLE` | `beta` s16 |
| 8 | `TM_POS_POINT` | `point` u8 |
| 9 | `TM_LABEL` | `label` u8 |
| 10 | `TM_GOTO` | `to` s16 Track offset |
| 11 | `TM_STOP` | none |
| 12 | `TM_GOTO_SYM_POINT` | `point` u8 |
| 13 | `TM_WAIT_NB_ANIM` | `count` u8, `counter` u8 |
| 14 | `TM_SAMPLE` | `sample` s16 |
| 15 | `TM_GOTO_POINT_3D` | `point` u8 |
| 16 | `TM_SPEED` | `speed` s16 |
| 17 | `TM_BACKGROUND` | `on` u8 |
| 18 | `TM_WAIT_NB_SECOND` | `seconds` u8, `timer` u32 |
| 19 | `TM_NO_BODY` | none |
| 20 | `TM_BETA` | `beta` s16 |
| 21 | `TM_OPEN_LEFT` | `dist` s16 |
| 22 | `TM_OPEN_RIGHT` | `dist` s16 |
| 23 | `TM_OPEN_UP` | `dist` s16 |
| 24 | `TM_OPEN_DOWN` | `dist` s16 |
| 25 | `TM_CLOSE` | none |
| 26 | `TM_WAIT_DOOR` | none |
| 27 | `TM_SAMPLE_RND` | `sample` s16 |
| 28 | `TM_SAMPLE_ALWAYS` | `sample` s16 |
| 29 | `TM_SAMPLE_STOP` | `sample` s16 |
| 30 | `TM_PLAY_ACF` | `name` string |
| 31 | `TM_REPEAT_SAMPLE` | `repeat` s16 |
| 32 | `TM_SIMPLE_SAMPLE` | `sample` s16 |
| 33 | `TM_FACE_TWINSEN` | `state` s16 |
| 34 | `TM_ANGLE_RND` | `beta` s16, `state` s16 |
| 35 | `TM_REM` | none |
| 36 | `TM_WAIT_NB_DIZIEME` | `tenths` u8, `timer` u32 |
| 37 | `TM_DO` | none |
| 38 | `TM_SPRITE` | `sprite` s16 |
| 39 | `TM_WAIT_NB_SECOND_RND` | `seconds` u8, `timer` u32 |
| 40 | `TM_AFF_TIMER` | none |
| 41 | `TM_SET_FRAME` | `frame` u8 |
| 42 | `TM_SET_FRAME_3DS` | `frame` u8 |
| 43 | `TM_SET_START_3DS` | `frame` u8 |
| 44 | `TM_SET_END_3DS` | `frame` u8 |
| 45 | `TM_START_ANIM_3DS` | `fps` u8 |
| 46 | `TM_STOP_ANIM_3DS` | none |
| 47 | `TM_WAIT_ANIM_3DS` | none |
| 48 | `TM_WAIT_FRAME_3DS` | `frame` u8 |
| 49 | `TM_WAIT_NB_DIZIEME_RND` | `tenths` u8, `timer` u32 |
| 50 | `TM_DECALAGE` | `decalage` s16 |
| 51 | `TM_FREQUENCE` | `frequence` s16 |
| 52 | `TM_VOLUME` | `volume` u8 |

### Conditions

| Code | Condition | Parameter | Compared as |
|---|---|---|---|
| 0 | `LF_COL` | none | s8 |
| 1 | `LF_COL_OBJ` | `obj` u8 | s8 |
| 2 | `LF_DISTANCE` | `obj` u8 | s16 |
| 3 | `LF_ZONE` | none | s8 |
| 4 | `LF_ZONE_OBJ` | `obj` u8 | s8 |
| 5 | `LF_BODY` | none | s8 |
| 6 | `LF_BODY_OBJ` | `obj` u8 | s8 |
| 7 | `LF_ANIM` | none | s16 |
| 8 | `LF_ANIM_OBJ` | `obj` u8 | s16 |
| 9 | `LF_L_TRACK` | none | u8 |
| 10 | `LF_L_TRACK_OBJ` | `obj` u8 | u8 |
| 11 | `LF_VAR_CUBE` | `var` u8 | u8 |
| 12 | `LF_CONE_VIEW` | `obj` u8 | s16 |
| 13 | `LF_HIT_BY` | none | s8 |
| 14 | `LF_ACTION` | none | s8 |
| 15 | `LF_VAR_GAME` | `var` u8 | s16 |
| 16 | `LF_LIFE_POINT` | none | s16 |
| 17 | `LF_LIFE_POINT_OBJ` | `obj` u8 | s16 |
| 18 | `LF_NB_LITTLE_KEYS` | none | s8 |
| 19 | `LF_NB_GOLD_PIECES` | none | s16 |
| 20 | `LF_COMPORTEMENT_HERO` | none | s8 |
| 21 | `LF_CHAPTER` | none | s8 |
| 22 | `LF_DISTANCE_3D` | `obj` u8 | s16 |
| 23 | `LF_MAGIC_LEVEL` | none | s8 |
| 24 | `LF_MAGIC_POINT` | none | s8 |
| 25 | `LF_USE_INVENTORY` | `item` u8 | s8 |
| 26 | `LF_CHOICE` | none | s16 |
| 27 | `LF_FUEL` | none | s16 |
| 28 | `LF_CARRY_BY` | none | s8 |
| 29 | `LF_CDROM` | none | s8 |
| 30 | `LF_ECHELLE` | `zone` u8 | s8 |
| 31 | `LF_RND` | `max` u8 | u8 |
| 32 | `LF_RAIL` | `zone` u8 | s8 |
| 33 | `LF_BETA` | none | s16 |
| 34 | `LF_BETA_OBJ` | `obj` u8 | s16 |
| 35 | `LF_CARRY_OBJ_BY` | `obj` u8 | s8 |
| 36 | `LF_ANGLE` | `obj` u8 | s16 |
| 37 | `LF_DISTANCE_MESSAGE` | `obj` u8 | s16 |
| 38 | `LF_HIT_OBJ_BY` | `obj` u8 | s8 |
| 39 | `LF_REAL_ANGLE` | `obj` u8 | s16 |
| 40 | `LF_DEMO` | none | s8 |
| 41 | `LF_COL_DECORS` | none | u8 |
| 42 | `LF_COL_DECORS_OBJ` | `obj` u8 | s8 |
| 43 | `LF_PROCESSOR` | none | s8 |
| 44 | `LF_OBJECT_DISPLAYED` | `obj` u8 | s8 |
| 45 | `LF_ANGLE_OBJ` | `obj` u8 | s16 |

`move` is a u8 `MOVE_*` value, plus a u8 for the moves listed under Encoding.
A condition's parameter is an object, var, zone or item number, as named.

## How far the tables are verified

`--check` decodes every script and requires that it ends exactly at its stored
length on its only `END`, that every jump lands on an instruction start, and
that every `SET_TRACK`, `SET_TRACK_OBJ` and `SET_COMPORTEMENT_OBJ` offset lands on
an instruction start in the script it names. All shipped data passes:

| Data | Cubes | Track scripts | Life scripts | Instructions |
|---|---|---|---|---|
| GOG Classic, GOG DOS and Steam (identical `SCENE.HQR`) | 222 | 3115 | 3115 | 151,024 |
| Activision US CD (`TWINSEN.iso`) | 222 | 3115 | 3115 | 150,992 |
| Demo | 185 | 564 | 564 | 13,523 |

In the GOG data that covers 43,767 jump and behaviour offsets and 11,052
`SET_TRACK`, `SET_TRACK_OBJ` and `SET_COMPORTEMENT_OBJ` offsets.
To see what they catch, `--sweep` gives every opcode that occurs in the data a
wrong size, one change at a time (an extra byte, a byte field read as two, a
two-byte field read as one, the last field dropped), and every condition a wrong
compare width and a wrong parameter count. In the GOG data 543 of the 555
changes break the check. The other 12 shift into bytes that still decode, so
those sizes rest on the source alone: `SUB_LIFE_POINT_OBJ` and `PARM_SAMPLE` occur once each and
`PLUIE` three times, and `TM_OPEN_UP`, `TM_OPEN_DOWN`, `TM_START_ANIM_3DS` and
`TM_WAIT_FRAME_3DS` are followed by opcodes that take no operands.

Never used by any shipped script, so checked against the source only:

- Life: `ADD_FUEL`, `ADD_MESSAGE_OBJ`, `BRUTAL_EXIT`, `COMPORTEMENT`,
  `END_MESSAGE_OBJ`, `INC_CHAPTER`, `NEVERIF`, `OFFSET`,
  `RESTORE_COMPORTEMENT_OBJ`, `SAMPLE_RND`, `SNIF`, `SUB_FUEL`, `SUB_MAGIC_POINT`.
- Track: `AFF_TIMER`, `DO`, `LOOP`, `NOP`, `PLAY_ACF`, `SAMPLE_RND`, `SET_FRAME_3DS`.
- Conditions: `CDROM`, `COL_DECORS_OBJ`, `ECHELLE`, `FUEL`, `MAGIC_LEVEL`, `MAGIC_POINT`.

## What the data says

- No shipped script asks for the venetian-blind picture reveal: all 32 `LM_PCX`
  and `LM_PCX_MESS_OBJ` uses pass effect 0, the fade, so `EffectPcx`'s
  `PCX_V_SHADE` branch is reachable only from a modified scene.

## LBA1

LBA1 scripts work the same way, and `--game lba1` decodes them with the same
checks. Its tables are read off the released LBA1 source at
[lba1-classic `7404567`](https://github.com/LBALab/lba1-classic/tree/7404567be143c36f1c885537f3d5d0273f4d4728),
the commit that matches the GOG build: `DoLife`, `DoFuncLife` and `DoTest` in
`GERELIFE.C`, `DoTrack` in `GERETRAK.C`, `LoadScene` in `DISKFUNC.C`, and the
opcode numbers in `COMMON.H`. That source is not in this repository, so the tool
carries LBA1's opcode numbers itself; `--game lba1 --table` prints its tables.

| Data | Scenes | Track scripts | Life scripts | Instructions |
|---|---|---|---|---|
| GOG and CD (identical `SCENE.HQR`) | 120 | 1238 | 1238 | 49,470 |
| Demo | 120 | 533 | 533 | 20,366 |

That covers 14,747 jump offsets and 3,249 `SET_TRACK`, `SET_TRACK_OBJ` and
`SET_COMPORTEMENT_OBJ` offsets in the GOG data. `--sweep` catches 343 of 349 changes there. The other six still
decode, so those sizes rest on the source alone: an extra byte after
`FADE_ALARM_RED`, `FADE_PAL_RED` and `PROJ_ISO` (one use each) or `TM_CLOSE`, and
`SUB_FUEL` or `TM_REPEAT_SAMPLE` without its operand. The preview release's
`SCENE.HQR` (17 scenes) has an older layout and does not load.

How LBA1 differs from LBA2:

- **Scene file.** Scene `N` is entry `N`, from 0. The header before the hero
  start is 39 bytes, not 56, with three values per ambiance sample, not five.
  An object's record is 34 bytes: `Flags` u16, `GenAnim` u8, `Move` u16,
  `NbBonus` u8, and no `ANIM_3DS` block.
- **No switch.** There is no `SWITCH`, `CASE`, `AND_IF` or `BREAK`; branching is
  `IF`, `SWIF`, `ONEIF`, `OR_IF` and `ELSE` only.
- **Narrower values.** Game flags are bytes: `SET_FLAG_GAME` writes one and
  `LF_FLAG_GAME` compares one, where LBA2's game variables are s16. Animation
  numbers (`ANIM`, `ANIM_OBJ`, `ANIM_SET`, `TM_ANIM`) are u8, not u16. Life
  points, fuel and animation conditions compare a byte.
- **Signed compares.** Every byte compare is signed; LBA2 compares Track labels
  and cube variables unsigned.
- **Changed operands.** `TM_LOOP` takes none and restarts the Track; LBA2's is a
  counted loop. `SET_GRM` takes one byte, not two. `SET_DIR` reads an extra
  byte only for `MOVE_FOLLOW`.
- **-1 as a target.** `TM_GOTO -1` and `SET_COMPORTEMENT -1` stop the script,
  and the data uses both (cube 8 objects 13 to 15, cube 15 object 2). LBA2's
  data never does, so `--check` accepts -1 only for LBA1.
- **Opcode numbers.** About 70 of LBA1's 101 Life opcodes keep their number in
  LBA2, some renamed (`FLAG` became `VAR`, `PLAY_FLA` became `PLAY_ACF`).
  `PLAY_MIDI` moved from 65 to 100 as `PLAY_MUSIC`. LBA1's `LABEL` (10),
  `SET_LIFE` (21) and `SET_LIFE_OBJ` (22) sit where LBA2 has `PALETTE`,
  `SET_CAMERA` and `CAMERA_CENTER`, and its palette fades, `MESSAGE_SENDELL`,
  `HOLOMAP_TRAJ`, `TEXT` and the projection opcodes have no LBA2 counterpart.
  LBA2 adds 72 Life opcodes, 18 Track opcodes and 16 conditions.
  `scripts/dev/lba1_script_remap.py` maps the numbers between the two.

As in LBA2, every Track change a Life script makes (2,868 in the GOG data) lands
on a Track label, and no compiled script holds a `COMPORTEMENT` or `LABEL`
marker.
