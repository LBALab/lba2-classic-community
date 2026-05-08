# Engine glossary

Domain terms for the LBA2 engine. Each entry links to the source file where the
concept is defined or primarily implemented.

Truth hierarchy: code > this document > external sources.

## World

| Term | Also known as | Definition | Code location | Reference |
|------|---------------|------------|---------------|-----------|
| Cube | Scene, Room | A single game level or room. The world has 223 cubes (0-222). `NumCube` holds the current index. See [SCENES.md](SCENES.md) for the full index with location names. | `SOURCES/OBJECT.CPP` (`ChangeCube`) | -- |
| CubeMode | -- | 0 = interior (isometric grid), 1 = exterior (3D heightmap). | `SOURCES/COMMON.H` (`CUBE_INTERIEUR`, `CUBE_EXTERIEUR`) | -- |
| BufCube | Collision grid | 64x64x25 cell grid loaded per interior scene. Each cell holds a collision value. | `SOURCES/GRILLE.CPP` | -- |
| Zone | Trigger zone | Axis-aligned bounding box that triggers an action when the hero enters it. | `SOURCES/COMMON.H` (`T_ZONE`), `SOURCES/OBJECT.CPP` (`CheckZoneSce`) | -- |
| Zone type | -- | Numeric 0-9 (`MAX_TYPES_ZONE=10`). Type 0 = cube change, type 1 = camera, type 6 = ladder. No named constants in source. | `SOURCES/COMMON.H`, `SOURCES/OBJECT.CPP` | -- |
| HQR | -- | Archive file format used for all game resources. | `SOURCES/COMMON.H` (file name constants) | [LBA File Info](https://lbafileinfo.kaziq.net/index.php/LBA_2_files) (incomplete; missing video.hqr) |

## HQR resource files

Purposes sourced from [LBA File Info](https://lbafileinfo.kaziq.net/index.php/LBA_2_files),
verified against `#define` constants in `SOURCES/COMMON.H` (lines 66-80).
LBA File Info omits `video.hqr`; the list below is complete per the source code.

| File | Purpose |
|------|---------|
| `lba2.hqr` | Ending credits data |
| `scene.hqr` | Scripts defining active scene content |
| `body.hqr` | 3D models of characters |
| `anim.hqr` | Animations for 3D models |
| `anim3ds.hqr` | 3DS-format animations |
| `objfix.hqr` | 3D models of fixed objects |
| `samples.hqr` | Sound samples (excluding voices) |
| `text.hqr` | In-game dialogue text |
| `screen.hqr` | Full-screen images |
| `sprites.hqr` | Compressed sprites; see [MENU.md](MENU.md) for **`SPRITE_CURSOR`** (173) and the menu pointer |
| `spriraw.hqr` | Raw-format sprites |
| `ress.hqr` | Miscellaneous resources |
| `lba_bkg.hqr` | Isometric background maps, bricks, objects |
| `holomap.hqr` | Holomap data and textures |
| `video.hqr` | Video/cinematic data |
| `scrshot.hqr` | Demo screenshot images |

### ILE/OBL pairs (exterior island surfaces)

Each island has a `.ile` (3D surface + textures) and `.obl` (3D objects) file.
Loaded dynamically by `SOURCES/3DEXT/LOADISLE.CPP`; island base names (for index)
in `SOURCES/EXTFUNC.CPP` (lines 202-213). Table below shows asset filenames.

| Files | Island |
|-------|--------|
| `Citabau.ile/obl` | Citadel Island (good weather) |
| `Citadel.ile/obl` | Citadel Island (rain) |
| `Desert.ile/obl` | White Leaf Desert |
| `Emeraude.ile/obl` | Emerald Moon |
| `Celebrat.ile/obl` | Celebration Island (no statue) |
| `Celebra2.ile/obl` | Celebration Island (statue emerged) |
| `Otringal.ile/obl` | Otringal |
| `Platform.ile/obl` | Island of the Wannies |
| `Mosquibe.ile/obl` | Island of the Mosquibees |
| `Knartas.ile/obl` | Francos Island |
| `Ilotcx.ile/obl` | Island CX |
| `Ascence.ile/obl` | Elevator to the Undergas |
| `Souscelb.ile/obl` | Volcano Island |
| `Moon.ile/obl` | Emerald Moon (second visit) |

### VOX files (voice packs)

`xx_NNN.vox` where `xx` is the language code. See [LBA File Info](https://lbafileinfo.kaziq.net/index.php/LBA_2_files) for per-island mapping.

## Objects

| Term | Also known as | Definition | Code location | Reference |
|------|---------------|------------|---------------|-----------|
| T_OBJET | -- | Full game object: position, body, anim, scripts, collision, flags, stats. Up to `MAX_OBJETS` (100) per scene. Hero is always object 0. | `SOURCES/DEFINES.H` | -- |
| T_OBJ_3D | Obj | 3D sub-struct within `T_OBJET`: X/Y/Z position, Alpha/Beta/Gamma rotation, Body/Anim indices, frame state. | `LIB386/H/OBJECT/AFF_OBJ.H` | -- |
| ListObjet | -- | Array of `T_OBJET[MAX_OBJETS]`. Populated by `ChangeCube()` on scene load. | `SOURCES/OBJECT.CPP` | -- |
| GenBody | Body index | Which 3D model the object currently uses. See enum table below. | `SOURCES/DEFINES.H` (`GEN_BODY_*`) | -- |
| GenAnim | Animation ID | Which animation is currently playing. See enum table below. | `SOURCES/DEFINES.H` (`GEN_ANIM_*`) | -- |
| ZV | Zone de Vie, Bounding volume | Axis-aligned bounding box of an object: `XMin/XMax`, `YMin/YMax`, `ZMin/ZMax`. Hero ZV is ~500 units wide, nearly filling a 512-unit grid cell. | `SOURCES/DEFINES.H` (T_OBJET fields) | -- |

## Scripts

| Term | Also known as | Definition | Code location | Reference |
|------|---------------|------------|---------------|-----------|
| Life Script | Behavior script, LM_* | Bytecode controlling object behavior (AI decisions, dialogue triggers, state changes). Executed per-frame by `DoLife()`. 131 `LM_*` opcodes in `SOURCES/COMMON.H`; often cited as 155 total (parameter variants or external count). | `SOURCES/GERELIFE.CPP`, `SOURCES/COMMON.H` | -- |
| Track Script | Movement script, TM_* | Bytecode controlling object movement paths (go-to-point, wait, loop). Executed per-frame by `DoTrack()`. 53 macros total. | `SOURCES/GERETRAK.CPP`, `SOURCES/COMMON.H` | -- |
| PtrLife / OffsetLife | -- | Pointer to life script bytecode and current execution offset within it. | `SOURCES/DEFINES.H` (T_OBJET fields) | -- |
| PtrTrack / OffsetTrack | -- | Pointer to track script bytecode and current execution offset. | `SOURCES/DEFINES.H` (T_OBJET fields) | -- |

## Hero

| Term | Also known as | Definition | Code location | Reference |
|------|---------------|------------|---------------|-----------|
| Comportement | Behavior mode | Hero's current play style, affecting movement, combat, and abilities. | `SOURCES/COMMON.H`, `SOURCES/PERSO.CPP` | -- |
| ListVarGame | Game variables | Array of 256 quest/story state variables (`MAX_VARS_GAME`). Set by `LM_SET_VAR_GAME`, read by life scripts. | `SOURCES/GLOBAL.CPP` | -- |
| DemoSlide | Demo mode | Built-in non-playable mode. Player input is ignored, dialogues auto-advance. | `SOURCES/GLOBAL.CPP`, `SOURCES/C_EXTERN.H` | -- |
| Input | Input bitmask | Bitfield of currently pressed actions. See input names table below. | `SOURCES/PERSO.CPP` (`MyGetInput`) | -- |
| Beta | Facing angle | Object orientation, 0-4095 (4096 = 360 degrees). 0 = +Z (north), 1024 = +X (east), 2048 = -Z (south), 3072 = -X (west). | `SOURCES/DEFINES.H` (`MAX_ANGLE=4096`) | -- |

## Collision grid (interior scenes)

Interior scenes use an isometric grid-based collision system.

| Property | Value | Source |
|----------|-------|--------|
| Grid cells X/Z | 64 x 64 | `SIZE_CUBE_X=64`, `SIZE_CUBE_Z=64` in `SOURCES/DEFINES.H` |
| Y levels | 25 | `SIZE_CUBE_Y=25` in `SOURCES/DEFINES.H` |
| Cell size (XZ) | 512 world units | `SIZE_BRICK_XZ=512` in `SOURCES/DEFINES.H` |
| Y level height | 256 world units | `SIZE_BRICK_Y=256` in `SOURCES/DEFINES.H` |
| Full rotation | 4096 units | `MAX_ANGLE=4096` in `SOURCES/DEFINES.H` |

### Collision values

| Value | Meaning |
|-------|---------|
| 0 | Empty (passable if floor below) |
| 1 | Solid wall |
| 2-5 | Simple slopes (connect adjacent Y levels) |
| 6-13 | Composite slopes |

### Walkability rules

A cell is walkable when:
- `col=0` with solid floor below (`col=1` at Y-1, or Y=0 for implicit ground), OR
- `col=2-13` (slope provides its own floor)
- Head clearance: 5 Y-levels above must be empty

The hero ZV is ~500 units wide (+-250), leaving only ~6 units clearance per side
in a 512-unit cell. Off-center positions cause corner clipping.

## Movement (approximate at 25 FPS)

| Parameter | Value | Notes |
|-----------|-------|-------|
| Walk speed | 48 units/frame | After 8-frame startup delay |
| Turn rate (in-place) | ~61 beta/frame avg | 6-frame startup, accelerates 58 to 90 |
| Turn rate (walking) | ~41 beta/frame | UP+LEFT or UP+RIGHT composite input |
| Beta range | 0-4095 (4096 = 360 degrees) | 0=north, 1024=east, 2048=south, 3072=west |

## Enum reference tables

### GenAnim (animation IDs)

| Value | Constant | Meaning |
|-------|----------|---------|
| 0 | GEN_ANIM_RIEN | Idle |
| 1 | GEN_ANIM_MARCHE | Walk |
| 2 | GEN_ANIM_RECULE | Back up |
| 3 | GEN_ANIM_GAUCHE | Turn left |
| 4 | GEN_ANIM_DROITE | Turn right |
| 5 | GEN_ANIM_ENCAISSE | Hit (take damage) |
| 6 | GEN_ANIM_CHOC | Shock/collision |
| 7 | GEN_ANIM_TOMBE | Fall |
| 8 | GEN_ANIM_RECEPTION | Landing |
| 9 | GEN_ANIM_RECEPTION_2 | Landing (alt) |
| 10 | GEN_ANIM_MORT | Death |
| 11 | GEN_ANIM_ACTION | Action |
| 12 | GEN_ANIM_MONTE | Climb up |
| 13 | GEN_ANIM_ECHELLE | Ladder |
| 14 | GEN_ANIM_SAUTE | Jump |
| 15 | GEN_ANIM_LANCE | Throw |
| 16 | GEN_ANIM_CACHE | Hide |
| 17 | GEN_ANIM_COUP_1 | Melee hit 1 |
| 18 | GEN_ANIM_COUP_2 | Melee hit 2 |
| 19 | GEN_ANIM_COUP_3 | Melee hit 3 |
| 20 | GEN_ANIM_TROUVE | Find/discover |
| 21 | GEN_ANIM_NOYADE | Drowning |
| 22 | GEN_ANIM_CHOC2 | Shock 2 |
| 23 | GEN_ANIM_SABRE | Sword swing |
| 24 | GEN_ANIM_DEGAINE | Draw weapon |
| 25 | GEN_ANIM_SAUTE_GAUCHE | Jump left |
| 26 | GEN_ANIM_SAUTE_DROIT | Jump right |
| 27 | GEN_ANIM_POUSSE | Push |
| 28 | GEN_ANIM_PARLE | Talk |
| 29 | GEN_ANIM_DART | Dart |
| 30 | GEN_ANIM_DESCEND | Climb down |
| 31 | GEN_ANIM_ECHDESC | Ladder descend |
| 32 | GEN_ANIM_ARRIMAGE | Dock/attach |
| 33 | GEN_ANIM_SKATE | Skate |
| 34 | GEN_ANIM_SKATEG | Skate left |
| 35 | GEN_ANIM_SARBACANE | Blowgun |
| 36 | GEN_ANIM_GANT_DROIT | Right glove |
| 37 | GEN_ANIM_GANT_GAUCHE | Left glove |
| 38 | GEN_ANIM_PISTOLASER | Laser pistol |
| 39 | GEN_ANIM_FOUDRE | Lightning |
| 40 | GEN_ANIM_ESQUIVE_DROITE | Dodge right |
| 41 | GEN_ANIM_ESQUIVE_GAUCHE | Dodge left |
| 42 | GEN_ANIM_ESQUIVE_AVANT | Dodge forward |
| 43 | GEN_ANIM_ESQUIVE_ARRIERE | Dodge backward |
| 44 | GEN_ANIM_FEU | Fire |
| 45 | GEN_ANIM_SARBATRON | Sarbatron |
| 46 | GEN_ANIM_GAZ | Gas |
| 47 | GEN_ANIM_LABYRINTHE | Labyrinth |

### GenBody (body/model IDs)

| Value | Constant | Meaning |
|-------|----------|---------|
| 0 | GEN_BODY_NORMAL | Default body |
| 1 | GEN_BODY_TUNIQUE | Tunic |
| 2 | GEN_BODY_SABRE | With sword |
| 3 | GEN_BODY_SARBACANE | With blowgun |
| 4 | GEN_BODY_SARBATRON | With sarbatron |
| 5 | GEN_BODY_GANT | With glove |
| 6 | GEN_BODY_PISTOLASER | With laser pistol |
| 7 | GEN_BODY_MAGE | Wizard robe |
| 8 | GEN_BODY_MAGE_SARBACANE | Wizard + blowgun |
| 9 | GEN_BODY_FEU | Fire |
| 10 | GEN_BODY_TUNIQUE_TIR | Tunic (firing) |
| 11 | GEN_BODY_MAGE_TIR | Wizard (firing) |
| 12 | GEN_BODY_LABYRINTHE | Labyrinth |

### Move types

| Value | Constant | Meaning |
|-------|----------|---------|
| 0 | NO_MOVE | Stationary |
| 1 | MOVE_MANUAL | Player-controlled |
| 2 | MOVE_FOLLOW | Follow target |
| 3 | MOVE_TRACK | Following track script |
| 4 | MOVE_FOLLOW_2 | Follow (variant) |
| 5 | MOVE_TRACK_ATTACK | Track + attack |
| 6 | MOVE_SAME_XZ | Match target XZ |
| 7 | MOVE_PINGOUIN | Penguin slide |
| 8 | MOVE_WAGON | Wagon/rail |
| 9 | MOVE_CIRCLE | Circular |
| 10 | MOVE_CIRCLE2 | Circular (variant) |
| 11 | MOVE_SAME_XZ_BETA | Match XZ + facing |
| 12 | MOVE_BUGGY | Vehicle (buggy) |
| 13 | MOVE_BUGGY_MANUAL | Vehicle (manual) |

### Comportement (hero behavior modes)

| Value | Constant | Meaning |
|-------|----------|---------|
| 0 | C_NORMAL | Normal mode |
| 1 | C_SPORTIF | Athletic mode |
| 2 | C_AGRESSIF | Aggressive/combat mode |
| 3 | C_DISCRET | Stealth mode |
| 4 | C_PROTOPACK | Protopack (jetpack predecessor) |
| 5 | C_DOUBLE | Double |
| 6 | C_CONQUE | Conch horn |
| 7 | C_SCAPH_INT_NORM | Diving suit interior (normal) |
| 8 | C_JETPACK | Jetpack |
| 9 | C_SCAPH_INT_SPOR | Diving suit interior (athletic) |
| 10 | C_SCAPH_EXT_NORM | Diving suit exterior (normal) |
| 11 | C_SCAPH_EXT_SPOR | Diving suit exterior (athletic) |
| 12 | C_BUGGY | Buggy vehicle |
| 13 | C_SKELETON | Skeleton disguise |

### Object flags (bit positions)

| Bit | Constant | Meaning |
|-----|----------|---------|
| 0 | CHECK_OBJ_COL | Check object-to-object collision |
| 1 | CHECK_BRICK_COL | Check wall/brick collision |
| 2 | CHECK_ZONE | Check zone triggers |
| 3 | SPRITE_CLIP | Sprite clipping |
| 4 | PUSHABLE | Can be pushed |
| 5 | COL_BASSE | Low collision |
| 6 | CHECK_CODE_JEU | Check game code triggers |
| 7 | CHECK_ONLY_FLOOR | Floor collision only |
| 9 | INVISIBLE | Not rendered |
| 10 | SPRITE_3D | 3D sprite |
| 11 | OBJ_FALLABLE | Can fall |
| 12 | NO_SHADOW | Shadow disabled |
| 13 | OBJ_BACKGROUND | Background object |
| 14 | OBJ_CARRIER | Can carry other objects |
| 15 | MINI_ZV | Reduced bounding volume |
| 16 | POS_INVALIDE | Invalid position |
| 17 | NO_CHOC | No shock/impact |
| 18 | ANIM_3DS | Uses 3DS animation |
| 19 | NO_PRE_CLIP | Skip pre-clipping |
| 20 | OBJ_ZBUFFER | Z-buffer enabled |
| 21 | OBJ_IN_WATER | In water |

### WorkFlags (bit positions)

| Bit | Constant | Meaning |
|-----|----------|---------|
| 0 | WAIT_HIT_FRAME | Waiting for hit frame |
| 1 | OK_HIT | Hit registered |
| 2 | ANIM_END | Animation finished |
| 3 | NEW_FRAME | New animation frame this tick |
| 4 | WAS_DRAWN | Object was rendered this frame |
| 5 | OBJ_DEAD | Object is dead |
| 6 | AUTO_STOP_DOOR | Auto-closing door |
| 7 | ANIM_MASTER_ROT | Master rotation animation |
| 8 | FALLING | Object is falling |
| 9 | OK_SUPER_HIT | Super hit registered |
| 10 | FRAME_SHIELD | Shield frame active |
| 11 | DRAW_SHADOW | Shadow drawn |
| 12 | ANIM_MASTER_GRAVITY | Master gravity animation |
| 13 | SKATING | Skating movement |
| 14 | OK_RENVOIE | Deflection OK |
| 15 | LEFT_JUMP | Jumping left |
| 16 | RIGHT_JUMP | Jumping right |
| 17 | WAIT_SUPER_HIT | Waiting for super hit |
| 18 | TRACK_MASTER_ROT | Track master rotation |
| 19 | FLY_JETPACK | Flying with jetpack |
| 20 | DONT_PICK_CODE_JEU | Don't pick up game code items |
| 21 | MANUAL_INTER_FRAME | Manual inter-frame |
| 22 | WAIT_COORD | Waiting for coordinates |
| 23 | CHECK_FALLING | Checking fall state |

### Input names

Available in the `Input` bitmask (see `MyGetInput`):

`UP`, `DOWN`, `LEFT`, `RIGHT`, `THROW`, `COMPORTEMENT`,
`INVENTORY`, `ACTION`, `TALK`, `RETURN`, `MENUS`, `HOLOMAP`,
`PAUSE`, `DODGE`, `NORMAL`, `SPORTIF`, `AGRESSIF`, `DISCRET`, `CAMERA`
