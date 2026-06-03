# Global variable bounded-context map

> **Status: DRAFT / living document (v0).** A reasoning overlay, not a refactor plan.
> Domain assignments are provisional and meant to be argued with. Nothing here changes
> behaviour — the polyrec/projrec equivalence tests freeze the engine, so this map exists
> to make the implicit *global bus* legible, not to move code.

## Why this exists

The engine communicates almost entirely through ~200 global variables declared in
`SOURCES/C_EXTERN.H` and defined in `SOURCES/GLOBAL.CPP` (plus a handful defined in
`KEYBOARD.CPP`, `MESSAGE.CPP`, `OBJECT.CPP`). There is no encapsulation boundary, so a
function cannot be reasoned about in isolation: to know what `HitObj` does you must know
`GodMode` gates it, and to change `GodMode` safely you must know everyone who reads it.
That is a web, not a tree, and webs do not fit in one's head.

This map assigns each global to the domain that *owns* it (writes it, is conceptually
responsible for it) and flags the domains that merely *read* it. The reads across a
boundary are the coupling edges. The goal is the first step of a strangler-fig: draw the
boundaries explicitly *over* the frozen code, the way the platform seam (PR #284–#286)
wraps SDL without rewriting the renderer. You cannot delete the bus, but you can publish
its schedule.

## The eight domains

1. **Gameplay / simulation** — entities, AI behaviour, inventory and stats, zones, player physics and control state.
2. **Rendering** — projection, sort tree, cameras, palettes, shadow, sprite scaling, cinema framing, environment effects.
3. **World / cube transform** — cube and world coordinate systems, scene/cube origins, transition coordinates. *The busiest coupling axis; likely the first strangler-fig target.*
4. **Resources / assets** — HQR handles, memory-pool budgets, body/anim/sprite/texture buffers.
5. **Audio** — sample and music volumes, the ambiance scheduler, stereo.
6. **Platform / IO** — raw input device state, display mode, timing, file paths, version banners, CD/video.
7. **UI / text** — menus, dialog and speech presentation, on-screen text buffers, fonts.
8. **Orchestration / lifecycle** — main-loop and transition flags, scene mode, save/restore state, checksum.

A ninth concern cuts across all of them and is *not* a domain: **persistence**. The block
under `C_EXTERN.H:302` ("Globales sauvées dans les fichiers de sauvegarde .LBA") marks
~50 globals as savegame state spanning gameplay, world, audio params, and lifecycle.
Persistence is an aspect, not a bounded context — worth tracking separately when the
save format is touched.

## The engine / game / platform lens

A deeper axis cuts *through* these domains: which code is reusable **engine**, which is
LBA2-specific **game**, which is **platform**. It is validated empirically against the
sibling LBA1 source (`lba1-classic`) at the module level — see
[ENGINE_GAME_SEAM.md](ENGINE_GAME_SEAM.md) for the three-way diff and evidence. Short
version: Rendering, World/cube transform, and Resources are **engine**; Platform/IO is
**platform**; Gameplay is **game** on an engine substrate; Audio, UI/text, and
Orchestration are **engine mechanism with game content**. The membrane between engine and
game is the Life/Track script VM (`GERELIFE`/`GERETRAK`), which is present in all three
Adeline trees. Prefer strangler-fig facades on engine seams — they pay off for LBA1 too.

## Domains and their globals

Cross-boundary readers are the domains (beyond the owner) that consume the value.
Array/cluster groups are collapsed to one row. `—` means single-owner / no significant
cross-read found at v0.

### Gameplay / simulation (~55)

| Global | Role | Cross-boundary readers |
|---|---|---|
| `ListObjet[]` | The entity table (all actors) | Rendering, World, UI, Audio |
| `NbObjets` | Entity count | Rendering |
| `ListExtra[]`, `ListDart[]` | Projectiles / extras / darts | Rendering |
| `ListComportement[]`, `PtrComportement` | AI behaviour table + current | Orchestration (save) |
| `Comportement`, `SaveComportement`, `SaveComportementHero` | Hero behaviour + saved copies | Orchestration |
| `ListVarCube[]`, `ListVarGame[]` | Scene/game script variables | Rendering, UI, Orchestration |
| `MagicLevel`, `MagicPoint` | Magic stats | Rendering (gauge), UI |
| `MagicBall`, `MagicBallType`, `MagicBallCount`, `MagicBallFlags` | Magic-ball state | Rendering |
| `MagicHitForce[]`, `MagicBallHitForce[]`, `MagicBallSprite[]` | Combat damage tables | Rendering |
| `Weapon`, `ActionNormal`, `InventoryAction` | Action / weapon mode | UI |
| `NbGoldPieces`, `NbZlitosPieces`, `NbLittleKeys`, `NbCloverBox` | Inventory counts | UI |
| `TabInv[]` | Inventory object table | UI |
| `NbZones`, `ListZone` | Trigger zones | World |
| `StartYFalling`, `RealFalling`, `StepFalling`, `RealShifting`, `StepShifting` | Falling / shifting physics | World |
| `PtrZoneClimb`, `FlagClimbing` | Ladder-climb state | — |
| `LastJoyFlag`, `LastMyJoy`, `LastMyFire`, `Jumping`, `Pushing` | Player control state (gameplay-written, saved) | Platform (source), Orchestration (save) |
| `NumObjFollow`, `ExtraConque`, `PingouinActif` | Follow / conque / penguin mechanics | — |
| `ListArdoise[]`, `CurrentArdoise`, `NbArdoise` | Holomap clue slate | UI |
| `ListBrickTrack`, `NbBrickTrack` | Track-script working data | — |
| `Bulle`, `SaveBodyHero`, `AnimNumObj`, `APtObj` | Hero body / speech-bubble / anim cursor | Rendering, UI |
| `FlagReajustPosTwinsen` | Reposition-on-cube-change flag | World |

> **Correction applied (vs the seed taxonomy):** `LastJoyFlag`/`Jumping`/`Pushing`/`LastMyJoy`/`LastMyFire` *look* like Platform input but are written by gameplay logic (`OBJECT.CPP`, `BUGGY.CPP`) off the raw input, and are part of the savegame, so they belong here. Verified by grepping the assignment sites.

### Rendering (~45)

| Global | Role | Cross-boundary readers |
|---|---|---|
| `ListTri[]` | Sort tree (painter's order) | — |
| `PtrPal`, `PtrPalNormal`, `PtrPalCurrent`, `PtrPalEclair`, `PtrPalBlack`, `PtrTransPal` | Active / variant palettes | Resources (writes on load), UI |
| `PtrXplPalette`, `PtrXplHeader`, `PalettePcx[]` | Explosion / PCX palettes | Resources |
| `IdxPalette`, `LastIdxPalette` | Current/previous palette index | Resources |
| `PtrNuances` | Gouraud shading ramp | — |
| `Shadow`, `ShadowX/Y/Z`, `ShadowCol`, `ShadowLevel` | Drop-shadow state | — |
| `AllCameras`, `FollowCamera`, `FollowCamBaseDist` | Camera mode | World |
| `VueCamera`, `FlagCameraForcee`, `AddBetaCam`, `CameraZone`, `DefVueDistance[]`, `DefAlphaCam[]` | Camera view / forced cam | Gameplay |
| `ScaleFactorSprite`, `SpriteX`, `SpriteY` | Sprite display transform | — |
| `CinemaMode`, `TimerCinema`, `LastYCinema`, `DebCycleCinema`, `DureeCycleCinema` | Cutscene letterbox framing | Orchestration |
| `FlagFade`, `FlagPal`, `FadeMenu`, `FlagBlackPal`, `FlagShadeMenu`, `FlagMessageShade` | Palette fade / shade flags | UI |
| `FlagRain`, `RainEnable`, `FlagWater`, `FlagDrawHorizon`, `DetailLevel` | Environment effects / LOD | — |
| `DrawZVObjets` | Debug bounding-box draw | — |
| `ListIncrustDisp[]` | On-screen incrust overlays | UI |

### World / cube transform (~25)

| Global | Role | Cross-boundary readers |
|---|---|---|
| `NumCube`, `NewCube` | Current / requested cube id | Gameplay, Rendering, Orchestration |
| `Island`, `Planet` | Current island / planet | Gameplay, Rendering, Audio |
| `WorldXCube`, `WorldYCube`, `WorldZCube` | World-space cube origin | Rendering, Gameplay |
| `CubeStartX/Y/Z`, `StartXCube/Y/Z`, `SceneStartX/Y/Z` | Cube / scene entry origins | Gameplay, Rendering |
| `NewPosX/Y/Z` | Pending teleport position | Gameplay |
| `PhantomX`, `PhantomY`, `PhantomIsland` | Death-cube offsets | Gameplay |
| `XpOrgw`, `YpOrgw` | World origin projected to screen | Rendering |
| `Nxw/Nyw/Nzw`, `SaveNxw/.../Nzw`, `OldX/OldY/OldZ` | Spatial working / delta scratch | Rendering, Gameplay |
| `TabAllCube[]` | All-cubes layout table | Resources, Gameplay |

### Resources / assets (~30)

| Global | Role | Cross-boundary readers |
|---|---|---|
| `HQR_Anims`, `HQR_Bodys`, `HQR_ObjFix`, `HQR_Samples` | HQR archive handles | Gameplay, Rendering, Audio |
| `HQRPtrSprite`, `HQRPtrSpriteRaw`, `HQRPtrAnim3DS` | Extra-object HQR handles | Rendering |
| `PtrZvExtra`, `PtrZvExtraRaw`, `PtrZvAnim3DS` | Extra-object bounding volumes | Rendering, Gameplay |
| `SpriteMem`, `SpriteRawMem`, `Anim3DSMem`, `SampleMem`, `AnimMem`, `BodyMem`, `ObjFixMem`, `IsleObjMem`, `CubeInfosMem`, `ListDecorsMem`, `MapPGroundMem`, `ListTexDefMem`, `MapSommetYMem`, `MapIntensityMem`, `SCCMem` | Memory-pool budgets | — |
| `PtrScene`, `PtrSceneMem` | Scene buffer | Gameplay, Rendering |
| `BufferTexture`, `AnimateTexture`, `BufferAnim` | 3D texture/anim buffers | Rendering |
| `BufferPof`, `BufferFile3D`, `BufferBrick` | Model / brick load buffers | Rendering |
| `ListAnim3DS` | 3DS animation list | Rendering |
| `LbaFont` | Loaded font buffer | UI |

### Audio (~18)

| Global | Role | Cross-boundary readers |
|---|---|---|
| `SampleAmbiance[]`, `SampleRepeat[]`, `SampleRnd[]`, `SampleFreq[]`, `SampleVol[]` | Ambiance scheduler config | Gameplay (writes per cube) |
| `SamplePlayed`, `TimerNextAmbiance`, `SecondEcart`, `SecondMin` | Ambiance scheduling state | Gameplay |
| `CubeJingle`, `SampleAlwaysMove` | Per-cube jingle / always-on sample | World, Gameplay |
| `SampleVolume`, `VoiceVolume`, `MasterVolume`, `SamplesEnable` | Mix levels / enable | Platform, Orchestration (save) |
| `ParmSampleVolume`, `ParmSampleDecalage`, `ParmSampleFrequence` | Saved sample params | Orchestration (save) |
| `RestartMusic`, `BufSpeak` | Music restart / speech buffer | UI |
| `ReverseStereo` | Stereo channel swap | Platform |

> The ambiance scheduler is the clearest example of the seed taxonomy being wrong: it is
> gameplay *scheduling* audio *playback*. Splitting it out names the gameplay↔audio
> coupling instead of burying it in Platform.

### Platform / IO (~15)

| Global | Role | Cross-boundary readers |
|---|---|---|
| `MyKey` | Raw input scancode | Gameplay, UI |
| `LastInputWasKeyboard` | Last-input-device flag (KEYBOARD.CPP) | UI (save menu) |
| `VideoFullScreen`, `DisplayFullScreen` | Display mode | Rendering |
| `TimerProto` | Timing | Orchestration |
| `GamePathname`, `OldGamePathname`, `PathFla` | Data / video paths | Resources |
| `FlaFromCD`, `FlagPlayAcf`, `MessageNoCD[]` | CD / video playback | UI |
| `Version`, `BuildInfo`, `DistribVersion`, `Version_US`, `PlayerName` | Version banners / player id | UI, Orchestration |

### UI / text (~15)

| Global | Role | Cross-boundary readers |
|---|---|---|
| `NumObjDial`, `DialNbLine`, `NumObjSpeak`, `FlagSpeak` | Dialog / speech presentation | Gameplay, Audio |
| `BufText`, `BufOrder` | Dialog text / ordering buffers | — |
| `GameChoice`, `GameNbChoices`, `GameListChoice[]` | Menu selection state | Orchestration |
| `GameOptionMenu[]`, `FlagMenuMouse`, `FlecheForcee` | Menu options / cursor | Platform |
| `NewGameTxt[]`, `PleaseWait[]` | UI strings | — |
| `String[]`, `Value` | Shared string-format / script scratch | All (cross-cutting) |

### Orchestration / lifecycle (~25)

| Global | Role | Cross-boundary readers |
|---|---|---|
| `GameInProgress` | In-game vs menu mode | Gameplay, Rendering, UI |
| `CubeMode`, `LastCubeMode`, `ModeLabyrinthe` | Interior/exterior/labyrinth scene mode | Rendering, World, Gameplay |
| `FlagChgCube`, `FlagLoadGame`, `FlagTheEnd` | Transition / load / end triggers | World, Gameplay |
| `FirstTime`, `FirstLoop`, `FirstSave` | First-iteration guards | — |
| `SavingEnable`, `NumVersion`, `Checksum` | Save enable / format version / checksum | — |
| `DemoSlide`, `FlagSlideShow`, `DebugFps` | Demo / debug modes | Rendering |
| `SnapshotRequested`, `SnapshotName` | Polyrec dev harness | — |

## The shared bus — worst offenders

These are read by three or more domains. They are the heart of why the engine does not
fit in one's head, and they tell you exactly which axes any module split must negotiate:

| Global | Domains touched | What it really is |
|---|---|---|
| `ListObjet[]` | Gameplay, Rendering, World, UI/Audio | The entity table — the single most-shared structure |
| `NumCube` / `NewCube` | World, Gameplay, Rendering, Orchestration | Which cube we are in / going to |
| `Island` / `Planet` | World, Gameplay, Rendering, Audio | Which world we are in |
| `WorldXCube/Y/Z` | World, Rendering, Gameplay | The world-space origin everything projects against |
| `CubeMode` | Orchestration, Rendering, World, Gameplay | Interior vs exterior — flips the whole render/sim path |
| `PtrPal` / `PtrPalNormal` | Rendering, Resources, UI | The active palette |
| `Comportement` | Gameplay, Orchestration, (UI) | The hero's current behaviour |
| `GameInProgress` | Orchestration, Gameplay, Rendering, UI | In-game vs menu — the mode switch |
| `String[]` | All | Shared scratch buffer; resists single ownership |

The pattern: the offenders are **coordinates** (cube/island/world origin), **the entity
table**, **the palette**, and **the mode switches** (in-game vs menu, interior vs
exterior). Those four clusters are the negotiation surface for any real boundary.

## Dead / vestigial globals

Surfaced while mapping; candidates for a later cleanup pass (verify before removing):

| Global | Observation |
|---|---|
| `BodyComportement` | Declared in `C_EXTERN.H:4`; no definition or read site found in `SOURCES`/`LIB386`. |
| `NbPolyPoints` | Under the `LIB_SVGA` comment (`C_EXTERN.H:175`); no live def/use — likely dead or ASM-only. |
| `FlagPalettePcx` | Extern-declared (`C_EXTERN.H:119`) but its `GLOBAL.CPP` definition is commented out. |

## Anti-patterns this map makes visible

- **The global-state bus itself.** `C_EXTERN.H` / `GLOBAL.CPP` are a ~200-entry shared
  blackboard. Every domain boundary above is violated by something reaching across it.
  This is the root smell; the rest are symptoms.
- **God object — `T_OBJET`** (`SOURCES/DEFINES.H`) and the `T_OBJ_3D` it wraps
  (`LIB386/H/OBJECT/AFF_OBJ.H`). One struct carries position, orientation, body/anim,
  animation frames, bounding box, AI state, and collision. Every system pokes the parts
  it cares about, so the struct couples gameplay, rendering, world, and collision into a
  single type — the data-side equivalent of the global bus.
- **God functions — `AffScene` and `ChangeCube`** (both `SOURCES/OBJECT.CPP`). `AffScene`
  orchestrates the whole exterior render; `ChangeCube` tears down and rebuilds world,
  resources, audio, and entity state on every cube transition. Each reaches across every
  domain in this map. `ChangeCube`'s phases are decomposed in
  [LIFECYCLES.md](LIFECYCLES.md) — they tour the domains in dependency order, which is the
  sequence any World/cube facade must preserve.

## How a clean engine would draw these lines

Two reference points, for contrast — not as a refactor mandate:

- **Doom 3 / idTech 4 "no god classes."** id's discipline was that no single type owns
  everything; subsystems (renderer, sound, game, file system) sit behind narrow
  interfaces and talk through them, not through shared globals. The equivalent move here
  is the platform seam already in flight (PR #284–#286): a C-linkage port with a
  host stub behind it. Generalised, every domain box above wants the same treatment — a
  facade (`Sort_Insert(...)`, `World_CurrentCube()`) that callers go through instead of
  touching the globals directly. The facade becomes the contract; the globals become its
  private state.
- **ECS, for the simulation layer.** In an entity-component-system the domains above
  *are* the components, and a system declares which components it reads and writes in its
  signature. The question this whole document answers by hand — "what global state does
  this touch?" — becomes mechanical and local. `T_OBJET` is the photographic negative of
  that: it is every component welded into one struct, on one global array.

The realistic path for a behaviour-faithful port is neither rewrite: it is strangler-fig.
Pick the busiest boundary — **World / cube transform**, by the offender table above —
wrap its globals behind a facade without moving them, prove behaviour is unchanged with
the polyrec/projrec harness, and repeat. This map is the input to choosing which boundary
to draw first.

## Open questions for the next pass

- Should **persistence** get its own annotation column (savegame yes/no) rather than a
  prose note? ~50 globals are save state and it cross-cuts every domain.
- `CubeMode` and `GameInProgress` are mode switches read everywhere — do they belong in
  Orchestration, or is there a distinct **mode/state-machine** context hiding here?
- `Nxw/Nyw/Nzw` and `OldX/Y/Z` are spatial scratch shared by World and Rendering — owner
  is genuinely ambiguous; confirm by usage before trusting the World assignment.
- Counts are approximate (clusters collapsed to single rows). A future pass could pin an
  exact owner per individual global if the facade work needs it.

## Related docs

Three orthogonal views of the same engine — read together:

- **This doc** — *structure*: which domain owns which global.
- **[ENGINE_GAME_SEAM.md](ENGINE_GAME_SEAM.md)** — *layer*: engine vs game vs platform,
  validated against the LBA1 source.
- **[LIFECYCLES.md](LIFECYCLES.md)** — *time*: main-loop frame order, `ChangeCube` phases,
  object/anim/behaviour state machines. The shared-bus offenders above (`ListObjet`,
  `CubeMode`, `Comportement`) are exactly its lifecycle pivots.
