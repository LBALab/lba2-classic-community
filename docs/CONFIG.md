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

### Original keys (Adeline)

| Key | Purpose | Source | Menu |
|-----|---------|--------|------|
| LastSave | Player name for quick load | ReadConfigFile / WriteConfigFile | (implicit) |
| Shadow | Shadow quality (0–3) | ReadConfigFile / WriteConfigFile | Options → Detail |
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

### Community / modernized additions

- Document any keys added in this fork (e.g. SDL backend, new paths). Leave a section for future additions so new config can be clearly marked.

## Code reference

| Concept           | File                      | Function/Symbol                          |
| ----------------- | ------------------------- | ---------------------------------------- |
| Config read/write | PERSO.CPP                 | ReadConfigFile, WriteConfigFile          |
| Volume persistence| AMBIANCE.CPP              | ReadVolumeSettings, WriteVolumeSettings  |
| Input persistence | INPUT.CPP                 | ReadInputConfig, WriteInputConfig        |
| Config path       | DIRECTORIES.CPP           | GetCfgPath, GetDefaultCfgPath            |
| DefFile API       | LIB386/SYSTEM/DEFFILE.CPP | DefFileBufferInit, DefFileBufferRead*, DefFileBufferWrite* |

## Cross-references

- [MENU.md](MENU.md) for options menu flow
- [AUDIO.md](AUDIO.md) for volume/master volume behavior
- [GFX_OPTIONS.md](GFX_OPTIONS.md) for DetailLevel / Shadow effects
