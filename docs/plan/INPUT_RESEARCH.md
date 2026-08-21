# Input system: research

The input system as it stands, across every device that reaches it, with the open issues mapped
onto it and the gaps that no issue records yet.

This is findings only. It proposes nothing, orders nothing and commits nothing; the planning doc
comes after review. It exists because [REFACTOR_ROADMAP.md](REFACTOR_ROADMAP.md) area 8 names
input as the area the issue tracker is asking for, and because an area that is going to be pinned
with tests has to be described first.

Companions: [INPUT_SIM_PLAN.md](INPUT_SIM_PLAN.md) for the simulator and the throttle-drop class,
[INPUT_REPLAY_RESEARCH.md](INPUT_REPLAY_RESEARCH.md) for why no recorded-input subsystem was
inherited, and [INPUT_DOOM3_RESEARCH.md](INPUT_DOOM3_RESEARCH.md) for what a mature version of
this system looks like elsewhere.

Measured against `main` at `97e9f303`. Numbers age; re-run the commands at the end.

## The finding that organises everything else

**There are two funnels, not one, and only one of them is bindable.**

The engine has a proper binding layer: `DefKeys[]` and `GamepadKeys[]` are folded by `InitInput`
into one flat (key, mask) table, and `GetInput` scans it to rebuild the `Input` action bitfield.
Anything reading `Input` inherits every binding and every alternate key for free. 154 sites do.

Alongside it, and reaching the same consumers, is a raw scancode path. `TabKeys[]` and `MyKey`
carry physical keys, and 93 sites compare them directly. That path is not bindable, does not know
about alternates, and is where every input bug of the last few months has been found.

The important part is not that the second path exists for legacy reasons. It is that **new code
keeps being written into it, and three devices depend on it by design**:

- the touch overlay presses hardcoded scancodes into `TabKeys`
- the gamepad synthesises hardcoded scancodes for menu navigation
- text entry reads `GetAscii`, which takes the raw path and is not bindable

So "route the raw-key sites through the binding layer" is not a cleanup of old code. It is a
question about how three current devices work.

## The path, layer by layer

| Layer | File | What it does | Covered by a test |
|---|---|---|---|
| Event pump | [LIB386/SYSTEM/EVENTS.CPP](../../LIB386/SYSTEM/EVENTS.CPP) | `SDL_PollEvent` loop, fans out to seven per-subsystem `HandleEventsXxx` handlers, two of them weak | no |
| Keyboard sample | [LIB386/SYSTEM/KEYBOARD.CPP](../../LIB386/SYSTEM/KEYBOARD.CPP) (138) | clears `TabKeys[0..255]`, re-reads `SDL_GetKeyboardState`, sets `Key` to the lowest scancode down | `tests/input_device` |
| Mouse | [LIB386/SYSTEM/MOUSE.CPP](../../LIB386/SYSTEM/MOUSE.CPP) (229) | event-driven deltas, wheel, box clamp | `tests/input_device` |
| Gamepad | [SOURCES/JOYSTICK.CPP](../../SOURCES/JOYSTICK.CPP) (470) | writes virtual scancodes >= 1024 into `TabKeys`; raw axes for the camera | no |
| Touch | [SOURCES/TOUCH_INPUT.CPP](../../SOURCES/TOUCH_INPUT.CPP) (507) | on-screen buttons write scancodes into `TabKeys` via `ApplyVirtualKeys` | no |
| Funnel | [LIB386/SYSTEM/INPUT.CPP](../../LIB386/SYSTEM/INPUT.CPP) (59) | rebuilds `Input` from the combined table; `NoRepeatInput` latch | `tests/input_funnel` |
| Binding table | [SOURCES/INPUT.CPP](../../SOURCES/INPUT.CPP) (446) | `DefKeys`, `GamepadKeys`, `InitInput`, cfg read and write, `MyGetInput` | **none** |
| Layout helper | [SOURCES/MENU_KEYNAV.CPP](../../SOURCES/MENU_KEYNAV.CPP) (33) | keypad legend to navigation keys | `tests/menu_keynav`, 40 cases |
| Consumption | everywhere | 154 `Input &`, 93 raw scancode compares, 21 `CheckKey`, 81 `MyGetInput()` loops | two automation fixtures |

The shape of the coverage is the thing to notice. The layer below the binding table is tested, the
layer above it is tested, and **no test links `SOURCES/INPUT.CPP`**. `tests/input_funnel` gets
close, but it builds its own fake binding table with a local `setBinding` helper precisely so it
can test the funnel without the real one.

## Per device

### Keyboard

The reference device, and the one with a parity obligation. `DefKeysDefault95` is the retail
layout, 36 slots of `{Key1, Key2}`. It is the only place the 1997 control scheme is written down,
which makes it an asset as well as a table.

Sampling is level-triggered: `UpdateKeyboardState` clears and re-reads the whole keyboard each
call, so a press and release between two polls never existed. `Key` is the lowest set scancode, so
which key "the" key is among simultaneous presses is an artifact of scancode ordering rather than
a decision.

### Mouse

Event-driven, unlike everything else, and entirely outside the binding layer. It drives the menu
pointer, the camera orbit and the wheel zoom, with its own cfg tunables (`MouseCamera`,
`MouseSensitivityX/Y`, `MouseCameraDivisor`, `MouseCameraSmoothing`, documented in
[CONTROLLER.md](../CONTROLLER.md)). No mouse control is rebindable and none appears in either
config screen.

### Gamepad

Two separate paths, and neither one makes the other visible.

**Gameplay** goes through the binding layer properly. `GamepadKeys[]` sits beside `DefKeys[]` in
the combined table, so a pad button is a first-class binding that honours `NoRepeatInput` exactly
like a key. This was deliberate and is commented as such in `InitInput`.

**Menus** do not. `JoystickMenuNavOnly` and `JoystickMenuAction` return hardcoded `K_UP`,
`K_DOWN`, `K_LEFT`, `K_RIGHT` and `K_ENTER`, which screens then compare as raw scancodes. Menu
navigation on a pad is therefore unbindable, and a screen has to handle both paths to be fully
pad-driveable. #509 is exactly a screen that handles neither.

**Analog is destroyed at the funnel, as it was in 1997.** `ApplyStickDirection` quantises a stick
into 8 sectors with hysteresis and emits digital virtual scancodes; magnitude is discarded. The
imported 1997 `JoyMakeBitfield` did the same with a fixed third-of-range threshold (see G3).

The one exception is the right stick when
`GamepadCameraAnalog && FollowCamera && CubeMode == CUBE_EXTERIEUR`, where raw axes go to
`ApplyManualCameraNudge` instead, bypassing the funnel entirely. So the engine has exactly one
analog consumer, it is the camera, and it got there by not using the input system.

### Touch

An on-screen overlay of 14 buttons, each with a hardcoded scancode in a static table:
`{BTN_DPAD_UP, "U", ..., K_UP}`, `{BTN_SPACE, "ACT", ..., K_SPACE}`. `ApplyVirtualKeys` re-applies
them into `TabKeys` after the keyboard poll clears it, through the weak-symbol hook.

The hook design is good and is the same one the control harness uses. The table is not: the
buttons press *default* scancodes, and nothing rebuilds them when bindings change.

### Text entry

`GetAscii`, on the raw path: it converts `Key`, the poll's first held scancode, and consults no
binding. Three call sites in the game build, in two functions: the save-name field twice and the
cheat code reader. It read SDL's keyboard array directly until it was found to be a third path
rather than one of the two -- outside the poll, so no injected key reached it, and outside the
recorder, so a session sitting on the save-name screen wrote no polls at all and a replay of it
waited there forever.

There is still no virtual keyboard, and [GAMEMENU.CPP](../../SOURCES/GAMEMENU.CPP) says so in a
`TODO(input)`: neither the touch overlay nor a pad writes `Key`, so non-keyboard users still
cannot enter save or profile names, and are routed around the field entirely by the
`!LastInputWasKeyboard` branch in `ChoosePlayerName`, which gives a new slot a datetime stem and
overwrites an existing one.

## The two world modes ask different questions of input

Not visible from the input code at all, which is the point. `CubeMode` never appears in
`MOVE_MANUAL`: the hero is driven by identical tank-control code in isometric interiors and
perspective exteriors. What differs is the camera, and it differs in a way that decides whether a
camera-relative control scheme is even well-defined.

| | Interior (isometric) | Exterior (perspective) |
|---|---|---|
| Projection call | `SetIsoProjection(xcentre, ycentre)` | `SetFollowCamera(..., AlphaCam, BetaCam, GammaCam, VueDistance)` |
| Camera orientation | **no angle parameter exists**; `TypeProj = TYPE_ISO` and that is all | `BetaCam`, computed from hero `Beta`, orbitable by the player |
| Camera motion | pans on the tile grid, half-step interpolation toward the hero's brick | orbits, follows, and is overridden by camera zones and cutscenes |
| Screen "up" in world terms | one constant, for the whole game | changes continuously |
| Auto camera (`FollowCamera`) | never runs | exterior only, and suppressed by `CameraZone` and `CinemaMode` |

**Tank controls are camera-invariant, which is why one code path serves both.** "Left" means
"rotate the hero left" regardless of where the camera is, so a fixed iso camera and a free orbiting
one can share `MOVE_MANUAL` without either knowing about the other. That is not an accident of the
port; it is the property that let a 1997 game ship both modes with one control scheme.

**The consequence for #372 runs opposite to the intuition.** Its keystone, camera-relative
movement, is explicitly built on "the new free dynamic camera", which in code is
`FollowCamera && CubeMode == CUBE_EXTERIEUR`. But that is the *harder* of the two modes to make
camera-relative:

- **Interiors are the easy case.** The iso camera has no orientation parameter, so screen space
  maps to world space through one constant rotation, forever. Camera-relative movement there is a
  fixed basis change and nothing else, which is exactly how modern isometric games use an analog
  stick.
- **Exteriors are the hard case.** `BetaCam` is continuously variable and is not owned by the
  player: camera zones and cutscenes take it away. Camera-relative movement has to survive the
  moment the camera is yanked, which is the classic bug where a camera cut reverses the player's
  direction mid-stride.
- **And exteriors have a feedback loop interiors do not.** `BetaCam` is derived from hero `Beta`
  by the auto-follow. Make the hero's direction derive from `BetaCam` as well and the two chase
  each other: the hero turns, the camera follows, the stick's meaning rotates, the hero turns
  further. Real implementations break the loop by latching the reference frame when the stick is
  pushed, or by suspending auto-follow while it is held. The Auto camera already has the machinery
  for the second (`FollowCamForgetManualGesture`, the re-engage grace period), which is worth
  knowing before anyone estimates this.

So the shape of the question is: does a control-scheme change apply to both modes, giving the
player one scheme and needing the harder exterior case solved, or to exteriors only, giving the
player an invisible scheme change at every door? Neither is free, and #372 does not currently say
which it means.

## What retail parity means here, concretely

Worth pinning down because "keyboard must have parity with retail" is the constraint that decides
what the binding work may and may not touch.

Parity is `DefKeysDefault95` plus the funnel's behaviour, not the whole system. The table is data
and is directly checkable. The funnel's semantics are the subtler half: `NoRepeatInput` masks a
bit that was already held so a held key does not repeat, and `ClearNoRepeatInput` releases it.
That is the mechanism the whole 1997 menu and dialogue layer is built on, and it is what a
rewrite would most easily get wrong in a way no screenshot catches.

`tests/input_funnel` already pins some of it, including the regression where a mid-frame
`GetInput` rebuild dropped held movement. It does so against a synthetic table, so it pins the
funnel's rules without pinning the retail layout. Those are two different assertions and only one
of them exists.

## The open issues, clustered

Thirteen open issues touch input. They are not thirteen problems.

**Cluster 1: screens that bypass the binding layer.** #509 (the disc prompt cannot be answered
with a gamepad, and `while (flag == -1)` has no other exit), #508 (the pad config screen's Key2
column is unreachable from the keyboard, and the keypad period is offered but unbindable), #507
(the umbrella: layout against preference). Closed siblings #497 and #506 are the same shape. This
cluster is the 93 raw scancode sites, and it is the only cluster producing user-visible defects
today.

**Cluster 2: the config file's vocabulary.** #61, human-readable key names alongside raw
scancodes, with acceptance criteria already written. It is the smallest issue here and it is a
prerequisite for anything that wants to express a binding set as data.

**Cluster 3: control-scheme features.** #4 (LBA1 Enhanced Edition style controls), #372 (the
omnidirectional proposal from field testing, explicitly a direction decision before it is a task), #365
(hold-to-lock Z-targeting), #369 (weapon wheel), #370 (inventory items on the D-pad). All five
are profile features: they are alternative mappings of actions to controls, and #372 says in its
own text that the retail scheme must remain the default.

**Cluster 4: devices that cannot express what they need to.** #359 (the Android on-screen pad is
4-way, wants 8-way).

**Cluster 5: timing and harness.** #412 (fixed-step sim plus interpolated render plus input
latching, the structural fix for the throttle-drop class), #433 (harness ergonomics). #514 is
adjacent: the camera orbit follow-through is right for a stick that springs back and wrong for a
mouse that does not, which is a device-difference problem wearing a camera hat.

The cluster ordering that falls out: 1 is defects, 2 is a prerequisite, 3 is blocked on a
direction call and on 1, 4 is blocked on an architectural property described below, 5 is a
separate campaign already under way.

## Prior art for cluster 3, and what "LBA1 Enhanced Edition controls" actually means

#4 asks for "similar controls to LBA1 Enhanced Edition" and its body is one sentence. The phrase
names a specific build, and it is worth pinning down before anyone builds against it.

Read from the installed files rather than from the web, because the web accounts of this game
describe the mobile release and are wrong about the desktop one. The Steam install of
*Little Big Adventure* ships **three** things side by side, which is the first thing to get
straight:

| Path | What it is |
|---|---|
| `TLBA1C.exe` plus `SDL2.dll` / `SDL3.dll` | *Twinsen's Little Big Adventure Classic*, the 2022 official re-release, the sibling of this project |
| `DotEmu/LBA.exe` | the **Enhanced Edition**, a separate cocos2d-x build (`libcocos2d.dll`, `libEGL.dll`, `libGLESv2.dll`), a mobile engine brought to desktop |
| `original/`, `Speedrun/` | the DOS data and a DOSBox-wrapped build |

So "Enhanced Edition" is not the Steam retitle, and the two are not the same executable.

### What the Enhanced Edition actually does

From `DotEmu/resources/lba_strings_en.xml` and the asset names.

**Two selectable control modes**, `controlMode`: `controlTouch` ("Touch") and `controlVPad`
("Virtual Pad"). The entire options screen is four rows: Control mode, Volume, Hotspots, Language.

**It has keyboard support and rebinding on desktop.** The web sources say it does not, and that is
true of the mobile release only. The strings carry a `key_configuration` screen titled "Controls"
with `keyboard_control_label` "Keyboard" and `gamepad_control_label` "Gamepad", plus
`press_a_key_label` and `press_a_button_label` prompts and a `key_confirmation` step.

**The bindable action set is about three, against this engine's 36.** The rebind rows named in the
strings are `Action`, `Jump` and `Attack/Ball`. Everything else is a fixed HUD control
(`btn_hide`, `btn_inventory`, `btn_pause`) or a pad glyph. The Virtual Pad overlay is three
buttons: `btn_virtual_action`, `btn_virtual_attack`, `btn_virtual_jump`. Pad glyphs shipped are
A, B, X, Y, L1, R1, L3, R3, Back and Start, of which A, B, X, Y and R1 appear as in-HUD prompts.

**The behaviour wheel is gone, replaced by dedicated buttons and contextual gestures**, and the
tutorial strings state each scheme twice, once per input:

| Action | Mouse and touch | Gamepad |
|---|---|---|
| Run | double-click | press and hold the action button |
| Jump | click Twinsen, hold, drag away to aim | jump button, **default distance** |
| Magic ball | same drag, past jump range, release to throw | press and hold the attack button |
| Attack | double-click the NPC | attack button |
| Hide (Discreet) | footsteps button in the HUD | hide button |
| Camera | mouse wheel to zoom | zoom in and out buttons |

**Hotspots are an affordance layer with an on/off toggle.** `spot_attack`, `spot_look`,
`spot_cube`, `spot_inventory_get`, `spot_inventory_use`, `spot_scenaric` mark interactable things
on screen. That is how the Enhanced Edition replaced "select the right behaviour and walk into
it": it tells the player what is interactable instead of making the behaviour the question.

**The camera zooms and does not rotate**, which is consistent with LBA1 being isometric throughout.

### Three things this says for cluster 3

**The Enhanced Edition's own pad scheme is deliberately less expressive than its pointer scheme.**
The mouse gets direction and distance out of one drag for both jump and magic ball. The pad gets
"Pressing the jump button will use the default distance." DotEmu hit exactly the problem G3
describes, an analog gesture with no button equivalent, and answered it by dropping to a
constant rather than by finding a stick idiom. Worth knowing before this project assumes the pad
is the richer input.

**It could do this because LBA1 is isometric throughout, which LBA2 is not.** Checked against
`../lba1-classic`: gameplay uses `SetIsoProjection( 320-8-1, 240, SIZE_BRICK_XZ )`, and every
perspective `SetProjection` call in that tree is in `GAMEMENU.C` (the behaviour-menu model spin),
`HOLOMAP.C` (the globe) or one scripted case in `GERELIFE.C`. There is no `CubeMode` and no
exterior world mode. A screen point therefore maps to a world direction through one constant
rotation, everywhere, always, which is what makes tap-to-move well defined at all. LBA2 has the
second mode, so the same scheme lands cleanly in interiors and meets the variable `BetaCam`
problem in exteriors. That is the seam described above, reached from the other end.

**There are three shipped answers to "what replaces the behaviour wheel", not two.** Retail keeps
it explicit. The Enhanced Edition removed it and split the job: dedicated buttons for Jump and
Attack, contextual inference for Run, a HUD toggle for Discreet, and a hotspot layer so the player
no longer has to guess what a behaviour is for. *Twinsen's Quest* (Microids, 2024, a remake rather
than a port) kept it explicit with dedicated run and dodge buttons, WASD and pad, rebindable.
#372 proposes a fourth arrangement and cites none of them.

Sources: the installed files at `E:\Steam\steamapps\common\Little Big Adventure` for everything
above the Twinsen's Quest line; [Microids announcement](https://www.microids.com/microids-announces-little-big-adventure-twinsens-quest/)
and [Steam](https://store.steampowered.com/app/2318070/Little_Big_Adventure__Twinsens_Quest/) for
that one. The [Magicball Network mobile review](https://magicball.net/lba1/articles/lba1-mobile-review/)
describes the touch scheme on mobile and matches the Touch-mode strings here.

## What the official LBA2 binaries say

Read from `E:\Steam\steamapps\common\Little Big Adventure 2`. That install ships two official
builds side by side, both stamped by 2.21, and neither is the 1997 executable:

| File | Build stamp |
|---|---|
| `TLBA2C.exe` | `tlba2-classic-Steam` |
| `TLBA2.exe` | `tlba2-retro-Steam v3.2.3 / 2point21 (Sep  6 2022)` |

This matters because these are the direct ancestor of this fork, so what they do is the baseline
that "parity" is measured against, not a third-party opinion.

**The cfg key scheme is inherited, not invented here.** Both carry the format strings `%s%d_1` and
`%s%d_2`, one parameterised helper taking the prefix, where this tree has two literals
(`Input%d_1`, `Gamepad%d`). `TLBA2C.exe` also carries `WinMode` and `AutoCameraCenter`, the legacy
key [CONFIG.md](../CONFIG.md) records this engine as still reading.

**They already have a gamepad key-name table, and this fork inherited it.** 25 entries in both
binaries:

```
Action Down, Action Right, Action Left, Action Up, Back, Select, Start,
Left Stick, Right Stick, Left Shoulder, Right Shoulder,
D-Pad Up/Down/Left/Right, L-Axis Left/Right/Up/Down, R-Axis Left/Right/Up/Down,
Left Trigger, Right Trigger
```

`CONFIG.CPP:253` holds the same list, same positional face-button naming, abbreviated in five
places (`LB`/`RB` for Left and Right Shoulder, `LT`/`RT` for the triggers, `D-Up` for D-Pad Up,
`L-Axis Btn` for Left Stick) and with `Guide` added. The abbreviations do not appear to be forced:
"Right Shoulder" is 14 characters and `string[14] = 0` leaves room for it. Matching the official
spelling is a cosmetic change with a real argument behind it, which is more than most cosmetic
changes have.

**Naming face buttons by position rather than letter is theirs and is worth keeping.** "Action
Down" rather than "A" sidesteps the Xbox and Nintendo layout swap. The internal enum here is
`K_GAMEPAD_A`/`B`/`X`/`Y` with comments mapping to `SDL_GAMEPAD_BUTTON_SOUTH` and friends, so the
positional truth is one layer down from the names. Worth knowing before anyone "fixes" the labels.

**The official builds have no keyboard key-name table at all.** Keyboard bindings are displayed
through the format `Key %02X`, a raw hex scancode, with `Joy %d` and `Btn %d` as the other
fallbacks. No `Space`, `Enter` or `Escape` strings exist in either binary. Two things follow:

- **#61's complaint is inherited.** A numeric, unreadable representation of a keyboard binding is
  what the official release does, so this is not a regression introduced by the fork.
- **The pattern #61 wants already exists in-tree, for the other device.** The gamepad has a name
  table; the keyboard does not. This fork already improved the *display* half by routing keyboard
  names through `GetKeyScancodeName`, which is SDL's name and English-only. What neither build has
  is the *file* half: the cfg is still integers on both sides, which is exactly what #61 asks for
  and what Doom 3's three-column `keynames[]` supplies.

**Stick quantisation is now confirmed across three generations.** `L-Axis Left/Right/Up/Down` and
`R-Axis ...` appear in the official table as bindable keys, so the 2022 port also presents a stick
as eight digital directions. With `JoyMakeBitfield` in the 1997 import and `ApplyStickDirection`
here, every version of this game has thrown the magnitude away at the same place. G3 is a property
of the lineage, not a port decision.

**Not found:** the bindable action labels are not in either binary. They come
from the localised text data, as they do here, so comparing this engine's 36 action names against
the official set needs an HQR extraction rather than a strings pass.

## What the retail text data carries

Extracted from `Common/TEXT.HQR` (442,979 bytes, the Steam LBA2 install) with
`scripts/dev/text_dump.py`, written for this and following the format in [TEXT.md](../TEXT.md).
The `sys` bank holds 164 slots, ids 6 to 268, in each of six languages.

**The 36 action labels are ids 100 to 135, in this engine's `I_*` bit order**, reached through
`GetMultiText(START_ACTION_TXT + n, ...)` at [CONFIG.CPP:384](../../SOURCES/CONFIG.CPP):

```
100 Forwards            108 Talk/Search             118 Inventory help/Keyboard configuration
101 Backwards           109 Center camera/Select    119 Save        125..131 Weapon 1..7
102 Turn left           110 Menus                   120 Load        132 Meca-Pinguin
103 Turn right          111 Holomap                 121 Options Menu  133 Jetpack On/Off
104 Use weapon          112 Pause                   122 Turn camera   134 Protection spell
105 Behavior menu       113 Sidestep                123 Next cameral level   135 Lightning spell
106 Inventory           114..117 Normal / Sporty /  124 Previous camera level
107 Behavior action              Aggressive / Discreet Behavior
```

Two things fall out. The retail vocabulary names 109 "Center camera/Select", which is the exact
pairing #365 proposes to split, so that issue is arguing with a shipped label. And id 123 reads
"Next cameral level" in the retail English data. [TEXT.md](../TEXT.md) records TEXT.HQR as a frozen
asset that cannot be regenerated, so that typo is not ours to fix.

### A localised key-name table already ships, and the engine uses four entries of it

**Ids 200 to 268 are 69 keyboard key names, present in all six languages.** Verified by dumping
each: id 226 reads `Left shift`, `Maj Gauche`, `Linke Umschalttaste`, `Mayús Izquierda`,
`Shift Sinistra`, `SHIFT Esquerda`. The table covers the numeric keypad including `Num Pad .` and
`Num Pad Enter`, the navigation cluster, the left and right variants of shift, ctrl and alt, the
function keys, the digit row, Space, Enter, Backspace, Tab, the lock keys, Print Screen, Pause,
and both Windows keys. It does not cover the letter keys or Escape.

**The engine reads four of them.** `GetKeyText` ([CONFIG.CPP:336](../../SOURCES/CONFIG.CPP)) calls
`GetMultiText(START_KEYS_TXT + 16 + button, ...)` with `button` reduced to 0..3, which lands on ids
216 to 219, `Up`/`Down`/`Left`/`Right`, and only on the legacy multi-axis joystick path (the Z, R,
U, V and H axes). Every keyboard binding instead goes to `GetKeyScancodeName`, which is SDL's name
and English only.

This changes what #61 is. It asks for human-readable key names; the readable, localised names are
already in the shipped data and have been since 1997, unused for their own purpose. The work is
wiring plus a scancode-to-id map, not authoring, and it would give the key config screen the same
six languages the rest of the menu has. What #61 also asks for, names in the **cfg file**, is a
separate half that this table does not supply, since the ids are LBA's own and not a text
representation of a scancode.

It also completes the comparison in the Doom 3 section: that engine's `keynames[]` carries a
localised display string as its third column. This engine has that column too. It is in TEXT.HQR
and nothing reads it.

### The two config screens draw their vocabulary from different systems

The keyboard config screen is retail throughout: ids 160 to 165 give it `Action`, `Key 1`, `Key 2`,
`Default Configuration`, `Restore Previous Configuration`, `Accept Configuration`, and its rows are
the action labels above. The gamepad config screen is community strings by way of
`GetLocalizedMenuLabel` and the hardcoded English `kNames[]` table at
[CONFIG.CPP:253](../../SOURCES/CONFIG.CPP).

That split is not a defect but it is the reason the two screens have drifted, which is what #508
reports and what #507 records as a pattern.

### The game menu is now mostly community strings

`GAMEMENU.CPP` has 9 `GetMultiText` sites against 15 `GetLocalizedMenuLabel` ones, with 24 labels
defined in [MENU_LABELS.H](../../SOURCES/MENU_LABELS.H). The retail main menu is ids 70 to 75,
`Resume Game` / `New Game` / `Load Game` / `Save Game` / `Options Menu` / `Quit`; the behaviour
names are 80 onward; the options strings are 11 to 50. Everything the fork has added since,
resolution, vsync, display mode, Auto camera, the controller screen, the language pickers and the
movie fit modes, has no TEXT.HQR id and cannot be given one.

So the menu's vocabulary is already two systems, and any input work that adds a surface adds to
the community half. [TEXT.md](../TEXT.md) states the rule this follows from: TEXT.HQR is a frozen
asset, so a new string has to come from the in-source table. A binding set expressed as data would
need its own name source for the same reason.

## Gaps this research found that no issue records

The reason for looking at the whole system rather than the issue list.

**G1. Rebinding the keyboard silently breaks the touch overlay.** The overlay presses default
scancodes. `K_UP` and `K_GRAY_UP` are the same scancode, so the D-pad works by default; rebind
`I_UP` to something else and the on-screen D-pad stops moving the hero, with no error and no way
for the player to connect the two. The overlay is also not re-layoutable for the same reason.
Nothing reads `DefKeys` in `TOUCH_INPUT.CPP`.

**G2. Menu navigation on a pad is a second, hardcoded binding table.** `GamepadKeys` governs
gameplay; `JoystickMenuNavOnly` governs menus and is not data. A screen must handle both to be
pad-complete, and neither config screen mentions the second one exists.

**G3. #372's walk-versus-run by stick actuation is architecturally blocked, and the block is
deeper than the funnel.** Two separate facts, and the second is the one that matters.

The funnel discards magnitude: `ApplyStickDirection` quantises a stick into 8 sectors and emits
digital scancodes. But **the 1997 code did the same thing**, so this is not something the port
lost. `JoyMakeBitfield` in the imported `SOURCES/JOYSTICK.CPP` (`333929ab`) read `joyGetPosEx`,
compared each of six axes against a threshold of a third of its range, and set one of two bits per
axis plus four POV-hat bits. `GetJoys(U32 *bitfield)` returned that bitfield, and the modern
function keeps the signature with `(void)bitfield;` as a fossil. If anything the current
quantiser is the more careful one, with 8 sectors, hysteresis and a configurable
`GamepadDeadzone` where 1997 had a fixed third.

The real block is that **nothing downstream could consume a magnitude if it survived.** Hero
locomotion is animation-driven, not velocity-driven. In `MOVE_MANUAL`
([OBJECT.CPP:4048](../../SOURCES/OBJECT.CPP)) a direction bit selects an animation and nothing
else: `Input & I_UP` calls `InitAnim(GEN_ANIM_MARCHE, ANIM_REPEAT, numobj)`, `I_DOWN` selects
`GEN_ANIM_RECULE`, and `I_LEFT` selects the turn-in-place animation `GEN_ANIM_GAUCHE` when the
hero is idle or advances `Obj.Beta` by `GetDeltaMove(&ptrobj->BoundAngle.Move)` when already
moving, an interpolator step that likewise takes no input scalar. The displacement itself is baked
into the animation keyframes and applied by `ObjectSetInterDep`, which is the mechanism
[MOVEMENT_FRAMERATE.md](../MOVEMENT_FRAMERATE.md) documents for #358.

So the hero's speed is a property of which animation is playing, and which animation plays is
chosen by `Comportement`. Speed is a mode, not an axis. There is no `forwardmove` scalar anywhere
for a stick to fill, which is the field Doom 3's `usercmd_t` spends a `signed char` on.

That makes #372's actuation-based walk/run the same underlying change as #412 and #358:
decoupling locomotion from animation selection. That belongs on the issue, because it moves the
request from "add an analog path" to "change how the hero moves".

The same property caps #359 differently: an 8-way virtual stick is reachable by copying the pad's
own quantiser into the touch overlay, because 8-way is all the hero can hear anyway. Anything
finer has nowhere to go.

**G4. There is no text entry for non-keyboard users, and the workaround is silent.** The
`TODO(input)` at GAMEMENU.CPP:1175 is accurate and the routing around it means a pad or touch
player never sees a name field, gets a datetime stem, and is not told why. This is the "virtual
keyboard" item as it actually exists today: not missing UI, but a silently different code path.

**G5. Dead input state is exported on the god-header bus.** `TabInputBit` and `NbInput` are
defined in `SOURCES/INPUT.CPP`, declared in `SOURCES/INPUT.H`, and read by nothing: `InitInput`
assigns `NbInput` and then uses its own static arrays, and `TabInputBit` is never read at all.
The only code that genuinely uses them is `SOURCES/CONFIG/INPUT.CPP`, and that directory is
referenced by no CMakeLists in the tree. It is the standalone DOS-era config utility, carrying its
own `MAIN.CPP`, `C_EXTERN.H`, `LBA2.CFG` and palette, and nothing builds it. Its header also
declares `TabInputBit` at a different size from the game's (`MAX_INPUT + 1` against
`MAX_INPUT * 2 + 1`). So two exported globals sit on the shared bus for the benefit of a tool that
is not built, which is exactly what an ownership move has to resolve rather than carry.

**G6. The binding table is the one layer with no test, and it sits between two that have one.**
Stated in the roadmap; confirmed here. `tests/input_funnel` deliberately fakes the table, so
`DefKeysDefault95`, `InitInput`'s folding, and the cfg read and write round trip are all
unasserted. The cfg round trip is the one with a history: `ReadInputConfig` carries a guard for
an all-zero binding table, and `WriteInputConfig` carries a comment about a first-session rebind
that used to vanish.

**G7. Suppression has no owner.** `NoRepeatInput` is a single file-static in a 59-line
translation unit, and the macros over it are used at 109 sites. Any of them can clear a
suppression another one set, and nothing records who set it.

**G8. Four bindable actions do not reach `Input` at all.** Slots 32 to 35, the spells, are read
by direct `CheckKey(DefKeys[..])` in PERSO.CPP because only the low 32 slots fit the bitfield.
They are in the config screen and in the cfg like every other binding, so from the player's side
they are ordinary, and from the engine's side they are a hole in the table.

**G9. "Profile" is already taken.** `--profile <name>` keeps a run's saves and settings under a
name ([CLI_ARGS.CPP](../../SOURCES/CLI_ARGS.CPP)), and `ChoosePlayerName` calls a save a profile
too. An input profile is a third meaning. Worth settling the vocabulary before anything is named.

## What a test could see

The question roadmap area 8 turns on, so recording what was found rather than what it would cost.

**Already pure and testable, untested.** `DefKeysDefault95` as data. `InitInput`'s folding of two
tables into one. `ReadInputConfig` and `WriteInputConfig` as a round trip, including the all-zero
guard. A key-name mapping, if #61 lands, is a pure bidirectional lookup, which is the shape
`tests/menu_keynav` already demonstrates with 40 cases over 33 lines of source.

**Pure but not yet extracted.** The layout legend beyond the keypad: modifier pairs, and the
alternates in `DefKeysDefault95` that are layout facts rather than preferences. `MENU_KEYNAV.CPP`
is the first piece and its header argues carefully for where the boundary is.

**Not reachable by a host test.** Consumption. 93 raw scancode sites and 81 `MyGetInput` loops
mean anything a screen does with a key. The existing net is the automation fixtures:
`test_input_injection.sh` and `test_input_hold_throttle.sh` drive input directly, and
`test_askchoice_menus_consume.sh`, `test_demo_behaviour_menu.sh` and the six `test_ui_menu_*.sh`
fixtures exercise screens that read input. They need retail data and a display, so they run when
someone remembers.

## What the Doom 3 reading contributes

Four of its findings land on gaps above rather than on general principle, which is why that
research is a companion rather than an appendix.

- Its `InhibitUsercmd( subsystem, bool )` bitmask is G7's shape: each subsystem owns one bit and
  cannot clear another's.
- Its `keynames[]` has three columns, name, code, and a **localised** display string, with
  `KeyNumToString( n, localized )` choosing. #61 asks for two of those three; the third matters
  here because the game already localises its menu labels but shows SDL's English scancode names
  on the key config screen.
- Its `KeysFromBinding` / `BindingFromKey` / `NumBinds` reverse lookups are what a config screen
  and a profile system need, and are what `CONFIG.CPP` currently does by indexing `DefKeys`
  positionally.
- Its split between a binding that resolves to an action id and a binding that is a text command
  is the design this engine already half has. The action table here *is* that, which is worth
  knowing before anyone proposes replacing it.

Its `usercmd_t` plus consistency-hash replay is the answer to a different question, the one
[INPUT_REPLAY_RESEARCH.md](INPUT_REPLAY_RESEARCH.md) asked, and it is recorded there rather than
counted as an input-system finding.

## Open questions

1. **Does the touch overlay's default-scancode coupling (G1) reproduce?** It follows from the
   code, but it has not been run. A rebind of `I_UP` and an Android or touch-emulation run would
   settle it, and decides whether G1 is a filed bug or a design note.

2. **Is menu pad navigation meant to be bindable (G2)?** If not, then the hardcoded path is
   correct and only needs to be documented and made complete. If so, it is a second table to fold
   in. That is a product call, not a code question.

3. **What does retail parity cover?** The default table is checkable. Whether `NoRepeatInput`'s
   exact semantics are part of the parity promise, or an implementation detail free to change
   behind the fixtures, decides how much of the funnel is frozen.

4. **Which of the 93 raw scancode sites actually want the binding layer?** Some are screens that
   should honour bindings. Some are genuinely physical, like a console toggle or a debug key. The
   split has not been made, and the size of cluster 1 depends entirely on it.

5. **What would an analog action look like here (G3)?** Not whether to build it. Whether the
   bitfield gains a parallel array of magnitudes, or whether analog stays a camera-style bypass,
   is the question #372 and #359 both wait on.

6. **Does the radius of a walking turn change with frame rate?** A hypothesis that falls out of
   G3, not a measurement. Holding a direction and a rotation together is supported and always was:
   the left/right block in `MOVE_MANUAL` is a separate `if`, and its rotate branch is taken
   precisely *because* a walk animation is playing (`GenAnim != GEN_ANIM_RIEN`), so the hero walks
   an arc. But the two halves of that arc are driven differently. Turning is
   `ChangeSpeedMove(&ptrobj->BoundAngle.Move, +/-1024)` read back through `GetDeltaMove`, which is
   `Acc += elapsed * Speed` against `TimerRefHR` with a carried remainder, so it is velocity times
   dt and frame-rate independent. Walking is animation keyframe translation applied once per
   rendered frame, which is #358 and is not. If both hold, then at high frame rates the hero turns
   correctly while covering less ground, and the same held input carves a tighter circle than it
   does at 60 fps. Measurable with `--fixed-dt` and a held `input up left`, against the sweep
   already in [MOVEMENT_FRAMERATE.md](../MOVEMENT_FRAMERATE.md). Worth knowing because it would
   mean #358 changes the shape of a path and not only its length.

## Reproduce

```bash
# Structure
wc -l SOURCES/INPUT.CPP SOURCES/JOYSTICK.CPP SOURCES/TOUCH_INPUT.CPP SOURCES/MENU_KEYNAV.CPP \
      LIB386/SYSTEM/INPUT.CPP LIB386/SYSTEM/KEYBOARD.CPP LIB386/SYSTEM/MOUSE.CPP

# The two funnels
grep -rn 'Input *& *I_' --include=*.CPP --include=*.H SOURCES LIB386 | wc -l          # 154
grep -rnE '(MyKey|Key) *== *K_' --include=*.CPP SOURCES | grep -v INPUT.CPP | wc -l   # 93
grep -rnE '(MyKey|Key) *== *K_' --include=*.CPP SOURCES | grep -v INPUT.CPP \
    | cut -d: -f1 | sort | uniq -c | sort -rn                                         # GAMEMENU 38, PERSO 19, CONFIG 14
grep -rn 'MyGetInput()' --include=*.CPP SOURCES | wc -l                               # 81
grep -rn 'ClearNoRepeatInput\|ClearWaitNoInput\|InitWaitNoInput\|InitWaitNoKey' \
     --include=*.CPP SOURCES | wc -l                                                  # 109

# Coverage
grep -rn 'SOURCES/INPUT.CPP' tests/*/CMakeLists.txt      # nothing
grep -rhoE '(SOURCES|LIB386)/[A-Za-z_0-9/]+\.CPP' tests/input_device/CMakeLists.txt \
     tests/input_funnel/CMakeLists.txt tests/menu_keynav/CMakeLists.txt
ls tests/automation/ | grep -iE 'input|menu'

# Gaps
grep -n 'K_' SOURCES/TOUCH_INPUT.CPP | head -20          # G1: hardcoded scancodes
grep -n 'return K_' SOURCES/JOYSTICK.CPP                 # G2: synthesised menu keys
sed -n '/ApplyStickDirection/,/^}/p' SOURCES/JOYSTICK.CPP | grep -n 'SetVirtualKeyDown'  # G3
grep -n 'TODO(input)' SOURCES/GAMEMENU.CPP               # G4
grep -rn 'TabInputBit\|NbInput' --include=*.CPP --include=*.H SOURCES LIB386  # G5
grep -rn 'CheckKey(DefKeys' SOURCES/PERSO.CPP            # G8
grep -n '"--profile"' SOURCES/CLI_ARGS.CPP               # G9

# Issues
gh issue list --state open --limit 400 --json number,title --jq '.[] | "\(.number)\t\(.title)"' \
  | grep -iE 'input|key|control|gamepad|joystick|pad|mouse|touch|bind|profile'

# Retail text data (paths are the Steam LBA2 install)
T='.../Little Big Adventure 2/Common/TEXT.HQR'
scripts/dev/text_dump.py "$T" --bank sys --range 100:135   # the 36 action labels
scripts/dev/text_dump.py "$T" --bank sys --range 200:268   # 69 key names
scripts/dev/text_dump.py "$T" --bank sys --range 160:165   # key config screen columns
scripts/dev/text_dump.py "$T" --bank sys --range 70:75     # retail main menu
for L in 0 1 2 3 4 5; do scripts/dev/text_dump.py "$T" --lang $L --bank sys --range 226:226; done
grep -c GetMultiText SOURCES/GAMEMENU.CPP                  # 9 retail sites
grep -c GetLocalizedMenuLabel SOURCES/GAMEMENU.CPP         # 15 community sites
```
