# Configuration File (lba2.cfg)

lba2.cfg stores user preferences and last-save info. Read at startup, written at exit. Options changed in the menu (see [MENU.md](MENU.md)) are persisted here. This doc distinguishes **original** keys from **community** additions so future changes can be tracked.

## Path and discovery

- Filename: `lba2.cfg` ([SOURCES/COMMON.H](SOURCES/COMMON.H) line 83: `CFG_NAME`)
- User config path: `GetCfgPath(PathConfigFile, ..., CFG_NAME)` → `directoriesCfgDir` + filename ([SOURCES/DIRECTORIES.CPP](SOURCES/DIRECTORIES.CPP))
- Default (assets): `GetDefaultCfgPath()` → `directoriesResDir` + filename (same folder as `lba2.hqr`, i.e. the game assets directory)
- If user config missing: copy from assets to config dir ([SOURCES/INITADEL.C](SOURCES/INITADEL.C) lines 103–117). Game exits with error if default not found.
- **Default config source**: The default `lba2.cfg` lives in the assets folder (GOG/Steam game directory). The repo has `SOURCES/LBA2.CFG` and `SOURCES/CONFIG/LBA2.CFG` as reference/templates; the running game uses whatever is in `directoriesResDir` when the user has no config yet.

## File format

- DefFile format: `Key: value` or `Key= value`, `;` comments ([LIB386/SYSTEM/DEFFILE.CPP](LIB386/SYSTEM/DEFFILE.CPP))
- API: `DefFileBufferInit(file, buffer, size)`, `DefFileBufferReadString`/`ReadValue`/`ReadValueDefault`, `DefFileBufferWriteString`/`WriteValue`

## Lifecycle

- **Read**: `ReadConfigFile()` in [SOURCES/PERSO.CPP](SOURCES/PERSO.CPP) (line 1828), called from `InitGame()` (line 1934)
- **Write**: `WriteConfigFile()` in PERSO.CPP (line 1871), called from `TheEndInfo()` at exit (line 2073)
- Options menu changes globals only; config is written once at exit. No intermediate saves when changing options.

## Keys: what each does

### Accepted values

| Key | Type | Accepted values | Default | Clamping / notes |
|-----|------|-----------------|---------|------------------|
| LastSave | string | Player name, max 100 chars | (empty) | Used for quick load |
| Shadow | int | 1–3 | 3 | Overwritten by DetailLevel when leaving Options. 1=none on extras, 2=no impact shadows, 3=full |
| AllCameras | int | 0, 1 | 1 | 0=OFF, 1=ON |
| ReverseStereo | int | 0, 1 | 0 | 0=OFF, 1=ON |
| DetailLevel | int | 0–3 | 3 | 0=min (no rain, no sea, no horizon), 1=486, 2=base Pentium, 3=max. Drives Shadow, RainEnable, MaxPolySea, FlagDrawHorizon |
| FullScreen | int | 0, 1 | 1 | 0=windowed, 1=fullscreen. Invalid values → 1 |
| FlagDisplayText | string | ON, OFF | ON | Case-insensitive. Any other value → ON |
| WaveVolume | int | 0–127 | 97 | Sample/SFX volume |
| VoiceVolume | int | 0–127 | 112 | Voice volume |
| MusicVolume | int | 0–127 | 127 | Music/jingle volume (stored as JingleVolume in code) |
| CDVolume | int | 0–127 | 66 | CD audio volume (no-op when no CD) |
| MasterVolume | int | 0–127 | 127 | Master volume, scales samples and music |
| Input0_1..Input35_2 | int | Key scancodes | DefKeysDefault95 | 36 inputs × 2 keys each (`MAX_INPUT` in INPUT.H). Only read when WinMode=1 |
| WinMode | int | 0, 1 | 0 | 0=ignore Input* keys, use defaults; 1=read Input* keys. WriteInputConfig always writes WinMode=1 |
| CompressSave | int | 0, 1 | 1 | 0=uncompressed saves, 1=compressed |
| Version, Version_US | int | Distributor ID | UNKNOWN_VERSION | Activision, EA, Virgin, etc. |
| Language | string | English, Français, Deutsch, Español, Italiano, Portugues | Français | Must match `TabLanguage[]` exactly (case-insensitive) |
| LanguageCD | string | Same as Language | Français | Voice CD language; only used with CDROM build |
| FlagKeepVoice | string | ON, OFF | ON | Keep voice files on HD |

### Original keys (Adeline)

| Key | Purpose | Source | Menu |
|-----|---------|--------|------|
| LastSave | Player name for quick load | ReadConfigFile / WriteConfigFile | (implicit) |
| Shadow | Shadow quality (1–3) | ReadConfigFile / WriteConfigFile | Options → Detail |
| AllCameras | Scenario cameras ON/OFF | ReadConfigFile / WriteConfigFile | Options |
| ReverseStereo | Stereo invert | ReadConfigFile / WriteConfigFile | Options |
| DetailLevel | Graphics detail (0–3) | ReadConfigFile / WriteConfigFile | Options |
| FullScreen | Fullscreen toggle | ReadConfigFile / WriteConfigFile | Options |
| FlagDisplayText | Show subtitles during voice | ReadConfigFile / WriteConfigFile | Options |
| WaveVolume, VoiceVolume, MusicVolume, CDVolume, MasterVolume | Volume sliders | ReadVolumeSettings / WriteVolumeSettings | Options → Volume |
| Input0_1..Input35_2, WinMode | Keyboard mappings | ReadInputConfig / WriteInputConfig | Options → Keyboard |
| CompressSave | Save compression format | ReadConfigFile | (installer) |
| Version, Version_US | Distributor version | ReadConfigFile | (installer) |
| Language, LanguageCD, FlagKeepVoice | Language / voice CD | MESSAGE.CPP | (installer / CONFIG tool) |

**Note:** Language, LanguageCD, FlagKeepVoice are read by the game but not written by `WriteConfigFile`. They are typically set by the installer or standalone CONFIG tool.

### Community / modernized additions

- Document any keys added in this fork (e.g. SDL backend, new paths). Leave a section for future additions so new config can be clearly marked.

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
- [AUDIO.md](AUDIO.md) for volume/master volume behavior
- [GFX_OPTIONS.md](GFX_OPTIONS.md) for DetailLevel / Shadow effects

