# Game menu and settings: plan

How the Options menu gets sorted by category
([Discussion #782](https://github.com/LBALab/lba2-classic-community/discussions/782)), how
presets reach players (#838), how `lba2.cfg` becomes a file players can read, and how the first
slice starts the game-menu refactor in [REFACTOR_ROADMAP.md](REFACTOR_ROADMAP.md) §3 without
closing off a Modern UI that uses the room a larger screen gives. Status: proposed.

It builds on [PRINCIPLES.md](../PRINCIPLES.md) and the rings in
[ARCHITECTURE.md](../ARCHITECTURE.md#the-rings-and-the-core). The words it uses (Classic,
Default, Modern, Custom, Experimental, ring, preset) are defined in
[GLOSSARY.md](../GLOSSARY.md#project-and-settings). The menu and the config file are shell: they
show and set settings, and never change the game's rules.

## Where it stands

- [SOURCES/GAMEMENU.CPP](../../SOURCES/GAMEMENU.CPP) is 5,597 lines, the original file that has
  grown most since the 1997 import (REFACTOR_ROADMAP, "Growth since the 1997 import"). Mouse,
  labels and fixed UI navigation are already in their own files (`GAMEMENU_MOUSE.CPP`,
  `MENU_LABELS.CPP`, `MENU_KEYNAV.CPP`, `MENU_INPUT.CPP`).
- A page is a `U16` array, `[selected, count, y centre, dial, (type, text id)...]`, the format
  both games share ([LBA1_PORT_PLAN.md](LBA1_PORT_PLAN.md) §5). Each page has its own
  `Gere*Menu` loop with a `switch` on the text id.
- Those handlers write the globals directly (`FollowCamera ^= 1` in `GereCameraMenu`). Only the
  Fix retail bugs row goes through `Settings_Apply`, which is why Vsync needs a hand-written
  `ConfigFile_MarkVSyncSetThisRun()` to be saved.
- Advanced options is full at 8 rows, all a page holds at 640x480 (#738). PR #849 made room for
  the interior camera by turning the Auto camera row into a Camera page.
- 14 of the 60 settings-table rows give a value from before (`SETTING_BEFORE`); count with
  `grep -c "SETTING_BEFORE(" SOURCES/*.CPP` less the comment in `RETAILFIX.CPP`. No row says
  which ring it is in or which menu category it belongs to.
- The 1997 Options menu is one flat page of 8 rows (333929a `SOURCES/GAMEMENU.CPP`,
  `GameOptionMenu`): return, volume, reverse stereo, detail level, scenario cameras, keyboard
  config, video size, display text.

## What the principles ask

1. **A preset is values, never a menu** (principle 1). Choosing Classic sets every setting the
   port added to its value from before. A row hidden from view is not a setting switched off:
   whatever reads the setting checks it. One menu for every preset also keeps a config
   reproducible without its menu state (EPmager on #838).
2. **Modern offers choices, Experimental holds the tuning** (principle 7). The Auto camera is one
   or two Modern rows; its tuning values are on the Experimental page. The same for the graphics
   filters.
3. **Promise only what you test** (principle 7). The Modern preset turns on every Modern setting
   at once, so that combination is itself tested, not only each feature alone.
4. **Every row is one more to test** (principle 6). A page opens when it has rows to hold:
   Gameplay opens with its second row, and until then Fix retail bugs stays where it is.
5. **The original stays reachable** (principle 1). The 1997 menu code stays, as the reference and
   for the 1997 Options page later.

## Presets

Three presets set values; Custom is what the row shows when the values match none of them.

| Preset | Sets |
|---|---|
| **Classic** | Every setting the port added to its value from before, and Fix retail bugs to No: the game as the 1997 CD release played. Settings the 1997 game already had (volumes, detail level, language) stay the player's. |
| **Default** | What a fresh install gets. Retail fixes on, port features off unless they have moved on by default. |
| **Modern** | Default, plus every setting in the Modern ring at its Modern value. |

- Applying a preset also puts the Experimental tuning values back to their defaults. Changing any
  setting afterwards, tuning included, makes the row read Custom, as Fix retail bugs already does.
- Custom is computed from the values, never stored. A player who brings an edited `lba2.cfg` sees
  Custom unless the file happens to match a preset, which answers EPmager's question on #838.
- **Show experimental settings** is a separate switch. Off, the Experimental page is not offered;
  its values still apply.
- Reset settings goes to Default, after a confirmation step.

For this the settings table gains two fields: the **ring** (none for a 1997 setting, Modern or
Experimental), from which the Modern preset and the Experimental page are built, and the
**category**, which the menu and the config file both group by. A setting moves inward by
changing its ring field in the PR that brings the evidence
([ARCHITECTURE.md, Moving inward](../ARCHITECTURE.md#moving-inward)).

## Order of work

1. **Row tables, nothing visible changes.** Move the Options pages into `OPTIONS_MENU.CPP`: one
   table per page and one loop for every page, writing only through `Settings_Apply`. `DoGameMenu`
   still draws them. Every `ui menu-*` capture matches main byte for byte.
2. **Categories (#782).** The hub and the category pages below, new labels in six languages,
   goldens remade. PR #835's filter rows land here or in Display first, by agreement with its
   author. A check fails a label that uses a reserved word (GLOSSARY).
3. **Ring and category fields in the settings table**, and the config file grouped by category
   (see below).
4. **Windowed list with a scroll bar (#738)**, for the resolution list first.
5. **Reset settings** to Default, with a confirmation step.
6. **A value from before on every row the port added (#846)**, then the **Classic** preset.
7. **Default and Modern presets**, the preset row, and a fixture with every Modern setting on.
8. **Experimental page generated from the settings table**, behind Show experimental settings.
   Then the camera presets (#733), the console row (#679), settings backups, each in its own PR.
9. **The save list into its own file** (roadmap §3, third slice).
10. **The 1997 Options page**, and the main menu as the per-game part once LBA1 needs it.

Steps 1 and 2 are the first slice. The rest is direction, settled one PR at a time.

## The page model

A page is a table of rows, not a loop. A row says:

| Field | What |
|---|---|
| kind | page (opens another), choice (cycles values), slider (a range), action (with a confirmation step where it cannot be undone), binding |
| label | a `TEXT.HQR` id for a 1997 row, or a `MENU_LABELS` entry |
| setting | the settings-table row it reads and writes, if any |
| shown when | a check: the platform has the feature, the data has speech, Show experimental settings is on |
| help | one line saying what the row does, and its cost if any |

One loop draws and runs any page, writes through `Settings_Apply`, and repaints the backdrop on
entry and exit in one place (today seven page handlers each repeat it). Nothing in the table is a
pixel position, so a second layout can read the same tables.

## Categories

| Page | Rows | Waiting for a place |
|---|---|---|
| Language and interface | Texts, Voices, Display text | On-screen prompts (#371) |
| Display and graphics | Resolution, Fullscreen, Vsync, Detail level, Video size, Image (a page: the filters) | |
| Sound | Samples, Voice, Music, General volume, Reverse stereo | |
| Controls and camera | Keyboard, Gamepad, Camera (a page), Movie camera | Camera presets (#733), modern controls (#372) |
| Gameplay | opens with its second row | play-changing enhancements |
| Advanced and reset | Settings (Classic, Default, Modern, Custom), Reset settings, Show experimental settings, Fix retail bugs (until Gameplay opens), Console (#679), Experimental (a page) | settings backups, cheats |

The hub has the return row and six categories: 7 rows. Every page stays within 8 rows at
640x480; Advanced and reset reaches 7 with the return row. A row that cannot apply here (no
speech in the data, an always-fullscreen device) is hidden, as today. Whether a row that depends
on another setting is hidden or greyed out waits for the first such row. Where the preset choice
is first offered (first launch, as EPmager suggests on #838) is open.

## The config file

`lba2.cfg` should be as easy to reason about as the menu: the same groups, in the same order,
with the same names.

### What a fresh file looks like today

Measured 2026-11-03 with a macOS build of main from 2026-11-02 on GOG Classic data, by reading
the `lba2.cfg` that `lba2cc --headless --no-autosave --game-dir <data> --user-dir <empty folder>
--fixed-dt 16 --tick 30 --exit` writes:

- 212 keys. 29 comment lines, all from the 1997 seed ([SOURCES/LBA2.CFG](../../SOURCES/LBA2.CFG)),
  covering only its first keys; nothing the port added is explained.
- Keys come in the seed's order, then in the order each writer appends. The gamepad bindings are
  split in two runs (`Gamepad2` to `Gamepad35`, then the stick-camera settings, then `Gamepad0`,
  `Gamepad0_2` and the rest), and the Auto camera's 22 tuning keys sit between the mouse keys and
  the retail fixes.
- Bindings are raw scancodes (`Gamepad0: 1048`), which #61 tracks.
- Every value is written, defaults included, so a player cannot tell what they changed.
- Two keys read as each other: `FullScreen` is the video size and `DisplayFullScreen` the window.

Writing every default has a cost beyond reading: a value in the player's file is the player's.
When a setting later moves on by default, a player whose file holds the old default keeps it,
though they never chose it. Moving inward cannot reach existing players.

### Proposal

1. **The settings table is the template.** No hand-kept template file: the writer emits keys
   grouped by the category field, in menu order, each under a one-line comment generated from
   the row (help, range, default). Experimental keys come last under their own heading; game
   state that is not a preference (`LastSave`, the installer's `Version`) after that.
2. **The file holds what the player chose.** A key at its default is left out unless the
   player's own file named it (the provenance from #574 tells the two apart). Left out, it
   follows the default, including a later one.
3. **A reference file beside it**, every key grouped and commented at its default, regenerated
   when the build changes and never read. Players copy lines from it.
4. **Bring your own config.** A file is a layer in the chain
   [PORTABILITY_PLAN.md](PORTABILITY_PLAN.md) already reads (defaults, game-data config, the
   profile's config, flags), so a partial file sets only its keys. Settings backups (EPmager,
   Discussion #782) save and load such files from the menu. The preset row shows what the result
   matches.
5. **Readable names** for bindings (#61), read alongside the scancodes.

Point 2 changes what a settings row means to an existing file, which is a core contract
(principle 2), so it starts from a Decision. The migration it needs: the first write in the new
form keeps a copy of the old file and drops keys equal to their default.

## A north star for the Modern UI

Not a promise; the direction the steps above should not close off.

1. **Pages are data.** As in the page model above.
2. **The UI scales in whole steps.** `scale = max(1, floor(min(W / 640, H / 480)))`, and pages
   lay out on a logical canvas of the framebuffer divided by the scale. 1920x1080 is 2x on a
   960x540 canvas; 640x480 is 1x and looks as today. Needs a scaled `Font()` (it blits 1:1
   today) and scaled plasma; sprites already have `ScaleSprite`.
3. **The same page lays out by width.** Below 840 logical pixels, one column as today. From 840,
   categories on the left, rows on the right with their values, a help line for the selected row
   and a prompt bar for the device in use.
4. **One focus for every input.** Keyboard, pad, mouse and touch move the same cursor, as the
   fixed UI navigation already does.
5. **The original parts.** Palette colours, one frame, the game font at one size, plasma for the
   cursor, darkening instead of blending.
6. **Last: the menu as a state of the main loop**, not a loop of its own: one clock, the live
   scene behind it. This touches the clock and recording (the outer core), so it starts from a
   Decision. See [ENGINE_RENDER_SPLIT_RESEARCH.md](ENGINE_RENDER_SPLIT_RESEARCH.md) and
   [ENGINE_TICK_POLICY_SURVEY.md](ENGINE_TICK_POLICY_SURVEY.md) for why the modal loops own the
   clock today.

A new renderer for the Options pages first reproduces today's 640x480 layout, held to the
existing goldens byte for byte. Only then does it get wide layouts.

### Long lists

A plain page does not scroll. A long list of one kind (resolutions, the Experimental page) is a
windowed list as the save list is, with a scroll bar beside the window: a framed track, a framed
thumb sized to the window's share of the list, Page Up and Page Down or the shoulder buttons for
a window at a time, the wheel and a drag for the pointer.

The Experimental page is generated from the settings table: every row in the Experimental ring,
grouped by category, labelled with its console name and help text, in English. An experiment
then needs no hand-made row and no translations. Two layouts are mocked: the game font in the
wide layout, and a dense table in the 8x8 font like the key binding screen.

### Controls

Today keyboard and gamepad are two binding screens, each one 36-row table in the 8x8 font
(`DrawScreenConfig` in [SOURCES/CONFIG.CPP](../../SOURCES/CONFIG.CPP)). The Modern page splits
the 36 actions into seven groups of four to seven (movement, actions, behaviours, weapons, magic
and items, camera, menus), one pane each, with Key 1, Key 2 and Gamepad side by side as key
chips. A key bound to two actions is framed red. The 1997 screen stays.

## Testing against the original

The menu does not touch the game's rules, so the world hash cannot see it. Four checks do:

1. **A refactor changes nothing.** Every menu page is captured before and after and must match
   main byte for byte. First change one row by hand and see a golden fail. These fixtures need
   game data and do not run in CI: each PR runs them on US CD and GOG data and says so.
2. **The 1997 code stays 1997.** `DoGameMenu`, `DrawGameMenu`, `DrawOneChoice` and the plasma
   stay put; a diff against 333929a shows only the port's known edits, listed in a comment where
   they sit.
3. **Classic is the original.** Choosing Classic sets every row's value from before:
   [test_ring_off_is_gone.sh](../../tests/automation/test_ring_off_is_gone.sh) with them all off,
   a host test that the preset touches every row that has one, and
   `scripts/ci/check-settings-before.py` extended so a row that switches a behaviour on must say
   its value from before.
4. **The 1997 page.** Its table against 333929a's `GameOptionMenu` (8 rows, known text ids), and
   captures beside GOG DOS `LBA2.EXE` under DOSBox-X. A visual check, not pixel-exact.

The page tables and the config layout are plain data, so host tests can hold them without game
data: at most 8 rows a plain page, every label in six languages, every row naming a setting that
exists, every setting with a category, a written file that reads back to the same values.

## Open questions

1. Category names, and whether the wide layout's left pane uses shorter ones.
2. Image: one Modern choice for the three filters, or three rows (#835)?
3. The scroll bar beside the rows (they move 14 pixels off-centre at 640x480) or inside the
   row's right end.
4. Experimental: game-font list or dense table.
5. A UI scale row (Auto, 1x, 2x, 3x) for small screens at high resolutions.
6. Where the preset choice is first offered: first launch, Options, or both.
7. The reference file's name, and whether it is written every run or only when the build changes.
8. Who checks the translations of the new labels.

## Not in this plan

The inventory, holomap, dialogue and HUD. They share the drawing parts and would follow the
same scale rule, but each is its own piece of work.
