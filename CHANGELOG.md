# Changelog

All notable changes to the LBA2 Classic Community engine fork.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> Engine semver is independent from `NUM_VERSION` (save-file on-disk format,
> see `SOURCES/COMMON.H`). Save-format changes are tracked there, not here.

## [Unreleased]

### Android

- Plugging in or pairing a keyboard no longer closes the game. Android
  restarted the game's screen when a keyboard appeared, and the game quit
  instead of restarting, with no message
  ([#756](https://github.com/LBALab/lba2-classic-community/pull/756)).
- Crash reports from 64-bit Android phones name every frame of the crash. On
  phones with a smaller address space, such as the Galaxy S22+, most frames
  were numbers that matched no library
  ([#756](https://github.com/LBALab/lba2-classic-community/pull/756)).
- Quitting no longer crashes on the way out. The game closed either way, but
  almost every quit aborted in Android's own audio or drawing code and left a
  crash report in the log
  ([#757](https://github.com/LBALab/lba2-classic-community/pull/757)).
- On first launch the game now waits for you to come back from the All files
  access page, however long you take there. It used to stop waiting after
  about 15 seconds on some phones and then report the game data missing. If
  you come back without turning access on, the game now says so instead of
  asking you to copy files that are already there.

### Display & render

- At widths such as 800, 1280 and 1920, walls in front of a magic ball, a
  bonus, a door or another sprite now cover it right up to the sprite's left
  edge; a strip up to 16 pixels wide used to show through. In the two
  labyrinth scenes the bricks at the left edge of the screen are drawn again
  instead of leaving a strip of the same width empty. 640x480 and 1024x768
  were never affected
  ([#763](https://github.com/LBALab/lba2-classic-community/pull/763)).
- Doors and other scenery near the top of the screen indoors no longer lose a
  brick-shaped piece to black. The wall bricks drawn back over them were moved
  down to the top edge of the screen instead of being cut off there, and
  brought the empty opening behind the door with them. It happened at every
  resolution, wherever the camera left a door near the top; recentring the
  camera hid it until next time
  ([#356](https://github.com/LBALab/lba2-classic-community/issues/356)).

### Gameplay

- Options → Advanced options has a new row, **Fix retail bugs**. Some bugs
  are in the original 1997 game, and fixing them changes how it plays. They
  are fixed by default; choose No to play by the original's rules, including
  for fixes added in later versions. Each fix also has its own setting in
  `lba2.cfg` and the console (`fix_` followed by its name); changing one there
  shows the row as Custom
  ([#760](https://github.com/LBALab/lba2-classic-community/issues/760)).
- Two characters, or the camera and a sound, very far apart no longer
  measure as close together, or as no distance at all. Past about 46,000
  units, which only opposite corners of an outdoor scene reach, this port's
  arithmetic overflowed. The 1997 game measured these correctly, and now
  this port does too
  ([#600](https://github.com/LBALab/lba2-classic-community/issues/600)).
- Walking into a slanted edge of the ground outdoors, such as the foot of a
  cliff, slides Twinsen along it again instead of stopping him dead. This
  port's arithmetic overflowed and worked out the slide as nothing. The 1997
  CD release slides here; the 2022 Classic release stops, as this port did.
  Catching on the corners of buildings, rocks and other scenery is a
  separate case and is not changed
  ([#745](https://github.com/LBALab/lba2-classic-community/issues/745)).

### Input & controller

- Holding a direction through the popup for a newly found item no longer
  leaves Twinsen stuck afterwards. With the key still held, Forward did not
  move him and Left or Right turned him at half speed until every direction
  was let go. The 1997 code has the same slip, so this is a retail fix, and
  Fix retail bugs (above) switches it off
  ([#747](https://github.com/LBALab/lba2-classic-community/issues/747)).
- On the Game Controller screen, Left, Right and Tab on the keyboard switch
  between the two binding columns, as they do on the keyboard screen. The
  second column could only be reached with the pad, so a pad that was not
  working could not be fixed there
  ([#770](https://github.com/LBALab/lba2-classic-community/pull/770)).
- Saving over a save with an automatic name, such as
  `Citadel Island - 2026-07-23 18-32-07`, now renames it to the current
  island and time. It used to keep the place and time of the first save. With
  a keyboard, Enter on that name now saves, and the first letter typed
  replaces it; before, the name could be neither kept nor typed over, so the
  save could not be overwritten without deleting the whole name first. A name
  you typed stays as it is
  ([#736](https://github.com/LBALab/lba2-classic-community/issues/736)).
- When a save name is too wide for the name box, the box now shows the end of
  the name with the text cursor right after it, inside the frame. The cursor
  used to be drawn past the box's right edge, or off screen for the longest
  automatic names
  ([#779](https://github.com/LBALab/lba2-classic-community/issues/779)).
- In fullscreen the mouse cursor now stays inside the game window. The cursor
  is invisible in play, so a click between camera drags could land on another
  monitor and pause the game. Alt-Tab and Cmd-Tab still switch away. The new
  `MouseConfine` setting (`mouse_confine` in the console) also confines it in
  a window (2) or switches it off (0)
  ([#726](https://github.com/LBALab/lba2-classic-community/issues/726)).
- With free orbit (`MouseCameraDrag` 0), the mouse leaves the camera's
  relative mode when you go indoors or open a menu. It used to stay in it,
  because the code that switched it off only ran outdoors.
- On a touch screen, tapping a menu no longer draws the mouse pointer where
  the finger lands. Taps still select and confirm as before, and a mouse
  brings the pointer back. Using a mouse after touching the screen also hides
  the on-screen touch buttons, as a keyboard or controller already did.

### Music

- Coming back from the main menu after a scene's music has finished leaves the
  music silent, as the 1997 game does, instead of starting the last track again
  partway through. Opening the menu more than once during a track also resumes
  it where it was, instead of jumping back
  ([#427](https://github.com/LBALab/lba2-classic-community/issues/427)).

### Stability

- Opening the inventory with the conch empty, or down to its last charge, no
  longer crashes the game. The empty bar under the conch is a box whose right
  edge sits one pixel left of its left edge: the original draws nothing for
  it, and this port kept drawing until it crashed
  ([#762](https://github.com/LBALab/lba2-classic-community/pull/762)).
- A clover leaf reviving Twinsen reads back its snapshot of the scene in the
  layout it was written in, instead of guessing, which could crash in a scene
  with very few characters
  ([#772](https://github.com/LBALab/lba2-classic-community/issues/772)).

### Documentation

- [docs/SCRIPTS.md](docs/SCRIPTS.md) describes the compiled Life and Track
  scripts every scene object runs: where they sit in `SCENE.HQR`, how jumps and
  conditions are encoded, and the operands of every opcode.
  `scripts/dev/scene_disasm.py` lists a cube's scripts as text, finds every use
  of an opcode, and with `--check` decodes all of them. Every script in the GOG,
  Steam, US CD and demo data decodes. `--game lba1` does the same for LBA1, and
  the page lists how LBA1's scripts differ
  ([#181](https://github.com/LBALab/lba2-classic-community/issues/181)).

### Contributors

Thanks to EPmager for reporting the black bricks, and for a screenshot from the
current build that showed where to look. Thanks to EPmager as well for
reporting that a direction held through a found-item popup left Twinsen stuck
([#747](https://github.com/LBALab/lba2-classic-community/issues/747)).
Thanks to EPmager for reporting that the main menu restarted finished music
partway through
([#427](https://github.com/LBALab/lba2-classic-community/issues/427)).
Thanks to EPmager for reporting that a click outside the window during a
camera play test paused the game
([#726](https://github.com/LBALab/lba2-classic-community/issues/726)).
Thanks to EPmager for reporting that Twinsen catches on edges outdoors, and
for checking it against the CD release
([#745](https://github.com/LBALab/lba2-classic-community/issues/745)).

## [0.13.0] - 2026-10-18

### Android: updating from 0.12.0 erases your saves, once

- Android will not install this version over 0.12.0 or any earlier build. It
  says *"App not installed as package conflicts with an existing package"*, and
  the only way through is to uninstall the old app first. **The uninstall erases
  your saves and settings.** Earlier builds kept them in the app's private
  storage, which nothing but the app itself can read on a phone that is not
  rooted, so there is no way to copy them out first. On a rooted phone there
  is, and [docs/ANDROID.md](docs/ANDROID.md#updating-without-losing-your-progress)
  says how.
- Game data in `/sdcard/lba2cc/`, where the guide told you to put it, survives
  the uninstall. Game data in `/sdcard/Android/data/org.lbalab.lba2cc/files/`
  does not: move it to `/sdcard/lba2cc/` before you uninstall.
- This is the last time. Each earlier APK was signed with a throwaway key, so
  Android treated every release, and the two architectures of one release, as
  different apps. From this version every APK is signed with the same key, so
  the next version installs over this one. Rolling `latest` builds downloaded
  since 2026-09-08 already carry it.
- Saves and settings now live in `/sdcard/lba2cc/user/LBA2/`, beside the game
  data, where a file manager or a PC over USB can reach them and an uninstall
  leaves them alone. Grant All Files Access when the game asks on first launch:
  without it the game cannot write there and falls back to a folder the
  uninstall erases. Granting it later carries those saves across on the next
  launch
  ([#660](https://github.com/LBALab/lba2-classic-community/pull/660)).

### Known issues

- The 32-bit Android build (`armeabi-v7a`) still crashes loading most scenes.
  Of 25 checkpoint saves tested on a 64-bit phone running it, 17 crash while
  their scene loads and 7 resume. New Game reaches the first scene; how far a
  game gets from there has not been measured. The 64-bit build (`arm64-v8a`)
  is not affected: use it on any device that runs it. A device with a 32-bit
  kernel, such as a TV box, has not been tested
  ([#684](https://github.com/LBALab/lba2-classic-community/issues/684),
  [#685](https://github.com/LBALab/lba2-classic-community/pull/685)).

### Game data & disc images

- A rip of a retail CD plays as it is, soundtrack included, where 0.12.0 read
  only GOG's disc image. A folder holding only a `.bin` and a `.cue` counts as
  the game folder wherever the game looks for one, Android's `/sdcard/lba2cc/`
  included, and `--disc <file>` names an image directly. The CD themes play
  from the US and Brazilian Activision and the European EA pressings,
  split-track and big-endian rips included. An ISO has no room for them, so
  rip to BIN/CUE. `disc` in the console shows what was mounted and where each
  theme comes from
  ([#460](https://github.com/LBALab/lba2-classic-community/pull/460), [#462](https://github.com/LBALab/lba2-classic-community/pull/462), [#472](https://github.com/LBALab/lba2-classic-community/pull/472), [#675](https://github.com/LBALab/lba2-classic-community/pull/675)).
- `scripts/dev/disc_extract.py` writes a playable game-data folder out of a
  disc image, cutting the CD audio into the theme files the game looks for,
  and `--from-drive` reads the soundtrack straight off a disc in a drive. It is
  for a physical disc, a container the game mounts without audio (MDX, NRG,
  CCD), or loose files to mod; a bin/cue rip does not need it
  ([#471](https://github.com/LBALab/lba2-classic-community/pull/471)).
- Game data whose file names mix upper and lower case (`ObjFix.HQR`) is found
  on a case-sensitive filesystem, where the boot used to stop as if the files
  were missing. Messages about missing files name them as they ship
  (`LBA2.HQR`)
  ([#699](https://github.com/LBALab/lba2-classic-community/pull/699)).
- An AppImage started from a file manager finds the game data in the folder it
  sits in, `Common/` included. It used to look only in the working directory
  ([#486](https://github.com/LBALab/lba2-classic-community/pull/486)).
- Running the game from inside an install whose data sits in `Common/` finds it
  without `--game-dir`. It used to depend on the scan of the parent folder,
  which stops after 24 entries and so could miss the install on a large Steam
  library. Reported from a Steam Deck
  ([#476](https://github.com/LBALab/lba2-classic-community/pull/476)).
- When the folder you run from has no game data and the game boots an install
  it found next door, it now warns and names it. A demo folder beside a retail
  one used to load the retail data without a word
  ([#479](https://github.com/LBALab/lba2-classic-community/pull/479)).
- An `LBA2_GAME_DIR` that does not hold the game data is no longer passed over
  for whichever install turns up next. The game tries a `Common/` inside it,
  then opens the folder picker, or stops a headless run with an error naming
  the variable. The boot banner's `Assets:` line says which probe found the
  data
  ([#469](https://github.com/LBALab/lba2-classic-community/pull/469)).

### Camera

- The Auto camera (off by default) behaves differently around buildings,
  scripted camera shots and scenery. Each change is a setting, on by default;
  with all of them off the camera is as it was, apart from the fixes below.
  [docs/CAMERA.md](docs/CAMERA.md) lists them with their `lba2.cfg` keys.
  - Coming out of a building, the camera starts straight behind the hero
    instead of off to one side. Between two outdoor scenes a script changes,
    the pan and tilt the player set are kept (`cam_scene_carry`).
  - A scripted shot that fires once no longer takes the view while the player
    is steering (`cam_zone_skip`).
  - Leaving a scripted shot, the camera eases back to the angle and tilt the
    player had instead of keeping the shot's (`cam_zone_return`).
  - The camera tilts over a building or a bridge behind the hero only while it
    is in the way, then eases back. It used to keep the steep tilt for good
    (`cam_obstruct`, `cam_obstruct_hurry`)
    ([#701](https://github.com/LBALab/lba2-classic-community/pull/701)).
  - The camera comes round behind the hero about twice as quickly, the pace of
    the 2022 Classic release, and ends exactly behind him instead of up to a
    degree short (`cam_ease_ms`: 300, or 600 for the old pace). The point it
    looks at trails the hero and catches up, so he leads the frame a little
    while he runs (`cam_target_ms`)
    ([#702](https://github.com/LBALab/lba2-classic-community/pull/702)).
  - A scripted shot taking the view, and handing it back, is a 0.6 second
    blend instead of a cut (`cam_shot_blend_ms`). A sign that names a shot
    shows it at once and blends back when its message closes
    ([#708](https://github.com/LBALab/lba2-classic-community/pull/708), [#709](https://github.com/LBALab/lba2-classic-community/pull/709)).
  - Letting go of the right stick or the mouse eases the camera's turn to a
    stop instead of halting it (`cam_glide`)
    ([#512](https://github.com/LBALab/lba2-classic-community/pull/512)).
- `cam_world_anchor`, experimental and off by default: the Auto camera holds a
  direction in the world instead of turning with the hero. With
  `cam_hold_angle 0` it comes back behind him as he walks, paced by the
  distance walked (`cam_world_return`)
  ([#703](https://github.com/LBALab/lba2-classic-community/pull/703)).
- `cam_zone_anchor`, experimental and off by default: inside the box of a
  scripted shot that fires once, the Auto camera holds the shot's angle, as
  the classic camera does
  ([#701](https://github.com/LBALab/lba2-classic-community/pull/701)).
- With the Auto camera on, a scripted shot that aims the view once as the hero
  walks in no longer slides back behind him within a second. It keeps the
  view, as under the classic camera
  ([#521](https://github.com/LBALab/lba2-classic-community/pull/521)).
- With the Auto camera holding its angle, turning Twinsen on the spot and then
  touching the right stick or the mouse no longer tears the view round, 75
  degrees in one frame in one measured case. The camera turns from where it is
  ([#512](https://github.com/LBALab/lba2-classic-community/pull/512)).
- The console's mouse and stick camera settings (`cam_mouse*`, `cam_stick*`,
  `pad_deadzone`, `cam_smooth`) are held to the ranges `lba2.cfg` holds them
  to; `cam_mouse_div 0` divided by zero. `cam_manual_hold` is capped at 600
  frames: far above that, nudging the camera brought its tilt assist back at
  full strength
  ([#544](https://github.com/LBALab/lba2-classic-community/pull/544), [#704](https://github.com/LBALab/lba2-classic-community/pull/704)).
- `zonelist` prints all seven of a zone's `Info` values and its flags, so a
  scripted shot's angle, pitch and distance show, and it names escalator and
  rail zones. [docs/ZONES.md](docs/ZONES.md) says what each zone type reads
  from those values
  ([#539](https://github.com/LBALab/lba2-classic-community/pull/539)).

### Display & render

- `TextureFilter` in `lba2.cfg` (`gfx_texfilter` in the console,
  `LBA2_TEXFILTER` for one run) smooths the square texels of terrain, sea and
  sky seen up close: 1 blends neighbouring texels across, 2 in both
  directions, 0 (the default) draws as before. 2 costs about 7% of frame time
  on terrain at 1728x1080
  ([#452](https://github.com/LBALab/lba2-classic-community/pull/452)).
- `LBA2_ISO_DIV=2`, up to 4, draws interiors at a half, up to a quarter, of the
  chosen resolution and stretches them to the window, so a room fills a large
  screen. Outdoor scenes keep the full resolution, and the Display menu shows
  the chosen one indoors too. It is read once at launch
  ([#452](https://github.com/LBALab/lba2-classic-community/pull/452), [#714](https://github.com/LBALab/lba2-classic-community/pull/714)).
- `DitherShading` in `lba2.cfg` (`gfx_dither`), off by default, dithers the
  terrain's shading so the steps between light levels become a gradient
  ([#455](https://github.com/LBALab/lba2-classic-community/pull/455)).
- `PresentFilter` in `lba2.cfg` (`gfx_presentfilter`, `LBA2_PRESENT_FILTER`)
  chooses how the frame is stretched to the window: nearest (the default),
  linear, or pixelart, which keeps pixels sharp and blends only their seams.
  Pixelart needs SDL 3.4 or newer
  ([#692](https://github.com/LBALab/lba2-classic-community/pull/692)).
- On a scaled display, a Mac's Retina panel for one, the game draws at the
  panel's own resolution instead of at half of it, enlarged and blurred by the
  system
  ([#694](https://github.com/LBALab/lba2-classic-community/pull/694)).
- The resolutions below 640x480 that the console and `--resolution` accept
  (320x200, 320x240, 400x300) no longer corrupt memory on every scene load and
  UI screen, and the inventory and the letterboxed dialogue box start at the
  screen's left edge. The UI is still cut off at these sizes, and the console
  says so
  ([#555](https://github.com/LBALab/lba2-classic-community/pull/555), [#566](https://github.com/LBALab/lba2-classic-community/pull/566)).
- GOG installs start fullscreen, as Steam installs did. GOG's settings file
  never mentions the setting, so it fell through to a windowed default. An
  existing settings file is left alone
  ([#654](https://github.com/LBALab/lba2-classic-community/pull/654)).
- Moving between menu pages no longer flashes white for a frame on the menu
  with the sea behind it
  ([#524](https://github.com/LBALab/lba2-classic-community/pull/524)).

### Input & controller

- Below about 60 frames per second a held throw or sword attack never got
  going: the magic ball never left Twinsen's hand and the sword combo broke.
  The fixed timestep's catch-up steps read the held button as let go; it stays
  held across them now
  ([#458](https://github.com/LBALab/lba2-classic-community/pull/458)).
- Skipping the reply after choosing an answer in a conversation, with the menu
  key (F10, Esc or the pad's Start), no longer opens the main menu as well
  ([#511](https://github.com/LBALab/lba2-classic-community/pull/511)).
- Keypad Enter confirms a save name instead of typing a capital X, and keys
  that are not characters (arrows, Home, End, F2 and others) no longer type
  letters there. Keypad Enter also works at the prompt asking for the game
  disc
  ([#505](https://github.com/LBALab/lba2-classic-community/pull/505), [#506](https://github.com/LBALab/lba2-classic-community/pull/506)).
- A `lba2.cfg` with every keyboard and gamepad binding unset left the game
  unplayable, with no way to reach the screen to rebind them. The game now puts
  the default layout back, and says so in the log
  ([#453](https://github.com/LBALab/lba2-classic-community/pull/453)).

### Saves, settings & profiles

- `--user-dir <dir>` (or `LBA2_USER_DIR`) chooses the folder a run writes its
  saves, settings and log to. That folder used to be fixed per user, so every
  install on a machine shared one
  ([#482](https://github.com/LBALab/lba2-classic-community/pull/482)).
- `--profile <name>` (or `LBA2_PROFILE`) keeps a run's saves and settings under
  a name of their own, in `profiles/<name>/`. A profile remembers the install
  it was first given, and only `--bind-game-dir` moves it. Without a profile
  the game writes where it always did
  ([#482](https://github.com/LBALab/lba2-classic-community/pull/482), [#496](https://github.com/LBALab/lba2-classic-community/pull/496)).
- An empty `portable.txt` beside the binary makes the tree self-contained: the
  user folder becomes `User/<build>/` next to the binary, so a copy on a USB
  stick carries its saves. A `profile:` line in it names a default profile
  ([#482](https://github.com/LBALab/lba2-classic-community/pull/482)).
- The boot banner's `Writes:` line names the profile and how its folder was
  chosen. The `Release` line names the release the config declares and what
  the game data measures as, so a mismatch is reported. An install with no
  config takes the answer from its data, so American data now gets the
  American sprites and disc label
  ([#482](https://github.com/LBALab/lba2-classic-community/pull/482), [#487](https://github.com/LBALab/lba2-classic-community/pull/487)).
- `ShowDistribLogo` in `lba2.cfg`, or `distrib logo <on|off>` in the console,
  turns the publisher splash on or off. It is on when a config declares a
  release and off when nothing does
  ([#487](https://github.com/LBALab/lba2-classic-community/pull/487), [#489](https://github.com/LBALab/lba2-classic-community/pull/489)).
- The config shipped with the game data is read under the player's own instead
  of being copied into it on first run. A user folder first run against one
  release and later pointed at another kept the first one's splash, new-game
  sprite and CD voice folder
  ([#482](https://github.com/LBALab/lba2-classic-community/pull/482)).
- Saves an install ships in its `SAVE/` folder are handed over when a save
  folder is first created, which makes the 1997 demo's three playable sections
  loadable
  ([#482](https://github.com/LBALab/lba2-classic-community/pull/482)).
- A Spanish retail install no longer starts in French, and nor does a config
  naming a language the game cannot read: that now means English, and is
  logged. `--language Español` works from a Windows command prompt
  ([#464](https://github.com/LBALab/lba2-classic-community/pull/464)).
- Flags meant for one run no longer change the setting for good:
  `--resolution`, `--fixed-timestep` and `--language` used to write themselves
  into `lba2.cfg`. A change made in a menu or with a console command is still
  kept. The new `--vsync <on|off>` works the same way
  ([#492](https://github.com/LBALab/lba2-classic-community/pull/492), [#494](https://github.com/LBALab/lba2-classic-community/pull/494), [#501](https://github.com/LBALab/lba2-classic-community/pull/501)).
- The volumes are console settings, `snd_master`, `snd_music`, `snd_wave`,
  `snd_voice` and `snd_cd` from 0 to 127, and `snd_reverse_stereo` swaps the
  channels. A change takes effect at once and is kept, as one made in the menu
  is
  ([#570](https://github.com/LBALab/lba2-classic-community/pull/570)).
- `cam_hd on`, and `on` or `yes` given to any other on/off setting the console
  shares with `lba2.cfg`, no longer switches the setting off
  ([#704](https://github.com/LBALab/lba2-classic-community/pull/704)).
- On Windows, with the game on one drive and the user folder on another,
  `save\shoot\` and `recordings\` are created, so screenshots and recordings
  can be written
  ([#613](https://github.com/LBALab/lba2-classic-community/pull/613)).
- On macOS and Android, a save named without a keyboard on an island with an
  accent in its name (in French, German, Spanish or Portuguese) is written
  again. It was silently dropped while the menu said "Game saved". Save files
  now get plain ASCII names, while the save list still shows the accented
  name, and a save that cannot be written says "Save failed"
  ([#741](https://github.com/LBALab/lba2-classic-community/pull/741)).
- The game no longer rewrites the modification time of installed `.VOX` voice
  files, which the 1997 CD voice cache reads to pick what to delete, and the
  cache skips the game folder
  ([#484](https://github.com/LBALab/lba2-classic-community/pull/484)).

### Recording & replay

- `--record <name>` writes a play session to a file, and `--replay <name>`
  plays it back into the same simulation and reports the first tick where the
  two stop matching, so a bug found in play can be handed over as a file that
  reproduces it. `--record-telemetry` adds every value behind the per-tick
  digest, so a divergence names the field that moved. Recording from boot
  needs `--fixed-dt`, and [docs/RECORDING.md](docs/RECORDING.md) says why
  ([#595](https://github.com/LBALab/lba2-classic-community/pull/595), [#602](https://github.com/LBALab/lba2-classic-community/pull/602), [#604](https://github.com/LBALab/lba2-classic-community/pull/604), [#633](https://github.com/LBALab/lba2-classic-community/pull/633)).
- `rec start|stop|play|info` in the console records and replays from wherever
  you are, with no flags. `rec play` takes the latest recording and puts you
  back where you were when it ends. Recordings go in `recordings/` beside
  `save/`, and the boot banner names the folder
  ([#610](https://github.com/LBALab/lba2-classic-community/pull/610), [#615](https://github.com/LBALab/lba2-classic-community/pull/615), [#616](https://github.com/LBALab/lba2-classic-community/pull/616), [#617](https://github.com/LBALab/lba2-classic-community/pull/617)).
- A replay boots into the state its recording began from, so it needs no
  `--load`, and carries on through the in-game menu. One that waits for input
  the recording does not hold, or covers too little of the file, says so
  instead of reporting a match
  ([#643](https://github.com/LBALab/lba2-classic-community/pull/643), [#657](https://github.com/LBALab/lba2-classic-community/pull/657), [#659](https://github.com/LBALab/lba2-classic-community/pull/659), [#663](https://github.com/LBALab/lba2-classic-community/pull/663)).
- A replay runs under the settings its recording was made with and gives yours
  back when it ends; `--replay-own-settings` keeps yours and reports the ones
  that differ. `settings_before [prefix]` sets settings to the value that
  switches off what they added: `settings_before cam_` is the Auto camera
  without its later behaviours
  ([#704](https://github.com/LBALab/lba2-classic-community/pull/704), [#705](https://github.com/LBALab/lba2-classic-community/pull/705)).
- `scripts/dev/dump_recording.py` reads a recording without the engine: its
  header, keyframes, console commands, clock, analog input and telemetry, and
  the savegames it carries
  ([#605](https://github.com/LBALab/lba2-classic-community/pull/605), [#622](https://github.com/LBALab/lba2-classic-community/pull/622)).
- Linux and Windows play a scene out the same way. Random numbers came from
  each platform's `rand()`, which differ; the engine now carries its own
  generator, reproducing the sequence Linux already had, so a recording
  replays on the other platform too
  ([#596](https://github.com/LBALab/lba2-classic-community/pull/596)).
- Ambient sound no longer changes what actors do. Under `--fixed-dt`, and
  while recording or replaying, whether a sample is still playing is answered
  from its length on the game clock, so a run with sound on is reproducible
  ([#595](https://github.com/LBALab/lba2-classic-community/pull/595)).
- A replay gets through the save-name screen and saves under the name the
  session typed. That screen now reads the same input as the rest of the game,
  as do the cheat codes at the pause menu, and a recording carries which device
  the player last used, which decides whether the screen offers typing
  ([#646](https://github.com/LBALab/lba2-classic-community/pull/646), [#649](https://github.com/LBALab/lba2-classic-community/pull/649), [#651](https://github.com/LBALab/lba2-classic-community/pull/651)).
- Under `--fixed-dt`, and so while recording, the game clock no longer runs at
  double speed while the console is open, and the "game saved" message, Game
  Over, slideshows and logo screens end on their own instead of waiting for a
  key. Turning a page of the slate no longer freezes the game
  ([#595](https://github.com/LBALab/lba2-classic-community/pull/595), [#641](https://github.com/LBALab/lba2-classic-community/pull/641)).

### Console & scripting

- Commands for driving the game from a script: `key` holds a key down, menus
  and dialogue included; `stick <x> <y>` and `mouse <dx> <dy>` move the right
  stick and the mouse as the devices do; `camnudge` turns the Auto camera;
  `behaviour` and `weapon` read or set Twinsen's
  ([#458](https://github.com/LBALab/lba2-classic-community/pull/458), [#511](https://github.com/LBALab/lba2-classic-community/pull/511), [#512](https://github.com/LBALab/lba2-classic-community/pull/512), [#607](https://github.com/LBALab/lba2-classic-community/pull/607)).
- Commands for watching it: `camtrace 1` logs the camera once a frame,
  whichever camera holds the view, with the look-at point, the eases, blends
  and shots in progress and the obstruction tilt; `objtrace <n>` logs one
  object's position, angles, animation, track and behaviour; `inputflow`
  counts input on both sides of each layer it crosses, so a lost press shows
  where it was lost; `dumpstate [path]` writes the `--dump-state` JSON on
  demand, now with a camera block
  ([#503](https://github.com/LBALab/lba2-classic-community/pull/503), [#512](https://github.com/LBALab/lba2-classic-community/pull/512), [#517](https://github.com/LBALab/lba2-classic-community/pull/517), [#551](https://github.com/LBALab/lba2-classic-community/pull/551), [#594](https://github.com/LBALab/lba2-classic-community/pull/594), [#701](https://github.com/LBALab/lba2-classic-community/pull/701), [#702](https://github.com/LBALab/lba2-classic-community/pull/702), [#708](https://github.com/LBALab/lba2-classic-community/pull/708)).
- `flags` lists the quest state a save carries: the chapter, then every named
  game variable that is set, such as the diploma and the four dark-monk keys
  (`flags all` includes the zeros). `vargame <n>` names the variable
  ([#584](https://github.com/LBALab/lba2-classic-community/pull/584)).
- `--listen <port>` takes console commands from a socket on 127.0.0.1 while
  the game runs, with `scripts/dev/lba2ctl.py` as the client and `stream on`
  sending it the trace logs. It runs any command, so it is only in builds
  configured with `-DLBA2_CONTROL_SERVER=ON`
  ([#551](https://github.com/LBALab/lba2-classic-community/pull/551)).
- `skipmodals 1` also skips the screen that shows an item being picked up, so
  a scripted run that finds one no longer stops there
  ([#556](https://github.com/LBALab/lba2-classic-community/pull/556)).

### Stability

- Closing the holomap after zooming into an island's plan no longer crashes the
  game, which it did on every island but the Citadel
  ([#676](https://github.com/LBALab/lba2-classic-community/pull/676)).
- Ordinary actions that read or wrote outside their memory, which could crash
  the game or corrupt what it drew later: opening the options screen in a
  language whose menu texts are long, opening the holomap after changing
  scene, opening the game menu in a scene that animates ten textures, and
  entering one GOG scene whose map is a byte larger than its header declares
  ([#552](https://github.com/LBALab/lba2-classic-community/pull/552), [#553](https://github.com/LBALab/lba2-classic-community/pull/553)).
- Drawing the scenery after a change of scene no longer reads past a block
  when a scene's script places a piece naming a brick the block does not hold
  ([#575](https://github.com/LBALab/lba2-classic-community/pull/575)).
- A mod or repack that compresses a picture's palette, in `SCREEN.HQR` for
  one, no longer crashes the game when a scene shows that picture with a
  message. No shipped release compresses one
  ([#678](https://github.com/LBALab/lba2-classic-community/pull/678)).
- A crash writes a `CRASH` block to `adeline.log` with the signal, the build,
  the game state and the call stack, and the next launch keeps that log as
  `adeline.prev.log`. On Android the system's own crash record is saved beside
  it as `crash-<time>.pb`. Tagged releases attach a `-symbols.tar.xz` per
  artifact, which `scripts/dev/symbolize_crash.py` uses to turn a block into
  functions and lines
  ([#682](https://github.com/LBALab/lba2-classic-community/pull/682), [#683](https://github.com/LBALab/lba2-classic-community/pull/683)).

### Android

- The on-screen controls show while you use them, where they were drawn over
  every screen, logos, menus and cutscenes included. They are full while a
  finger is down, dim about 1.5 seconds after it lifts, and go after about 6,
  or as soon as a pad or a key is used. The first touch after a pad only wakes
  them
  ([#655](https://github.com/LBALab/lba2-classic-community/pull/655)).
- The first launch of a new install no longer stops at *"The graphics mode
  848x480 could not be set"* while Android asks for All Files Access; the game
  waits for the answer and for its window
  ([#662](https://github.com/LBALab/lba2-classic-community/pull/662)).
- The bar across the top of the screen with the app's name and version is
  gone, and the game always runs fullscreen. Android has no fullscreen row in
  the Display menu, so a windowed setting from an original-disc or GOG install
  could not be undone
  ([#654](https://github.com/LBALab/lba2-classic-community/pull/654)).
- The APKs are about a quarter of their old size, 6.5 MB on arm64 where 0.12.0
  was 23 MB. They no longer carry the compiler's debug info; the symbol tables
  stay
  ([#682](https://github.com/LBALab/lba2-classic-community/pull/682)).

### Build & packaging

- `-DLBA2_BUILD_DEMO=ON` builds the 1997 playable demo, to run on the demo's
  own data: three fixed save slots, no cutscenes, and its own
  `Twinsen/LBA2-Demo` folder, so it never follows a retail install
  ([#474](https://github.com/LBALab/lba2-classic-community/pull/474)).
- `-DPLATFORM_BACKEND=` chooses what the game is built on: `sdl3`, the default
  and what every release ships; `sdl2`, a desktop build on SDL 2.0.22 or newer,
  sound included, with no folder picker; or `null`, which needs no SDL at all
  and draws offscreen. See
  [docs/PLATFORM_ABSTRACTION.md](docs/PLATFORM_ABSTRACTION.md)
  ([#688](https://github.com/LBALab/lba2-classic-community/pull/688), [#689](https://github.com/LBALab/lba2-classic-community/pull/689), [#691](https://github.com/LBALab/lba2-classic-community/pull/691)).
- The game runs on a Nintendo Switch as homebrew, on the SDL2 backend.
  `scripts/dev/build-switch.sh` builds it in Docker, no SDK to install, into
  a ZIP to extract onto the SD card; the retail game data goes in
  `sdmc:/lba2/`, and saves and settings stay in `sdmc:/switch/lba2cc/user/`.
  Releases do not ship it yet. `scripts/dev/run-switch.sh` sends a build to
  the Homebrew Menu's NetLoader over the network and shows its log live. See
  [docs/SWITCH.md](docs/SWITCH.md)
  ([#716](https://github.com/LBALab/lba2-classic-community/pull/716)).
- The Linux tarball, macOS and Android builds bundle SDL 3.4.18 (was 3.2.16),
  so the pixelart present filter works in them. The Windows build and the
  AppImage use the SDL of the system they are built on
  ([#697](https://github.com/LBALab/lba2-classic-community/pull/697)).
- The AppImage carries AppStream metainfo, so software centres show a summary,
  licence and category instead of a bare file name, and its desktop entry
  lists categories that exist. The macOS About box shows a copyright line
  ([#488](https://github.com/LBALab/lba2-classic-community/pull/488)).
- Rolling `latest` AppImages no longer replace themselves with the newest
  stable release on first run; they follow the rolling channel. A rolling
  download made before this needs replacing once by hand
  ([#475](https://github.com/LBALab/lba2-classic-community/pull/475)).
- CI gained ASan and UBSan runs of the host tests, a clang build, a
  compiler-warning baseline, shell lint, architecture-boundary checks, a check
  that doc references resolve, an Android native build on every push and pull
  request, and retries on every download
  ([#515](https://github.com/LBALab/lba2-classic-community/pull/515), [#520](https://github.com/LBALab/lba2-classic-community/pull/520), [#523](https://github.com/LBALab/lba2-classic-community/pull/523), [#532](https://github.com/LBALab/lba2-classic-community/pull/532), [#546](https://github.com/LBALab/lba2-classic-community/pull/546), [#573](https://github.com/LBALab/lba2-classic-community/pull/573), [#587](https://github.com/LBALab/lba2-classic-community/pull/587), [#590](https://github.com/LBALab/lba2-classic-community/pull/590), [#591](https://github.com/LBALab/lba2-classic-community/pull/591)).
- `scripts/dev/verify-release.sh` checks every artifact a release publishes:
  the Windows ZIP's version info, the DMG's `Info.plist` and libraries, the
  APKs' manifest, signature and 16 KB alignment, each AppImage's `.zsync` and
  update channel, and that every `--version` matches its filename. A missing
  tool, Docker daemon or qemu binfmt costs only the checks that need it
  ([#475](https://github.com/LBALab/lba2-classic-community/pull/475), [#480](https://github.com/LBALab/lba2-classic-community/pull/480), [#481](https://github.com/LBALab/lba2-classic-community/pull/481), [#490](https://github.com/LBALab/lba2-classic-community/pull/490)).
- `./run_tests_docker.sh` works in a clone that has been built in, and stops
  at once when `docker` cannot reach a daemon
  ([#536](https://github.com/LBALab/lba2-classic-community/pull/536), [#537](https://github.com/LBALab/lba2-classic-community/pull/537)).

### Documentation

- The camera guide is split in two: [docs/CAMERA.md](docs/CAMERA.md) for
  players, every setting in one table, and
  [docs/CAMERA_INTERNALS.md](docs/CAMERA_INTERNALS.md) for how it works
  ([#710](https://github.com/LBALab/lba2-classic-community/pull/710)).
- New references: camera and other zones
  ([docs/ZONES.md](docs/ZONES.md)), recording
  ([docs/RECORDING.md](docs/RECORDING.md)), the speedrun movement techniques
  as the engine produces them
  ([docs/SPEEDRUN_MECHANICS.md](docs/SPEEDRUN_MECHANICS.md)), the version
  numbers in the code ([docs/VERSIONS.md](docs/VERSIONS.md)), bug hunting
  ([docs/BUG_HUNTING.md](docs/BUG_HUNTING.md)) and the external tools
  ([docs/TOOLING.md](docs/TOOLING.md))
  ([#473](https://github.com/LBALab/lba2-classic-community/pull/473), [#535](https://github.com/LBALab/lba2-classic-community/pull/535), [#539](https://github.com/LBALab/lba2-classic-community/pull/539), [#557](https://github.com/LBALab/lba2-classic-community/pull/557), [#583](https://github.com/LBALab/lba2-classic-community/pull/583), [#595](https://github.com/LBALab/lba2-classic-community/pull/595)).
- A knowledge bundle, [docs/knowledge/](docs/knowledge/index.md): what is true
  about the engine and why, one concept per file, each citing the code, in
  slices from input and movement to the camera
  ([#667](https://github.com/LBALab/lba2-classic-community/pull/667), [#671](https://github.com/LBALab/lba2-classic-community/pull/671), [#677](https://github.com/LBALab/lba2-classic-community/pull/677), [#681](https://github.com/LBALab/lba2-classic-community/pull/681), [#711](https://github.com/LBALab/lba2-classic-community/pull/711)).
- Plans and research moved to `docs/plan/`
  ([#532](https://github.com/LBALab/lba2-classic-community/pull/532)).

### Contributors

Thanks to Gartonio and EPmager for reporting on
[Discord](https://discord.gg/jsTPWYXHsh) that updating the Android app lost
their saves. That report is why saves now live where an update cannot reach
them and every release is signed with one key
([#660](https://github.com/LBALab/lba2-classic-community/pull/660)).

EPmager kept field-testing through this cycle, on Windows with a pad. His
reports are behind the world-anchored Auto camera
([#351](https://github.com/LBALab/lba2-classic-community/issues/351)), the
blend into and out of scripted shots
([#361](https://github.com/LBALab/lba2-classic-community/issues/361)), the
right stick snapping the camera round
([#450](https://github.com/LBALab/lba2-classic-community/issues/450)), the
main menu opening when skipping a line after a choice
([#451](https://github.com/LBALab/lba2-classic-community/issues/451)), keypad
Enter in the save-name box
([#497](https://github.com/LBALab/lba2-classic-community/issues/497)), the
main menu's extra blink
([#410](https://github.com/LBALab/lba2-classic-community/issues/410)) and the
Display menu showing the interior render size
([#499](https://github.com/LBALab/lba2-classic-community/issues/499)). He
helped pin down the magic ball report
([#456](https://github.com/LBALab/lba2-classic-community/issues/456)), and
reported what is still open: the camera clipping into scenery
([#363](https://github.com/LBALab/lba2-classic-community/issues/363)), the
first bat's jumping flight path, which `objtrace` was built to chase
([#498](https://github.com/LBALab/lba2-classic-community/issues/498)) and the
dither's checkerboard
([#500](https://github.com/LBALab/lba2-classic-community/issues/500)). A full
speedrun he recorded and sent in shaped the recording format and its dump
tool
([#622](https://github.com/LBALab/lba2-classic-community/pull/622),
[#633](https://github.com/LBALab/lba2-classic-community/pull/633)).

Thanks to [@sergiou87](https://github.com/sergiou87) for the platform layer:
an SDK-free host contract with the SDL3 backend behind it, a null backend that
builds the whole game without SDL
([#688](https://github.com/LBALab/lba2-classic-community/pull/688)), a
desktop SDL2 backend with audio on top of it
([#691](https://github.com/LBALab/lba2-classic-community/pull/691)), and the
Nintendo Switch port built on that
([#716](https://github.com/LBALab/lba2-classic-community/pull/716)).

Welcome to three new contributors since v0.12.0:
[@makseq](https://github.com/makseq) for texture filtering and scaled
interiors
([#452](https://github.com/LBALab/lba2-classic-community/pull/452)), recovery
from a config with every binding unset
([#453](https://github.com/LBALab/lba2-classic-community/pull/453)), dithered
terrain shading
([#455](https://github.com/LBALab/lba2-classic-community/pull/455)) and the
choosable present filter that landed as
[#692](https://github.com/LBALab/lba2-classic-community/pull/692);
[@iibaranov-IG](https://github.com/iibaranov-IG) for finding disc images on
Android
([#675](https://github.com/LBALab/lba2-classic-community/pull/675)); and
[@ColumbusLabs](https://github.com/ColumbusLabs) for the Display menu showing
the chosen resolution indoors
([#714](https://github.com/LBALab/lba2-classic-community/pull/714)).

Thanks to [@saisilcastro](https://github.com/saisilcastro) for reporting the
magic ball and attacks not firing
([#456](https://github.com/LBALab/lba2-classic-community/issues/456)), to
[@JeremyCB](https://github.com/JeremyCB) for confirming the fix on macOS, and
to [@aurelienfalco](https://github.com/aurelienfalco) for reporting game data
going unfound when its file names mix upper and lower case
([#687](https://github.com/LBALab/lba2-classic-community/issues/687)).

Thanks to [@xesf](https://github.com/xesf) for the Steam Deck install the game
could not find
([#476](https://github.com/LBALab/lba2-classic-community/pull/476)), and to
Gartonio for the title bar and the first-launch failure on a Retroid Pocket 5
([#654](https://github.com/LBALab/lba2-classic-community/pull/654),
[#662](https://github.com/LBALab/lba2-classic-community/pull/662)) and the
holomap crash on an Android handheld
([#676](https://github.com/LBALab/lba2-classic-community/pull/676)).

## [0.12.0] - 2026-07-16

### Display & resolution

v0.11.0 went wide. This one goes high: the render-height ceiling
lifts from the original 480 lines to 1080p, and the rest of the
renderer follows it up there.

- Resolution picker in the Display submenu, computed per monitor, lifting
  the render-height ceiling to 1080p. The list is derived from your
  display aspect (the `640x480` classic anchor plus a ladder of
  aspect-matched modes at 480/720/1080 heights), so every non-anchor
  entry fills the panel with no letterbox or pillarbox: a 16:9 panel
  offers `640x480, 848x480, 1280x720, 1920x1080`, an odd-aspect laptop
  its own widths. Picking one applies live and persists to `lba2.cfg`;
  the active mode carries a green plasma marker distinct from the cursor
  highlight
  ([#341](https://github.com/LBALab/lba2-classic-community/pull/341)).
  See [docs/RUNTIME_RESOLUTION.md](docs/RUNTIME_RESOLUTION.md).
- First-launch widescreen auto-detect: with no `--resolution` and no
  saved resolution in `lba2.cfg`, the engine boots a widescreen size at
  the original 480 height matched to the display aspect (for example
  `848x480` on 16:9); a 4:3 display keeps `640x480`. The auto value is
  never persisted, so it follows a monitor swap or a dock/undock, and any
  explicit choice (menu, console, `--resolution`) takes over
  ([#323](https://github.com/LBALab/lba2-classic-community/pull/323)).
- Full-screen videos scale to fit the window instead of floating small in
  a wide black frame. The 320x200 cinematics take the largest uniform
  scale that fills the frame, centred and boxed. At 640x480 that factor
  is exactly 2.0, so both settings are byte-identical there and the
  difference only shows at widescreen. A previously dead option in the
  options menu now drives it, reading "Movies fit to screen" (the new
  default) or "Movies at original size" (the classic 2x letterbox). One
  tradeoff worth knowing: engine cinema mode draws 40px top and bottom
  bars, which the original setting matches and fit does not, so bar
  geometry changes going from a cutscene into an FMV and back
  ([#430](https://github.com/LBALab/lba2-classic-community/pull/430)).
- Actor shadows draw below line 480 at HD vertical resolutions. Hero and
  NPC ground shadows vanished in a band at the bottom of the screen
  exactly `ModeResY - 480` tall: 240 lines at 720p, 600 at 1080p. The
  shadow span table was sized to 480
  ([#443](https://github.com/LBALab/lba2-classic-community/pull/443)).
- Sliding doors no longer leave a ghost of themselves behind in the
  doorway at widescreen resolutions. The on-screen test for
  background-baked objects used hardcoded 640x480 bounds, so an object
  outside that rect never registered as drawn and the redraw that
  un-bakes it never fired. The dead zone grew with width: nothing at
  640x480, the right ~20% at 848x480, the right ~47% and bottom ~19% at
  1280x720
  ([#432](https://github.com/LBALab/lba2-classic-community/pull/432)).
- VSync toggle in the Display submenu and a `vsync [on|off|toggle]`
  console verb, persisted via a `VSync` key in `lba2.cfg` and surviving a
  runtime resolution switch. The on-screen FPS counter is now responsive
  and guarded against a freeze
  ([#307](https://github.com/LBALab/lba2-classic-community/pull/307)).
- The 2D UI re-anchors on the Y axis as well: the authored 480-tall
  canvas now floats into a taller framebuffer (centre, bottom, or corner
  per surface) instead of pinning to the top, completing the vertical
  half of the widescreen re-anchor
  ([#328](https://github.com/LBALab/lba2-classic-community/pull/328)).
  The FPS counter pins to the bottom corner at any resolution
  ([#310](https://github.com/LBALab/lba2-classic-community/pull/310)).
  See [docs/WIDESCREEN.md](docs/WIDESCREEN.md).
- The framebuffer now presents through SDL's logical presentation
  (LETTERBOX) instead of a hand-computed destination rect, so SDL owns
  both the on-screen present rect and the window-to-framebuffer cursor
  mapping. The forward and inverse transforms can no longer drift apart,
  and the path is DPI-correct groundwork for high-density HD rendering.
  The present geometry was first extracted into a small SDL-free,
  host-tested module
  ([#325](https://github.com/LBALab/lba2-classic-community/pull/325), [#326](https://github.com/LBALab/lba2-classic-community/pull/326)).

### Movement & timing

Movement in the engine was never speed times time. Animation keyframes
advanced once per rendered frame, so how far Twinsen actually walked
depended on your frame rate. This cycle decouples the simulation from
rendering and pins it to a fixed step.

- Frame-rate-independent movement, on by default at a 16 ms step (a
  60 Hz simulation). Above roughly 60 fps (a high-refresh panel, vsync
  off, a fast machine, even at the classic 640x480) hero walk, sneak and
  jump and NPC patrols ran too slowly or inconsistently, and slow movers
  could stop outright; below 60 fps the simulation lost game time and
  movement came up short. It now advances in whole fixed steps with the
  remainder carried, sub-stepping to catch up after a slow frame and
  clamping at 8 steps so it stops trying below about 8 fps. At 60 fps
  every part of this is a deliberate no-op, so a display already in the
  sweet spot sees no change in feel. Turn it off or retune it live with
  the `fixedtimestep` console command, persisted to `lba2.cfg`
  ([#388](https://github.com/LBALab/lba2-classic-community/pull/388), [#389](https://github.com/LBALab/lba2-classic-community/pull/389)).
  See [docs/MOVEMENT_FRAMERATE.md](docs/MOVEMENT_FRAMERATE.md).
- Quest-item gives, scene flips and weapon input land reliably under the
  throttle. A one-frame event (an inventory item-use, a full-scene flip,
  a hero action or fire edge) promotes a frame that would have been
  skipped into a single simulation step, so the event cannot fall into
  the gap between steps. Held movement is deliberately excluded: that is
  state the throttle is right to skip
  ([#392](https://github.com/LBALab/lba2-classic-community/pull/392), [#411](https://github.com/LBALab/lba2-classic-community/pull/411), [#413](https://github.com/LBALab/lba2-classic-community/pull/413)).
- The holomap globe rotates at a constant rate rather than one
  proportional to frame rate, so holding a direction no longer spins it
  away on a high-refresh display
  ([#390](https://github.com/LBALab/lba2-classic-community/pull/390)).
- The end-credits walkers travel at a constant rate and stay staggered
  instead of bunching, in both the attract-reel credits and the ending
  ([#421](https://github.com/LBALab/lba2-classic-community/pull/421)).

### Transitions & fades

Scene changes and full-screen videos are bracketed by a fade through
black. Under SDL most of that had stopped working, in five separate
ways.

- Scene and FMV fades ramp again instead of cutting straight through,
  with actors no longer popping in at full brightness. The palette ramp
  was timed off the audio sample-fade countdown, which runs on wall-clock
  time and keeps advancing across the synchronous scene load, so the fade
  finished in a single step. It now runs on the managed clock
  ([#374](https://github.com/LBALab/lba2-classic-community/pull/374)).
- Actors, NPCs, doors and item drops fade with the scene instead of
  snapping. On the way out they vanished the instant the fade began,
  before the scene darkened; on the way in the terrain faded up without
  them and they appeared at full brightness a frame after the fade
  finished. Only the terrain was fading. One fix covers scene loads, FMV
  reveals, menu fades and white flashes
  ([#417](https://github.com/LBALab/lba2-classic-community/pull/417)).
- Lit-to-lit palette crossfades ramp instead of snapping to the target
  palette. About ten scenes fire one on entry, mostly island arrivals
  (Citadel harbor, White Leaf Desert city center, Otringal harbor,
  Francos), so it shows in the title-screen attract reel
  ([#418](https://github.com/LBALab/lba2-classic-community/pull/418)).
- The next scene no longer flashes at full brightness for a frame before
  fading in at the end of an FMV, and the video fades out at its tail
  again. Both were worse the higher your frame rate
  ([#415](https://github.com/LBALab/lba2-classic-community/pull/415)).
- The letterbox slides out at the end of a cutscene instead of sticking
  on screen, sometimes indefinitely if you stood still, and then popping
  ([#375](https://github.com/LBALab/lba2-classic-community/pull/375)).

See [docs/TRANSITIONS.md](docs/TRANSITIONS.md).

### Music

- A scene's music plays out and then hands over, instead of being
  deferred forever or cut mid-phrase. Entering an area while a different
  track was still looping left the new area silent until a menu toggle
  nudged it; the queued track now takes over once the current one
  finishes, and the chain keeps working across successive areas.
  Scripted music, cutscenes, FMVs and menus still switch immediately, as
  before
  ([#376](https://github.com/LBALab/lba2-classic-community/pull/376), [#420](https://github.com/LBALab/lba2-classic-community/pull/420)).
  See [docs/MUSIC.md](docs/MUSIC.md).

### Camera

The Auto camera (the opt-in `FollowCamera` third-person follow, off by
default, exterior scenes only) filled out further this cycle.

- Hold-angle, on by default: the camera keeps whatever angle you set
  with the stick or mouse instead of drifting back behind the hero once
  the grace window expires, which is how a free third-person camera
  behaves. It re-centres only on a scene change or an explicit Center
  camera. `cam_hold_angle 0` restores the classic drift-back; the
  `lba2.cfg` key is `FollowCamHoldAngle`
  ([#348](https://github.com/LBALab/lba2-classic-community/pull/348)).
- Manual camera control with a mouse: right-drag orbits and tilts the
  follow camera, the wheel zooms, and middle-click recenters behind the
  hero. All three axes run through one shared `ApplyManualCameraNudge`
  routine that clamps each axis and re-engages the follow grace period,
  so the source (mouse now, stick next) doesn't matter. Supersedes the
  earlier mouse-camera work
  ([#337](https://github.com/LBALab/lba2-classic-community/pull/337)).
  See [docs/CAMERA.md](docs/CAMERA.md).
- The gamepad right stick drives that same orbit and tilt through the
  shared routine, so the feel (clamps, tight orbit lerp, follow
  re-engage) matches the mouse. Per-axis deadzone and sensitivity, with
  the analog path suppressing the digital `RSTICK_*` scancodes so it
  never double-drives the camera
  ([#338](https://github.com/LBALab/lba2-classic-community/pull/338)).
  See [docs/CONTROLLER.md](docs/CONTROLLER.md).
- Auto-camera world awareness, so the follow camera reads the scene the
  way the classic camera and a human at the stick do. An HD vertical-FOV
  recompose pulls the boom in and steepens pitch in proportion to render
  height, so a taller framebuffer no longer fills the frame with sky; a
  smooth port of the classic `SearchCameraPos` lifts the eye over terrain
  between the camera and the hero; and a manual-override fade hands
  control back gradually after a manual pan. Scoped to the auto-cam
  exterior path (the classic camera, the 480 path, and isometric
  interiors are byte-identical); the 1080p recompose is flagged
  experimental
  ([#343](https://github.com/LBALab/lba2-classic-community/pull/343)).
- Live camera tuning from the console: `cam_*` cvars expose the follow
  feel constants (stick speed, re-center, distance, pitch) at runtime and
  persist to `lba2.cfg`, so the camera can be tuned without a recompile
  ([#339](https://github.com/LBALab/lba2-classic-community/pull/339)).
- `FollowCamera` fixes: it no longer re-renders the full terrain on every
  idle frame, and a rotation dead-zone gap that kept an idle exterior
  from settling is closed
  ([#308](https://github.com/LBALab/lba2-classic-community/pull/308)).
- The exterior near-clip plane now scales with render height, so HD modes
  no longer clip scene geometry early
  ([#342](https://github.com/LBALab/lba2-classic-community/pull/342)).

### Render

- Character eyes and other small round body features are the right size
  in interior and isometric scenes. They rendered a pixel too big, which
  read as oversized square billboards on Grobo and firefly-tart models
  indoors while outdoors was correct. The isometric branch of `Sphere()`
  had mistranslated the original assembly's negate as a bitwise NOT,
  dropping the `+1`. The exterior path was already correct and is
  byte-for-byte unchanged
  ([#380](https://github.com/LBALab/lba2-classic-community/pull/380)).

### Input & controller

- The gamepad is a first-class input source instead of a pass bolted on
  after the fact. It was OR'd into `Input` after `GetInput()` had already
  rebuilt the state and applied the repeat filter, which broke two
  things: holding camera recenter (pad B) stopped Twinsen dead, fatal in
  the continuous protopack sections where he then drowns, and a held
  menu, inventory or behaviour button re-opened the overlay instead of
  closing it. Bindings now fold into the same key and mask table the
  keyboard builds. Keyboard behaviour is unchanged
  ([#346](https://github.com/LBALab/lba2-classic-community/pull/346)).
- The end credits can be skipped and exited with a controller. It was
  the last screen still blind to the pad. Sticks and triggers are
  deliberately excluded, since a resting stick drifts past the deadzone
  and would end the credits by accident
  ([#367](https://github.com/LBALab/lba2-classic-community/pull/367)).
- The D-pad is no longer aliased to the left stick and can be bound
  independently. It no longer turns Twinsen; by default it casts the
  four spells (Protection, Jetpack, Pingouin, Foudre). Movement stays on
  the left stick and the triggers, and every action is rebindable to the
  D-pad in the controls menu
  ([#373](https://github.com/LBALab/lba2-classic-community/pull/373)).
  See [docs/CONTROLLER.md](docs/CONTROLLER.md).

### Saves

- Saves are portable across 32-bit and 64-bit builds and byte-compatible
  with retail. A 64-bit build wrote its own native struct layout, so its
  saves did not match retail's on disk; writes now go through a fixed
  32-bit wire layout. Nothing you already have is invalidated: retail
  32-bit saves load directly, and existing 64-bit saves from a community
  build load through a native-first read and migrate on the next save.
  The on-disk format version is unchanged
  ([#386](https://github.com/LBALab/lba2-classic-community/pull/386)).
  See [docs/SAVEGAME.md](docs/SAVEGAME.md).
- The per-scene autosave is back. Every genuine scene transition writes a
  fresh `autosave.lba` checkpoint again, closing the gap where a crash or
  a hard quit after crossing several scenes without opening the pause
  menu dropped you back at your last menu visit. Console `cube` warps and
  menu loads do not set it, so they cannot clobber your autosave
  ([#349](https://github.com/LBALab/lba2-classic-community/pull/349)).
- The in-game quick save shows the "Game saved" confirmation and plays
  the chime. It was writing the save correctly and saying nothing about
  it, which looked like it had done nothing
  ([#347](https://github.com/LBALab/lba2-classic-community/pull/347)).

### Disc image support

- The engine can read retail assets straight from a raw ISO/BIN disc
  image, so a GOG DRM-free Original Edition install (`LBA2.GOG`) plays
  with no extraction step: `VIDEO.HQR`, the music WAVs, and the `.VOX`
  voices that live only inside the BIN are resolved through the mounted
  image, including the external cue audio track. The image is detected by
  its ISO9660 content (the `CD001` volume descriptor), not by name, so a
  `.gog`, a raw `.bin`/`.iso`, or a `.cue`/`.dat` pair all work, and the
  reader is asset-source-agnostic for later LBA1 reuse
  ([#322](https://github.com/LBALab/lba2-classic-community/pull/322)).
  See [docs/DISC_IMAGE_SOURCE.md](docs/DISC_IMAGE_SOURCE.md).

### Console & harness

- F12 console keyboard overhaul: command history on Up/Down, Page Up/Down
  and the wheel for scrollback, mid-line editing (Left/Right/Home/End/
  Delete with a blinking cursor), Tab completion over commands, cvars and
  built-ins, clipboard shortcuts, a two-stage Esc, and character entry
  routed through SDL text input
  ([#331](https://github.com/LBALab/lba2-classic-community/pull/331)).
- Text selection with copy: Shift+arrows and Ctrl+A in the input line,
  mouse drag and double-click across the scrollback, the OS cursor shown
  while the console is open, right-click to copy, and a click in the game
  area closes the console
  ([#332](https://github.com/LBALab/lba2-classic-community/pull/332)).
- Hardening from an architecture review: a single CP437/Unicode table
  replacing two hand-synced switch tables, a loud table-overflow, and an
  input test seam. The mouse-selection, input-line, and render-overlay
  statics were then grouped into structs with centralized teardown
  ([#333](https://github.com/LBALab/lba2-classic-community/pull/333), [#334](https://github.com/LBALab/lba2-classic-community/pull/334), [#335](https://github.com/LBALab/lba2-classic-community/pull/335), [#336](https://github.com/LBALab/lba2-classic-community/pull/336)).
- Intentional blank lines are preserved in the console output
  ([#330](https://github.com/LBALab/lba2-classic-community/pull/330)).
- The control harness can drive and observe a playthrough headlessly,
  which is contributor-facing tooling with no gameplay effect. It gained
  state control (`teleport`, `varcube`, `vargame`, and cube vars in
  `--dump-state`), observability (`lifetrace` for what a Life script
  checks and does, `cubetrace` for scene-to-scene flow), input
  (`useitem`, and `input` for held hero input, metered in simulation
  ticks so a hold survives the throttle), a way past blocking dialogue
  modals (`skipmodals`), and zone geometry (`zonelist`), so a script can
  drive to a trigger rather than guess at it
  ([#393](https://github.com/LBALab/lba2-classic-community/pull/393), [#395](https://github.com/LBALab/lba2-classic-community/pull/395), [#396](https://github.com/LBALab/lba2-classic-community/pull/396), [#397](https://github.com/LBALab/lba2-classic-community/pull/397), [#398](https://github.com/LBALab/lba2-classic-community/pull/398), [#399](https://github.com/LBALab/lba2-classic-community/pull/399), [#400](https://github.com/LBALab/lba2-classic-community/pull/400), [#401](https://github.com/LBALab/lba2-classic-community/pull/401), [#438](https://github.com/LBALab/lba2-classic-community/pull/438)).
- The engine refuses a command line it would otherwise misread. A typo'd
  flag used to fall through, boot at defaults and exit 0, handing back a
  confident wrong answer; unknown flags and missing values now exit 2.
  One arity-aware table drives both validation and the usage text, which
  is split into `--help` (a short player-facing subset) and `--help-all`
  (the full grouped list including automation and self-test flags).
  `lba2cc --help` used to open a window and sit there
  ([#435](https://github.com/LBALab/lba2-classic-community/pull/435)).
- Harness runs are reproducible and no longer overwrite your saves.
  The engine pauses on focus loss, which is right for a player and
  silently wrong for automation: six identical parallel runs produced
  five distinct simulation states. `--headless` pins the SDL dummy
  drivers, `--no-autosave` suppresses the implicit save writers, and
  `--exec-at <tick>` schedules console commands on a given tick. A
  `--load` that resolves to nothing now fails the run instead of
  quietly booting a fresh game
  ([#436](https://github.com/LBALab/lba2-classic-community/pull/436)).
  See [docs/CONSOLE.md](docs/CONSOLE.md) and
  [docs/CONTROL.md](docs/CONTROL.md).

### Config & stability

- A corrupt `lba2.cfg` no longer costs you your saved resolution. On
  Windows a garbage run at the head of the file printed three
  `DefFileBufferInit: buffer too small` lines at boot and dropped the
  display back to auto-detect despite a saved `ResolutionX`/`ResolutionY`.
  No setting was ever lost, only unread. Over-long separator-less lines
  are now excised in place and the boot read buffer grows from 8 KB to
  64 KB. The underlying cause, that the config buffer aliases the shared
  graphics scratch buffer so an interrupted write can stamp graphics
  bytes into the file, is tracked separately
  ([#441](https://github.com/LBALab/lba2-classic-community/pull/441)).
- An explicit `--game-dir` that doesn't hold the game data is refused
  with a message naming what it wanted, instead of being silently
  discarded so the engine boots a different install and mentions it only
  in the banner. An explicit path now also probes a `Common/` inside it,
  matching how auto-discovery already probes `data/` and `game/`.
  Separately, a batch or headless run can no longer hang forever on the
  folder picker, and the recovery message points at the `README.txt` a
  release ships rather than a file that only exists in the source tree
  ([#437](https://github.com/LBALab/lba2-classic-community/pull/437)).
- First-launch data-discovery dialogs are shorter and correct per
  platform. The "couldn't open folder picker" box used to dump a
  per-distro Linux install matrix into a modal on every platform,
  including macOS and Windows, and Android had no path through the flow
  at all (SDL's file dialog hands back a `content://` URI, not a path).
  Each box is now one statement plus the action, and Android gets a
  self-contained copy-your-data message
  ([#345](https://github.com/LBALab/lba2-classic-community/pull/345)).
  See [docs/GAME_DATA.md](docs/GAME_DATA.md).
- A global log level, defaulting to INFO, gates the log file, the
  terminal and the F12 console together, set with `--log-level`, the
  `LBA2_LOG_LEVEL` environment variable, or the `loglevel` console
  command. Debug output used to be hardwired on. The stderr sink always
  emits, plain and severity-tagged when redirected, so a headless or CI
  run is legible without opening `adeline.log`
  ([#385](https://github.com/LBALab/lba2-classic-community/pull/385)).
- Atomic, crash-safe config writes. Every save funnelled through an
  `O_TRUNC` open on the live `lba2.cfg` and re-spliced the in-memory
  buffer back out across several writes, leaving a corruption window open
  about 30 times per settings change. Writes now go to a temp file with
  `fsync` and an atomic rename, batched into a single fsync and rename
  per save, so a crash or power loss can no longer truncate the config
  ([#340](https://github.com/LBALab/lba2-classic-community/pull/340)).
- HQR caches are sized in bytes rather than MiB. `AvailableMem()` returns
  MiB but `AdjustHQRMem()` treated it as bytes, so every cache clamped to
  its minimum regardless of installed RAM. At its minimum the
  island-object cache held about one decor body, reloading a body per
  visible object in object-dense exterior islands
  ([#321](https://github.com/LBALab/lba2-classic-community/pull/321)).
- VOC detection now gates on the 19-byte signature instead of the first
  byte, which is not a reliable discriminator (standard VOC starts with
  `C`, LBA voice banks with `0x00`, some LBA1 entries with `0x01`). The
  parser was extracted into a pure, SDL-free module pinned by a host test
  ([#320](https://github.com/LBALab/lba2-classic-community/pull/320)).
- The Windows console is set to UTF-8 so the boot banner renders
  ([#309](https://github.com/LBALab/lba2-classic-community/pull/309)), and
  the boot-log banner and section lines are bold cyan
  ([#329](https://github.com/LBALab/lba2-classic-community/pull/329)).
- Harness screenshots capture `Log` (the presented buffer) rather than
  `Screen`
  ([#313](https://github.com/LBALab/lba2-classic-community/pull/313)).

### Build & packaging

- `patchelf` added to the AppImage build
  ([#318](https://github.com/LBALab/lba2-classic-community/pull/318) by
  [@Samueru-sama](https://github.com/Samueru-sama)).
- clang-format is pinned to major 18 from a single source of truth. It
  was pinned to 17 in four places, each silently falling back to an
  unversioned `clang-format` (18 or newer on a current toolchain), so the
  fallback was the common path and contributors formatted with one major,
  passed the local hook, then failed CI. No reformatting churn: the tree
  was already clean under 18
  ([#444](https://github.com/LBALab/lba2-classic-community/pull/444)).
- CI pins SDL3 in the test image to `release-3.2.16` after a stale-image
  drift
  ([#324](https://github.com/LBALab/lba2-classic-community/pull/324)).

### Documentation

- New subsystem references, several written off the back of the bugs
  they explain: the movement and frame-rate model
  ([docs/MOVEMENT_FRAMERATE.md](docs/MOVEMENT_FRAMERATE.md)), the music
  state machine ([docs/MUSIC.md](docs/MUSIC.md)), the scene and FMV
  fade-in reveal system ([docs/TRANSITIONS.md](docs/TRANSITIONS.md)), and
  the text engine ([docs/TEXT.md](docs/TEXT.md)), whose format claims are
  verified by decoding the retail `TEXT.HQR` and which pins why that file
  is frozen: voice lines are keyed positionally, so reordering a bank
  desyncs every line after it across 39 voice files
  ([#388](https://github.com/LBALab/lba2-classic-community/pull/388), [#379](https://github.com/LBALab/lba2-classic-community/pull/379), [#416](https://github.com/LBALab/lba2-classic-community/pull/416), [#431](https://github.com/LBALab/lba2-classic-community/pull/431)).
- A shared definition of "bit exact"
  ([docs/BIT_EXACTNESS.md](docs/BIT_EXACTNESS.md)). The phrase appeared
  across some twenty docs with no agreed meaning, which let it both block
  and justify changes by assumption; it now splits into three jobs with
  different rules (format contract, ASM-parity oracle, regression
  tripwire)
  ([#414](https://github.com/LBALab/lba2-classic-community/pull/414)).
- Plans of record: the portable save format
  ([docs/plan/SAVE_WIRE_PLAN.md](docs/plan/SAVE_WIRE_PLAN.md)), harness input
  simulation ([docs/plan/INPUT_SIM_PLAN.md](docs/plan/INPUT_SIM_PLAN.md)), and
  render interpolation
  ([docs/plan/RENDER_INTERP_PLAN.md](docs/plan/RENDER_INTERP_PLAN.md)), the last
  parked on merge because the fixed-timestep work already fixed the
  problem it targeted
  ([#386](https://github.com/LBALab/lba2-classic-community/pull/386), [#438](https://github.com/LBALab/lba2-classic-community/pull/438), [#439](https://github.com/LBALab/lba2-classic-community/pull/439), [#440](https://github.com/LBALab/lba2-classic-community/pull/440)).
- An index of the 39 tracked build, CI and packaging scripts, each row
  naming what runs it ([scripts/README.md](scripts/README.md))
  ([#446](https://github.com/LBALab/lba2-classic-community/pull/446)).
- The widescreen docs are reconciled against the code after drifting
  behind it: HD-height render correctness is closed, and the remaining
  Phase 7 work is legibility rather than correctness
  ([#445](https://github.com/LBALab/lba2-classic-community/pull/445)).
- New references and plans from earlier in the cycle: an
  isometric-renderer hardcoded-pixel audit
  ([docs/ISO_SPACE_AUDIT.md](docs/ISO_SPACE_AUDIT.md)), an in-place
  Platform Abstraction Layer audit and extraction plan
  ([docs/plan/PLATFORM_PAL_PLAN.md](docs/plan/PLATFORM_PAL_PLAN.md)), a plan for
  hosting LBA1 on the lba2cc engine
  ([docs/plan/LBA1_PORT_PLAN.md](docs/plan/LBA1_PORT_PLAN.md)), the gamepad camera
  controls ([docs/CONTROLLER.md](docs/CONTROLLER.md)), and a note on the
  video seam's two halves
  ([#312](https://github.com/LBALab/lba2-classic-community/pull/312), [#317](https://github.com/LBALab/lba2-classic-community/pull/317), [#319](https://github.com/LBALab/lba2-classic-community/pull/319), [#327](https://github.com/LBALab/lba2-classic-community/pull/327)).

### Contributors

A huge thank you to EPmager, whose playtesting shaped most of this
release. Thirteen of the bugs fixed here started as his reports and
feedback on [Discord](https://discord.gg/jsTPWYXHsh), across nearly
every area the release touches: frame-rate-dependent movement
([#358](https://github.com/LBALab/lba2-classic-community/issues/358))
and the held-input weapon bugs that came with the fix
([#407](https://github.com/LBALab/lba2-classic-community/issues/407)),
the end-credits walkers
([#403](https://github.com/LBALab/lba2-classic-community/issues/403)),
the mistimed scene and FMV fades
([#404](https://github.com/LBALab/lba2-classic-community/issues/404)),
the cutscene letterbox
([#354](https://github.com/LBALab/lba2-classic-community/issues/354)),
a scene's music going silent
([#355](https://github.com/LBALab/lba2-classic-community/issues/355))
and then hard-cutting
([#406](https://github.com/LBALab/lba2-classic-community/issues/406)),
oversized billboards
([#357](https://github.com/LBALab/lba2-classic-community/issues/357)),
ghost doors
([#424](https://github.com/LBALab/lba2-classic-community/issues/424)),
culled actor shadows
([#408](https://github.com/LBALab/lba2-classic-community/issues/408)),
FMVs not scaling to widescreen
([#428](https://github.com/LBALab/lba2-classic-community/issues/428)),
the D-pad mappings
([#360](https://github.com/LBALab/lba2-classic-community/issues/360)),
and Mr. Bazoo ignoring a Meca-Penguin set off in his shop
([#409](https://github.com/LBALab/lba2-classic-community/issues/409)).
He field-tested the fixes too, on Windows with an Xbox controller and on
Android arm64. That setup is most of the point: a high frame rate with
vsync off, a gamepad, and an arm64 handheld are exactly the conditions a
Linux dev box and a headless CI run never reach, and most of these bugs
are invisible without them. The follow-up regressions the fixed-timestep
work introduced were caught the same way.

Thanks to [@xesf](https://github.com/xesf) for play-testing on the Steam
Deck.

Welcome to a new contributor since v0.11.0:
[@Samueru-sama](https://github.com/Samueru-sama) for the `patchelf`
addition to the AppImage build
([#318](https://github.com/LBALab/lba2-classic-community/pull/318)).

## [0.11.0] - 2026-06-05

### Widescreen & runtime resolution (opt-in)

- Widescreen support, selectable from the new **Display** options submenu
  in the game menu. Alongside the classic 4:3 mode, the menu offers a
  Widescreen mode derived from your monitor's aspect ratio (for example
  16:9 at 848×480). It keeps the game's native 480-line vertical
  resolution and simply reveals more of the scene to the sides, so it
  reads as a natural fit on a modern display without changing gameplay,
  art, or the HUD. The choice persists to `lba2.cfg`, and the same
  submenu now also holds the Windowed/Fullscreen toggle
  ([#237](https://github.com/LBALab/lba2-classic-community/pull/237), [#238](https://github.com/LBALab/lba2-classic-community/pull/238), [#240](https://github.com/LBALab/lba2-classic-community/pull/240), [#241](https://github.com/LBALab/lba2-classic-community/pull/241), [#244](https://github.com/LBALab/lba2-classic-community/pull/244), [#259](https://github.com/LBALab/lba2-classic-community/pull/259), [#290](https://github.com/LBALab/lba2-classic-community/pull/290)). See
  [docs/RUNTIME_RESOLUTION.md](docs/RUNTIME_RESOLUTION.md).
- Arbitrary render sizes are available but experimental: `--resolution
  WxH` at launch or the console `resolution` verb (320×200 to 1920×1024,
  width a multiple of 8), with a 15-second keep/revert safety dialog on a
  live switch. Taller, HD-height modes in particular still have
  unfinished surfaces (terrain and a few HD-only paths), so the curated
  Display-menu modes are the recommended path for now
  ([#212](https://github.com/LBALab/lba2-classic-community/pull/212), [#239](https://github.com/LBALab/lba2-classic-community/pull/239)).
- Under the hood, every 2D surface re-anchored to the live framebuffer
  size instead of a hardcoded 320/240 centre, so menus and overlays stay
  put at any width: the main menu, save/load, config, inventory, in-game
  messages and dialogs, the holomap and holo-globe, the
  behaviour-selector and found-object modals, and the ambient-sound pan.
  PCX cinematic dialogs are letterboxed inside the picture; Smacker
  cutscenes, the Adeline/EA bumper slides, and quest images centre with
  sampled-edge sidebars instead of stretch or garbage
  ([#213](https://github.com/LBALab/lba2-classic-community/pull/213), [#214](https://github.com/LBALab/lba2-classic-community/pull/214), [#216](https://github.com/LBALab/lba2-classic-community/pull/216), [#217](https://github.com/LBALab/lba2-classic-community/pull/217), [#218](https://github.com/LBALab/lba2-classic-community/pull/218), [#219](https://github.com/LBALab/lba2-classic-community/pull/219), [#221](https://github.com/LBALab/lba2-classic-community/pull/221), [#223](https://github.com/LBALab/lba2-classic-community/pull/223), [#224](https://github.com/LBALab/lba2-classic-community/pull/224), [#226](https://github.com/LBALab/lba2-classic-community/pull/226), [#228](https://github.com/LBALab/lba2-classic-community/pull/228),
  [#246](https://github.com/LBALab/lba2-classic-community/pull/246), [#247](https://github.com/LBALab/lba2-classic-community/pull/247), [#248](https://github.com/LBALab/lba2-classic-community/pull/248), [#260](https://github.com/LBALab/lba2-classic-community/pull/260), [#289](https://github.com/LBALab/lba2-classic-community/pull/289)).
- The 3D render pipeline routes through the framebuffer dimensions rather
  than literal 640/480: iso projection setup, brick bounding boxes and
  clip scans, the iso grid origin, Z-buffer allocation, and the vertical
  render extent (previously pinned to 480). Several row-stride and
  thumbnail bugs the hardcoded-dimensions audit surfaced were fixed along
  the way, including garbage rows in 16:9 save thumbnails
  ([#199](https://github.com/LBALab/lba2-classic-community/pull/199), [#200](https://github.com/LBALab/lba2-classic-community/pull/200), [#201](https://github.com/LBALab/lba2-classic-community/pull/201), [#203](https://github.com/LBALab/lba2-classic-community/pull/203), [#204](https://github.com/LBALab/lba2-classic-community/pull/204), [#205](https://github.com/LBALab/lba2-classic-community/pull/205), [#206](https://github.com/LBALab/lba2-classic-community/pull/206), [#209](https://github.com/LBALab/lba2-classic-community/pull/209), [#210](https://github.com/LBALab/lba2-classic-community/pull/210), [#211](https://github.com/LBALab/lba2-classic-community/pull/211), [#215](https://github.com/LBALab/lba2-classic-community/pull/215),
  [#220](https://github.com/LBALab/lba2-classic-community/pull/220), [#222](https://github.com/LBALab/lba2-classic-community/pull/222), [#258](https://github.com/LBALab/lba2-classic-community/pull/258), [#262](https://github.com/LBALab/lba2-classic-community/pull/262), [#291](https://github.com/LBALab/lba2-classic-community/pull/291)). See
  [docs/WIDESCREEN.md](docs/WIDESCREEN.md).

### Android & Android TV (new platform)

- Android and Android TV builds (arm64-v8a and armeabi-v7a). Because the
  engine already runs on SDL3, this was primarily a new build target (an
  NDK + SDL3 Android toolchain and APK packaging), plus a thin Android
  platform layer for what desktop doesn't need: touch input,
  storage-permission handling, the TV leanback launcher path, and 16 KB
  memory-page support for the newer Android page-size requirement. The
  Android specifics sit behind a `LIB386/SYSTEM/ANDROID` seam
  (external-files dir, permission prompt, TV detection, present-path
  staging buffer), so the rest of the engine stays ifdef-free
  ([#261](https://github.com/LBALab/lba2-classic-community/pull/261) by [@Link4Electronics](https://github.com/Link4Electronics);
  [#282](https://github.com/LBALab/lba2-classic-community/pull/282), [#283](https://github.com/LBALab/lba2-classic-community/pull/283), [#284](https://github.com/LBALab/lba2-classic-community/pull/284), [#285](https://github.com/LBALab/lba2-classic-community/pull/285), [#287](https://github.com/LBALab/lba2-classic-community/pull/287)). See [docs/ANDROID.md](docs/ANDROID.md).

### Boot diagnostics & logging

- Structured boot log with file and terminal sinks and an asset-preflight
  check, so a missing or incomplete game-data folder reports what's wrong
  instead of failing opaquely. Fatal boot errors now raise a native
  dialog and exit non-zero. The boot log also feeds an in-engine console
  buffer, starts before game-data discovery, colours its bookends and the
  console, and carries accurate config-bootstrap and gamepad-connect
  messages
  ([#269](https://github.com/LBALab/lba2-classic-community/pull/269), [#270](https://github.com/LBALab/lba2-classic-community/pull/270), [#272](https://github.com/LBALab/lba2-classic-community/pull/272), [#297](https://github.com/LBALab/lba2-classic-community/pull/297), [#298](https://github.com/LBALab/lba2-classic-community/pull/298), [#299](https://github.com/LBALab/lba2-classic-community/pull/299), [#300](https://github.com/LBALab/lba2-classic-community/pull/300)).
- Logging unified behind a single fan-out in `LIB386/SYSTEM`:
  `LogPrintf`/`LogPuts` route through it, display and SDL errors carry
  real severity, format-string risks are closed, and the log core moved
  into LIB386 so the game and engine share one path
  ([#274](https://github.com/LBALab/lba2-classic-community/pull/274), [#276](https://github.com/LBALab/lba2-classic-community/pull/276), [#277](https://github.com/LBALab/lba2-classic-community/pull/277), [#278](https://github.com/LBALab/lba2-classic-community/pull/278), [#279](https://github.com/LBALab/lba2-classic-community/pull/279)).
- A `lba2.cfg` left with embedded NUL bytes (for example a process killed
  mid-rewrite) is now treated as empty on load, so the next write
  regenerates a clean file from defaults instead of propagating the
  corruption on every save
  ([#296](https://github.com/LBALab/lba2-classic-community/pull/296)).

### New features

- GOD and SENDELL cheats, run from the console: `god` makes Twinsen
  ignore combat damage, and `sendell` lets him one-shot anything he hits.
  Run either again to toggle it off. A short, glorious turn as an
  invincible, omnipotent Twinsen
  ([#288](https://github.com/LBALab/lba2-classic-community/pull/288)).
- Exit-screen easter egg on a clean shutdown
  ([#264](https://github.com/LBALab/lba2-classic-community/pull/264)).

### Audio

- Gameplay music now resumes at its previous position when you return
  from the menu instead of restarting, and the resume path no longer
  destroys the SDL stream in the CD-ROM build. Music and samples pause
  while the window is unfocused, and cutscenes freeze on focus loss
  rather than desyncing their audio. Console `audio music pause`/`resume`
  verbs and an `audio global log` trace toggle were added for diagnosis
  ([#303](https://github.com/LBALab/lba2-classic-community/pull/303)). See [docs/AUDIO.md](docs/AUDIO.md).
- The engine no longer exits when the sample-sound driver fails to
  initialise; it continues without sample audio
  ([#233](https://github.com/LBALab/lba2-classic-community/pull/233)).

### Stability & fixes

- Installed voice files are guarded from accidental deletion, with a
  warning when a voice (`.vox`) pack is incomplete
  ([#295](https://github.com/LBALab/lba2-classic-community/pull/295)).
- Save-name text entry switches to the keyboard path only on a physical
  keyboard, so gamepad-only sessions stay on the auto-named-save path
  ([#286](https://github.com/LBALab/lba2-classic-community/pull/286)).
- The SDL window is created fullscreen up front when configured that way,
  instead of starting windowed and toggling
  ([#271](https://github.com/LBALab/lba2-classic-community/pull/271)).
- Timer fix: `LastTime` is held across the `TimerLock` window so a
  render-plus-vsync interval isn't dropped. That regression surfaced
  in the SDL port, now pinned by a unit test
  ([#252](https://github.com/LBALab/lba2-classic-community/pull/252)).

### Developer tooling & automation

- A headless CLI control harness for automation and regression: scripted
  runs, deterministic `--fixed-dt` mode, a `--demo` attract mode with a
  pinned RNG seed, plus `--dump-state`, `--language`, `--no-audio`,
  `--verbose`, save-corpus baselines, and harness speedups (skip the CD
  splash, disable vsync, parallel `--jobs`)
  ([#162](https://github.com/LBALab/lba2-classic-community/pull/162), [#163](https://github.com/LBALab/lba2-classic-community/pull/163), [#164](https://github.com/LBALab/lba2-classic-community/pull/164), [#166](https://github.com/LBALab/lba2-classic-community/pull/166), [#167](https://github.com/LBALab/lba2-classic-community/pull/167), [#169](https://github.com/LBALab/lba2-classic-community/pull/169), [#171](https://github.com/LBALab/lba2-classic-community/pull/171), [#172](https://github.com/LBALab/lba2-classic-community/pull/172), [#173](https://github.com/LBALab/lba2-classic-community/pull/173), [#175](https://github.com/LBALab/lba2-classic-community/pull/175), [#178](https://github.com/LBALab/lba2-classic-community/pull/178),
  [#230](https://github.com/LBALab/lba2-classic-community/pull/230), [#234](https://github.com/LBALab/lba2-classic-community/pull/234), [#249](https://github.com/LBALab/lba2-classic-community/pull/249), [#251](https://github.com/LBALab/lba2-classic-community/pull/251), [#254](https://github.com/LBALab/lba2-classic-community/pull/254)). See [docs/CONTROL.md](docs/CONTROL.md).
- Projection-regression tooling (`projrec`): captures every screen
  coordinate the projection pipeline produces and hashes it into a
  committed baseline, catching projection drift across platforms and the
  widescreen refactor
  ([#193](https://github.com/LBALab/lba2-classic-community/pull/193), [#194](https://github.com/LBALab/lba2-classic-community/pull/194), [#195](https://github.com/LBALab/lba2-classic-community/pull/195), [#197](https://github.com/LBALab/lba2-classic-community/pull/197)).
- UI-capture console verbs (`ui inventory|holomap|dialog|menu-options`)
  that render a UI surface headless to a byte-identical PNG golden,
  including a wide-width differential mode and a `--black-bg` cleanroom
  variant
  ([#182](https://github.com/LBALab/lba2-classic-community/pull/182), [#183](https://github.com/LBALab/lba2-classic-community/pull/183), [#184](https://github.com/LBALab/lba2-classic-community/pull/184), [#185](https://github.com/LBALab/lba2-classic-community/pull/185), [#186](https://github.com/LBALab/lba2-classic-community/pull/186), [#256](https://github.com/LBALab/lba2-classic-community/pull/256), [#257](https://github.com/LBALab/lba2-classic-community/pull/257)).
- `perftrace`: per-frame timing capture as a permanent dev tool
  ([#253](https://github.com/LBALab/lba2-classic-community/pull/253)).

### Documentation

- New architecture references: engine foundations and the LBA1 porting
  surface, an engine file-format reference (HQR / XCF / CD-image
  tooling), and a `CODESTYLE.md`
  ([#263](https://github.com/LBALab/lba2-classic-community/pull/263), [#292](https://github.com/LBALab/lba2-classic-community/pull/292), [#293](https://github.com/LBALab/lba2-classic-community/pull/293), [#294](https://github.com/LBALab/lba2-classic-community/pull/294)).
- [docs/RUNTIME_RESOLUTION.md](docs/RUNTIME_RESOLUTION.md), a
  hardcoded-dimensions audit, and extensive widescreen-campaign notes
  ([#207](https://github.com/LBALab/lba2-classic-community/pull/207), [#227](https://github.com/LBALab/lba2-classic-community/pull/227), [#229](https://github.com/LBALab/lba2-classic-community/pull/229)).
- Plus control-harness, logging, scenes, initialisation, and
  release-process doc updates across the cycle.

### Contributors

Thanks to [@Link4Electronics](https://github.com/Link4Electronics) for
bringing up the initial Android and Android TV build ([#261](https://github.com/LBALab/lba2-classic-community/pull/261)).


## [0.10.0] - 2026-05-17

### New features (opt-in)

- Cross-platform first-launch folder picker: when no retail game data is
  found via the usual discovery paths (`./data`, `../LBA2`,
  `LBA2_GAME_DIR`, `--game-dir`), the engine now prompts for the
  install folder via the native system file dialog instead of exiting
  with an error. Picked path is persisted to a user-config file for
  next run; pass `--pick-game-dir` to force the picker again. Works on
  Windows, macOS, and Linux (zenity / xdg-desktop-portal-* on Linux,
  with explanatory fallback text when no backend is installed)
  ([#107](https://github.com/LBALab/lba2-classic-community/issues/107),
  [#111](https://github.com/LBALab/lba2-classic-community/pull/111)).
- Save/load list now navigable with gamepad D-pad
  ([#79](https://github.com/LBALab/lba2-classic-community/pull/79)).
- Auto-named saves for gamepad users: gamepad input on the save-name
  prompt skips text entry and writes a scene-aware default name in the
  form `<scene> - YYYY-MM-DD HH-MM-SS` (e.g. `Citadel Island - 2026-05-17 14-30-00`).
  The scene portion is localised across the six menu languages (sourced
  from the retail `TEXT.HQR` file 2); the date/time portion is fixed
  `%Y-%m-%d %H-%M-%S`. Long names clip with a marquee in the save list;
  post-save success chime + "Game saved" overlay
  ([#151](https://github.com/LBALab/lba2-classic-community/pull/151)).
- Console `give` command now actually grants inventory items instead of
  only playing the found-object cinematic. It takes an item name
  (`give conch`), with the numeric index still accepted as a fallback;
  run `give` with no arguments to list every item name. Countable items
  accept an optional count (`give gem 5`, `give money 5000`,
  `give clover 3`)
  ([#143](https://github.com/LBALab/lba2-classic-community/pull/143)) —
  see [docs/CONSOLE.md](docs/CONSOLE.md).
- Console `slide` command jumps directly to a distributor/bumper slide
  (Adeline, EA, etc.); companion `distrib` command lists the available
  slide IDs
  ([#116](https://github.com/LBALab/lba2-classic-community/pull/116)).

### Stability & fixes

- `ScaleSprite` scaled path restored. A 2024 ASM→CPP port had stripped
  the scaled-output branches, leaving the function effectively unscaled
  for months. The most visible symptom in-game: world-space sprites that
  should shrink with distance no longer did — the magic ball, for
  example, stayed the same size as it flew away from Twinsen in exterior
  scenes instead of receding with distance. The regression went unnoticed
  because the existing test only varied numerator/x/y, not the scale
  factor. Mirrored back from `SCALESPT` ASM, and the test matrix extended
  to cover factor sweeps
  ([#148](https://github.com/LBALab/lba2-classic-community/pull/148)).
- `TempoRealAngle` extern conflict in savegame load: `DEBUG_TOOLS=ON`
  builds failed to compile because PR #62's stride-retry refactor
  introduced a wrong-typed `extern S32 TempoRealAngle` next to the
  existing file-scope `T_REAL_VALUE_HR` definition. Default builds were
  unaffected (the block is `#if`-gated out). Fix removes the redundant
  extern
  ([#146](https://github.com/LBALab/lba2-classic-community/pull/146)).
- `GereExtras` no longer reads uninitialised old-position memory
  ([#142](https://github.com/LBALab/lba2-classic-community/pull/142)).
- Undefined-behaviour hardening pass from the static-analysis sweep:
  shift-by-32 UB in the `POLYGOUR` dither rotate, signed-overflow in
  `I_WEAPON_7` / `A_FLIP` bit masks, and `TheEndCheckFile` marked
  `noreturn` to close null-deref paths flagged downstream
  ([#140](https://github.com/LBALab/lba2-classic-community/pull/140)).
- `deffile` config parser: signed-`char` EOL desync and self-aliasing
  `strcpy` cleaned up. Bug A was user-reported: a `lba2.cfg` containing
  `Language: Français` (or any other Latin-1 high-bit byte) corrupted on
  successive rewrites — 21 stray `çais` lines spliced in and all 144
  keyboard + gamepad bindings reset to zero. Bug B was an overlapping
  `strcpy(FileName, file)` caught by ASan against the GOG TLBA2 Classic
  package on shutdown. Both were old, both UB. Host-only regression
  test added under `host_quick`
  ([#115](https://github.com/LBALab/lba2-classic-community/issues/115),
  [#132](https://github.com/LBALab/lba2-classic-community/pull/132)).
- Terrain vertex projection: dropped the no-op / off-by-one `+0.5`
  rounding bias that desynced terrain vertex positions from the rest
  of the scene
  ([#145](https://github.com/LBALab/lba2-classic-community/pull/145)).

### Refactoring

- Framebuffer dimensions extracted to `RESOLUTION_X` / `RESOLUTION_Y`
  in a new `LIB386/H/SYSTEM/RESOLUTION.H`. Cinema bars and sort-tree
  preclip now route through `ModeDesiredX/Y` instead of hardcoded
  `640`/`480`. Foundation for the widescreen / higher-resolution
  rendering work
  ([#134](https://github.com/LBALab/lba2-classic-community/pull/134)).

### Game data, config, and UX

- `lba2.cfg` defaults modernised for the open-source era: fullscreen on
  by default (`DisplayFullScreen=1`), Version row defaults tightened to
  match the shipping engine
  ([#117](https://github.com/LBALab/lba2-classic-community/pull/117)).

### Tools

- `scripts/dev/extract_lba2_gog_media.py` extracts the retail media tree
  from the GOG Original Edition `LBA2.GOG` bin/cue image (ISO9660 reader
  with no external `mount`/`isoinfo` dependency). Lets users with the
  GOG install seed `./data` without owning the original CDs
  ([#119](https://github.com/LBALab/lba2-classic-community/issues/119),
  [#123](https://github.com/LBALab/lba2-classic-community/pull/123)).

### Cross-platform & build

- macOS native release artifact: a portable DMG with the `.app` bundle
  inside, drag-to-Applications layout, native multi-resolution `.icns`
  icon, and an `Info.plist` populated from the same `LBA2_*` cache vars
  that drive the runtime window title, `.desktop` entry, AppImage labels,
  and Windows resource. Built for arm64 (Apple Silicon) on tag push;
  static-linked SDL3, no `.dylib` dependencies, ad-hoc codesigned to
  satisfy Apple Silicon's mandatory bundle integrity check. First launch
  still needs the right-click → Open Gatekeeper bypass (Developer ID +
  notarization deferred); documented in the README inside the DMG, and
  the Sequoia (15+) `xattr -dr com.apple.quarantine` recovery flow is
  spelled out in [docs/RELEASING.md](docs/RELEASING.md). Intel-Mac users
  can produce a local DMG via
  `scripts/dev/build-macos-release.sh --preset macos_x86_64` while
  hosted Intel runners are unreliable
  ([#102](https://github.com/LBALab/lba2-classic-community/pull/102),
  [#103](https://github.com/LBALab/lba2-classic-community/pull/103),
  [#105](https://github.com/LBALab/lba2-classic-community/pull/105),
  [#106](https://github.com/LBALab/lba2-classic-community/pull/106),
  [#149](https://github.com/LBALab/lba2-classic-community/pull/149)).
- Linux binary tarball release artifact alongside the AppImage: same
  static-linked single-binary layout, drop-in for distros where AppImage
  FUSE is a hassle. `libxtst-dev` added to the tarball build deps after
  the first CI run surfaced the missing X11 link
  ([#124](https://github.com/LBALab/lba2-classic-community/pull/124),
  [#125](https://github.com/LBALab/lba2-classic-community/pull/125)).
- Post-release smoke-test script (`scripts/dev/verify-release.sh`)
  downloads each published Linux artifact, runs the discovery
  self-tests, and reports per-asset pass/fail. Pointers added to the
  releasing recipe and announcement steps
  ([#126](https://github.com/LBALab/lba2-classic-community/pull/126),
  [#127](https://github.com/LBALab/lba2-classic-community/pull/127)).
- Rolling `latest` pre-release: every push to `main` triggers a CI build
  of all three platforms (Linux AppImages, Windows ZIP, macOS DMG) and
  force-updates a `latest`-tagged GitHub Release. Marked pre-release; the
  most recent stable tag keeps GitHub's "Latest" promotion. Lets community
  testers grab bleeding-edge main builds without cloning + building
  ([#108](https://github.com/LBALab/lba2-classic-community/pull/108)) —
  see [docs/RELEASING.md](docs/RELEASING.md) "Rolling latest pre-release".
- Release workflow hardening: low-friction release-target additions,
  CI path filters so doc-only pushes skip the heavy matrix, pinned
  `setup-sdl` SHA + per-arch cache discriminator after a stale-cache
  drift incident
  ([#129](https://github.com/LBALab/lba2-classic-community/pull/129),
  [#130](https://github.com/LBALab/lba2-classic-community/pull/130)).

### CI & developer workflow

- `clang-tidy` static-analysis pass wired into CI, with a tuned
  check-list that silences bulk-noise categories (Watcom-era idioms,
  C-style casts in ported code) while keeping the high-signal UB /
  memory-safety / portability checks. The first analysis pass produced
  the UB-hardening fixes listed above
  ([#139](https://github.com/LBALab/lba2-classic-community/pull/139),
  [#141](https://github.com/LBALab/lba2-classic-community/pull/141)).
- Docs-only gate for the required build/test checks so README/CHANGELOG
  edits don't block on the full matrix
  ([#136](https://github.com/LBALab/lba2-classic-community/pull/136)).
- Optional `pre-commit` hook running `check-format.sh` for contributors
  who'd rather catch clang-format mismatches locally than in CI
  ([#133](https://github.com/LBALab/lba2-classic-community/pull/133)).

### Documentation

- [docs/WIDESCREEN.md](docs/WIDESCREEN.md) — phased plan (A–E) for the
  widescreen / higher-resolution work, with the framebuffer-extraction
  refactor above as its first landed phase
  ([#135](https://github.com/LBALab/lba2-classic-community/pull/135)).
- [docs/CI.md](docs/CI.md) — map of the CI workflow surface (matrix
  builds, gates, release flow) so contributors can reason about a
  failing job without spelunking the YAML
  ([#137](https://github.com/LBALab/lba2-classic-community/pull/137)).
- [docs/SPRITES.md](docs/SPRITES.md) — sprite/blit pipeline reference,
  including the `ScaleSprite` path layout and the test gap that hid the
  scaled-path regression for months.
- [docs/AUDIO.md](docs/AUDIO.md) extended with the retail audio asset
  inventory: what's ADPCM-WAV, what's MIDI, and which backend owns
  which path.

### Contributors

Welcome to a new contributor since v0.9.0:
[@b-tuma](https://github.com/b-tuma) — gamepad D-pad navigation for the
save/load list ([#79](https://github.com/LBALab/lba2-classic-community/pull/79)).

## [0.9.0] - 2026-05-09

First tagged release. Cross-platform builds (Linux AppImage, Windows ZIP)
published at
[v0.9.0](https://github.com/LBALab/lba2-classic-community/releases/tag/v0.9.0).

The list below is hand-curated from merged PRs and tells the story of the
fork's evolution.

> **Highlights since the fork**
>
> The biggest architectural shift is portability. The playable game now
> builds as pure C/C++ against SDL3 and `libsmacker`, with no ASM
> toolchain on the build path. The original 16/32-bit x86 assembly is
> still in the tree as the source of truth and powers the ASM↔C++
> equivalence tests, but it no longer runs in the playable binary.
> Native builds for Linux x86_64, macOS arm64/x86_64, and Windows
> (MSYS2 UCRT64). Community members are already running it on handheld
> devices in the wild.

### Stability & 64-bit hardening

- Magic-school grand-wizard crash on 64-bit hosts — fixed
  ([#78](https://github.com/LBALab/lba2-classic-community/issues/78),
  [#84](https://github.com/LBALab/lba2-classic-community/pull/84)).
  U32+pointer-wraparound trap in `CopyMask`, pinned by a host-only
  regression test.
- Renderer-side U32+pointer-wrap class named, documented, and swept
  across ~80 files in SVGA, pol_work, 3D, 3DEXT, GRILLE, INTEXT — one
  bug fixed (above), six "safe by convention" latent traps recorded
  ([#85](https://github.com/LBALab/lba2-classic-community/pull/85),
  [#86](https://github.com/LBALab/lba2-classic-community/pull/86),
  [#87](https://github.com/LBALab/lba2-classic-community/pull/87),
  [#88](https://github.com/LBALab/lba2-classic-community/pull/88),
  [#89](https://github.com/LBALab/lba2-classic-community/pull/89),
  [#90](https://github.com/LBALab/lba2-classic-community/pull/90))
- Endgame credits segfault on 64-bit hosts — fixed
  ([#65](https://github.com/LBALab/lba2-classic-community/issues/65),
  [#66](https://github.com/LBALab/lba2-classic-community/pull/66))
- Legacy 32-bit `.lba` saves load safely on 64-bit builds, with a corpus
  harness to prevent regressions
  ([#62](https://github.com/LBALab/lba2-classic-community/issues/62),
  [#63](https://github.com/LBALab/lba2-classic-community/pull/63))
- 32-bit on-disk ABI rule and credits HQR layout codified in docs
  ([#67](https://github.com/LBALab/lba2-classic-community/pull/67))
- Linux exterior draw-order bug — `SinTab`/`CosTab` arrays merged into a
  single contiguous block to match the original ASM layout
  ([#55](https://github.com/LBALab/lba2-classic-community/pull/55))
- Compiler warnings cleaned up across GCC, Clang, and MinGW
  ([#52](https://github.com/LBALab/lba2-classic-community/pull/52))
- Bezier C++ memory corruption — fixed
  ([#8](https://github.com/LBALab/lba2-classic-community/pull/8))

### New features (opt-in)

All new features default-off or default-to-original-behavior, in line with
[AGENTS.md Principle 3](AGENTS.md#principles).

- Quake-style debug console: built-in, always available, configurable
  toggle key, host-tested
  ([#23](https://github.com/LBALab/lba2-classic-community/issues/23),
  [#24](https://github.com/LBALab/lba2-classic-community/pull/24),
  [#60](https://github.com/LBALab/lba2-classic-community/pull/60)) —
  see [docs/CONSOLE.md](docs/CONSOLE.md)
- SDL3 gamepad support
  ([#38](https://github.com/LBALab/lba2-classic-community/issues/38),
  [#58](https://github.com/LBALab/lba2-classic-community/pull/58))
- Optional exterior auto-camera centering (`FollowCamera`)
  ([#37](https://github.com/LBALab/lba2-classic-community/issues/37),
  [#50](https://github.com/LBALab/lba2-classic-community/pull/50)) —
  see [docs/CAMERA.md](docs/CAMERA.md)
- Menu mouse QoL: hover, selection, marquee for long text, keyboard
  takes priority over mouse hover
  ([#41](https://github.com/LBALab/lba2-classic-community/pull/41),
  [#53](https://github.com/LBALab/lba2-classic-community/pull/53),
  [#56](https://github.com/LBALab/lba2-classic-community/pull/56))
- Original Adeline debug tools re-enabled behind `DEBUG_TOOLS=ON`
  ([#17](https://github.com/LBALab/lba2-classic-community/issues/17),
  [#22](https://github.com/LBALab/lba2-classic-community/pull/22)) —
  see [docs/DEBUG.md](docs/DEBUG.md)

### Game data, config, and UX

- Auto-discovery of retail game data (`./data`, `../LBA2`,
  `LBA2_GAME_DIR`, `--game-dir`); embedded default config; clearer
  startup errors
  ([#54](https://github.com/LBALab/lba2-classic-community/pull/54)) —
  see [docs/GAME_DATA.md](docs/GAME_DATA.md)
- Menus reorganized to match LBA2 Original from GOG
  ([#42](https://github.com/LBALab/lba2-classic-community/pull/42))
- Menu language selection
  ([#42](https://github.com/LBALab/lba2-classic-community/pull/42))
- Menu music and jingle handling — fixed
  ([#43](https://github.com/LBALab/lba2-classic-community/issues/43))
- Case-sensitive music paths on Linux — fixed
  ([#12](https://github.com/LBALab/lba2-classic-community/pull/12))

### Audio/video backends
- SDL audio backend with Miles parity gap tracking
  ([#27](https://github.com/LBALab/lba2-classic-community/issues/27),
  [#28](https://github.com/LBALab/lba2-classic-community/pull/28),
  [#9](https://github.com/LBALab/lba2-classic-community/pull/9))
- SDL renderer presentation path with LTO and inline rotates
  ([#11](https://github.com/LBALab/lba2-classic-community/pull/11),
  [#51](https://github.com/LBALab/lba2-classic-community/pull/51))
- Early SDL bring-up for renderer/audio in the fork's first
  cross-platform phase, plus follow-up SDL3 AIL-linking fixes
  ([#6](https://github.com/LBALab/lba2-classic-community/pull/6),
  [#9](https://github.com/LBALab/lba2-classic-community/pull/9),
  [#11](https://github.com/LBALab/lba2-classic-community/pull/11),
  [e9ba7d8](https://github.com/LBALab/lba2-classic-community/commit/e9ba7d8))
- Smacker FMV via libsmacker; video audio routed through the AIL backend
  ([#10](https://github.com/LBALab/lba2-classic-community/pull/10),
  [#29](https://github.com/LBALab/lba2-classic-community/issues/29),
  [#35](https://github.com/LBALab/lba2-classic-community/pull/35))

### Ported (ASM → C++)

- Object display, rendering helpers, and supporting math via the
  ASM-validation framework, with byte-for-byte equivalence tests
  ([#31](https://github.com/LBALab/lba2-classic-community/pull/31),
  [#32](https://github.com/LBALab/lba2-classic-community/pull/32))
- Early renderer/SVGA/object-display ASM→CPP ports (polygon fillers,
  blitters/sprites, and object rendering), which established the base for
  later validation-driven porting work
  ([#6](https://github.com/LBALab/lba2-classic-community/pull/6))
- See [docs/ASM_TO_CPP_REFERENCE.md](docs/ASM_TO_CPP_REFERENCE.md) for the
  full port-status table.

### Cross-platform & build

- First Linux release artifact: AppImage built in CI from an any-linux
  base, packaged as a single portable binary
  ([#74](https://github.com/LBALab/lba2-classic-community/pull/74))
- First Windows release artifact: a portable ZIP (`lba2cc-<version>-windows-x64.zip`)
  built in CI under MSYS2 UCRT64 with SDL3 and the GCC C++ runtime
  static-linked. Drop the single `lba2cc.exe` into your existing LBA2
  install folder — no DLLs, no installer, no registry entries. Follows
  the same release-workflow conventions as the AppImage flow (matrix
  build → idempotent gh-release attach), see `docs/RELEASING.md`.
- Product metadata (executable name, display name, description, desktop
  ID) centralized into CMake cache variables so the runtime window
  title, `.desktop` entry, and AppImage script all consume one source
  ([#83](https://github.com/LBALab/lba2-classic-community/issues/83)).
  Window title now shows the version (e.g. `LBA2 Classic Community
  0.9.0-dev`) — previously just `LBA2`.
- Default binary renamed `lba2` → `lba2cc` to avoid colliding with the
  retail executables (`lba2.exe`, `tlba2.exe`, `tlba2c.exe`). Path is
  now `build/SOURCES/lba2cc[.exe]`. Override-able via
  `-DLBA2_EXECUTABLE_NAME=...` for anyone shipping a custom branded build.
- Windows `.exe` now ships with an embedded application icon and
  populated File Properties → Details (Product Name, File Description,
  Product Version, Original Filename). Strings flow from the same
  `LBA2_*` cache vars that feed the runtime window title, `.desktop`
  entry, and AppImage labels. Pre-release versions (anything carrying
  `-dev`/`-dirty`/`-rc` suffix) are flagged with `VS_FF_PRERELEASE` in
  `VERSIONINFO`.
- macOS CI build (arm64); macOS x86_64 preset
  ([#52](https://github.com/LBALab/lba2-classic-community/pull/52))
- Portable Windows build via MSYS2 UCRT64
  ([#14](https://github.com/LBALab/lba2-classic-community/pull/14)) —
  see [docs/WINDOWS.md](docs/WINDOWS.md)
- CMake presets for Linux, macOS arm64/x86_64, Windows UCRT64, and
  cross-compile Linux→Windows
  ([#21](https://github.com/LBALab/lba2-classic-community/pull/21))
- SDL3 throughout (audio, video, renderer)
- Default Windows preset switched to Release
- `clang-format` with checked-in config; ASM and `LIB386/libsmacker/`
  excluded
  ([#36](https://github.com/LBALab/lba2-classic-community/pull/36))
- Host-only discovery tests run on Linux/macOS/Windows in CI without
  Docker or retail files
  ([#54](https://github.com/LBALab/lba2-classic-community/pull/54))

### Tests

- ASM↔CPP equivalence test framework with byte-for-byte assertions and
  randomized stress rounds
  ([#31](https://github.com/LBALab/lba2-classic-community/pull/31)) —
  see [docs/TESTING.md](docs/TESTING.md)
- Polyrec replay with `--bisect` for finding the first divergent draw
  call between ASM and CPP renderers
- Tightened assertions across `POLYFLAT`, `POLYGOUR`, `POLYDISC`,
  `POLYTEXT`, `POLYGTEX`, `AFF_OBJ`, FPU precision, and the resampler
  ([#44](https://github.com/LBALab/lba2-classic-community/pull/44))

### Documentation

- [AGENTS.md](AGENTS.md) — principles, "never" list, "when modifying X
  do Y" table for AI assistants and human contributors
  ([#40](https://github.com/LBALab/lba2-classic-community/pull/40))
- [docs/GLOSSARY.md](docs/GLOSSARY.md),
  [docs/LIFECYCLES.md](docs/LIFECYCLES.md),
  [docs/SCENES.md](docs/SCENES.md) — engine reference
  ([#25](https://github.com/LBALab/lba2-classic-community/pull/25),
  [#26](https://github.com/LBALab/lba2-classic-community/issues/26))
- [docs/MENU.md](docs/MENU.md), [docs/CONFIG.md](docs/CONFIG.md),
  [docs/SAVEGAME.md](docs/SAVEGAME.md) — subsystem references
  ([#30](https://github.com/LBALab/lba2-classic-community/pull/30))
- [docs/CONSOLE.md](docs/CONSOLE.md), [docs/DEBUG.md](docs/DEBUG.md),
  [docs/CAMERA.md](docs/CAMERA.md),
  [docs/FEATURE_WORKFLOW.md](docs/FEATURE_WORKFLOW.md)
- [docs/PLATFORM.md](docs/PLATFORM.md) — host-assumptions map
  (pointer width, endianness, FPU semantics, OS boundaries),
  [docs/PLATFORM_AUDITS.md](docs/PLATFORM_AUDITS.md) — per-file audit
  logs hung off it
  ([#85](https://github.com/LBALab/lba2-classic-community/pull/85),
  [#88](https://github.com/LBALab/lba2-classic-community/pull/88))
- [docs/ABI.md](docs/ABI.md) — disk-layout patterns for fat structs
  on 64-bit ([#67](https://github.com/LBALab/lba2-classic-community/pull/67))
- [docs/CRASH_INVESTIGATION.md](docs/CRASH_INVESTIGATION.md) —
  AddressSanitizer + gdb runbook
  ([#85](https://github.com/LBALab/lba2-classic-community/pull/85))
- [docs/COMPILER_NOTES.md](docs/COMPILER_NOTES.md),
  [docs/TESTING.md](docs/TESTING.md)
- [docs/FRENCH_COMMENTS.md](docs/FRENCH_COMMENTS.md) and
  [docs/ASCII_ART.md](docs/ASCII_ART.md) — preservation catalogs

### Pre-PR foundations

Most of what made this fork buildable and cross-platform predates the
PR workflow. The themes from that direct-commit era — CMake build
system and MinGW cross-compile to Windows ([@leokolln](https://github.com/leokolln)),
macOS M1 build, the bulk of the LIB386 ASM→C++ port, and the ASM↔CPP
equivalence test infrastructure ([@sergiou87](https://github.com/sergiou87)),
early renderer/SVGA/object-display ASM→CPP ports ([@xesf](https://github.com/xesf)),
all building on [@yaz0r](https://github.com/yaz0r)'s original buildable
prototype — are credited under Contributors below.

The earliest PR-tracked entries that sit alongside that work:

- File encoding preservation
  ([#1](https://github.com/LBALab/lba2-classic-community/pull/1))
- Preservation docs and build configuration clarity
  ([#13](https://github.com/LBALab/lba2-classic-community/pull/13))

### Contributors

Thanks to everyone who's worked on this fork to date:
[@gwen-gg](https://github.com/gwen-gg),
[@innerbytes](https://github.com/innerbytes),
[@leokolln](https://github.com/leokolln),
[@Link4Electronics](https://github.com/Link4Electronics),
[@noctonca](https://github.com/noctonca),
[@sergiou87](https://github.com/sergiou87),
[@xesf](https://github.com/xesf),
[@yaz0r](https://github.com/yaz0r).


Special thanks to the contributors whose foundational work this fork stands on:

- [@sergiou87](https://github.com/sergiou87) — macOS M1 build, the bulk of the LIB386 ASM→C++ port, and the ASM↔CPP equivalence test infrastructure that makes the cross-platform port verifiable.
- [@leokolln](https://github.com/leokolln) — the cross-platform build foundation: CMake build system and presets, MinGW cross-compile to Windows, UASM ASM modernisation, 64-bit Linux preset, AIL sound backend structure, and Linux CI.
- [@xesf](https://github.com/xesf) — implementation work across SVGA, 3D object rendering, brick handling, and `AffString`, plus the CopyMask Tavern crash fix and widescreen Steam Deck work.
- [@yaz0r](https://github.com/yaz0r) — the original buildable prototype that the cross-platform fork was built on.

A big thank you to Gwen Gourevich ([@gwen-gg](https://github.com/gwen-gg)) and [2.21](https://discord.gg/e2ZXpzrM) for open-sourcing the original code, and to the original Adeline
Software team whose code this builds on.

[Unreleased]: https://github.com/LBALab/lba2-classic-community/compare/v0.13.0...HEAD
[0.13.0]: https://github.com/LBALab/lba2-classic-community/compare/v0.12.0...v0.13.0
[0.12.0]: https://github.com/LBALab/lba2-classic-community/compare/v0.11.0...v0.12.0
[0.11.0]: https://github.com/LBALab/lba2-classic-community/compare/v0.10.0...v0.11.0
[0.10.0]: https://github.com/LBALab/lba2-classic-community/compare/v0.9.0...v0.10.0
[0.9.0]: https://github.com/LBALab/lba2-classic-community/compare/1f3e871...v0.9.0
