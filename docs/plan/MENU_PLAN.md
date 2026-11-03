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
- Five rows have no settings-table row to go through. Texts, Voices and Display text set
  `Language`, `LanguageCD` and `FlagDisplayText`, which `lba2.cfg` spells as words where a row
  holds a number; Resolution and the two binding screens have writers of their own. Of the 212
  keys in a fresh `lba2.cfg`, 60 come from the six settings tables and 144 are bindings.
- `ui menu-*` captures the Options, Advanced and Camera pages. Volume, Language, Display,
  Resolution and Controller have no capture.
- Advanced options is full at 8 rows, all a page holds at 640x480 (#738). PR #849 made room for
  the interior camera by turning the Auto camera row into a Camera page.
- 14 of the 60 settings-table rows give a value from before (`SETTING_BEFORE`); count with
  `grep -c "SETTING_BEFORE(" SOURCES/*.CPP` less the comment in `RETAILFIX.CPP`. No row says
  which ring it is in or which menu category it belongs to.
- `lba2.cfg` is written whole at exit and when the resolution changes; the console's `vsync`,
  `fixedtimestep` and `distrib` write their own keys. A crash, or a phone or handheld that kills
  the app, loses every other change made in that session.
- A fresh install never shows the menu first: with no save,
  [SOURCES/GAMEMENU.CPP](../../SOURCES/GAMEMENU.CPP) (`IsFirstGameLaunched`) goes straight into
  the intro and a new game, so a new player plays on the defaults before seeing Options.
- The 1997 Options menu is one flat page of 8 rows (333929a `SOURCES/GAMEMENU.CPP`,
  `GameOptionMenu`): return, volume, reverse stereo, detail level, scenario cameras, keyboard
  config, video size, display text.

## What the principles ask

1. **A preset is values, never a menu** (principle 1). Choosing Classic sets every setting a
   preset sets to its value from before. A row hidden from view is not a setting switched off:
   whatever reads the setting checks it. One menu for every preset also keeps a config
   reproducible without its menu state (EPmager on #838).
2. **Modern offers choices, Experimental holds the tuning** (principle 7). The Auto camera is one
   or two Modern rows; its tuning values are on the Experimental page. The same for the graphics
   filters.
3. **Promise only what you test** (principle 7). The Modern preset turns on every Modern setting
   it sets at once, so that combination is itself tested, not only each feature alone.
4. **Every row is one more to test** (principle 6). A page opens when it has rows to hold:
   Gameplay opens with its second row, and until then Fix retail bugs stays where it is.
5. **The original stays reachable** (principle 1). The 1997 menu code stays, as the reference and
   for the 1997 Options page later.

## Presets

Three presets set values, and the one the player chose is stored in `lba2.cfg` as the base their
own values sit on. Custom is what the preset row shows when the player's own values differ from
that base: computed, never chosen.

| Preset | Sets |
|---|---|
| **Classic** | Default, with every switch the port added at its value from before and Fix retail bugs on No: the 1997 CD release's rules. |
| **Default** | What a fresh install gets. Retail fixes on, port features off unless they have moved on by default. |
| **Modern** | Default, plus every Modern-ring switch at its Modern value. |

Today that is a short list: Classic differs from Default in the retail fixes, and Modern from
Default in the outdoor Auto camera. The indoor camera is a candidate for Modern once it passes
the two ring fixtures that leave it out today (`test_ring_off_is_gone.sh` and
`test_ring_watch_not_play.sh`): it is a different camera from the outdoor one, and it changes
what a save carries. The preset earns its place on first launch, where it is the one choice a new
player makes, and grows as Modern settings arrive.

A preset sets switches, never tuning values: every preset leaves tuning at its default. A
camera tuning row's value from before is the Auto camera before that behaviour, the start the
camera fixtures measure from
([Decision](../knowledge/decisions/a-camera-setting-names-its-value-from-before.md)), not
anything the 1997 game did. Seven of them differ from their default (`cam_ease_ms` is 300 by
default and 0 before, `cam_shot_blend_ms` 600 and 0, among others), so a Classic that applied them would give a
player who then turns the Auto camera on its earliest version, with cuts and no easing.

Classic is the original's rules on the player's screen. The lightning spell and some life
scripts read which actors were drawn, so a larger screen can change who the spell strikes
([quirk](../knowledge/quirks/the-game-counts-what-is-on-screen.md)); the 1997 game as it played
is Classic at 640x480.

### What a preset leaves alone

A preset sets how the game plays, not the player's screen, hands or ears. These are the player's
settings, and no preset changes them:

- every setting the 1997 game already had: volumes, reverse stereo, detail level, languages,
  display text, movie camera, video size;
- the screen: resolution, fullscreen, Vsync, the filters (#835) and the UI scale;
- the fixed timestep (`FixedTimestep`), which keeps movement the same at any frame rate: its value
  from before, 0, runs the simulation once per frame, so on a 144 Hz screen Classic would play
  fast;
- how the mouse and pad feel: sensitivity, invert, deadzone, mouse confine, menu mouse;
- the keyboard and gamepad bindings.

Without this, Classic would set the mouse confine to its value from before, Modern would turn on
texture filtering for a player who chose Modern for the Auto camera, and both would read Custom
a minute later.

### The chosen preset is the base

`lba2.cfg` holds the chosen preset (`Preset: Modern`) and, for a setting a preset sets, a value
only where the player's own differs from it. At read time the chosen preset's value takes the
place of the row's default: for a switch, Classic gives the value from before, Default the
compiled default, Modern the Modern value. The chain [PORTABILITY_PLAN.md](PORTABILITY_PLAN.md) reads becomes
compiled defaults, game-data config, chosen preset, the profile's own values, flags. The preset
covers only the settings presets set, so an install's config still sets the player's settings.

Storing the preset is what keeps each one true when a default moves or a setting moves inward:

- A Classic player's file names no value equal to Classic's. When the Auto camera one day moves
  on by default, they keep Classic's value, because their base is Classic, not the default.
- A Modern player gets a setting that joins the Modern ring later without choosing Modern again,
  and the row still reads Modern.
- A Default player follows the defaults as they move, which is what
  [The config file](#the-config-file) wants for every player.

Computed from the values alone, each preset fails one of these. A file that leaves out values
equal to the default takes Classic players along when a default moves; a file that writes every
value keeps Default and Modern players where they were.

The cost is that the same file gives different values in different builds when a preset's values
change, which EPmager asked to avoid on #838. A recording carries every value, so a run stays
reproducible, and on Classic every build gives the original's rules, which is what a runner wants
checked. A player who wants a setup frozen across builds names its values. The base needs the
writer to leave out a value equal to it, so it is one Decision with point 2 of
[The config file](#the-config-file).

### Choosing a preset

- The preset row opens a page: Classic, Default and Modern, each with a help line saying what it
  sets, and **Back to my settings** when there is something to go back to. The row itself shows
  the chosen preset, or Custom when the player's own values differ from it ("Settings: Custom").
- Choosing a preset asks for confirmation, then keeps the preset it replaces and the player's
  values as a settings backup, which Back to my settings restores: the preset, the player's values
  for the settings a preset sets, and the tuning values, not the whole file, so a volume or a
  resolution changed since stays. The backup is taken after the values in memory are written,
  since `lba2.cfg` is otherwise written only when the player leaves Options. A preset is never
  applied by cycling a row: one press past Custom would overwrite a player's setup with no way
  back.
- Applying a preset takes out the player's own values for every setting a preset sets and puts
  the Experimental tuning values back to their defaults (the backup keeps both). Changing a
  setting a preset sets, tuning included, makes the row read Custom; changing a player's setting
  does not.
- Custom is computed: the chosen preset is stored, Custom is not, and no player chooses it. A file
  with no `Preset` key is on Default, so a player who brings an edited `lba2.cfg` sees Custom where
  the file sets a value Default does not, which answers EPmager's question on #838. While the row
  reads Custom, the page's help line names the settings that differ from the chosen preset,
  Experimental ones included, so a value set from the console or a downloaded file can be found.
- The Fix retail bugs row stays as it is, and works differently: its Custom is a stored value the
  player cycles to, and each fix keeps its own setting under Yes and No
  ([SOURCES/RETAILFIX.H](../../SOURCES/RETAILFIX.H), `RetailFix_NextMenuValue`), so cycling it
  loses nothing. A preset sets this row and never the fixes under it, which keeps the promise
  that Yes and No never overwrite them; comparing values with a preset leaves the fixes out too,
  since under Yes or No they change nothing.
- **First launch** offers the preset page once, before the intro, so a new player knows Classic
  and Modern exist. First launch means the profile had no `lba2.cfg` of its own at boot: the
  check the intro uses today (`IsFirstGameLaunched`, no saves) would offer the page again to a
  player who deleted their saves. Escape changes nothing, which on a first launch is Default. A
  run under automation (`Control_IsActive`: `--load`, `--exec`, `--tick`) skips the page as it
  skips the logos ([SOURCES/PERSO.CPP](../../SOURCES/PERSO.CPP)), so a `ui` verb captures it.
  Afterwards the page is in Options.
- **Show experimental settings** is a separate switch. Off, the Experimental page is not offered;
  its values still apply. Its help says what the page is: English only, and its rows may change
  or go between builds.
- **Reset settings** puts every setting except the languages back to a fresh install, the preset
  to Default and the player's settings and bindings included, after a confirmation step and with
  a settings backup
  kept first. That is what makes it more than choosing Default. The binding pages have their own
  reset. A fresh install's value is not always the compiled default: an install's own `lba2.cfg`
  is a layer under the profile's, and it can name the player's settings (the demo's names the
  volumes, the detail level, scenario cameras, video size and all 72 keyboard bindings). So Reset
  takes keys out of the profile's file rather than writing defaults over that layer, which needs
  the writer to leave a key out ([The config file](#the-config-file), point 2).

For this every setting gains four fields: the **ring** (none for a 1997 setting or a retail fix,
Modern or Experimental), from which the Modern preset and the Experimental page are built;
whether **presets set** it, no for every player's setting; whether it is a **switch** or a tuning
value, since presets set only switches; and the **category**, which the menu and the config file
both group by. A Modern-ring switch also says its **Modern value** (on, for the switches there
are today). A setting moves inward by changing its ring field in the PR that brings
the evidence ([ARCHITECTURE.md, Moving inward](../ARCHITECTURE.md#moving-inward)).

These fields live in a table of the menu's own, keyed by the `lba2.cfg` key, not in `T_SETTING`.
The build is C++98, so a settings row is a positional initializer, already up to 17 fields, and a
row that gave its category would have to spell out every field before it. The menu's fields are
shell; the row's are the contract the engine, the console and recordings read. A check holds the
two tables to one entry per key. Texts, Voices and Display text get settings rows, with a list of
value names so `lba2.cfg` keeps spelling them as words; Resolution and the bindings stay kinds of
their own.

## Order of work

1. **Row tables, nothing visible changes.** First, on main, a `ui` capture for each page that has
   none, and a check that selects each row once and compares the `lba2.cfg` it writes with
   main's: a capture shows a page, not what its rows write. Then move the Options pages into
   `OPTIONS_MENU.CPP`: one table per page and one loop for every page, writing only through
   `Settings_Apply` (Texts, Voices and Display text get their settings rows here). `DoGameMenu`
   still draws them. Every capture matches main byte for byte. In its own commit, the same loop
   writes `lba2.cfg` when the player leaves Options, so a crash keeps the changes. The binding
   writers then have to write the player's own tables while a replay has borrowed them, as
   `Settings_WriteConfig` already does for a borrowed row: today `WriteInputConfig` writes
   `DefKeys`, which holds the recording's table for the length of a replay (`bindings_install` in
   [SOURCES/RECORD.CPP](../../SOURCES/RECORD.CPP)), so a replay that opens Options and is then
   killed would leave the recording's keys in the player's file.
2. **Categories (#782).** The hub and the category pages below, new labels in six languages,
   goldens remade. PR #835's filter rows land here or in Display first, by agreement with its
   author. The Camera page gains invert and sensitivity rows. A check fails a label that uses a
   reserved word (GLOSSARY).
3. **The menu's table of ring, presets-set and category fields**, and the config file grouped by
   category (see below). Keys with no settings row (the bindings, the resolution, `LastSave`, the
   install's own keys) get an entry naming their group, so the writer can place them too.
4. **Windowed list with a scroll bar (#738)**, for the resolution list first.
5. **An automatic settings backup** (a copy of `lba2.cfg` kept before values are replaced), then
   **Reset settings** to a fresh install, with a confirmation step. Reset takes keys out of the
   file, so the Decision on leaving keys out ([The config file](#the-config-file), point 2) comes
   before it.
6. **A value from before on every row the port added (#846)**, then the **Classic** preset.
   Today Classic needs only Fix retail bugs on No, since both camera switches are off by default;
   #846 is what keeps a switch that moves on by default later from staying on under Classic.
   Its console step, `settings_before` with no prefix, also sets player's settings (the mouse
   confine, the fixed timestep), so the preset reuses it through the presets-set field.
7. **Default and Modern presets**, the preset page with Back to my settings, the offer on first
   launch, and a fixture with every Modern setting on.
8. **Experimental page generated from the settings table**, behind Show experimental settings.
   Then the camera presets (#733), the console row (#679), saving and loading settings backups
   from the menu, each in its own PR.
9. **The save list into its own file** (roadmap §3, third slice).
10. **The 1997 Options page**, and the main menu as the per-game part once LBA1 needs it.

Steps 1 and 2 are the first slice. The rest is direction, settled one PR at a time.

## The page model

A page is a table of rows, not a loop. A row says:

| Field | What |
|---|---|
| kind | page (opens another), choice (cycles values), slider (a range), action (with a confirmation step where it cannot be undone), binding |
| label | a `TEXT.HQR` id for a 1997 row, or a `MENU_LABELS` entry |
| setting | the settings-table row it reads and writes, if any; resolution and bindings are kinds of their own |
| shown when | a check: the platform has the feature, the data has speech, Show experimental settings is on |
| active when | a check on another setting; while it fails the row is drawn dimmed and its help says what it needs |
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
| Advanced and reset | Reset settings, Show experimental settings, Fix retail bugs (until Gameplay opens), Console (#679), Experimental (a page) | settings backups, cheats |

The hub has the return row, the preset row and the categories: 7 rows, 8 once Gameplay opens.
The preset row sits on the hub, under the return row, because it is the choice most players
come for, and "Advanced" reads as "leave this alone". Every page stays within 8 rows at 640x480;
Advanced and reset reaches 6 with the return row. A row that cannot apply here (no speech in the
data, an always-fullscreen device) is hidden, as today.

The Camera page keeps Outdoors and Indoors, and gains invert and sensitivity for the mouse and
the stick. Players look for those on the camera page in any game, so they are player's settings
with rows, not tuning on the Experimental page. They only do something with Outdoors on Auto: the
mouse camera runs only with the Auto camera outdoors, and the stick camera only with the Auto
camera ([SOURCES/EXTFUNC.CPP](../../SOURCES/EXTFUNC.CPP)). That makes them the first rows that
depend on another setting. Outdoors is on the same page, so they are dimmed rather than hidden:
hidden, they would move the rows under the cursor each time the player cycles Outdoors.

The Resolution list offers 640x480, a widescreen rung 480 pixels high that fills the display's
width, and rungs 720 and 1080 high ([RUNTIME_RESOLUTION.md](../RUNTIME_RESOLUTION.md),
`Res_BuildMenuList`). Widescreen is finished; HD is not: at 720 and 1080 the UI is a 640x480
block in the middle, interiors are a 640-class blit, and the vertical framing is undecided
([WIDESCREEN.md, Phase 7](../WIDESCREEN.md#phase-7-native-hd)). The console's list already tags
those rungs experimental (`RES_CATALOG.CPP`); the menu shows them bare. The list marks them, and
their help line says what is unfinished, until Phase 7 lands (open question 10). Resolution is a
player's setting, so Modern never picks HD.

At 640x480 a page shows the selected row's help on a line below its rows where it fits, and the
preset page always: the help line is how a player learns what Default sets. From 840 logical
pixels the wide layout has a help line on every page.

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
   the row (help, range, default). Nineteen keys have no help text to generate it from today
   (nine rows, among them `FollowCamera`, `VSync` and `FixedTimestep`, plus the hand-written
   and seed keys), and ranges are written into the help by hand in two styles, so every key
   needs a help line and the range comes from the row's minimum and maximum. Experimental keys
   come last under their own heading; game state that is not a preference (`LastSave`) after
   that. The installer's keys (`Version` and the rest) belong to the game data, not the
   profile, so the writer never puts them there. The
   player's own lines come along: a key this build does not know is kept under a heading at the
   end, so a file shared with a newer build loses nothing, and a comment the player wrote stays
   with the key below it. Today's writer edits the file in place, so both survive now
   ([LIB386/SYSTEM/DEFFILE.CPP](../../LIB386/SYSTEM/DEFFILE.CPP), `DefFileBufferWriteString`);
   grouping moves keys, which it never does, so keeping a comment with its key is new code. The
   29 comment lines from the 1997 seed are not the player's: the first write in the new form drops
   the lines that match the seed.
2. **The file holds what the player chose.** First the chosen preset, then a value only where it
   differs from its base: the chosen preset's value for a setting presets set, the default for
   the rest. A key equal to its base is left out unless the player's own file named it (the
   provenance from #574 tells the two apart). Left out, it follows its base, including a later
   one. Two things in today's reader stand in the way. The 1997 seed
   ([SOURCES/LBA2.CFG](../../SOURCES/LBA2.CFG)) stands in for the game-data config when the
   install ships none, and three of its values differ from the rows' defaults: effects volume
   127 against 97, compressed saves on where a missing key reads off, and no stick binding for
   turning the camera. Today a new profile gets them because the seed is written into it; a file
   that holds only changes needs the seed named as that layer in the chain, or the rows to take
   its values. And `ReadInputConfig` ([SOURCES/INPUT.CPP](../../SOURCES/INPUT.CPP)) reads no
   keyboard binding unless `WinMode` is 1, the mark the binding writer sets (the seed has 0);
   once it is set, a missing binding already falls back to its default, so the reader drops the
   mark and reads each binding on its own.
3. **A reference file beside it**, every key grouped and commented at its default, regenerated
   when the build changes and never read. Players copy lines from it.
4. **Bring your own config.** A file loaded from the menu is applied key by key, as the console
   would set them, and then written: an import, so a partial file sets only its keys. It is not a
   layer in the chain [PORTABILITY_PLAN.md](PORTABILITY_PLAN.md) reads at every boot (defaults,
   game-data config, the profile's config, flags): under the profile's file it would change
   nothing while that file names every key, and over it a change made in the menu would be lost
   at the next boot. A `Preset` line in the file sets the base; without one the base stays as it
   was. Whether that line also takes out the player's own values for settings presets set, as
   choosing a preset in the menu does, is open question 11. A binding that takes a key another
   action already has is refused, as the keyboard page refuses it (`CheckKeyConfig`,
   [SOURCES/CONFIG.CPP](../../SOURCES/CONFIG.CPP)), and the import says which ones it skipped.
   Settings backups (EPmager, Discussion #782) save and load such files from the menu.
5. **Readable names** for bindings (#61), read alongside the scancodes.

Point 2 and the stored preset change what an existing file means, which is a core contract
(principle 2), so together they start from one Decision. The migration they need: an old file has
no `Preset` key, so it is on Default, and the first write in the new form keeps a copy of the old
file and drops each key equal to a default its row has had. Equal to
the current default is not enough: if a default has moved, an old file holds the old one, which
no longer matches, and the player stays on it, the case point 2 exists for. So the migration reads
a short table of the values releases wrote into files that are no longer the default, by key and
release, rather than a field on every row; which ones is a one-off check against the tags (the
camera settings switched on on 2026-10-13 shipped in the release that added them, so no release
wrote their old value). The table lists what a release wrote, not the code's default:
`DisplayFullScreen` defaulted to a window until 0.13.0, but every seed wrote fullscreen, so a
window in an old file is the player's choice, and an entry for it would put them in fullscreen.
From then on the file records the build that wrote it, so a later migration can be exact. A
player who had chosen an old default on purpose loses that choice once; the copy still holds it.

The migration also takes the installer's keys out of the profile. The 0.12.0 seed wrote
`Version: 0` into every profile made over data without a config, and later writers kept it. A
written `Version` counts as declared, so it takes the place of what the data says: harmless on
the Steam and GOG Classic data those profiles were made over, where the data says 0 too, but the
profile is shared by every game folder, so one begun on such a copy and later pointed at a
Twinsen's Odyssey copy runs it as the unknown release (checked 2026-11-03 on the Activision US CD
data: `unknown (0) from the config` against `activision (1)` without the line), with the LBA2 logo
on the new-game frame and the European CD volume name.

## A north star for the Modern UI

Not a promise; the direction the steps above should not close off.

1. **Pages are data.** As in the page model above.
2. **The UI scales in whole steps.** `scale = max(1, floor(min(W / 640, H / 480)))`, and pages
   lay out on a logical canvas of the framebuffer divided by the scale. 1920x1080 is 2x on a
   960x540 canvas; 640x480 is 1x and looks as today. This is part of finishing HD (the UI is an
   unscaled 640x480 block there today), so it waits on the same work. Needs a scaled `Font()` (it blits 1:1
   today) and scaled plasma; sprites already have `ScaleSprite`. A UI scale row (Auto, 1x, 2x,
   3x) lets the player choose, and Auto can use the display's content scale where the platform
   reports one (SDL3 has `SDL_GetDisplayContentScale`). A handheld shows why the row is not
   enough alone: a 7-inch 1280x800 screen gets 1x, small at arm's length, and 2x gives a 640x400
   canvas, where an 8-row page (8 × 50 + 7 × 6 = 442 pixels) does not fit. So pages window below
   480 logical pixels, as long lists do, or the larger scale is out of reach there; 1280x720 at
   2x is 360 pixels tall.
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
a window at a time, the wheel and a drag for the pointer. At 640x480 the rows of a page that
scrolls move 14 pixels left to make room; pages that do not scroll stay centred.

The Experimental page is generated from the settings table: every row in the Experimental ring,
grouped by category, labelled with its console name and help text, in English. An experiment
then needs no hand-made row and no translations. It is a dense table in the 8x8 font, like the
key binding screen: the 41 tuning rows in a six-row game-font window would be seven windows of
scrolling. The game-font list was mocked too.

### Controls

Today keyboard and gamepad are two binding screens, each one 36-row table in the 8x8 font
(`DrawScreenConfig` in [SOURCES/CONFIG.CPP](../../SOURCES/CONFIG.CPP)). The Modern page splits
the 36 actions into seven groups of four to seven (movement, actions, behaviours, weapons, magic
and items, camera, menus), one pane each, with Key 1, Key 2 and Gamepad side by side as key
chips. A key bound to two actions is allowed and framed red. The page names the buttons of the
pad in use (#371), and each device's bindings have their own reset. Rebound movement keys (WASD,
say) do not move the menu cursor, which uses fixed keys ([MENU.md](../MENU.md#navigation)), and
a PC player who rebinds will notice. The 1997 screen stays.

## Testing against the original

The menu does not touch the game's rules, so the world hash cannot see it. Four checks do:

1. **A refactor changes nothing.** Every menu page is captured before and after and must match
   main byte for byte, and each row is selected once and the `lba2.cfg` it writes compared with
   main's. First change one row by hand and see a golden fail. These fixtures need game data and
   do not run in CI: each PR runs them on US CD and GOG data and says so.
2. **The 1997 code stays 1997.** `DoGameMenu`, `DrawGameMenu`, `DrawOneChoice` and the plasma
   stay put; a diff against 333929a shows only the port's known edits, listed in a comment where
   they sit.
3. **Classic is the original.** Choosing Classic sets every switch a preset sets to its value
   from before, and leaves the player's settings and the tuning values alone, some of which the original did not have
   (texture filtering, mouse confine). So
   [test_ring_off_is_gone.sh](../../tests/automation/test_ring_off_is_gone.sh) runs with Classic
   applied over a player's setup that is not the default, texture filtering on say, which also
   shows that the player's settings cannot reach the world hash. Then a check that the preset
   touches every setting a preset sets and none of the player's, and
   `scripts/ci/check-settings-before.py` extended so a row that switches a behaviour on must say
   its value from before.
4. **The 1997 page.** Its table against 333929a's `GameOptionMenu` (8 rows, known text ids), and
   captures beside GOG DOS `LBA2.EXE` under DOSBox-X. A visual check, not pixel-exact.

The page tables and the config layout are plain data, so most of this needs no game data. The
page tables and the menu's own table can be host tests if they name settings by `lba2.cfg` key
and stay free of engine headers: at most 8 rows a plain page, every label in six languages. The
settings tables cannot: three of the four live in files that pull in the engine
(`CONFIG_FILE.CPP`, `FOLLOWCAM.CPP`, `AMBIANCE.CPP`), which is why
`scripts/ci/check-settings-before.py` reads them from the source. Every row naming a setting that
exists, and every setting with exactly one entry in the menu's table, are checks of that kind.
Host tests for the writer and the borrow: a written file that reads back to the same values, a
preset applied and then Back to my settings giving back every value a preset sets while keeping a
player's setting changed since, a key this build does not know surviving a write, and the binding
writers persisting the player's tables while a replay has borrowed them.

## Open questions

1. Category names. The wide layout's left pane uses the same ones: one concept, one name, so a
   name too long for the pane is shortened everywhere.
2. Image: one choice for the three filters, or three rows (#835)?
3. How many rows invert and sensitivity take on the Camera page: each has two axes and two
   devices, and the page holds 8 rows.
4. Whether a category page gets its own reset, as a row or as a key in the wide layout's prompt
   bar.
5. Whether "Fix retail bugs" reads well to players, since "retail" is a developer's word; if it
   changes, with the step 2 labels.
6. Whether the menu cursor follows rebound movement keys or the page says it does not.
7. The reference file's name, and whether it is written every run or only when the build changes.
8. Who checks the translations of the new labels.
9. What the preset row shows when the player's values differ from the chosen preset: Custom, or
   the preset's name marked as changed ("Modern, changed"). The help line names the preset either
   way. Leaning to "Modern, changed": the stored preset still decides every setting the player
   has not changed, and Custom hides it.
10. Whether the 720 and 1080 rungs of the resolution list sit behind Show experimental settings,
    or stay in the list marked as unfinished.
11. Whether a `Preset` line in an imported file takes out the player's own values for settings
    presets set, as choosing the preset in the menu does, and whether an imported key equal to
    the new base stays in the file as one the player named. Left alone, a Modern player who
    imports a Classic file keeps their indoor camera, loses the outdoor Auto camera that came
    from Modern, and reads "Classic, changed".

## Not in this plan

The inventory, holomap, dialogue and HUD. They share the drawing parts and would follow the
same scale rule, but each is its own piece of work.
