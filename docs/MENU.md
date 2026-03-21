# Game Menu

Primary UI for start/load/save and options. Entry point: main() in [SOURCES/PERSO.CPP](SOURCES/PERSO.CPP) calls `MainGameMenu(load)` at line 2398. In-game, Options/Save/Load keys open submenus; ESC/MENUS opens pause. Option changes are persisted in lba2.cfg at exit (see [CONFIG.md](CONFIG.md)).

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
│   ├── Sound volume (11)
│   │   ├── Back (26)
│   │   ├── Samples (19)       [slider]
│   │   ├── Voice (20)         [slider, if FlagSpeak]
│   │   ├── Music (21)         [slider]
│   │   └── General volume (23) [slider]
│   ├── Choose language [custom localized label]
│   │   ├── Back (26)
│   │   ├── Texts: <language> [custom localized label]
│   │   └── Voices: <language> [custom localized label, CDROM builds]
│   ├── Advanced options [custom localized label]
│   │   ├── Back (26)
│   │   ├── Reverse stereo channels OFF/ON (12/13)
│   │   ├── Detail Level (45)             [slider 0–3]
│   │   ├── Movie camera OFF/ON (46/47)
│   │   ├── Small videos / Full screen videos (27/28)
│   │   ├── Fullscreen display OFF/ON     [custom localized label]
│   │   └── Don't display text / Display text (16/17)
│   ├── Keyboard Config (14)
└── Quit (75)
```

`DoGameMenu` returns the selected text ID, or `1000` for ESC.

## Terms

| Term | Definition |
|------|------------|
| CURRENTSAVE | Quick-save file (current.lba); Resume is shown when it exists |
| SavingEnable | Flag allowing Save; false during intro |
| FlagSpeak | Voice support; controls Voice sample preview and whether Display Text can be turned OFF |
| MainGameMenu | Entry point for the main menu |

## Entry and flow

- `MainGameMenu(load)` in [SOURCES/GAMEMENU.CPP](SOURCES/GAMEMENU.CPP) (line 2651)
- First-time vs returning: `IsFirstGameLaunched()` skips to intro; otherwise `BuildGameMainMenu()` builds the visible entries
- Plasma background, `InitPlasmaMenu()`, `FlagShadeMenu`

## Main menu structure

The main menu uses a **template → build → drive** flow:

| Component | Role |
|-----------|------|
| **RealGameMainMenu** | Static template array with all 6 entries (70–75). Never passed to `DoGameMenu` directly. |
| **BuildGameMainMenu** | Copies from template into `GameMainMenu`, filtering by runtime state. Resume only if `CURRENTSAVE` exists; Load only if `IsExistSavedGame()`; Save only if `!firstloop` and `SavingEnable`. Always includes New Game, Options, Quit. Updates `nb`, `ycenter` (275 without Resume, 335 when Resume is present), and default selection. |
| **DoGameMenu** | Generic driver for any menu array. Runs input loop (Up/Down, Fire/Action), handles sliders (type 2–7), language cycling (types 8–9), draws via `DrawGameMenu`, returns selected text ID or `1000` (ESC). Used for main menu, Options, Volume, etc. |

Flow: `RealGameMainMenu` (template) → `BuildGameMainMenu(firstloop)` → `GameMainMenu` (filtered) → `DoGameMenu(GameMainMenu)` → returns 70–75 or 1000.

## Submenus

- **Options** (`OptionsMenu`): top-level submenu for Sound volume, Choose language, Advanced options, and Keyboard Config. Reuses original `text.hqr` labels where they already exist and keeps custom localized labels only for the new submenu concepts. Calls `GereVolumeMenu()`, `GereLanguageMenu()`, `GereAdvancedOptionsMenu()`, and `MenuConfig()`. Calls `SetDetailLevel()` on exit.
- **Volume** (`GereVolumeMenu`): `VolumeMenuVoice` / `VolumeMenuNoVoice` depending on `FlagSpeak`. Sliders for Sample, Voice, Music, and General volume. Persisted via [CONFIG.md](CONFIG.md).
- **Language** (`GereLanguageMenu`): cycles text language and voice language independently. Text changes reload `text.hqr` immediately; voice changes update the active `LanguageCD` setting.
- **Advanced options** (`GereAdvancedOptionsMenu`): toggles stereo handling, movie cameras, video playback size, window fullscreen mode, and subtitle display; includes the existing Detail Level slider.
- **Save/Load** (`SavedGameManagement`, `ChoosePlayerName`): Player name selection, save slots, `GameChoiceMenu[]`, `SavedConfirmMenu[]` for delete
- **Keyboard config** (`MenuConfig`): standalone config tool in [SOURCES/CONFIG/](SOURCES/CONFIG/) or in-game `ReadInputConfig`/`WriteInputConfig`

## Menu data format

- `U16 *ptrmenu`: `[selected, nb_entries, y_center, dia_num, (type, text_id)*]`
- `DrawGameMenu()`, `DoGameMenu()` in GAMEMENU.CPP (lines 1760, 1823)
- Text IDs from `text.hqr` / dialogue system (`GetMultiText`), plus **synthetic IDs** (e.g. `MENU_ID_*` in the 2100 range) resolved in `BuildCustomMenuText()`
- **Type** in each entry: 0 = plain text/toggle, 2–6 = volume sliders, 7 = Detail Level slider, 8 = text language cycle, 9 = voice language cycle

## Menu layout

Layout uses **logical coordinates** in the game framebuffer. Default size is **640×480** (`RESOLUTION_X` / `RESOLUTION_Y` in [SOURCES/INITADEL.C](SOURCES/INITADEL.C)); `ModeDesiredX` / `ModeDesiredY` ([LIB386/SVGA/SCREEN.CPP](LIB386/SVGA/SCREEN.CPP)) follow the allocated buffer. Menu code still uses many **fixed 320 / 639 / 479** values for centering and full-screen effects; `BoxStaticAdd` often uses `ModeDesiredX - 1` / `ModeDesiredY - 1` so dirty rectangles match the buffer.

| Constant | Value | Role |
|----------|-------|------|
| `LargeurMenu` | 550 | Half-width from center for selection bars and long-text clip/scroll |
| `HAUTEUR_STANDARD` | 50 | Height of one menu row |
| `MENU_SPACE` | 6 | Vertical gap between rows (`DrawGameMenu`) |
| Main menu `y` center (template) | 335 | `RealGameMainMenu[2]`; `BuildGameMainMenu` sets **275** when there is no Resume row, **335** when `CURRENTSAVE` exists |
| Options menu `y` center (template) | 260 | `GameOptionMenu[2]` |
| Horizontal center | `x = 320` | `DrawOneChoice` / `DrawGameMenu` (half of 640-wide layout) |

Save/load slot list: `NB_GAME_CHOICE` (5 visible rows), `Y_START_CHOICE` (205), **60** px between rows (`ChoosePlayerName`).

## Languages and localization

The **main menu** still uses only the six template actions (70–75). **Options (74)** adds in-game **Choose language** and **Advanced options** (see menu tree): top-level `GameOptionMenu` has **5** rows (Sound volume, Choose language, Advanced options, Keyboard Config, Back).

| Mechanism | Where | Notes |
|-----------|--------|--------|
| **`BuildCustomMenuText`** | [SOURCES/GAMEMENU.CPP](SOURCES/GAMEMENU.CPP) (`DrawOneChoice`) | Rows use either **`GetMultiText`** (classic dialogue IDs) or **synthetic menu IDs** (e.g. `MENU_ID_CHOOSE_LANGUAGE`); new headings can use **in-code UTF-8 strings** converted to CP850 for the font. |
| **Language submenu** (`GereLanguageMenu` / `LanguageMenu`) | Types **8** / **9** in `DoGameMenu` | **8**: cycle **UI text language** (`Language`, left/right); **`ReloadMultiTextFile`** applies immediately. **9** (CDROM): cycle **voice language** (`LanguageCD`). |
| **`Language` / `LanguageCD` in lba2.cfg** | `InitLanguage()` / `WriteConfigFile()` | Read at startup; **written at process exit** with other settings (see [CONFIG.md](CONFIG.md)). Not flushed when leaving Options mid-session. |
| **Long translated labels** | `DrawOneChoice` | If `SizeFont(string) > LargeurMenu`, the **selected** row scrolls the label horizontally inside the clip; unselected rows align left inside the bar. |
| **Voice preview (Volume menu)** | `DrawOneChoice`, type 3 | Uses `SAMPLE_VOICE_MENU + LanguageCD` so the voice test matches **CD/voice language**. |

For voice lines and packaged assets, see [GLOSSARY.md](GLOSSARY.md) and [CONFIG.md](CONFIG.md) (`Language`, `LanguageCD`).

## Implementation notes

- **Plasma background**: A hallmark of LBA1 and LBA2 menus. `InitPlasmaMenu()` (ANIMTEX.CPP) sets up an animated plasma texture; `DrawFireBar` / `DrawFire` render textured bars (selection highlight, sliders, input fields). The effect is plasma (not the separate fire algorithm); `SelectPlasmaBank(12)` for selection, `SelectPlasmaBank(4)` for sliders.
- **Save screenshot**: Generated at save time in `SaveGame()` (SAVEGAME.CPP); shown via `LoadGameScreen()` and `DrawScreenSave()`. See [SAVEGAME.md](SAVEGAME.md) for the full flow.
- **DEMO build**: Save/Load (72, 73) are disabled (greyed out, skipped by Up/Down). After ~50 min idle (or Shift+D), `SlideShow()` runs; returns `9999` to trigger credits.
- **Volume sliders**: Type 2–6 map to globals; left/right adjust with `VOLUME_TIMER_KEY` (5 ticks) debounce. Sample and General volume sliders play random SFX preview when focused.
- **DetailLevel → Shadow**: `SetDetailLevel()` (GAMEMENU.CPP line 458) derives Shadow, RainEnable, MaxPolySea, FlagDrawHorizon from DetailLevel. Shadow in config is read at startup but overwritten when leaving Options.
- **Localized custom labels**: The reworked Options tree reuses original `text.hqr` labels for Sound volume, General volume, stereo, camera, video size, and subtitle toggles. Only the new submenu headings and the display-fullscreen toggle still use in-code translations.
- **Options entry count**: Top-level `GameOptionMenu[1]` is **5** in `OptionsMenu()`. Nested volume/language/advanced menus set their own entry counts per `DEMO` / `FlagSpeak` / CDROM.

## Limitations

| Limit | Value | Notes |
|-------|-------|-------|
| Main menu entries | 6 | `MAX_MAIN_MENUS`, `GameMainMenu[4+6*2]` |
| Save slots visible | 5 | `NB_GAME_CHOICE`, `Y_START_CHOICE`; list scrolls |
| Player name length | 100 chars | `MAX_SIZE_PLAYER_NAME` |
| Resolution | 640×480 (default) | See **Menu layout**; window may scale; internal layout as above |
| Long text | No wrap | Wider than `LargeurMenu` (550px): see **Languages and localization** |
| Input | Keyboard/joystick | No mouse; Up/Down/Fire/Action |

**Note:** `IsExistSavedGame()` uses a hardcoded 100000L buffer; the save list elsewhere uses `MAX_PLAYER` derived from `BufSpeak` size.

## Future enhancements

Ideas that stay in the spirit of the original design:

- **More visible save slots** – 5 is arbitrary; 7–8 with tighter spacing (`NB_GAME_CHOICE`, `Y_START_CHOICE`) keeps the same UX.
- **Config saved when leaving Options** – Currently written only at exit; saving on Options back would avoid loss on crash.
- **Mouse support** – Click-to-select alongside keyboard/joystick; common in modern ports.
- **Additional Options entries** – Structure supports it; extend `GameOptionMenu` and add handlers.
- **Resolution scaling** – Scale layout for higher res while preserving the plasma/font look.
- **Open-source / community distributor logo** – A logo for community builds (engine branding, not game rebranding). Add a `DistribVersion` value for community builds; show the logo only when that value is set. The logo asset must be created and licensed as open-source (e.g. CC0, MIT) and live in the repo. Does not replace or obscure the original game identity; analogous to ScummVM/OpenMW splash screens. Game assets (GOG/Steam, etc.) remain under their original licenses.

## Historical notes

The menu code preserves 1997-era developer artifacts worth noting for preservation:

- **Detail Level hardware comments** – `SetDetailLevel()` (GAMEMENU.CPP lines 462–490) labels each quality tier with informal French: *machine bof* (crap machine), *486 ?*, *Pentium de base ?*, *machine de folie* (crazy machine). These reflect the target hardware of the day. See [FRENCH_COMMENTS.md](FRENCH_COMMENTS.md).
- **Distributor logos** – `DistribLogo()` and `AdelineLogo()` show different publisher logos (Activision, EA, Virgin) based on `DistribVersion`, reflecting regional publishers in 1997.
- **Isabelle** – `MAX_SIZE_PLAYER_NAME` in DEFINES.H is attributed: *Calculé avec des ',' par Isabelle* (calculated with commas by Isabelle).

For broader preservation (ASCII art, French comments, developer culture), see [FRENCH_COMMENTS.md](FRENCH_COMMENTS.md) and [ASCII_ART.md](ASCII_ART.md).

## Code reference

| Concept           | File         | Function/Symbol                                                       |
| ----------------- | ------------ | --------------------------------------------------------------------- |
| Main menu entry   | GAMEMENU.CPP | MainGameMenu, BuildGameMainMenu                                       |
| Menu render/input | GAMEMENU.CPP | DoGameMenu, DrawGameMenu                                               |
| Options submenu   | GAMEMENU.CPP | OptionsMenu, GereVolumeMenu, GereLanguageMenu, GereAdvancedOptionsMenu |
| Save/Load list    | GAMEMENU.CPP, SAVEGAME.CPP | PlayerGameList, ChoosePlayerName, DeleteSavedGame, LoadGameScreen, DrawScreenSave |
| Menu label text   | GAMEMENU.CPP | `BuildCustomMenuText`, `GetMultiText`                                 |
| UI language / text | MESSAGE.CPP | InitLanguage, GetMultiText, `Language`, `LanguageCD`                  |

## Cross-references

- [CONFIG.md](CONFIG.md) for options persistence and `Language` / `LanguageCD`
- [SAVEGAME.md](SAVEGAME.md) for save/load format and lifecycle
- [DEBUG.md](DEBUG.md) for bug save/load menu integration
- [CONSOLE.md](CONSOLE.md) for console availability in menu
