# Savegame System

Savegame files (`.lba`) store game state: scene, position, inventory, holomap, screenshot, and more. This doc covers the engine **lifecycle**, **save/load pipelines**, **binary layout**, and **version / compatibility** behaviour. Layout is verified against `SaveContexte` / `LoadContexte` in [SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP).

Truth hierarchy: **code > this document > external sources**.

## File paths

| Name | Constant | Purpose |
|------|----------|---------|
| `current.lba` | `CURRENTSAVE_FILENAME` | Quick-save; enables Resume in main menu |
| `autosave.lba` | `AUTOSAVE_FILENAME` | Auto-save during gameplay |
| `*.lba` | User-chosen | Named saves (player name → filename) |

Path resolution: `GetSavePath(outPath, pathMaxSize, saveFilename)` → `<userDir>/save/` + filename ([SOURCES/DIRECTORIES.CPP](SOURCES/DIRECTORIES.CPP)).

### Bug saves (`save/bugs/`)

**Purpose:** Repro / QA snapshots kept **separate** from player slots. Same **`.lba` binary format** and **`SaveGame()` / `LoadGame()`** machinery as normal saves ([SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP)); only **path**, **who triggers the write**, and **default compression** differ.

| Topic | Regular saves | Bug saves |
|--------|----------------|-----------|
| **Directory** | `GetSavePath(..., "<file>.lba")` → `<userDir>/save/` (root) for named slots; `current.lba` / `autosave.lba` for quick/auto | `GetBugPath(..., "<file>.lba")` → `<userDir>/save/bugs/` ([SOURCES/DIRECTORIES.CPP](SOURCES/DIRECTORIES.CPP) `GetBugPath`) |
| **Compression** | **`AutoSaveGame` / `CurrentSaveGame`:** `NumVersion = NUM_VERSION` only (uncompressed). **`CompressSave` in `lba2.cfg`:** merged at startup into `NumVersion` ([SOURCES/PERSO.CPP](SOURCES/PERSO.CPP) `ReadConfigFile`). **Menu “Save Game” (slot):** sets the compression high bit on `NumVersion` before `SaveGame` ([SOURCES/GAMEMENU.CPP](SOURCES/GAMEMENU.CPP)). | **DEBUG menu bug save** and **console `savebug`:** set `NumVersion` to **`NUM_VERSION` with `SAVE_COMPRESS`** before `SaveGame` ([SOURCES/GAMEMENU.CPP](SOURCES/GAMEMENU.CPP) case 2001; [SOURCES/CONSOLE/CONSOLE_CMD.CPP](SOURCES/CONSOLE/CONSOLE_CMD.CPP) `cmd_savebug`). |
| **Entry points** | Main menu save/load, autosave, argv/console **`load`** | **DEBUG_TOOLS:** in-game `G` / menu load case 2000 / menu save case 2001 ([SOURCES/GAMEMENU.CPP](SOURCES/GAMEMENU.CPP)). **Always-on console:** `savebug`, `loadbug`, `listbugs` ([CONSOLE.md](CONSOLE.md)). |

**Console `savebug`:** requires an **in-game loaded scene** (same gate as `give`); **`loadbug`** only checks that `bugs/<name>.lba` exists. See [CONSOLE.md](CONSOLE.md).

## Lifecycle

### Save

| Trigger | Function | Notes |
|---------|----------|-------|
| Menu Save | `SaveGame(TRUE)` | User-initiated |
| Quick-save (MENUS) | `CurrentSaveGame()` | Saves to `current.lba`; player name set to "CURRENT" |
| Auto-save | `AutoSaveGame()` | Saves to `autosave.lba` at scene transitions |
| Bug save | `SaveGame(TRUE)` | **`save/bugs/`** via DEBUG menu (DEBUG_TOOLS), in-game **`G`**, or console **`savebug`** |

**Code:** [SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP) – `SaveGame`, `CurrentSaveGame`, `AutoSaveGame`

### Load

| Trigger | Function | Notes |
|---------|----------|-------|
| Menu Load / Resume | `LoadGame()` | Full load; sets `NewCube`, `PlayerName`, restores state |
| Load screenshot only | `LoadGameScreen()` | Reads header + 160×120 image; used for menu preview |
| Load player name only | `LoadGamePlayerName()` | Reads header + player name; used when listing saves |
| Cube index only (lightweight) | `LoadGameNumCube()` | Used when `FlagLoadGame` is set before a cube transition (e.g. startup argv save, console **`load`** / **`loadbug`**) — reads header, optional decompress, skips screenshot, restores `ListVarGame` only |

**Code:** [SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP) – `LoadGame`, `LoadGameNumCube`, `LoadGameScreen`, `LoadGamePlayerName`. Full game load from disk is wired through `FlagLoadGame` in [SOURCES/OBJECT.CPP](SOURCES/OBJECT.CPP) `ChangeCube` (see [Version compatibility](#version-compatibility)).

## Save pipeline (`SaveGame`)

Order in [SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP) (retail path):

1. Build stream at `PtrSave` (from `BufSpeak + 50000` in the non-editor build).
2. Write **version byte** (`NUM_VERSION` in low 7 bits; high bit set if `CompressSave` requests compression — see [Version byte on disk](#version-byte-on-disk)), **cube** (`NumCube`), null-terminated **`PlayerName`**.
3. If compressing: reserve **4 bytes** for plaintext size (`sizefile`), remember `memoptr`.
4. Write **160×120** screenshot bytes, then **`SaveContexte(savetimerrefhr)`** (full game state).
5. Append **`ValidePos` / `LastValidePos`**; if `!ValidePos`, append **`ValideCube`**, **`SizeOfBufferValidePos`**, and **`BufferValidePos`**.
6. If compressing: LZSS-compress the block from `memoptr` onward, patch **`sizefile`**, write file with **`Save()`**.

Constants: `NUM_VERSION`, `SAVE_COMPRESS`, `MASK_NUM_VERSION` in [SOURCES/COMMON.H](SOURCES/COMMON.H).

## Full load pipeline (`LoadGame`)

1. **`LoadSize`** — reads into **`Screen`** up to **`640×480 + RECOVER_AREA`** (see [SOURCES/MEM.CPP](SOURCES/MEM.CPP)); larger files are rejected.
2. Read **`NumVersion`**, **`NewCube`**, **`PlayerName`** (NUL-terminated in the file; reader is **bounded** by **`MAX_SIZE_PLAYER_NAME`**).
3. If **`NumVersion & SAVE_COMPRESS`**: read **`sizefile`**, validate staging, copy compressed tail, **`ExpandLZ(..., mode 2)`** (matches `Compress_LZSS`).
4. **`PtrSave += 160 * 120`** — skip screenshot so the next reads align with **game context** offsets in this doc.
5. **`LoadContexte(&savetimerrefhr)`** — restores globals through camera / buggy / ardoise / `VueCamera`; returns **`flaginit`** (`0`/`1`) or **`-1`** on stream/bounds failure (see [Version compatibility](#version-compatibility)).
6. Read valid-position tail: **`ValidePos` / `LastValidePos`**, optional **`BufferValidePos`** blob.
7. Restart music, palette, **`CameraCenter(0)`**, restore timer from save, **`SaveTimer()`**. **`LoadGame`** returns **`flaginit`** to **`ChangeCube`**, which stores it in **`flagload`**.

Partial loaders (`LoadGameScreen`, `LoadGamePlayerName`, `LoadGameNumCube`) use the same header (+ optional decompress where applicable) but only consume a prefix of the stream.

## File format – header and screenshot

**Header** (before decompression):

| Offset | Size | Content |
|--------|------|---------|
| 0 | 1 | Version byte: `0x24` (uncompressed) or `0xA4` (compressed). High bit = `SAVE_COMPRESS` (0x80); low 7 bits = `NUM_VERSION` (36). |
| 1 | 4 | Scene/cube index (`NumCube` or `NewCube`) |
| 5 | var | Player name (null-terminated string) |
| — | 4 | *(if compressed)* Decompressed size |
| — | var | *(if compressed)* LZSS-compressed payload |

**After decompression** (or if uncompressed):

| Offset | Size | Content |
|--------|------|---------|
| 0 | 19200 | 160×120 screenshot (raw 8-bit indexed, game palette) |
| 19200 | var | Game context (see below) |

Compression: `Compress_LZSS` / `ExpandLZ` (mode 2). The payload (screenshot + game context) is compressed as one block; the header (version, cube, name) is always uncompressed.

### Version byte on disk

The first byte combines two ideas (macros in [SOURCES/COMMON.H](SOURCES/COMMON.H)):

| Constant | Value | Role |
|----------|-------|------|
| `NUM_VERSION` | `36` | Low **7 bits**: layout revision of the serialized game context (branches in `LoadContexte` / `SaveContexte`). |
| `SAVE_COMPRESS` | `0x80` | High bit: screenshot + context stored LZSS-compressed after the header. |
| `MASK_NUM_VERSION` | `~(SAVE_COMPRESS)` | Strip compression when comparing layout (e.g. `(NumVersion & MASK_NUM_VERSION) != NUM_VERSION` in `OBJECT.CPP`). |

Typical values: **`0x24`** = uncompressed layout 36; **`0xA4`** = compressed (`0x80 | 36`).

**Not an ABI tag:** the same first byte can appear on **original 32-bit** retail saves and **64-bit** community builds. It does **not** identify pointer width or compiler. Pointer-sized blobs in the stream still differ by platform (see [Version compatibility](#version-compatibility)).

## File format – game context (after screenshot)

Layout as written by `SaveContexte()` (SAVEGAME.CPP ~706). All offsets below are relative to the start of the game context (byte 0 = first byte after the 19200-byte screenshot).

### Game vars and cube vars

| Offset | Size | Content | Code |
|--------|------|---------|------|
| 0 | 512 | `ListVarGame[0..255]` (S16 each) | 256 × 2 bytes |
| 512 | 80 | `ListVarCube[0..79]` (U8 each) | 80 × 1 byte |

### Globals

| Offset | Size | Content | Code |
|--------|------|---------|------|
| 592 | 1 | `Comportement` (behavior mode) | LbaWriteByte |
| 593 | 4 | `(NbZlitosPieces<<16)+(NbGoldPieces&0xFFFF)` | LbaWriteLong |
| 597 | 1 | `MagicLevel` (0–4) | LbaWriteByte |
| 598 | 1 | `MagicPoint` (0–80) | LbaWriteByte |
| 599 | 1 | `NbLittleKeys` | LbaWriteByte |
| 600 | 2 | `NbCloverBox` | LbaWriteWord |
| 602 | 4 | `SceneStartX` | LbaWriteLong |
| 606 | 4 | `SceneStartY` | LbaWriteLong |
| 610 | 4 | `SceneStartZ` | LbaWriteLong |
| 614 | 4 | `StartXCube` | LbaWriteLong |
| 618 | 4 | `StartYCube` | LbaWriteLong |
| 622 | 4 | `StartZCube` | LbaWriteLong |
| 626 | 1 | `Weapon` | LbaWriteByte |
| 627 | 4 | `savetimerrefhr` (game timestamp) | LbaWriteLong |
| 631 | 1 | `NumObjFollow` (actor ID, usually Twinsen) | LbaWriteByte |
| 632 | 1 | `SaveComportementHero` | LbaWriteByte |
| 633 | 1 | `SaveBodyHero` | LbaWriteByte |

### Holomap (TabArrow)

| Offset | Size | Content | Code |
|--------|------|---------|------|
| 634 | 305 | `TabArrow[0..304].FlagHolo` (U8 each) | `MAX_OBJECTIF+MAX_CUBE` = 50+255 (HOLO.H) |

**FlagHolo bitfield:** `........` (bit 0 = activation, bit 1 = asked about, bit 2 = inside/outside for exterior scenes). Only low 2 bits are persisted; `LoadContexte` masks with `& 3`.

### Inventory (TabInv)

| Offset | Size | Content | Code |
|--------|------|---------|------|
| 939 | 400 | `TabInv[0..39]` – 40 items × 10 bytes each | LbaWrite per item |

**Per item (10 bytes):** `PtMagie` (s32), `FlagInv` (s32), `IdObj3D` (s16). `PtMagie` = magic points or value; `FlagInv` = flags; `IdObj3D` = 3D model variant.

### Checksum and extended payload

| Offset | Size | Content |
|--------|------|---------|
| 1339 | 4 | `Checksum` – engine validates; mismatch can trigger fallback position |
| 1343 | 47 | Input / magic / movement block (`LastMyFire` … `PingouinActif`) — see `LoadContexte` |
| 1390 | 4 | `PtrZoneClimb` stored as **U32** (load casts to `T_ZONE *`) |
| 1394 | 84 | `ListDart[0..2]` — **3** darts × **7×`S32`** each (`MAX_DARTS`, `SAVEGAME.CPP`) |
| **1478** | **4** | **`NbObjets`** (`S32`) — count for the following per-object records |

| 1482 | var | Object array: `NbObjets` × (fixed `T_OBJET` prefix + `T_OBJ_3D` without `CurrentFrame`) — size depends on **32 vs 64-bit** ABI |

The full `SaveContexte` / `LoadContexte` continues with patches, extras, zones, incrust, flows, camera, and more. See SAVEGAME.CPP lines 706–1076 (save) and 1080–1548 (load).

## ListVarGame – inventory and quest flags

`ListVarGame` holds 256 S16 values. Indices 0–39 map to inventory flags; others are quest/scenario state. Values: `0` = don't have; `1` = have (flag); `1–65535` = quantity for stackable items.

| Index | Constant | Item |
|-------|----------|------|
| 0 | FLAG_HOLOMAP | Holomap |
| 1 | FLAG_BALLE_MAGIQUE | Magic ball |
| 2 | FLAG_DART | Darts |
| 3 | FLAG_BOULE_SENDELL | Sendell's ball |
| 4 | FLAG_TUNIQUE | Tunic and Sendell's medallion |
| 5 | FLAG_PERLE | Incandescent pearl |
| 6 | FLAG_CLEF_PYRAMID | Pyramid shaped key |
| 7 | FLAG_VOLANT | Part for the car |
| 8 | FLAG_MONEY | Kashes (TwinSun) or Zlitos (Zeelich) |
| 9 | FLAG_PISTOLASER | Pisto-Laser |
| 10 | FLAG_SABRE | Emperor's sword |
| 11 | FLAG_GANT | Wannie's glove |
| 12 | FLAG_PROTOPACK | Proto-Pack |
| 13 | FLAG_TICKET_FERRY | Ferry Ticket |
| 14 | FLAG_MECA_PINGOUIN | Nitro-Meca-Penguin |
| 15 | FLAG_GAZOGEM | Can of GazoGem |
| 16 | FLAG_DEMI_MEDAILLON | Dissidents' Ring |
| 17 | FLAG_ACIDE_GALLIQUE | Gallic acid |
| 18 | FLAG_CHANSON | Ferryman's song |
| 19 | FLAG_ANNEAU_FOUDRE | Ring of lightning |
| 20 | FLAG_PARAPLUIE | Customer's umbrella |
| 21 | FLAG_GEMME | Gems |
| 22 | FLAG_CONQUE | Horn of the Blue Triton |
| 23 | FLAG_SARBACANE | Blowgun |
| 24 | FLAG_DISQUE_ROUTE / FLAG_VISIONNEUSE | Itinerary token / Visionneuse |
| 25 | FLAG_TART_LUCI | Slice of tart |
| 26 | FLAG_RADIO | Portable radio |
| 27 | FLAG_FLEUR | Garden Balsam |
| 28 | FLAG_ARDOISE | Magic slate |
| 29 | FLAG_TRADUCTEUR | Translator |
| 30 | FLAG_DIPLOME | Wizard's diploma |
| 31 | FLAG_DMKEY_KNARTA | Fragment of the Francos |
| 32 | FLAG_DMKEY_SUP | Fragment of the Sups |
| 33 | FLAG_DMKEY_MOSQUI | Fragment of the Mosquibees |
| 34 | FLAG_DMKEY_BLAFARD | Fragment of the Wannies |
| 35 | FLAG_CLE_REINE | Key for the passage to CX |
| 36 | FLAG_PIOCHE | Pick-ax |
| 37 | FLAG_CLEF_BOURGMESTRE | Franco Burgermaster's key |
| 38 | FLAG_NOTE_BOURGMESTRE | Franco Burgermaster's notes |
| 39 | FLAG_PROTECTION | Protective spell |
| 40 | FLAG_SCAPHANDRE | (Space suit – scenario var) |
| 79 | FLAG_CELEBRATION | Celebration crystal |
| 94 | FLAG_DINO_VOYAGE | Dino-Fly travel |
| 251 | FLAG_CLOVER | Clover leaves |
| 252 | FLAG_VEHICULE_PRIS | Vehicle taken |
| 253 | FLAG_CHAPTER | Chapter |
| 254 | FLAG_PLANETE_ESMER | Planet Esmer |

**Code:** `SOURCES/COMMON.H` (FLAG_*), `SOURCES/GLOBAL.CPP` (ListVarGame).

## Comportement (behavior mode)

| Value | Constant | Meaning |
|-------|----------|---------|
| 0 | C_NORMAL | Normal |
| 1 | C_SPORTIF | Athletic |
| 2 | C_AGRESSIF | Aggressive |
| 3 | C_DISCRET | Stealth |
| 4 | C_PROTOPACK | Protopack |
| 5 | C_DOUBLE | Twinsen and Zoé |
| 6 | C_CONQUE | Horn of the Blue Triton |
| 7 | C_SCAPH_INT_NORM | Diving suit interior (normal) |
| 8 | C_JETPACK | Super jetpack |
| 9 | C_SCAPH_INT_SPOR | Diving suit interior (athletic) |
| 10 | C_SCAPH_EXT_NORM | Diving suit exterior (normal) |
| 11 | C_SCAPH_EXT_SPOR | Diving suit exterior (athletic) |
| 12 | C_BUGGY | Buggy |
| 13 | C_SKELETON | Skeleton disguise |

**Code:** `SOURCES/COMMON.H` (C_* constants).

## Screenshot

Generated at save time in `SaveGame()`:

1. `ScaleBox(0, 0, 639, 479, Log, 0, 0, 159, 119, BufSpeak+50000L)` – downsample 640×480 frame to 160×120
2. `SaveBlock` – copy to temp buffer
3. `RemapPicture` – remap palette to game palette
4. `LbaWrite` – write 19200 bytes to file

Shown at load time: `LoadGameScreen()` returns pointer to image data; `DrawScreenSave()` (GAMEMENU.CPP) draws it. Used for main menu preview, save slot thumbnails, and delete confirmation. See [MENU.md](MENU.md).

## Config integration

`LastSave` in lba2.cfg stores the last-used player name for quick load. `CompressSave` (0/1) controls compression. See [CONFIG.md](CONFIG.md).

## Version compatibility

The **layout revision** is the low 7 bits of the first byte (`NUM_VERSION`, currently **36**). Branches in `LoadContexte` / `SaveContexte` change what is read and written after the fixed prefix (through checksum). The **compression** bit is independent (see [Version byte on disk](#version-byte-on-disk)).

### Version-specific layout (`LoadContexte`)

| Version | Change |
|---------|--------|
| &lt; 34 | Extra input fields (`LastStepFalling`, `LastStepShifting`); objects use `TempoRealAngle` instead of `BoundAngle`. **Only compiled in `DEBUG_TOOLS`, `TEST_TOOLS`, or `EDITLBA2`** — not in a default release player build. |
| ≥ 35 | Per-object **`SampleAlways`** (`S32`) added. |
| ≥ 36 | Per-object **`SampleVolume`** (`U8`) added. |

**Code:** [SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP) — `LoadContexte` / `SaveContexte` (object loop ~1216–1356).

### Checksum, `SceneStartX`, and `flaginit`

After reading the stored **`Checksum`**, the engine compares it to the runtime **`Checksum`** (scenario-dependent).

- **Mismatch:** `SceneStartX` is forced to **`-1`**. A warning may be shown in **DEBUG_TOOLS** / **TEST_TOOLS**. The loader then follows the **`if (SceneStartX == -1)`** branch in `LoadContexte` and **skips** restoring the large “extended” blob (objects, patches, flows, camera, …) — see the `if (SceneStartX == -1)` vs `else` structure in [SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP).
- **`LoadContexte` return value (`flaginit` inside `LoadGame`):** **`TRUE`** when the checksum / `SceneStartX == -1` short path ran (hero and globals repaired without consuming the extended blob), **`FALSE`** after a normal full restore of objects, patches, flows, etc.

**`LoadGame`** returns that value into **`flagload`** in **`ChangeCube`** ([SOURCES/OBJECT.CPP](SOURCES/OBJECT.CPP)):

```text
if (flagload < 0 || !flagload) InitLoadedGame(); else LoadFile3dObjects();
```

So **`flagload == 0`** (falsy → **`InitLoadedGame()`**) is the **usual** full load path; **`flagload < 0`** means **`LoadContexte`** hit a **stream truncation / bounds error** (issue #62) and also falls back to **`InitLoadedGame()`**; **non-zero positive `flagload`** skips **`InitLoadedGame`** and uses **`LoadFile3dObjects()`** only (extended state was already reconciled inside `LoadContexte`). **`LoadGameOldVersion`** returns **`TRUE`** (non-zero), so it always takes the **`LoadFile3dObjects()`** branch after the minimal restore.

### Version mismatch and `LoadGameOldVersion`

- **Release build:** When loading a save, **`LoadGame()` → `LoadContexte`** is always used. If the save’s layout revision **≠** `NUM_VERSION`, the byte stream **misaligns** — expect corruption or crashes; there is **no** automatic fallback.
- **`DEBUG_TOOLS` or `TEST_TOOLS`:** In `ChangeCube`, if **`(NumVersion & MASK_NUM_VERSION) != NUM_VERSION`**, the engine shows the French warning (*Sauvegarde trop ancienne…*) and calls **`LoadGameOldVersion()`** instead of **`LoadGame()`**.

**`LoadGameOldVersion`** ([SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP)): same header + optional decompress + skip screenshot, then restores only the **early** context (`ListVarGame`, `ListVarCube`, comportement, money/magic/keys, start positions, holomap, inventory). It does **not** restore darts, objects, patches, extras, zones, incrust, flows, camera, etc. Returns **`TRUE`**.

**Call site:** [SOURCES/OBJECT.CPP](SOURCES/OBJECT.CPP) — `ChangeCube`, `if (FlagLoadGame)` block (search for `LoadGameOldVersion` / `FlagLoadGame`).

### 32-bit vs 64-bit and pointers

Much of the extended payload is written as **`memcpy`-sized struct regions** (e.g. `T_OBJ_3D` minus `CurrentFrame`, full **`T_EXTRA`**, **`S_PART_FLOW`**). Sizes and padding depend on the **compiler and pointer width** of the binary that **wrote** the file.

**Consequence:** A save from the **original 32-bit** game is **not guaranteed** to load on this **64-bit** fork even when the first byte matches **`0x24` / `0xA4`**. Saves from this port match the port’s ABI.

**Community (issue #62):** When loading the extended object array, the engine may pick a **278-byte-per-object** (32-bit wire) stride vs the native stride using the same **`NbPatches`** sniff as [scripts/save_probe.py](../scripts/save_probe.py), then expand the embedded **`T_OBJ_3D`** (without `CurrentFrame`) with pointer fields cleared / numeric fields preserved. If both sniff positions look plausible, **`SaveLoadGuessObjectWireStride`** prefers the stride whose sniffed patch count equals **`NbPatches` from the scene already loaded from disk** (before the save stream overwrites it), so patch data stays aligned. If **both** sniff positions match that count (common when **`NbPatches` is 0** and padding reads as zero), the engine picks the **278-byte** retail wire stride. Stride sniffing compares the raw **`S32`** at each candidate offset to the scene count (memcmp) before plausibility rules. Sniffed patch counts are capped at **`MAX_PATCHES`** so values like misread **`384000`** do not win the **`s32 > s64`** tie-break and force the wrong wire. If **both** sniffs still fail and **neither** raw `NbPatches` word at the 278- nor 64-bit-native offset matches the scene hint (see `SaveLoadGuessObjectWireStride` in [SOURCES/SAVEGAME_LOAD_BOUNDS.CPP](SOURCES/SAVEGAME_LOAD_BOUNDS.CPP)), the guess returns **0** and `LoadContexte` uses the **host native** per-object stride — this avoids forcing retail wire when the save is actually a **native/port** blob (fixes regression on saves like **009** where both sniffs were unusable garbage). When only one stride offset fits the buffer, an exact **raw S32 == scene `NbPatches`** still picks that layout. Override with environment **`LBA2_SAVE_LOAD_ABI=32`** to force the legacy stride (useful when the heuristic is ambiguous).

**`PtrZoneClimb`:** stored and restored as a **32-bit** value (`LbaWriteLong` / read as `U32`); it does not round-trip a true 64-bit pointer. See `SaveContexte` / `LoadContexte` in [SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP).

### Recommendations

- **Forward compatibility:** A newer engine can load older saves via `LoadContexte` version branches (when revisions match or debug fallback applies).
- **Backward compatibility:** An older engine loading a newer revision misaligns the stream; avoid.
- **Upgrade:** After loading an old save in a build that supports it, save again to rewrite the current format.

### Format hardening (issue #62 — implemented vs planned)

**Implemented** ([SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP), [SOURCES/SAVEGAME_LOAD_BOUNDS.CPP](SOURCES/SAVEGAME_LOAD_BOUNDS.CPP), [docs/SAVEGAME_ISSUE62_CHECKLIST.md](SAVEGAME_ISSUE62_CHECKLIST.md)):

- **Whole-file read cap:** `LoadSize` into the **`Screen`** / **`BufSpeak+50000`** buffer with size **`640×480 + RECOVER_AREA`** (same allocation as [SOURCES/MEM.CPP](SOURCES/MEM.CPP)); oversize files are rejected.
- **Player name:** NUL-terminated read capped at **`MAX_SIZE_PLAYER_NAME`**; missing NUL → load failure.
- **Compressed block:** declared **`sizefile`** and compressed tail validated against buffer geometry **before** `ExpandLZ` / staging `memcpy`.
- **`LoadContexte`:** read cursor may not pass a configured end (`SaveLoadSetReadLimit`); on truncation **`LoadContexte` returns `-1`** and **`LoadGame`** returns **`LOADGAME_ERR_CONTEXT` (`-2`)** (not `FALSE`) so **`ChangeCube` does not call `InitLoadedGame()`** on a half-parsed `ListObjet` (that path could **SIGSEGV**). **`CheckProtoPack`** is skipped for the same return. Other **`LoadGame` failures** still use the legacy **`InitLoadedGame()`** path.
- **Counts:** `NbObjets` ≤ **`MAX_OBJETS`**, `NbPatches` ≤ **`MAX_PATCHES`**, patch **`Offset`/`Size`** within **`PtrSceneMem`**, extras / incrust / flows / flow-dot counts bounded (`MAX_EXTRAS`, `MAX_INCRUST_DISP`, `MAX_FLOWS`, `MAX_FLOW_DOTS`), `NbZones` ≤ **`MAX_ZONES`**, valid-position blob ≤ **`SIZE_BUFFER_VALIDE_POS`**.
- **Legacy object blob:** optional **136-byte** wire decode + field migration (see *32-bit vs 64-bit* above).
- **Clip window / cinema:** After reading **`ClipWindowYMin`** / **`ClipWindowYMax`** from the context tail, if either value is outside **`[0, ModeDesiredY-1]`**, **`YMin >= YMax`**, or negative, the pair is treated as corrupt (often tail misalignment when wire stride is wrong). The engine resets clip to **full height** (`0` … **`ModeDesiredY-1`**, fallback **479**) and clears **`CinemaMode`** plus **`TimerCinema`**, **`LastYCinema`**, **`DebCycleCinema`**, **`DureeCycleCinema`** so letterbox / menu paths do not run with impossible geometry. Debug NDJSON may log **`H12c`** when this fires.
- **Checkpoint reload:** [SOURCES/VALIDPOS.CPP](SOURCES/VALIDPOS.CPP) sets the read limit around **`BufferValidePos`**.

**Still future / larger change:**

- **Canonical wire format** or explicit **`NUM_VERSION` 37+** serializer that does not depend on host `sizeof` for pointer-bearing structs.
- **More host tests** with tiny binary fixtures hitting `LoadGame` end-to-end; optional save inspection via [scripts/save_probe.py](../scripts/save_probe.py) (LZSS via **`save_decompress`** — see [Tooling](#tooling-save_probe--save_decompress)).

## For save editors

- **`EDITLBA2`:** The tree still contains **`#ifdef EDITLBA2`** / **`#ifndef EDITLBA2`** branches around save/load and tools; the classic community build does not ship that editor configuration. A future cleanup may fold or remove those dead paths; behaviour documented here is the **non-editor** path unless stated otherwise.
- **Offsets:** All byte offsets in this doc are relative to the stated base (header, screenshot, or game context).
- **Endianness:** Little-endian (x86).
- **Validation:** Engine checks `NUM_VERSION` (low 7 bits of version byte) and `Checksum`. Checksum mismatch sets `SceneStartX=-1` (fallback to scene start) and, in DEBUG_TOOLS/TEST_TOOLS, shows: *"Warning: The save doesn't match the scenario!"* (French: *La sauvegarde ne correspond pas au scénario !*).
- **Scene codes:** Hex scene codes (e.g. `0x00` = Twinsen's house, `0x5D` = Celebration temple) are documented in [LBA File Info – Savegame](https://lbafileinfo.kaziq.net/index.php/LBA2:Savegame) (scene codes table by island).

## Creating quest saves

To create a valid quest save for testing scenarios:

1. **Start from a known-good save** – Load a normal save, play to the desired scenario, save. Use that `.lba` as template.
2. **Modify ListVarGame** – Set inventory flags (0/1) and quantities at the offsets in the table above. Ensure consistency (e.g. `FLAG_CHAPTER` matches story progress).
3. **Modify Holomap** – TabArrow `FlagHolo` bytes; bit 0 = active, bit 1 = asked.
4. **Modify position** – `SceneStartX/Y/Z`, `StartXCube/Y/Z`, `NumCube` (in header) must be consistent.
5. **Preserve header** – Version byte, player name, compression flag. Keep `Checksum` or accept possible fallback on load.
6. **Test** – Load in the engine; watch for `SceneStartX=-1` fallback (checksum mismatch).

## Future: console commands

A complete save format doc enables console commands such as:

- `save_var <index> <value>` – set `ListVarGame[index]`
- `save_holomap <index> <0|1|2|3>` – set `TabArrow[index].FlagHolo`
- `save_inv <slot> <ptmagie> <flaginv> <idobj3d>` – set inventory slot
- `save_behavior <0–13>` – set `Comportement`
- `save_magic <0–4>` – set `MagicLevel`
- `save_pos` – print current position
- `save_load <path>` / `save_save <path>` – load/save from console
- `quest_load <name>` – load from `quests/` directory (future)

## Code reference

| Concept | File | Function/Symbol |
|---------|------|----------------|
| Save | SAVEGAME.CPP | SaveGame, CurrentSaveGame, AutoSaveGame |
| Load | SAVEGAME.CPP | LoadGame, LoadGameNumCube, LoadGameScreen, LoadGamePlayerName |
| Context | SAVEGAME.CPP | SaveContexte, LoadContexte |
| Compression | SAVEGAME.CPP, LZSS.CPP, LIB386/SYSTEM/LZ.CPP | Compress_LZSS, ExpandLZ |
| Whole-file read | LIB386/SYSTEM/LOADSAVE.CPP | `LoadSize` (capped load for save paths; `Load` still used elsewhere) |
| `Screen` buffer size | MEM.CPP | `SmartMalloc` entry for `Screen` |
| Post-load branch | OBJECT.CPP | `ChangeCube` (`FlagLoadGame`, `flagload`, `InitLoadedGame`, `LoadFile3dObjects`) |
| Screenshot capture | SAVEGAME.CPP | ScaleBox, SaveBlock, RemapPicture |
| Screenshot display | GAMEMENU.CPP | LoadGameScreen, DrawScreenSave |
| Menu / listing | GAMEMENU.CPP | Player listing, `FindPlayerFile`, etc. |
| Console load | CONSOLE/CONSOLE_CMD.CPP | `load` command (`FlagLoadGame`, `LoadGameNumCube`) |
| Startup argv save | PERSO.CPP | Resolves path, sets `FlagLoadGame`, `LoadGameNumCube` in init flow |
| Paths | DIRECTORIES.CPP | GetSavePath |

## Tooling (`save_probe` / `save_decompress`)

- **[scripts/save_probe.py](../scripts/save_probe.py)** — Header, optional **LZSS** decompression via **`save_decompress`**, `NbObjets` at game-context offset **1478**, heuristic **`obj3d_abi`** (32 vs 64) using **`NbPatches`**, strict patch-blob fit, and extras count bound (reports **`ambiguous`** when neither stride wins clearly — use **`--obj3d-abi 32|64`** to force). Environment **`LBA2_SAVE_PROBE_ABI=32|64`** applies when **`--obj3d-abi auto`** (CLI overrides env). **`--json-lines`** prints **NDJSON** (includes **`version_byte_hex`**, **`dump_lines`** as a string array when **`--dump`**). **`num_version` < 34** sets **`layout_warning`**. Flags: **`--dump`**, **`--json-lines`**, **`--compare`**, **`--recursive`**, **`--summary`** (counts on stderr, safe with **`--json-lines`**). Set **`LBA2_SAVE_DECOMPRESS`** to the helper if it is not found under `build/tools/` or `out/build/*/tools/` (or **`PATH`**).
- **`save_decompress`** (CMake target) — stdin = compressed tail, argv\[1\] = decompressed byte count, stdout = raw payload. Uses **`ExpandLZ(..., MinBloc=2)`** from [LIB386/SYSTEM/LZ.CPP](LIB386/SYSTEM/LZ.CPP). Build: `cmake --build <dir> --target save_decompress` (option **`LBA2_BUILD_SAVE_TOOLS`**, default ON).
- **[scripts/save_probe_lz_selftest.py](../scripts/save_probe_lz_selftest.py)** — Golden LZ vectors aligned with [tests/SYSTEM/test_lz.cpp](tests/SYSTEM/test_lz.cpp). **`make save-probe-lz-selftest`** configures with **`LBA2_BUILD_SAVE_TOOLS=ON`**, builds **`save_decompress`**, runs the script.

## Cross-references

- [SAVEGAME_ISSUE62_CHECKLIST.md](SAVEGAME_ISSUE62_CHECKLIST.md) — tracked work for [#62](https://github.com/LBALab/lba2-classic-community/issues/62) (load safety / legacy layout); merge into this doc when stable.
- [MENU.md](MENU.md) for Save/Load menu flow and screenshot display
- [CONFIG.md](CONFIG.md) for LastSave and CompressSave
- [DEBUG.md](DEBUG.md) for DEBUG_TOOLS bug save/load (G/L keys, menu cases 2000/2001)
- [CONSOLE.md](CONSOLE.md) for `savebug` / `loadbug` / `listbugs`
- [GLOSSARY.md](GLOSSARY.md) for Comportement, GenBody, GenAnim

## External resources

- **[LBA File Info – Savegame](https://lbafileinfo.kaziq.net/index.php/LBA2:Savegame)** – Community reverse-engineering: full binary layout with fan names, holomap location list, scene codes by island. This doc’s layout is verified against the engine; LBA File Info provides additional context and scene code tables.
- **[LBALab/metadata](https://github.com/LBALab/metadata)** – JSON metadata for HQR/asset files (indices, names). Useful for mapping `IdObj3D` and inventory IDs to human-readable names; does not cover savegame format.

## LBALab save tools (community)

[LBALab](https://github.com/LBALab) hosts open-source LBA2 save utilities (Rust). Engine code remains the source of truth; these tools reflect community reverse-engineering.

| Tool | Purpose | Notes |
|------|---------|-------|
| [LBA2SD](https://github.com/LBALab/LBA2SD) | Save de/compressor | LZSS decompression works; compressor not yet implemented. Confirms 0x24 / 0xA4. |
| [LBA2R2I](https://github.com/LBALab/LBA2R2I) | Screenshot extractor | Reads decompressed saves, extracts 160×120 image to PNG/JPEG/etc. |
| [LBA2I2R](https://github.com/LBALab/LBA2I2R) | Custom screenshot writer | Writes images into decompressed saves. |

**Screenshot palette (LBA2R2I):** The save stores 8-bit indexed pixels; the engine remaps to the game palette at save time. LBA2R2I includes a hardcoded 256-color RGB palette for converting raw pixels to viewable images. Useful when extracting screenshots without the game’s palette. *Attribution: [LBALab/LBA2R2I](https://github.com/LBALab/LBA2R2I) `r2i.rs`.*

**Header interpretation:** LBALab tools treat bytes 1–4 as “system_hour” (1 byte) + “zeros” (3 bytes). The engine uses bytes 1–4 as the 4-byte scene/cube index (`NumCube`). For de/compression and screenshot extraction the offset to the payload can still align; for full-format tools, use the engine layout in this doc.
