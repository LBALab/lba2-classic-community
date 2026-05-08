# Engine lifecycles

State machines and execution sequences in the LBA2 engine. Each section lists
states with their code-level constants, the typical sequence, and where the
logic lives.

Truth hierarchy: code > this document > external sources.

## Main loop frame order

Each game frame executes these steps in order. Defined in `MainLoop()`.


| Step | Function                        | What it does                        |
| ---- | ------------------------------- | ----------------------------------- |
| 1    | `MyGetInput()` + `ManageTime()` | Read player input and advance timer |
| 2    | `DoDir(i)` per object           | Compute movement direction          |
| 3    | `DoTrack(i)` per object         | Execute track script (pathing)      |
| 4    | `PtrDoAnim(i)` per object       | Advance animations                  |
| 5    | `CheckZoneSce(i)` per object    | Test zone triggers                  |
| 6    | `DoLife(i)` per object          | Execute life script (behavior AI)   |
| 7    | `AffScene()`                    | Render the scene                    |


**Code:** `SOURCES/PERSO.CPP` (MainLoop), `SOURCES/GERETRAK.CPP` (DoTrack),
`SOURCES/GERELIFE.CPP` (DoLife), `SOURCES/OBJECT.CPP` (CheckZoneSce)

## Scene (cube) loading

Triggered by `ChangeCube()` when the hero enters a zone-type-0 trigger or a
life script executes `LM_CHANGE_CUBE`.


| Phase | What happens                                             |
| ----- | -------------------------------------------------------- |
| 1     | Unload current scene objects and zones                   |
| 2     | Load collision grid (`BufCube[]`) for the new cube       |
| 3     | Load objects into `ListObjet[]` (up to `MAX_OBJETS=100`) |
| 4     | Load zones into `ListZone[]` (up to `MAX_ZONES=255`)     |
| 5     | Set `CubeMode` (0=interior, 1=exterior)                  |
| 6     | Initialize camera position                               |


**Code:** `SOURCES/OBJECT.CPP` (`ChangeCube`), `SOURCES/GRILLE.CPP` (grid loading)

## Object lifecycle

Objects are loaded when a scene starts and can be killed during gameplay.


| State | Meaning                               | Terminal? |
| ----- | ------------------------------------- | --------- |
| Alive | Object exists, scripts run each frame | No        |
| Dead  | `OBJ_DEAD` bit set in `WorkFlags`     | Yes       |


**Happy path:** Alive (scripts running) -> `LM_KILL_OBJ` or `LM_SUICIDE` -> Dead

**Transitions:**

- `LM_KILL_OBJ` (opcode 37): another object's life script kills this object
- `LM_SUICIDE` (opcode 38): object kills itself
- Both set `OBJ_DEAD` (bit 5) in `WorkFlags`

**Code:**

- Constants: `SOURCES/COMMON.H` (`OBJ_DEAD`, `LM_KILL_OBJ`, `LM_SUICIDE`)
- Transitions: `SOURCES/GERELIFE.CPP` (`DoLife` opcode handlers)

## Hero behavior modes

The hero's `Comportement` determines movement style, available actions, and
combat behavior. Changed by player input or by `LM_COMPORTEMENT_HERO` in
life scripts.

Modes 0–3 are player-switchable; 4–13 are context-triggered. Full enum
(constants and meanings): [GLOSSARY.md#comportement-hero-behavior-modes](GLOSSARY.md#comportement-hero-behavior-modes).

**Player-switchable core modes:** Normal <-> Athletic <-> Aggressive <-> Stealth
(via `COMPORTEMENT` input or `NORMAL`/`SPORTIF`/`AGRESSIF`/`DISCRET` direct inputs).

Other modes are context-triggered by life scripts (`LM_COMPORTEMENT_HERO`,
opcode 30).

**Code:**

- Constants: `SOURCES/COMMON.H` (`C_NORMAL` through `C_SKELETON`)
- Transitions: `SOURCES/PERSO.CPP`, `SOURCES/GERELIFE.CPP`

## Animation state

Each object has an animation that progresses through frames each tick.


| State   | Meaning                                             | Terminal?            |
| ------- | --------------------------------------------------- | -------------------- |
| Playing | `LastFrame` < `NbFrames`, `NEW_FRAME` set each tick | No                   |
| Ended   | `ANIM_END` (bit 2) set in `WorkFlags`               | Yes (until new anim) |


**Happy path:** Anim set (via `LM_ANIM` / `LM_ANIM_OBJ`) -> Playing -> `ANIM_END`

Scripts typically check `ANIM_END` before assigning the next animation.
Looping animations restart automatically without setting `ANIM_END`.

**Code:**

- State: `T_OBJ_3D.LastFrame`, `T_OBJ_3D.NbFrames` in `LIB386/H/OBJECT/AFF_OBJ.H`
- Flags: `ANIM_END`, `NEW_FRAME` in `SOURCES/COMMON.H`
- Anim engine: `LIB386/ANIM/`

