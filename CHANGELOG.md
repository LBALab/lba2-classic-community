# Changelog

All notable changes to the LBA2 Classic Community engine fork.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> Engine semver is independent from `NUM_VERSION` (save-file on-disk format,
> see `SOURCES/COMMON.H`). Save-format changes are tracked there, not here.

## [Unreleased]

_Nothing yet._

## [0.12.0] - 2026-07-16

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

### Display & resolution

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
  ([docs/SAVE_WIRE_PLAN.md](docs/SAVE_WIRE_PLAN.md)), harness input
  simulation ([docs/INPUT_SIM_PLAN.md](docs/INPUT_SIM_PLAN.md)), and
  render interpolation
  ([docs/RENDER_INTERP_PLAN.md](docs/RENDER_INTERP_PLAN.md)), the last
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
  ([docs/PLATFORM_PAL_PLAN.md](docs/PLATFORM_PAL_PLAN.md)), a plan for
  hosting LBA1 on the lba2cc engine
  ([docs/LBA1_PORT_PLAN.md](docs/LBA1_PORT_PLAN.md)), the gamepad camera
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

[Unreleased]: https://github.com/LBALab/lba2-classic-community/compare/v0.12.0...HEAD
[0.12.0]: https://github.com/LBALab/lba2-classic-community/compare/v0.11.0...v0.12.0
[0.11.0]: https://github.com/LBALab/lba2-classic-community/compare/v0.10.0...v0.11.0
[0.10.0]: https://github.com/LBALab/lba2-classic-community/compare/v0.9.0...v0.10.0
[0.9.0]: https://github.com/LBALab/lba2-classic-community/compare/1f3e871...v0.9.0
