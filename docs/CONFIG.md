# Configuration file (lba2.cfg)

lba2.cfg stores user preferences and last-save info. Read at startup, written at exit. Options changed in the menu (see [MENU.md](MENU.md)) are persisted here. This doc distinguishes original keys from community additions so future changes can be tracked.

## Path and discovery

- Filename: `lba2.cfg` ([SOURCES/COMMON.H](../SOURCES/COMMON.H) line 83: `CFG_NAME`)
- User config path: `GetCfgPath(PathConfigFile, ..., CFG_NAME)` → `directoriesCfgDir` + filename ([SOURCES/DIRECTORIES.CPP](../SOURCES/DIRECTORIES.CPP))
- Default (assets): `GetDefaultCfgPath()` → `directoriesResDir` + filename (same folder as `LBA2.HQR`, i.e. the game assets directory)
- If user config missing: copy from assets to config dir ([SOURCES/INITADEL.C](../SOURCES/INITADEL.C)). If the assets folder has no `lba2.cfg`, the engine writes an embedded copy (generated at build time from [SOURCES/LBA2.CFG](../SOURCES/LBA2.CFG) via [cmake/embed_lba2_cfg.cmake](../cmake/embed_lba2_cfg.cmake)) into the user config path instead of exiting.
- **Default config source**: Prefer the file in the asset directory (`directoriesResDir`). The repo has `SOURCES/LBA2.CFG` and `SOURCES/CONFIG/LBA2.CFG` as reference/templates. See [GAME_DATA.md](GAME_DATA.md) for where assets live.

## File format

- DefFile format: `Key: value` or `Key= value`, `;` comments ([LIB386/SYSTEM/DEFFILE.CPP](../LIB386/SYSTEM/DEFFILE.CPP))
- API: `DefFileBufferInit(file, buffer, size)`, `DefFileBufferReadString`/`ReadValue`/`ReadValueDefault`, `DefFileBufferWriteString`/`WriteValue`

## Lifecycle

- **Read**: `ReadConfigFile()` in [SOURCES/CONFIG_FILE.CPP](../SOURCES/CONFIG_FILE.CPP), invoked from `InitProgram()`
- **Write**: `WriteConfigFile()` in the same file, called from `TheEndInfo()`
- Not to be confused with [SOURCES/CONFIG.CPP](../SOURCES/CONFIG.CPP), which is the in-game key-binding menu
- Options menu changes globals only; config is written once at exit. No intermediate saves when changing options.
- **A setting forced for one run is not written back.** The write serialises globals, so without this a
  flag whose own help says "this run only" would leave its value in the player's config for every later
  launch. `--fixed-timestep`, `--language`, `--vsync` and `LBA2_TEXFILTER` go through
  `Settings_ValueToPersist` in [SOURCES/SETTINGS.H](../SOURCES/SETTINGS.H), which puts the stored preference back
  while the live value is still the one that was forced. Host test: [`tests/settings/`](../tests/settings/).
  `--resolution` reaches the same end differently: `Res_ResolutionShouldPersist` is false for it, so
  `WriteConfigFile` leaves `ResolutionX/Y` as it found them. Only two things make a resolution a
  preference to keep, the config's own value and a resolution picked during the run (Display submenu,
  `resolution` console verb); an auto-detected one is left out of the file entirely so it re-derives
  from the display each launch.
- Which flags may write is declared in `CLI_ARGS.CPP`'s `writes` column, printed under `--help-all` as
  "[keeps this in your settings]", and held to by
  [`tests/automation/test_cli_flag_contract.sh`](../tests/automation/test_cli_flag_contract.sh). Only
  six may: `--profile`, `--pick-game-dir` and `--bind-game-dir`, whose job is to record a choice;
  `--load`, because restoring a save makes its player the current one and the config records that in
  `LastSave` exactly as loading from the menu does; and `--exec` / `--exec-at`, which carry console
  commands and so carry whatever those commands persist. Everything else must leave the settings
  byte-identical.

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
| DisplayFullScreen | int | 0, 1 | 1 | 0=windowed display, 1=fullscreen display. Invalid values → 0 |
| FlagDisplayText | string | ON, OFF | ON | Case-insensitive. Any other value → ON |
| WaveVolume | int | 0–127 | 97 | Sample/SFX volume |
| VoiceVolume | int | 0–127 | 112 | Voice volume |
| MusicVolume | int | 0–127 | 127 | Music/jingle volume (stored as JingleVolume in code) |
| CDVolume | int | 0–127 | 66 | CD audio volume (no-op when no CD); still supported in config but no longer shown in the in-game volume submenu |
| MasterVolume | int | 0–127 | 127 | Master volume, scales samples and music |
| Input0_1..Input35_2 | int | Key scancodes | DefKeysDefault95 | 36 inputs × 2 keys each (`MAX_INPUT` in INPUT.H). Only read when WinMode=1 |
| WinMode | int | 0, 1 | 0 | 0=ignore Input* keys, use defaults; 1=read Input* keys. WriteInputConfig always writes WinMode=1 |
| CompressSave | int | 0, 1 | 1 | 0=uncompressed saves, 1=compressed |
| Version | int | 0–5, distributor ID | 0 (UNKNOWN_VERSION) | Which publisher's edition this is (`DistribVersion`): Activision, EA, Virgin, regional variants. Installer-written; set via the `distrib` console command. See [VERSIONS.md](VERSIONS.md) |
| Version_US | int | any | -1 when absent | Read into `Version_US` and never used anywhere. Unrelated to `Version`. See [VERSIONS.md](VERSIONS.md#version_us) |
| ShowDistribLogo | int | 0, 1 | 1 when `Version` is declared, 0 when it is not | Whether the publisher splash is drawn. Set via the `distrib logo` console command. The default is derived rather than stored, because a release that declares nothing ships no publisher branding either and a value read off the data is not grounds for showing one. Read only, never written back. See [VERSIONS.md](VERSIONS.md) |
| Language | string | English, Français, Deutsch, Español, Italiano, Portugues | English | Must match `TabLanguage[]` exactly (case-insensitive) |
| LanguageCD | string | Same as Language | English | Voice CD language; only used with CDROM build |
| FlagKeepVoice | string | ON, OFF | ON | Keep voice files on HD |
| MenuMouse | int | 0, 1 | 1 | 1 = menu cursor, hover/left-click confirm, wheel for sliders and save list; 0 = keyboard/joystick only (classic) |
| TextureFilter | int | 0–2 | 0 | Filtered texture sampling in the software fillers. 0=off (unchanged output), 1=horizontal 2-tap, 2=bilinear 4-tap. `LBA2_TEXFILTER` overrides for one run without persisting. See [GFX_OPTIONS.md](GFX_OPTIONS.md) |
| FixedTimestep | int | 0–100 (ms) | 16 | Sim throttle, so movement is frame-rate independent above 60 fps; 0 restores the historical per-frame simulation. Set by the `fixedtimestep` console verb; `--fixed-timestep` overrides for one run without persisting. See [MOVEMENT_FRAMERATE.md](MOVEMENT_FRAMERATE.md) |
| VSync | int | 0, 1 | 1 | Cap the frame rate to the display refresh. Invalid values → 1. Set by the Display submenu's toggle and the `vsync` console verb; `--vsync <on\|off>` overrides for one run without persisting. The Display submenu prints it, so a UI capture has to pin it; see [CONTROL.md](CONTROL.md#environmental-hygiene) |
| DitherShading | int | 0, 1 | 0 | Ordered dither on Gouraud shade rows, softening the 16-step ramp banding. See [GFX_OPTIONS.md](GFX_OPTIONS.md) |

### Original keys (Adeline)

| Key | Purpose | Source | Menu |
|-----|---------|--------|------|
| LastSave | Player name for quick load | ReadConfigFile / WriteConfigFile | (implicit) |
| Shadow | Shadow quality (1–3) | ReadConfigFile / WriteConfigFile | Options → Detail |
| AllCameras | Scenario cameras ON/OFF | ReadConfigFile / WriteConfigFile | Options |
| ReverseStereo | Stereo invert | ReadVolumeSettings / WriteVolumeSettings | Options, cvar `snd_reverse_stereo` |
| DetailLevel | Graphics detail (0–3) | ReadConfigFile / WriteConfigFile | Options |
| FullScreen | Video playback size | ReadConfigFile / WriteConfigFile | Options → Advanced options |
| DisplayFullScreen | Window/display fullscreen toggle | ReadConfigFile / WriteConfigFile | Options → Advanced options |
| FlagDisplayText | Show subtitles during voice | ReadConfigFile / WriteConfigFile | Options → Advanced options |
| WaveVolume, VoiceVolume, MusicVolume, MasterVolume | Volume sliders | ReadVolumeSettings / WriteVolumeSettings | Options → Sound volume, cvars `snd_wave` `snd_voice` `snd_music` `snd_master` |
| CDVolume | CD audio volume | ReadVolumeSettings / WriteVolumeSettings | config only, cvar `snd_cd` |
| Input0_1..Input35_2, WinMode | Keyboard mappings | ReadInputConfig / WriteInputConfig | Options → Keyboard |
| CompressSave | Save compression format | ReadConfigFile | (installer) |
| Version | Distributor edition (`DistribVersion`) | ReadConfigFile / `distrib` console | (installer; `distrib` console) |
| Version_US | None; read but never used | ReadConfigFile | (never written) |
| ShowDistribLogo | Publisher splash on or off | ReadConfigFile / `distrib logo` console | (`distrib logo` console) |
| LanguageInstall, Demo, PathInstall | None; installer bookkeeping, never read | (installer only) | (installer) |
| Language, LanguageCD, FlagKeepVoice | Language / voice CD | MESSAGE.CPP, ReadConfigFile / WriteConfigFile | Options → Choose language |

**Note:** `FlagKeepVoice` remains installer / CONFIG-tool managed. `Language` and `LanguageCD` are now also written by the in-game Options menu.

### Community / modernized additions

| Key | Purpose | Source | Menu |
|-----|---------|--------|------|
| MenuMouse | Optional mouse UX in game menus (`FlagMenuMouse` in code). Default 1 (on). Set 0 to match classic keyboard/joystick-only menus. See [MENU.md](MENU.md) | ReadConfigFile / WriteConfigFile | Options → Advanced options |
| FollowCamera | Auto camera for exterior scenes (0=classic, 1=auto). Community addition, not in original game; menu label is "Auto camera" / "Classic camera" | ReadConfigFile / WriteConfigFile | Options → Advanced options |
| TextureFilter, DitherShading | Software-rasterizer smoothing, both off by default. Console cvars `gfx_texfilter` / `gfx_dither` | ReadConfigFile / WriteConfigFile | console only |

## How a key is declared

A key is one row in a table, in the module that owns the setting. The cfg reader, the cfg writer and
the console's cvar table all read that table rather than naming the setting, so adding a key is a
single row and its rule, not merely its range, reaches every surface that can write it: the console hands a typed value to the setting's owner rather than deciding for itself, so a key answers the console and the config file the same way. The row type is `T_SETTING` in
[SOURCES/SETTINGS.H](../SOURCES/SETTINGS.H).

Three tables exist today: `BootSettings` in [SOURCES/CONFIG_FILE.CPP](../SOURCES/CONFIG_FILE.CPP) for the boot and
display keys, `FollowCamSettings` in [SOURCES/FOLLOWCAM.CPP](../SOURCES/FOLLOWCAM.CPP) for the Auto camera's, and
`AudioSettings` in [SOURCES/AMBIANCE.CPP](../SOURCES/AMBIANCE.CPP) for the volumes and the stereo swap.
**Row order is the file's key order**, which players see: the config is rewritten in table order, so
moving a row reorders every existing `lba2.cfg` on the next save.

**A row states what happens to a value outside its range**, because the engine does not do one thing
here, it does four. This is the vocabulary behind the "Clamping / notes" column above:

| Rule | Meaning | Example key |
|------|---------|-------------|
| `SETTING_CLAMP` | move it to the nearer bound | DetailLevel |
| `SETTING_OR_DEFAULT` | out of range counts as unset, so the default stands | FullScreen, TextureFilter |
| `SETTING_TRUTHY` | any non-zero is on | FollowCamHDRecompose |
| `SETTING_RAW` | no range at all; whatever the file said stands | AllCameras, Shadow |

Picking the wrong one is invisible to a normal config: a file the engine wrote holds only in-range
values, so the rules can only differ on a value the engine did not write. They are covered by a host
test in [`tests/settings/`](../tests/settings/) rather than by any read-write comparison.

Two further columns, both optional:

- **`legacy`** names an older spelling to read when the current key is absent, so a renamed key keeps
  working and moves to its new name on the next write. `FollowCamera` reads `AutoCameraCenter` this way.
- **`stored` / `forced`** mark a key some flag can force for one run. The reader keeps what the file
  held, and the writer puts that back rather than the forced value, which is what stops
  `--fixed-timestep` or `--vsync` leaving a one-run choice behind. See the lifecycle note above.

Not everything is a row. Keys that are not a single integer stay hand-written: `LastSave` and
`Language` are strings, the key bindings and volumes have their own blocks
(`ReadInputConfig` / `ReadVolumeSettings`), and `ResolutionX/Y` follow the separate rule described
above. Side effects of a setting, applying fullscreen or swapping stereo, run after the table has
loaded the values rather than inside it.

## Code reference


| Concept            | File                      | Function/Symbol                                            |
| ------------------ | ------------------------- | ---------------------------------------------------------- |
| Config read/write  | [CONFIG_FILE.CPP](../SOURCES/CONFIG_FILE.CPP) | `ReadConfigFile`, `WriteConfigFile`, `BootSettings` |
| Setting declaration | [SETTINGS.H](../SOURCES/SETTINGS.H) | `T_SETTING`, `Settings_Coerce`, `Settings_ValueToPersist` |
| Per-module tables | [FOLLOWCAM.CPP](../SOURCES/FOLLOWCAM.CPP) | `FollowCamSettings`, `FollowCam_ReadConfig`, `FollowCam_WriteConfig` |
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

