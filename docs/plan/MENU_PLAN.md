# Game menu and settings: plan

How the Options menu gets sorted by category
([Discussion #782](https://github.com/LBALab/lba2-classic-community/discussions/782)), how
presets reach players (#838), how `lba2.cfg` becomes a file players can read, and how the first
slice starts the game-menu refactor in [REFACTOR_ROADMAP.md](REFACTOR_ROADMAP.md) §3 without
closing off a Modern UI that uses the room a larger screen gives. Status: proposed.

It builds on [PRINCIPLES.md](../PRINCIPLES.md) and the rings in
[ARCHITECTURE.md](../ARCHITECTURE.md#the-rings-and-the-core). The words it uses (Classic,
Modern, Custom, Experimental, ring, preset) are defined in
[GLOSSARY.md](../GLOSSARY.md#project-and-settings), which also keeps the menu's two words apart:
options are the place (Options, Advanced options, More options), settings the values chosen
there (Reset settings). The menu and the config file are shell: they show and
set settings, and never change the game's rules.

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
  the intro and a new game, so a new player plays on the defaults before seeing Options. It
  also skips the Adeline logo ([SOURCES/PERSO.CPP](../../SOURCES/PERSO.CPP)).
- The publisher logo (Activision, EA or Virgin) is shown before the Adeline logo when the
  install's config declares a `Version`, and not when it declares none
  ([VERSIONS.md](../VERSIONS.md)). `ShowDistribLogo` overrides that either way; it is read, never
  written, and only the console's `distrib logo` sets it.
- The 1997 Options menu is one flat page of 8 rows (333929a `SOURCES/GAMEMENU.CPP`,
  `GameOptionMenu`): return, volume, reverse stereo, detail level, scenario cameras, keyboard
  config, video size, display text.

## The prototype

A playable prototype,
[lba2-menu-prototype](https://github.com/noctonca/lba2-menu-prototype),
draws the Modern menu this plan describes with the game's own data: the hub and the category
pages, the presets with Undo preset, Reset settings and Undo reset,
Keys and buttons, More options, in both layouts, with keys, a pad, a mouse and touch. It builds
lba2cc's `LIB386` from a submodule, copies the few game-layer drawing helpers it needs, and holds
its settings in memory: nothing writes `lba2.cfg`. The maintainer (on an Xbox too) and EPmager
have played it, and its rows, words and layout changed with what they found.

The prototype is this plan's reference for how the menu looks and behaves: which rows each page
has and in what order, their labels and help, the layouts, the legend, the scroll bar, the key
chips, the questions, the pointer. This plan does not restate them. A PR that carries prototype
code into lba2cc names the prototype commit it was checked against. Where to read it:

- its tables in `src/menu.cpp` (the Options pages), `src/bindings.cpp` (Keys and buttons) and
  `src/more_options.cpp` (More options);
- `docs/UX.md`, the layout and wording rules and why, each one the self test checks marked with
  the check's name;
- the README, for each page as a player meets it, and for the testing tools;
- `lba2menu --selftest` and `tools/sweep.py`, which check those rules on every page at seven
  screen sizes, and `tools/baseline.py`, which records a run state by state
  and picture by picture so a change meant to move code can show it moved nothing.

Its game menu's title is the product's name: the gold LBA 2 logo from the 1997 intro, with a
lower-case cc of this project's own under the 2, so the mark reads LBA, 2, cc. It is drawn in
code at the screen's resolution from an outline traced from the intro (`video.hqr` entry 16,
the European release) and a cc drawn by rule, and builds in as the intro builds it; in play the
game menu has it smaller and still (decided 2026-11-17 and 2026-11-18). With the game font it
falls back to the name in one line of text. Whether lba2cc takes it is open question 13.

Its code is to be carried into lba2cc, the whole menu (decided 2026-11-11);
[Order of work](#order-of-work) says which part goes where.

### Where lba2cc departs from it

The prototype tries things this plan has not taken, and stands in for lba2cc where it has no
engine. lba2cc does not carry these over:

- **Presets set the look.** The prototype's presets also set the background, the font, the
  Outdoors and Indoors cameras and the screen (Classic 640x480, Modern the native size), and
  Undo preset, Reset settings and Undo reset put the screen back. Here a preset sets how the
  game plays and leaves the player's screen alone ([What a preset leaves alone](#what-a-preset-leaves-alone)),
  and a fresh install keeps the Auto camera off until it moves on by default (#839).
- **Background and Font rows** on Language and interface: lba2cc has neither the Twinsun
  background nor the Zilla Slab face.
- **Rows that stand in for work not done here.** Image stands in for the filters (#835, open
  question 2), Invert camera and Camera speed for the invert and sensitivity rows (open
  question 3), Button names for the prompts #371 adds, the Console row's version for lba2cc's
  own.
- **Its own machinery.** Its window, input and clock (lba2cc has its own), its copies of the
  game's drawing helpers (lba2cc has the originals), its settings held in memory, its switches
  for testing (screen sizes, the pad's names, help shown whole, the key list overlay), and the
  "original" page that stands for today's lba2cc Options in its captures.

## What the principles ask

1. **A preset is values, never a menu** (principle 1). Choosing Classic sets every setting a
   preset sets to its value from before. A row hidden from view is not a setting switched off:
   whatever reads the setting checks it. One menu for every preset also keeps a config
   reproducible without its menu state (EPmager on #838).
2. **Modern offers choices, More options holds the tuning** (principle 7). The Auto camera is one
   or two Modern rows; its tuning values are on the More options page. The same for the graphics
   filters.
3. **Promise only what you test** (principle 7). The Modern preset is the defaults, so what it
   promises is what a fresh install plays. A setting goes on by default only with the evidence
   [moving inward](../ARCHITECTURE.md#moving-inward) asks for, and the Modern-ring settings are
   also tested all on at once, since a player can turn them all on.
4. **Every row is one more to test** (principle 6). A page opens when it has rows to hold:
   Gameplay opens with its second row, and until then Fix retail bugs stays where it is.
5. **The original stays reachable** (principle 1). The 1997 menu code stays, as the reference and
   for the 1997 Options page later.

## Presets

Two presets set values, and the one the player chose is stored in `lba2.cfg` as the base their
own values sit on. The preset row names that base ("Preset: Modern") and marks it changed when
the player's own values differ from it ("Preset: Modern, changed"): computed, never chosen.

| Preset | Sets |
|---|---|
| **Classic** | Every switch the port added at its value from before, and Fix retail bugs on No: the 1997 CD release's rules. The publisher logo as the release declares it. |
| **Modern** | Every switch at its default: what a fresh install gets. Retail fixes on, no publisher logo, and a port feature on once it has moved on by default. |

The question a player answers is the original's rules or the port's. Being in the Modern ring
does not put a setting in the Modern preset: a Modern-ring setting is a tested menu row, on or
off by default, and it reaches the preset only by going on by default, which takes its own
evidence ([moving inward](../ARCHITECTURE.md#moving-inward)). A third preset between the two, the
defaults plus every Modern-ring switch on, would hold only the settings tested well enough to be
offered but not well enough to be on for everyone; a player turns those on by their rows.

Today the two differ in Fix retail bugs alone: every switch the port added is off by default,
which is its value from before, and nothing is in the Modern ring yet (the Auto camera is
Experimental until #839). The publisher logo will be the second difference.

Modern is the preset a fresh install plays, and nobody is asked to choose (decided 2026-11-16).
A player who wants the original's rules sets Classic in Options.

**The publisher logo** (decided 2026-11-16) becomes a setting the presets set. Activision, EA and
Virgin no longer distribute the game, so their logo before the Adeline logo is part of the
original's presentation, not the port's: off by default, so Modern shows none, and its value
from before is today's rule, the logo the install's `Version` declares, so Classic shows it as
1997 did. That makes `ShowDistribLogo` a setting like the others, written when the player
changes it: its values are Off and As the release says, never the resolved yes or no, so a
profile pointed at another game folder still follows that folder's declaration. Its row is on
More options. The demo's bumper is the demo's own presentation and stays as it is.

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

Without this, Classic would set the mouse confine to its value from before, choosing Modern
would put a player's texture filtering back to its default, and both would read changed a minute
later.

### The chosen preset is the base

`lba2.cfg` holds the chosen preset (`Preset: Modern`) and, for a setting a preset sets, a value
only where the player's own differs from it. At read time the chosen preset's value takes the
place of the row's default: for a switch, Classic gives the value from before and Modern the
default. The chain [PORTABILITY_PLAN.md](PORTABILITY_PLAN.md) reads becomes
compiled defaults, game-data config, chosen preset, the profile's own values, flags. The preset
covers only the settings presets set, so an install's config still sets the player's settings.

Storing the preset is what keeps each one true when a default moves or a setting moves inward:

- A Classic player's file names no value equal to Classic's. When the Auto camera one day moves
  on by default, they keep Classic's value, because their base is Classic, not the default.
- A Modern player follows the defaults as they move: a setting that goes on by default later
  reaches them without choosing Modern again, which is what
  [The config file](#the-config-file) wants for every player.

Computed from the values alone, one preset fails. A file that leaves out values equal to the
default takes Classic players along when a default moves; a file that writes every value keeps
Modern players where they were.

The cost is that the same file gives different values in different builds when a preset's values
change, which EPmager asked to avoid on #838. A recording carries every value, so a run stays
reproducible, and on Classic every build gives the original's rules, which is what a runner wants
checked. A player who wants a setup frozen across builds names its values. The base needs the
writer to leave out a value equal to it, so it is one Decision with point 2 of
[The config file](#the-config-file).

### Choosing, undoing and resetting

The prototype shows the pages and their words; these are the rules under them.

- Choosing a preset asks for confirmation, then keeps the preset it replaces and the player's
  values as a settings backup, which Undo preset restores: the preset, the player's values
  for the settings a preset sets, and the tuning values, not the whole file, so a volume or a
  resolution changed since stays. The backup is taken after the values in memory are written,
  since `lba2.cfg` is otherwise written only when the player leaves Options. A preset is never
  applied by cycling a row: one press past a changed preset would overwrite a player's setup
  with no way back.
- Applying a preset takes out the player's own values for every setting a preset sets and puts
  the tuning values back to their defaults (the backup keeps both). Changing a
  setting a preset sets, tuning included, marks the preset changed; changing a player's setting
  does not.
- Changed is computed: the chosen preset is stored, changed is not, and no player chooses it. A
  file with no `Preset` key is on Modern, so a player who brings an edited `lba2.cfg` sees "Modern,
  changed" where the file sets a value Modern does not, which answers EPmager's question
  on #838. While the preset reads changed, the help names the settings that differ from
  the chosen preset, Experimental ones included, so a value set from the console or a downloaded
  file can be found.
- The Fix retail bugs row stays as it is, and works differently: its Custom is a stored value the
  player cycles to, and each fix keeps its own setting under Yes and No
  ([SOURCES/RETAILFIX.H](../../SOURCES/RETAILFIX.H), `RetailFix_NextMenuValue`), so cycling it
  loses nothing. A preset sets this row and never the fixes under it, which keeps the promise
  that Yes and No never overwrite them; comparing values with a preset leaves the fixes out too,
  since under Yes or No they change nothing.
- **A fresh install opens on the game menu** (decided 2026-11-16), after the logos, the Adeline
  logo included, as any other start has them: New game, Options, Quit. Today, with no save, the game goes straight into the intro
  and a new game, so a new player plays before they could set anything. New game still plays the
  intro (`Introduction` in [SOURCES/GAMEMENU.CPP](../../SOURCES/GAMEMENU.CPP)), so nothing is
  lost. No question is asked on first launch: Modern is what a fresh install plays, and the
  preset page is in Options. A run under automation (`Control_IsActive`: `--load`, `--exec`,
  `--tick`) skips the logos ([SOURCES/PERSO.CPP](../../SOURCES/PERSO.CPP)) and goes where it is
  told, as today.
- **More options is always offered**, a row on Advanced and reset; no switch shows or hides it
  (decided 2026-11-03). Its help warns instead, and only its Experimental-ring rows carry the
  experimental tag (see [Long lists](#long-lists)).
- **Reset settings** puts every setting except the languages back to a fresh install, the preset
  to Modern and the player's settings and bindings included, after a confirmation step and with
  a settings backup kept first, which **Undo reset** restores. That is what makes it more than
  choosing Modern. The binding pages have their own reset. A fresh install's value is not always
  the compiled default: an install's own `lba2.cfg` is a layer under the profile's, and it can
  name the player's settings (the demo's names the volumes, the detail level, scenario cameras,
  video size and all 72 keyboard bindings). So Reset takes keys out of the profile's file rather
  than writing defaults over that layer, which needs the writer to leave a key out
  ([The config file](#the-config-file), point 2).

For this every setting gains four fields: the **ring** (none for a 1997 setting or a retail fix,
Modern or Experimental), from which More options tags its experimental rows and the Modern-ring
fixture picks its settings;
whether **presets set** it, no for every player's setting; whether it is a **switch** or a tuning
value, since presets set only switches; and the **category**, which the menu and the config file
both group by. A setting moves inward by changing its ring field in the PR that brings
the evidence ([ARCHITECTURE.md, Moving inward](../ARCHITECTURE.md#moving-inward)). A tuning row
needs two more for the More options page: a **step** for Left and Right (`cam_ease_ms` moves by
50, the stick deadzone by 500), and **value names** for a choice (`mouse_confine` 0, 1 and 2 read
Never, Fullscreen and Always).

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
   `OPTIONS_PAGES.CPP` and the loop into `OPTIONS_MENU.CPP`
   ([Where the code goes](#where-the-code-goes)): one table per page and one loop for every
   page, writing only through `Settings_Apply` (Texts, Voices and Display text get their
   settings rows here). `DoGameMenu` still draws them, Return rows included. Every capture
   matches main byte for byte. The draw record and the walks
   ([Testing](#testing-against-the-original), points 5 and 6) come with the loop, so step 2's
   pages are checked as they are made. In its own commit,
   the same loop writes `lba2.cfg` when the player leaves Options, so a crash keeps the
   changes. The binding writers then have to write the player's own tables while a replay has
   borrowed them, as
   `Settings_WriteConfig` already does for a borrowed row: today `WriteInputConfig` writes
   `DefKeys`, which holds the recording's table for the length of a replay (`bindings_install` in
   [SOURCES/RECORD.CPP](../../SOURCES/RECORD.CPP)), so a replay that opens Options and is then
   killed would leave the recording's keys in the player's file.
2. **Categories (#782).** The hub and the category pages as the prototype has them, new labels in
   six languages, goldens remade. PR #835's filter rows land here or in Display first, by
   agreement with its author. The Camera page's rows join Controls and camera, with invert and
   sensitivity. A check fails a label that uses a reserved word (GLOSSARY).
3. **The menu's table of ring, presets-set and category fields**, and the config file grouped by
   category (see below). Keys with no settings row (the bindings, the resolution, `LastSave`, the
   install's own keys) get an entry naming their group, so the writer can place them too.
4. **Windowed list with a scroll bar (#738)**, for the resolution list first.
5. **An automatic settings backup** (a copy of `lba2.cfg` kept before values are replaced), then
   **Reset settings** to a fresh install, with a confirmation step. Reset takes keys out of the
   file, so the Decision on leaving keys out ([The config file](#the-config-file), point 2) comes
   before it.
6. **A value from before on every row the port added (#846)**, then the **Classic** preset.
   Today Classic needs only Fix retail bugs on No, since every switch the port added is off by
   default, and the publisher logo's value from before;
   #846 is what keeps a switch that moves on by default later from staying on under Classic.
   Its console step, `settings_before` with no prefix, also sets player's settings (the mouse
   confine, the fixed timestep), so the preset reuses it through the presets-set field.
7. **The Modern preset**, the preset page with Undo preset, the publisher logo off by default,
   the game menu on a fresh install in place of the intro, and a fixture with every Modern-ring
   setting on at once.
8. **More options generated from the settings table**, a row on Advanced and reset.
   Then the camera presets (#733), the console row (#679) with the version, the Frame rate row,
   saving and loading settings backups from the menu, each in its own PR.
9. **The save list into its own file** (roadmap §3, third slice).
10. **The 1997 Options page**, with Detail level, Video size, Reverse stereo and scenario cameras
    where the original had them, and the main menu as the per-game part once LBA1 needs it.

Steps 1 and 2 are the first slice. The rest is direction, settled one PR at a time.

The code comes from the prototype. A part is first brought to this plan's model inside the
prototype, where its self test and `tools/baseline.py`, compared state by state and picture by
picture before and after, show it behaves as it did; then it moves.

- **Step 1** takes the page model: the row tables, the stack, the loop, the draw record and the
  walks. The tables hold lba2cc's pages as they are today and write through `Settings_Apply`
  where the prototype writes its settings in memory. `DoGameMenu` still draws them, so the
  captures still match main.
- **Steps 2, 7 and 8** take the hub and the category pages, the presets with Undo preset, and
  More options, which reads lba2cc's settings tables where the
  prototype reads a copy. Keys and buttons ([Controls](#controls)) comes the same way. What
  [Where lba2cc departs from it](#where-lba2cc-departs-from-it) lists stays behind.
- **The prototype's drawing** (the one column with the help in the dialogue window, the wide
  composition and its band) draws the Options pages only once it draws today's pages to main's
  captures, as [A north star](#a-north-star-for-the-modern-ui) asks of any new renderer. The
  1997 drawing stays ([Testing](#testing-against-the-original), point 2).

### Where the code goes

The prototype holds the menu in one file, `menu.cpp`: its stand-in settings and presets, the
page tables, the loop, both layouts' drawing, the pointer, the voice, and its test output. In
lba2cc these parts go to separate files, each with one job:

| File | Holds | From the prototype |
|---|---|---|
| `OPTIONS_MENU.CPP` | the stack, [the mode](#one-mode), the loop and the row kinds' handlers; the draw record | `menu.cpp`, navigation |
| `OPTIONS_PAGES.CPP` | the Options pages' row tables: the hub, the categories, the preset page, Advanced and reset | `menu.cpp`, pages as data |
| `OPTIONS_PRESETS.CPP` | the presets, Undo preset, Reset settings and Undo reset, the backup they keep | `menu.cpp`, presets and undo |
| `OPTIONS_KEYS.CPP` | Keys and buttons: its pages made from the binding tables, the chips, the device, the wait for a key | `bindings.cpp` |
| `OPTIONS_MORE.CPP` | More options, its page made from the settings tables | `more_options.cpp` |
| `OPTIONS_DRAW.CPP` | drawing a page from the stack and recording it: through `DoGameMenu` first, the Modern layouts later | `menu.cpp`, drawing |

What stays where it is: the values in the six settings tables
([SOURCES/SETTINGS.H](../../SOURCES/SETTINGS.H)), with the fields step 3 adds; the labels in
`MENU_LABELS.CPP`, by id; the binding writers; `GAMEMENU.CPP` for the 1997 pages and the main
menu, which opens Options; the console.

The lines between them:

- Only `OPTIONS_DRAW.CPP` knows pixels or which layout is up. The tables and the loop hold no
  position.
- Only the settings tables hold values. A row names its settings row, and every change goes
  through `Settings_Apply`; no page writes a global.
- The presets read and write through the settings tables and the binding tables, never through
  a page.
- Tests and `state` read the draw record and the stack, not a getter for each thing they check.
  The prototype's self test reads a `Menu_*` getter for each of many things besides the draw
  record, and More options and Keys and buttons add their own, each added for one check.

## The page model

A page is a table of rows, not a loop. A row says:

| Field | What |
|---|---|
| kind | page (opens another), choice (cycles values), slider (a range), action (with a confirmation step where it cannot be undone), binding |
| label | a `TEXT.HQR` id for a 1997 row, or a `MENU_LABELS` entry |
| setting | the settings-table row it reads and writes, if any; resolution and bindings are kinds of their own |
| shown when | a check: the platform has the feature, the data has speech |
| active when | a check on another setting; while it fails the row is drawn dimmed and its help says what it needs |
| help | a short line saying what the row does, and its cost if any, then more if there is more to say |

One loop draws and runs any page, writes through `Settings_Apply`, and repaints the backdrop on
entry and exit in one place (today seven page handlers each repeat it). Nothing in the table is a
pixel position, so a second layout can read the same tables.

The loop is only a caller. Each frame it calls one update and one draw for the page on top of the
stack, then presents; neither waits for a key or presents a frame itself, as `Menu_Update` and
`Menu_Draw` in the prototype's `src/main.cpp` do not. Today `GAMEMENU.CPP` calls `Flip`,
`BoxUpdate` or `CopyScreen` 67 times. Kept to that shape, the last step of
[the north star](#a-north-star-for-the-modern-ui) changes who calls the update and the draw, not
the pages, and a host that owns the frame can run the menu as it is: the prototype's web build
hands every frame back to the browser.

The loop keeps three things so a page can change under the player:

- **Where the player is** is one stack of pages, each with the row its cursor is on, the same
  for every layout. A layout works out its panes from the stack, so a screen that changes size
  in the menu, or crosses from the one column to the wide layout, keeps the page and the row.
- **The cursor holds a row, not a position.** "Shown when" is checked as the page is drawn, so a
  row that comes or goes (Undo preset, Undo reset) leaves the cursor on the row it was on.
- **Live rows** show the engine's state: Resolution the screen in use, Fullscreen the window as
  it is. They hold no value of their own, so a preset leaves them alone and they are never drawn
  as changed.

The loop also records what it drew each frame: each row's label, value, box and fill, and where
its text ends; the help, its window and the lines it needed; the cursor. The checks in
[Testing against the original](#testing-against-the-original) read that record, not pixels.
Which device was used last is one value, read by the Controls page, the Console row, the legend
and the wait for a key. Whether there is a keyboard is a second value: on a desktop always, on
a console only once a key has been typed. Until then the legend names the pad from the start
and Keys and buttons offers no Keyboard view (decided 2026-11-11, after playing on an Xbox).

### One mode

The menu is in one mode at a time, held as one value, not a flag for each:

- **A page**, the top of the stack: the cursor moves along its rows.
- **A question**, two answers with the safe one first and the cursor on it; Esc gives the safe
  answer. Classic, Reset settings and a key already in use on Keys and buttons all ask one. A
  question is a page pushed on the stack whose rows are its answers, so Esc, the depth, the draw
  record and `state` need nothing of their own.
- **Waiting for a key** on Keys and buttons: every key and button goes to the binding until one
  is taken or the wait is left.

The Console row opens lba2cc's console (`Console_Toggle`), which takes the input itself,
so it is not a mode of the menu either.

The prototype keeps a flag for each (first launch, question, closed, console, and Keys and
buttons' wait with its own question), tested one after another at the top of every frame, and
has two questions written apart: Classic and Reset settings in `menu.cpp`, the key in use in
`bindings.cpp`. Each mode there is one more flag, and one more test at the top of the frame.

### A row kind brings its own handlers

A row kind (setting, binding, More options' value, an answer) gives the loop a small set of
handlers: what Left, Right, Enter and Del do on it, what the voice says on it, and how it is
drawn and recorded. The loop calls the handlers of the row under the cursor and knows nothing
else of the kind. A page made from another table (Keys and buttons from the bindings, More
options from the settings tables) builds its rows and brings its kinds' handlers; it adds no
branch to the loop.

In the prototype each such page is tested for by name in the loop, the voice, the drawing, the
place string, `state` and the cursor's getters: moving More options onto the stack touched
all six. Handlers by kind keep a new page to its own file.

## Categories

The hub holds the preset row and six categories: Language and interface, Display and graphics,
Sound, Controls and camera, Gameplay (once it has its second row; until then Fix retail bugs
stays on Advanced and reset), and Advanced and reset. The prototype's tables say which rows each
holds. These are the parts that rest on lba2cc rather than on the prototype:

- **From 1997, on More options** (decided 2026-11-11): Detail level (each step down takes away
  animated water, rain, the far scenery, then shadows; `SetDetailLevel` in
  [SOURCES/GAMEMENU.CPP](../../SOURCES/GAMEMENU.CPP)) and Video size, labelled Movies (at
  original size or fit to screen; original size was for slow CD drives), were for the machines
  of the day. Reverse stereo stays on Sound: speakers the wrong way round are not a machine's
  limit. Movie camera, the 1997 scenario cameras, is camera behaviour: off skips the outdoor
  shots that cut once as Twinsen enters a zone ([CAMERA_INTERNALS.md](../CAMERA_INTERNALS.md)),
  so it goes with the cameras. The 1997 Options page (step 10) keeps all of them where they were.
- **Invert and sensitivity** for the mouse and the stick go with the cameras: players look for
  them there in any game, so they are player's settings with rows, not tuning on More options.
  They only do something with Outdoors on Auto: the mouse camera runs only with the Auto camera
  outdoors, and the stick camera only with the Auto camera
  ([SOURCES/EXTFUNC.CPP](../../SOURCES/EXTFUNC.CPP)). They are the first rows that depend on
  another setting, and they are dimmed rather than hidden, so cycling Outdoors never moves the
  rows under the cursor.
- **The Resolution list** offers 640x480, a widescreen rung 480 pixels high that fills the
  display's width, and rungs 720 and 1080 high
  ([RUNTIME_RESOLUTION.md](../RUNTIME_RESOLUTION.md), `Res_BuildMenuList`). Widescreen is
  finished; HD is not: at 720 and 1080 the UI is a 640x480 block in the middle, interiors are a
  640-class blit, and the vertical framing is undecided
  ([WIDESCREEN.md, Phase 7](../WIDESCREEN.md#phase-7-native-hd)). The console's list already tags
  those rungs experimental (`RES_CATALOG.CPP`); the menu shows them bare. The list marks them,
  and their help says what is unfinished, until Phase 7 lands (open question 9).
- **Frame rate** shows the frame count in a corner while the player plays, as the console's
  `fps` and the 1997 SPEED code do (`DebugFps`, drawn by `AffNbFps` in
  [SOURCES/OBJECT.CPP](../../SOURCES/OBJECT.CPP)). It is not kept in `lba2.cfg`: off at every
  start, as it is today, and presets do not set it.
- **Console** opens the console (#679): the full console after a key or touch, read-only after
  a pad button, which any button closes. Its value is the version, read-only: the version string
  and the commit's first 7 on a build off main (`0.13.3-dev 011394f`), the number alone on a
  release. Today the version is only in the window title (hidden in fullscreen and on Android),
  the log, `--version` and the console's `buildinfo`, so a player asked "which build?" has to
  know where to look.
- **No Debug page.** The debug tools (the hit boxes `zv`, `horizon`, the traces, `godmode`) are
  console variables and commands outside the settings table, never saved, and stay in the
  console.
- A row that cannot apply here (no speech in the data, an always-fullscreen device) is hidden,
  as today.

At every size the selected row's help is shown, in the 1997 dialogue window in the one column
and in the band in the wide layout, typed in the dialogue voice on every cursor move. A help
text is a short line, then more windows where there is more to say, which Space or the pad's Y
turns; the writer ends a window with `@P`, as `TEXT.HQR` ends a 1997 dialogue page
(`GetNextLine` in [SOURCES/MESSAGE.CPP](../../SOURCES/MESSAGE.CPP)).

### Languages

- **The Text row changes the whole menu** (decided 2026-11-12): labels, values, page titles,
  answers, the column heads and the legend's words, not only the help. The 1997 rows keep their
  `TEXT.HQR` text; every new label comes in the six languages, as step 2 says. Key names, numbers
  and More options' tuning rows stay as they are. A text is translated where it is drawn and
  measured, so every width the layout takes is the translated one.
- **Every window fits two lines in every language.** Measured on the 1997 game's own `TEXT.HQR`,
  its translations of a sentence run 1.1 times the English typically and up to 1.5 times
  (German) in their longest tenth, so English is not held to a budget; each window is measured
  in all six languages instead, at the narrowest place it is shown.
- **Text too wide swings, as 1997 did** (decided 2026-11-12). `DrawOneChoice`
  ([SOURCES/GAMEMENU.CPP](../../SOURCES/GAMEMENU.CPP), "Shake Baby, shake...") draws a row wider
  than the menu from its left, clipped, and under the cursor slides it until its end shows and
  back (`InitShakeChoice`, 48 pixels a second). lba2cc already does this for long save names.
  It is a fallback, so a translation that does not fit can still be read: text that fits stays
  the rule, and the layout check still fails text that does not.

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
   choosing a preset in the menu does, is open question 10. A binding that takes a key another
   action already has is refused, as the keyboard page refuses it (`CheckKeyConfig`,
   [SOURCES/CONFIG.CPP](../../SOURCES/CONFIG.CPP)), and the import says which ones it skipped.
   Settings backups (EPmager, Discussion #782) save and load such files from the menu.
5. **Readable names** for bindings (#61), read alongside the scancodes.

Point 2 and the stored preset change what an existing file means, which is a core contract
(principle 2), so together they start from one Decision. The migration they need: an old file has
no `Preset` key, so it is on Modern, and the first write in the new form keeps a copy of the old
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

Not a promise; the direction the steps above should not close off. The prototype draws points
2, 3 and 5.

1. **Pages are data.** As in the page model above.
2. **The UI scales to the screen.** A screen 3:2 or wider takes one 960x540 composition at the
   largest scale it fits, centred both ways, as 1997 centred its 640x480 canvas: 1280x720 is
   1.33x, 1440x900 1.5x, 1920x1080 2x, 2560x1440 2.67x, 3840x2160 4x. A narrower screen (4:3,
   5:4) takes the one 640x480 column at `max(1, min(W / 640, H / 480))`. 640x480 is 1x and looks
   as today, and at a whole step the menu is drawn exactly as a whole-step-only rule would draw
   it. Between whole steps, frames, plasma and the darkening's edges are scaled linearly, and
   the game font is drawn over them at the screen's resolution: each of its pixels an exact
   square of the scale, a screen pixel it covers in part taking that share of its colour, so
   strokes keep one weight (nearest neighbour was tried and gave strokes of 1 and 2 pixels by
   turns). Zilla Slab is drawn at the scale. The prototype draws this (decided 2026-11-16, from
   EPmager's proposal).
   This is part of finishing HD ([WIDESCREEN.md, Phase 7](../WIDESCREEN.md#phase-7-native-hd):
   the UI is an unscaled 640x480 block there today, and its bitmap fonts item), so it waits on
   the same work. The font between whole steps needs a full-colour text layer drawn over the
   paletted frame before it is shown, as parked HD UI work built for Zilla Slab (a `UI_TEXT`
   layer in the pre-present hook, not published); the menu would be its first user, and dialogue,
   inventory and holomap text can share it. Until that layer exists the menu scales in whole
   steps only: the largest whole step the composition fits, 1280x720 to 1680x1050 then at 1x
   with a border. Also needs a scaled `Font()` (it blits 1:1 today) and scaled plasma; sprites
   already have `ScaleSprite`. A UI scale row (Auto, 1x, 2x,
   3x) lets the player choose, and Auto can use the display's content scale where the platform
   reports one (SDL3 has `SDL_GetDisplayContentScale`). A handheld shows why the row is not
   enough alone: a 7-inch 1280x800 screen gets the composition at 1.33x (1x with whole steps
   only), small at arm's length, and 2x gives a 640x400 canvas. So pages window below 480 logical pixels, as long lists do, or the
   larger scale is out of reach there; 1280x720 at 2x is 360 pixels tall.
3. **The same page lays out by width.** The one column is today's 640x480 menu. The wide
   composition goes one pane at a time, as the 1997 menu goes from a page to a sub-page, with a
   band under the rows holding the help and the keys the selected row takes. Two panes side by
   side were tried: at 960 wide the right pane was 416 pixels, short of the 556 six English
   labels and their values need. The prototype's `docs/UX.md` has the rest.
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

A long list of one kind (resolutions, More options) is a windowed list as the save list is, with
a scroll bar beside it; at 640x480 the help's dialogue window leaves room for few rows, so most
pages scroll there too.

More options is generated from the settings table: every tuning value and Experimental-ring
setting with no row of its own elsewhere, grouped by the table that declares it (the Auto camera
22, the mouse 9, the gamepad 7, the graphics 3), labelled with its console name and help text, in
English, then the From 1997 group. Grouped by category, 38 of the 41 would fall under Controls
and camera. A row in the Experimental ring carries an "experimental" tag after its name and its
help starts "Experimental:" (today `cam_zone_anchor` and `cam_world_anchor`,
[CAMERA.md](../CAMERA.md)); the ring field gives the tag, so a setting that moves inward loses
it with no change to the page. An experiment then needs no hand-made row and no translations. It
is a dense table in the 8x8 font, like the key binding screen.

### Controls

Today keyboard and gamepad are two binding screens, each one 36-row table in the 8x8 font
(`DrawScreenConfig` in [SOURCES/CONFIG.CPP](../../SOURCES/CONFIG.CPP)). Keys and buttons splits
the 36 actions into seven groups (movement, actions, behaviours, weapons, magic and items,
camera, menus), shows one device at a time, and opens on the device used last. A key another
action already has asks first, as choosing a preset does, where the 1997 screen refuses it
(decided 2026-11-11): the refusal named the action but left the player to find it, clear it and
come back. Nothing changes that the player did not choose, and the page never puts one key on
two actions, which would fire both in play; a clash that comes in from an edited `lba2.cfg` is
shown on both actions.

**No binding locks a pad-only player out** (decided 2026-11-11). Start always opens the game
menu, as Esc does on the keyboard: Game menu's pad button is fixed, its help says so, and Start
is never bound to another action, so no answer to the question above can take it. The menu's own
pad buttons are fixed as its keys are: A confirms, B goes back, and the d-pad and left stick
move. Today only the directions are fixed ([SOURCES/MENU_INPUT.CPP](../../SOURCES/MENU_INPUT.CPP)):
the game menu confirms only on Recentre camera, Behaviour action or Enter (`DoGameMenu`), and
opens only on Esc or Menus. Measured 2026-11-11 on main with scripted pad presses, on the
default and `LBA2_GAMEPAD_STICK_LAYOUT` builds: moving both confirm actions off the pad leaves no
pad button that picks a row, and clearing Menus on the pad leaves no way to open the menu. On the
Vita layout, Circle confirms only while it stays on Dodge. A keyboard still gets out; an Xbox,
Switch, Vita or Android TV player does not (an Android TV remote's Back does nothing). Which
button confirms differs by device (Circle on the Vita, A on the east on Nintendo pads), so the
fixed buttons belong in the build-time input profiles of #836. Each device's bindings have
their own reset. Rebound movement keys (WASD, say) do not move the menu cursor, which uses fixed
keys ([MENU.md](../MENU.md#navigation)), and a PC player who rebinds will notice. The 1997 screen
stays.

## Testing against the original

The menu does not touch the game's rules, so the world hash cannot see it. Six checks do:

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
5. **The layout keeps its rules.** The prototype's self test rules (`docs/UX.md`), read from the
   draw record (see [The page model](#the-page-model)) on every page at each size the resolution
   list offers: one cursor on the screen, a pane's rows sharing their edges, each help window
   fitting its lines, nothing in the wide layout's margins, a key name fitting its chip, and the
   rest. The walks run in all six Text languages: every label fits its room, and a drawn text
   with no translation is a problem. A capture says a page changed; a rule says which row is
   wrong and by how many pixels.
6. **The keys go where they say.** Walks that work on any page, since pages are data: every page
   can be reached by keys from the hub; Esc from whatever Enter opened puts the cursor back on the
   row it left; Left then Right on a setting gives its value back; a dimmed row takes no key. A
   pad-only walk offers Start to every pad binding and clears them all, then still reaches
   Options with pad buttons alone.

Each problem found by 5 or 6 comes with the command line that opens the screen it was found on.
Both need game data for the font, as the captures do. The rules are written once, in the
engine's automation, and take the row and group counts from the tables rather than restate them.
The prototype's self test holds them today, with a second copy in Python (`tools/sweep.py`) that
reads the same record from outside as a cross-check, and both still restate some counts (the
actions per group, the More options rows per group); carried over, they read them from the
tables.

The page tables and the config layout are plain data, so most of this needs no game data. The
page tables and the menu's own table can be host tests if they name settings by `lba2.cfg` key
and stay free of engine headers: every label in six languages, every row naming a setting that
exists. The settings tables cannot: three of the four live in files that pull in the engine
(`CONFIG_FILE.CPP`, `FOLLOWCAM.CPP`, `AMBIANCE.CPP`), which is why
`scripts/ci/check-settings-before.py` reads them from the source. Every setting with exactly one
entry in the menu's table is a check of that kind.
Host tests for the writer and the borrow: a written file that reads back to the same values, a
preset applied and then Undo preset giving back every value a preset sets while keeping a
player's setting changed since, a key this build does not know surviving a write, and the binding
writers persisting the player's tables while a replay has borrowed them.

## Open questions

1. Category names. The wide layout's hub uses the same ones: one concept, one name, so a
   name too long for it is shortened everywhere.
2. Image: one choice for the three filters, or three rows (#835)?
3. How many rows invert and sensitivity take: each has two axes and two devices. The prototype
   has one of each.
4. Whether a category page gets its own reset, as a row or as a key in the legend.
5. Whether "Fix retail bugs" reads well to players, since "retail" is a developer's word; if it
   changes, with the step 2 labels.
6. Whether the menu cursor follows rebound movement keys or the page says it does not.
7. The reference file's name, and whether it is written every run or only when the build changes.
8. Who checks the translations of the new labels.
9. Whether the 720 and 1080 rungs of the resolution list stay in the list marked as unfinished,
   or are left out until Phase 7.
10. Whether a `Preset` line in an imported file takes out the player's own values for settings
    presets set, as choosing the preset in the menu does, and whether an imported key equal to
    the new base stays in the file as one the player named. Left alone, a Modern player who
    turned the Auto camera on and imports a Classic file keeps the Auto camera and reads
    "Classic, changed".
11. Whether the main menu and the 1997 key binding screens stop stretching the menu picture too
    (`Image_LoadStretched`), as the Options pages will.
12. Whether Reset settings changes the screen. Resolution and Fullscreen are player's settings,
    which Reset puts back, but putting them back changes the screen under the menu. The
    prototype puts it back.
13. The logo with the cc as lba2cc's game menu title. The traced outline lives in the repo
    (decided 2026-11-18), not built at load from the player's `video.hqr`. The logo belongs to
    the game's rights holders and the README says game art is not redistributable, so it waits
    on 2.21's permission, asked by the maintainer; their answer goes beside the outline and in
    the README's licence section.

## Not in this plan

The inventory, holomap, dialogue and HUD. They share the drawing parts and would follow the
same scale rule, but each is its own piece of work.
