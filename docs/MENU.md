# Game Menu

Primary UI for start/load/save and options. Entry points: startup (via [SOURCES/FLOW.CPP](SOURCES/FLOW.CPP) → `MainGameMenu`) and in-game MENUS key ([SOURCES/PERSO.CPP](SOURCES/PERSO.CPP) line 2518). Option changes are persisted in lba2.cfg at exit (see [CONFIG.md](CONFIG.md)).

## Entry and flow

- `MainGameMenu(load)` in [SOURCES/GAMEMENU.CPP](SOURCES/GAMEMENU.CPP) (line 2443)
- First-time vs returning: `IsFirstGameLaunched()` skips to intro; otherwise `BuildGameMainMenu()` builds the visible entries
- Plasma background, `InitPlasmaMenu()`, `FlagShadeMenu`

## Main menu structure

- `RealGameMainMenu[]` defines 6 entries (text IDs 70–75): Resume, New Game, Load, Save, Options, Quit
- `BuildGameMainMenu()` filters entries based on: `CURRENTSAVE_FILENAME` exists (Resume), `IsExistSavedGame()` (Load), `SavingEnable` (Save)
- `DoGameMenu(GameMainMenu)` returns selected text ID; switch handles 70–75 and 1000 (ESC)

## Submenus

- **Options** (`OptionsMenu`): Volume, Reverse Stereo, Detail Level, Cameras, Keyboard Config, Fullscreen, Display Text. Uses `GameOptionMenu[]` (text IDs 11–47). Calls `GereVolumeMenu()`, `MenuConfig()`, toggles globals (`AllCameras`, `ReverseStereo`, `VideoFullScreen`, `FlagDisplayText`). Calls `SetDetailLevel()` on exit.
- **Volume** (`GereVolumeMenu`): `VolumeMenuVoice` / `VolumeMenuNoVoice` depending on `FlagSpeak`. Sliders for Sample, Voice, Music, CD, Master. Persisted via [CONFIG.md](CONFIG.md).
- **Save/Load** (`SavedGameManagement`, `ChoosePlayerName`): Player name selection, save slots, `GameChoiceMenu[]`, `SavedConfirmMenu[]` for delete
- **Keyboard config** (`MenuConfig`): standalone config tool in [SOURCES/CONFIG/](SOURCES/CONFIG/) or in-game `ReadInputConfig`/`WriteInputConfig`

## Menu data format

- `U16 *ptrmenu`: `[selected, nb_entries, y_center, dia_num, (type, text_id)*]`
- `DrawGameMenu()`, `DoGameMenu()` in GAMEMENU.CPP (lines 1591, 1661)
- Text IDs from `text.hqr` / dialogue system (`GetMultiText`)

## Code reference

| Concept           | File         | Function/Symbol                 |
| ----------------- | ------------ | ------------------------------- |
| Main menu entry   | GAMEMENU.CPP | MainGameMenu, BuildGameMainMenu |
| Menu render/input | GAMEMENU.CPP | DoGameMenu, DrawGameMenu        |
| Options submenu   | GAMEMENU.CPP | OptionsMenu, GereVolumeMenu     |

## Cross-references

- [CONFIG.md](CONFIG.md) for options persistence
- [DEBUG.md](DEBUG.md) for bug save/load menu integration
- [CONSOLE.md](CONSOLE.md) for console availability in menu
