# Game Menu

Primary UI for start/load/save and options. Entry points: startup (via [SOURCES/FLOW.CPP](SOURCES/FLOW.CPP) → `MainGameMenu`) and in-game MENUS key ([SOURCES/PERSO.CPP](SOURCES/PERSO.CPP) line 2518). Option changes are persisted in lba2.cfg at exit (see [CONFIG.md](CONFIG.md)).

## Menu tree

Hierarchy as displayed in-game. Text IDs in parentheses; `[if …]` = conditional visibility.

```
Main Menu
├── Resume (70)              [if CURRENTSAVE exists]
├── New Game (71)
├── Load (72)                 [if saves exist]
│   └── ChoosePlayerName → save slots (text 0–6)
├── Save (73)                 [if in game, SavingEnable]
│   └── ChoosePlayerName → save slots (text 0–6)
│       └── SavedConfirmMenu: Cancel (24) / Delete (48)
├── Options (74)
│   ├── Back (26)
│   ├── Volume (11)
│   │   ├── Back (26)
│   │   ├── Samples (19)       [slider]
│   │   ├── Voice (20)         [slider, if FlagSpeak]
│   │   ├── Music (21)         [slider]
│   │   ├── CD (22)            [slider]
│   │   └── Master (23)        [slider]
│   ├── Reverse Stereo (12/13)
│   ├── Detail Level (45)     [slider 0–3]
│   ├── Cameras (46/47)
│   ├── Keyboard Config (14)
│   ├── Fullscreen (27/28)
│   └── Display Text (16/17)
└── Quit (75)
```

`DoGameMenu` returns the selected text ID, or `1000` for ESC.

## Entry and flow

- `MainGameMenu(load)` in [SOURCES/GAMEMENU.CPP](SOURCES/GAMEMENU.CPP) (line 2443)
- First-time vs returning: `IsFirstGameLaunched()` skips to intro; otherwise `BuildGameMainMenu()` builds the visible entries
- Plasma background, `InitPlasmaMenu()`, `FlagShadeMenu`

## Main menu structure

The main menu uses a **template → build → drive** flow:

| Component | Role |
|-----------|------|
| **RealGameMainMenu** | Static template array with all 6 entries (70–75). Never passed to `DoGameMenu` directly. |
| **BuildGameMainMenu** | Copies from template into `GameMainMenu`, filtering by runtime state. Resume only if `CURRENTSAVE` exists; Load only if `IsExistSavedGame()`; Save only if `!firstloop` and `SavingEnable`. Always includes New Game, Options, Quit. Updates `nb`, `ycenter`, and default selection. |
| **DoGameMenu** | Generic driver for any menu array. Runs input loop (Up/Down, Fire/Action), handles sliders (type 2–7), draws via `DrawGameMenu`, returns selected text ID or `1000` (ESC). Used for main menu, Options, Volume, etc. |

Flow: `RealGameMainMenu` (template) → `BuildGameMainMenu(firstloop)` → `GameMainMenu` (filtered) → `DoGameMenu(GameMainMenu)` → returns 70–75 or 1000.

## Submenus

- **Options** (`OptionsMenu`): Volume, Reverse Stereo, Detail Level, Cameras, Keyboard Config, Fullscreen, Display Text. Uses `GameOptionMenu[]` (text IDs 11–47). Calls `GereVolumeMenu()`, `MenuConfig()`, toggles globals (`AllCameras`, `ReverseStereo`, `VideoFullScreen`, `FlagDisplayText`). Calls `SetDetailLevel()` on exit.
- **Volume** (`GereVolumeMenu`): `VolumeMenuVoice` / `VolumeMenuNoVoice` depending on `FlagSpeak`. Sliders for Sample, Voice, Music, CD, Master. Persisted via [CONFIG.md](CONFIG.md).
- **Save/Load** (`SavedGameManagement`, `ChoosePlayerName`): Player name selection, save slots, `GameChoiceMenu[]`, `SavedConfirmMenu[]` for delete
- **Keyboard config** (`MenuConfig`): standalone config tool in [SOURCES/CONFIG/](SOURCES/CONFIG/) or in-game `ReadInputConfig`/`WriteInputConfig`

## Menu data format

- `U16 *ptrmenu`: `[selected, nb_entries, y_center, dia_num, (type, text_id)*]`
- `DrawGameMenu()`, `DoGameMenu()` in GAMEMENU.CPP (lines 1591, 1661)
- Text IDs from `text.hqr` / dialogue system (`GetMultiText`)
- **Type** in each entry: 0 = plain text, 2–6 = volume sliders (Sample/Voice/Music/CD/Master), 7 = Detail Level slider

## Implementation notes

- **Plasma background**: `InitPlasmaMenu()` (ANIMTEX.CPP) sets up animated plasma texture; `SelectPlasmaBank()` switches palette (12 = selection highlight, 4 = slider bar). `DrawFireBar` / `DrawFire` render the fire/plasma effect.
- **Main menu live preview**: While on main menu, `DrawGameMenu` triggers a save screenshot load and draws it at (240, 10) so the player sees their current game state.
- **DEMO build**: Save/Load (72, 73) are disabled (greyed out, skipped by Up/Down). After ~50 min idle (or Shift+D), `SlideShow()` runs; returns `9999` to trigger credits.
- **Volume sliders**: Type 2–6 map to globals; left/right adjust with `VOLUME_TIMER_KEY` (5 ticks) debounce. Sample and Master sliders play random SFX preview when focused.
- **DetailLevel → Shadow**: `SetDetailLevel()` (GAMEMENU.CPP line 210) derives Shadow, RainEnable, MaxPolySea, FlagDrawHorizon from DetailLevel. Shadow in config is read at startup but overwritten when leaving Options.
- **Options entry count**: `GameOptionMenu[1]` is 7 or 8 depending on `FlagSpeak` (Display Text hidden when no voice support).

## Limitations

| Limit | Value | Notes |
|-------|-------|-------|
| Main menu entries | 6 | `MAX_MAIN_MENUS`, `GameMainMenu[4+6*2]` |
| Save slots visible | 5 | `NB_GAME_CHOICE`, `Y_START_CHOICE`; list scrolls |
| Player name length | 100 chars | `MAX_SIZE_PLAYER_NAME` |
| Resolution | 640×480 | Menu centered at x=320, `LargeurMenu=550` |
| Long text | No wrap | Text wider than 550px scrolls horizontally |
| Input | Keyboard/joystick | No mouse; Up/Down/Fire/Action |

**Note:** `IsExistSavedGame()` uses a hardcoded 100000L buffer; the save list elsewhere uses `MAX_PLAYER` derived from `BufSpeak` size.

## Future enhancements

Ideas that stay in the spirit of the original design:

- **More visible save slots** – 5 is arbitrary; 7–8 with tighter spacing (`NB_GAME_CHOICE`, `Y_START_CHOICE`) keeps the same UX.
- **Config saved when leaving Options** – Currently written only at exit; saving on Options back would avoid loss on crash.
- **Mouse support** – Click-to-select alongside keyboard/joystick; common in modern ports.
- **Additional Options entries** – Structure supports it; extend `GameOptionMenu` and add handlers.
- **Resolution scaling** – Scale layout for higher res while preserving the plasma/font look.

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
