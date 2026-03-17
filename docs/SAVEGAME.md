# Savegame System

Savegame files (`.lba`) store game state: scene, position, inventory, holomap, screenshot, and more. This doc covers the engine lifecycle and full binary layout. Layout is verified against `SaveContexte` / `LoadContexte` in [SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP).

Truth hierarchy: **code > this document > external sources**.

## File paths

| Name | Constant | Purpose |
|------|----------|---------|
| `current.lba` | `CURRENTSAVE_FILENAME` | Quick-save; enables Resume in main menu |
| `autosave.lba` | `AUTOSAVE_FILENAME` | Auto-save during gameplay |
| `*.lba` | User-chosen | Named saves (player name → filename) |

Path resolution: `GetSavePath(outPath, pathMaxSize, saveFilename)` → `<userDir>/save/` + filename ([SOURCES/DIRECTORIES.CPP](SOURCES/DIRECTORIES.CPP)).

## Lifecycle

### Save

| Trigger | Function | Notes |
|---------|----------|-------|
| Menu Save | `SaveGame(TRUE)` | User-initiated |
| Quick-save (MENUS) | `CurrentSaveGame()` | Saves to `current.lba`; player name set to "CURRENT" |
| Auto-save | `AutoSaveGame()` | Saves to `autosave.lba` at scene transitions |
| Bug save (DEBUG_TOOLS) | `SaveGame(TRUE)` | Debug save to bug directory |

**Code:** [SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP) – `SaveGame`, `CurrentSaveGame`, `AutoSaveGame`

### Load

| Trigger | Function | Notes |
|---------|----------|-------|
| Menu Load / Resume | `LoadGame()` | Full load; sets `NewCube`, `PlayerName`, restores state |
| Load screenshot only | `LoadGameScreen()` | Reads header + 160×120 image; used for menu preview |
| Load player name only | `LoadGamePlayerName()` | Reads header + player name; used when listing saves |

**Code:** [SOURCES/SAVEGAME.CPP](SOURCES/SAVEGAME.CPP) – `LoadGame`, `LoadGameNumCube`, `LoadGameScreen`, `LoadGamePlayerName`

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
| 1343 | var | Input state, darts, objects, patches, extras, zones, incrust, flows, camera, etc. |

The full `SaveContexte` / `LoadContexte` continues with objects, zones, camera, and more. See SAVEGAME.CPP lines 706–1076 (save) and 1080–1548 (load).

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

The version byte (low 7 bits) is `NUM_VERSION` (36). Layout changes between versions; old saves can fail or load incorrectly.

### Version-specific layout (LoadContexte)

| Version | Change |
|---------|--------|
| &lt; 34 | Input state: `LastStepFalling`, `LastStepShifting`; objects: `TempoRealAngle` instead of `BoundAngle`. *(DEBUG_TOOLS only)* |
| ≥ 35 | Objects: `SampleAlways` field added |
| ≥ 36 | Objects: `SampleVolume` field added |

**Code:** SAVEGAME.CPP lines 1216–1356.

### Version mismatch handling

- **Release build:** Always uses `LoadGame()` → `LoadContexte`. If the save version differs, the stream can misalign and produce corruption or crashes.
- **DEBUG_TOOLS / TEST_TOOLS:** If `(NumVersion & MASK_NUM_VERSION) != NUM_VERSION`, shows: *"Warning: Save too old. I'll try to load it, but be careful!"* (French: *Sauvegarde trop ancienne. Je vais tenter de la charger, mais méfie !!*) and calls `LoadGameOldVersion()` instead.

### LoadGameOldVersion (fallback)

A simplified loader that reads only the core context (ListVarGame, ListVarCube, Comportement, magic, position, TabArrow, TabInv) and sets the hero position. It does **not** restore: darts, object positions, patches, extras, zones, incrust, flows, camera state, etc. Use when a full load would fail; the game will start with correct inventory and position but lose scene detail.

**Code:** SAVEGAME.CPP `LoadGameOldVersion`, OBJECT.CPP ~1495.

### Recommendations

- **Forward compatibility:** A newer engine can load older saves via `LoadContexte` version branches.
- **Backward compatibility:** An older engine loading a newer-version save will misalign the stream; avoid.
- **Upgrade:** After loading an old save, save again to upgrade to the current format.

## For save editors

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
| Compression | SAVEGAME.CPP, COMPRESS.CPP | Compress_LZSS, ExpandLZ |
| Screenshot capture | SAVEGAME.CPP | ScaleBox, SaveBlock, RemapPicture |
| Screenshot display | GAMEMENU.CPP | LoadGameScreen, DrawScreenSave |
| Paths | DIRECTORIES.CPP | GetSavePath |

## Cross-references

- [MENU.md](MENU.md) for Save/Load menu flow and screenshot display
- [CONFIG.md](CONFIG.md) for LastSave and CompressSave
- [DEBUG.md](DEBUG.md) for bug save/load
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
