# Configuration file (lba2.cfg)

lba2.cfg stores user preferences and last-save info. Read at startup, written at exit. Options changed in the menu (see [MENU.md](MENU.md)) are persisted here. This doc distinguishes original keys from community additions so future changes can be tracked.

## Path and discovery

- Filename: `lba2.cfg` ([SOURCES/COMMON.H](../SOURCES/COMMON.H) line 83: `CFG_NAME`)
- User config path: `GetCfgPath(PathConfigFile, ..., CFG_NAME)` → `directoriesCfgDir` + filename ([SOURCES/DIRECTORIES.CPP](../SOURCES/DIRECTORIES.CPP))
- Default (assets): `GetDefaultCfgPath()` → `directoriesResDir` + filename (same folder as `lba2.hqr`, i.e. the game assets directory)
- If user config missing: copy from assets to config dir ([SOURCES/INITADEL.C](../SOURCES/INITADEL.C)). If the assets folder has no `lba2.cfg`, the engine writes an embedded copy (generated at build time from [SOURCES/LBA2.CFG](../SOURCES/LBA2.CFG) via [cmake/embed_lba2_cfg.cmake](../cmake/embed_lba2_cfg.cmake)) into the user config path instead of exiting.
- **Default config source**: Prefer the file in the asset directory (`directoriesResDir`). The repo has `SOURCES/LBA2.CFG` and `SOURCES/CONFIG/LBA2.CFG` as reference/templates. See [GAME_DATA.md](GAME_DATA.md) for where assets live.

## File format

- DefFile format: `Key: value` or `Key= value`, `;` comments ([LIB386/SYSTEM/DEFFILE.CPP](../LIB386/SYSTEM/DEFFILE.CPP))
- API: `DefFileBufferInit(file, buffer, size)`, `DefFileBufferReadString`/`ReadValue`/`ReadValueDefault`, `DefFileBufferWriteString`/`WriteValue`

## Lifecycle

- **Read**: `ReadConfigFile()` in [SOURCES/PERSO.CPP](../SOURCES/PERSO.CPP) (line 1701), invoked from `InitProgram()` at line 1825
- **Write**: `WriteConfigFile()` in PERSO.CPP (line 1757), called from `TheEndInfo()` (line 1955)
- Options menu changes globals only; config is written once at exit. No intermediate saves when changing options.

## Keys: what each does

### Accepted values

| Key | Type | Accepted values | Default | Clamping / notes |
|-----|------|-----------------|---------|------------------|
| LastSave | string | Player name, max 100 chars | (empty) | Used for quick load |
| Shadow | int | 1–3 | 3 | Overwritten by DetailLevel when leaving Options. 1=none on extras, 2=no impact shadows, 3=full |
| AllCameras | int | 0, 1 | 1 | 0=OFF, 1=ON |
| FollowCamera | int | 0, 1 | 0 | Auto camera (user-facing name; Enhanced Edition–style third-person follow in exterior). Config key stays `FollowCamera` for stability. 0=classic (default), 1=auto. Also reads legacy key `AutoCameraCenter` |
| ReverseStereo | int | 0, 1 | 0 | 0=OFF, 1=ON |
| DetailLevel | int | 0–3 | 3 | 0=min (no rain, no sea, no horizon), 1=486, 2=base Pentium, 3=max. Drives Shadow, RainEnable, MaxPolySea, FlagDrawHorizon |
| FullScreen | int | 0, 1 | 1 | 0=small videos, 1=fullscreen videos. Invalid values → 1 |
| DisplayFullScreen | int | 0, 1 | 0 | 0=windowed display, 1=fullscreen display. Invalid values → 0 |
| FlagDisplayText | string | ON, OFF | ON | Case-insensitive. Any other value → ON |
| WaveVolume | int | 0–127 | 97 | Sample/SFX volume |
| VoiceVolume | int | 0–127 | 112 | Voice volume |
| MusicVolume | int | 0–127 | 127 | Music/jingle volume (stored as JingleVolume in code) |
| CDVolume | int | 0–127 | 66 | CD audio volume (no-op when no CD); still supported in config but no longer shown in the in-game volume submenu |
| MasterVolume | int | 0–127 | 127 | Master volume, scales samples and music |
| Input0_1..Input35_2 | int | Key scancodes | DefKeysDefault95 | 36 inputs × 2 keys each (`MAX_INPUT` in INPUT.H). Only read when WinMode=1 |
| WinMode | int | 0, 1 | 0 | 0=ignore Input* keys, use defaults; 1=read Input* keys. WriteInputConfig always writes WinMode=1 |
| CompressSave | int | 0, 1 | 1 | 0=uncompressed saves, 1=compressed |
| Version, Version_US | int | Distributor ID | 0 (UNKNOWN_VERSION) | Activision, EA, Virgin, regional variants. Set via `distrib` console command. |
| Language | string | English, Français, Deutsch, Español, Italiano, Portugues | English | Must match `TabLanguage[]` exactly (case-insensitive) |
| LanguageCD | string | Same as Language | English | Voice CD language; only used with CDROM build |
| FlagKeepVoice | string | ON, OFF | ON | Keep voice files on HD |
| MenuMouse | int | 0, 1 | 1 | 1 = menu cursor, hover/left-click confirm, wheel for sliders and save list; 0 = keyboard/joystick only (classic) |

### Original keys (Adeline)

| Key | Purpose | Source | Menu |
|-----|---------|--------|------|
| LastSave | Player name for quick load | ReadConfigFile / WriteConfigFile | (implicit) |
| Shadow | Shadow quality (1–3) | ReadConfigFile / WriteConfigFile | Options → Detail |
| AllCameras | Scenario cameras ON/OFF | ReadConfigFile / WriteConfigFile | Options |
| ReverseStereo | Stereo invert | ReadConfigFile / WriteConfigFile | Options |
| DetailLevel | Graphics detail (0–3) | ReadConfigFile / WriteConfigFile | Options |
| FullScreen | Video playback size | ReadConfigFile / WriteConfigFile | Options → Advanced options |
| DisplayFullScreen | Window/display fullscreen toggle | ReadConfigFile / WriteConfigFile | Options → Advanced options |
| FlagDisplayText | Show subtitles during voice | ReadConfigFile / WriteConfigFile | Options → Advanced options |
| WaveVolume, VoiceVolume, MusicVolume, MasterVolume | Volume sliders | ReadVolumeSettings / WriteVolumeSettings | Options → Sound volume |
| CDVolume | CD audio volume | ReadVolumeSettings / WriteVolumeSettings | config only |
| Input0_1..Input35_2, WinMode | Keyboard mappings | ReadInputConfig / WriteInputConfig | Options → Keyboard |
| CompressSave | Save compression format | ReadConfigFile | (installer) |
| Version, Version_US | Distributor version | ReadConfigFile / `distrib` console | (installer; `distrib` console) |
| Language, LanguageCD, FlagKeepVoice | Language / voice CD | MESSAGE.CPP, ReadConfigFile / WriteConfigFile | Options → Choose language |

**Note:** `FlagKeepVoice` remains installer / CONFIG-tool managed. `Language` and `LanguageCD` are now also written by the in-game Options menu.

### Community / modernized additions

| Key | Purpose | Source | Menu |
|-----|---------|--------|------|
| MenuMouse | Optional mouse UX in game menus (`FlagMenuMouse` in code). Default 1 (on). Set 0 to match classic keyboard/joystick-only menus. See [MENU.md](MENU.md) | ReadConfigFile / WriteConfigFile | Options → Advanced options |
| FollowCamera | Auto camera for exterior scenes (0=classic, 1=auto). Community addition, not in original game; menu label is "Auto camera" / "Classic camera" | ReadConfigFile / WriteConfigFile | Options → Advanced options |

## Code reference


| Concept            | File                      | Function/Symbol                                            |
| ------------------ | ------------------------- | ---------------------------------------------------------- |
| Config read/write  | PERSO.CPP                 | ReadConfigFile, WriteConfigFile                            |
| Volume persistence | AMBIANCE.CPP              | ReadVolumeSettings, WriteVolumeSettings                    |
| Input persistence  | INPUT.CPP                 | ReadInputConfig, WriteInputConfig                          |
| Config path        | DIRECTORIES.CPP           | GetCfgPath, GetDefaultCfgPath                              |
| DefFile API        | LIB386/SYSTEM/DEFFILE.CPP | DefFileBufferInit, DefFileBufferRead*, DefFileBufferWrite* |


## Cross-references

- [MENU.md](MENU.md) for options menu flow
- [SAVEGAME.md](SAVEGAME.md) for LastSave and CompressSave usage
- [AUDIO.md](AUDIO.md) for volume/master volume behavior
- [GFX_OPTIONS.md](GFX_OPTIONS.md) for DetailLevel / Shadow effects
- [CAMERA.md](CAMERA.md) for camera system and Auto camera (`FollowCamera` key)

