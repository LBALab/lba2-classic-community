# Quest controls

Default Meta Quest 3 Touch controller mapping in the OpenXR build. The
bindings live in `LIB386/SYSTEM/OPENXR.CPP`; `SOURCES/JOYSTICK.CPP` maps the
OpenXR actions onto the same virtual scancodes as the SDL gamepad path, and
`SOURCES/INPUT.CPP` (`GamepadKeysDefault`) maps those scancodes to the game
actions.

## Gameplay

| Input | Action | Notes |
|---|---|---|
| Left stick | Move | Camera-relative in first person |
| Left stick click | Dodge | `I_ESQUIVE` |
| Right stick | Camera orbit / turn | Right-stick X turns in first person |
| Right stick click | Recentre camera | `I_RETURN` |
| Right trigger | Throw / use weapon | In first person, hold to arm the magic ball and release to throw |
| Left trigger | Backward; spell modifier when held with a face button | `I_DOWN` |
| Left trigger + X | Penguin spell | `K_GAMEPAD_DPAD_RIGHT` |
| Left trigger + Y | Jetpack spell | `K_GAMEPAD_DPAD_DOWN` |
| Left trigger + A | Protection spell | `K_GAMEPAD_DPAD_UP` |
| Left trigger + B | Lightning spell | `K_GAMEPAD_DPAD_LEFT` |
| Right grip | Behaviour action | `I_ACTION_M` |
| Left grip | Behaviour menu | `I_COMPORTEMENT`; closes on release |
| Right A | Talk / search / use | `I_ACTION_ALWAYS` |
| Right B | Camera mode | Classic / Auto / First person |
| Left X | Inventory | `I_INVENTORY` |
| Left Y | Holomap | `I_HOLOMAP` |
| Left menu button | In-game menu | `I_MENUS` |
| Head tracking | Look | Body and hands follow when full body is enabled |

## First person

| Input | Action |
|---|---|
| Head tracking | Look |
| Left stick | Camera-relative movement |
| Right stick X | Continuous turn (about 180°/s) |
| Right stick Y | Unused |
| Right stick click | Recentre camera |
| Right trigger | Arm the magic ball on press, throw on release |
| Left trigger | Backward; spell modifier when held with a face button |
| Right grip | Behaviour action |
| Left grip | Behaviour menu; closes on release |
| Right B | Camera mode switch; leaves first person |
| Face buttons | Same as gameplay |

## Interior stereo

| Input | Action |
|---|---|
| Left stick | Move hero |
| Right stick X | Pan the room |
| Left thumbrest touched + right stick Y | Zoom |
| Head tracking | Interior hover look |
| Right stick click | Recentre camera |
| Right trigger | Throw / use |
| Right grip | Behaviour action |
| Left grip | Behaviour menu; closes on release |
| Right B | Camera mode switch |
| Face buttons | Same as gameplay |

## Menus

| Input | Action |
|---|---|
| Left stick | Move selection |
| A | Confirm |
| B or Start / left menu | Back or close |
| A in the load list | Load the selected save |

## Inventory

| Input | Action |
|---|---|
| A | Select / use the highlighted item |

## Dialogue

| Input | Action |
|---|---|
| A | Skip the current dialogue, like ESC |

## Cheats

Touch the right thumbrest and press a face button. The chord is edge
triggered, so holding it applies the cheat once. Touching the pad alone does
nothing, and the same buttons keep their normal meaning when the thumb is off
it.

| Input | Action |
|---|---|
| Right thumbrest + X | Full: refill health, magic and clover box |
| Right thumbrest + Y | Gold: add 50 kashes or zlitos |

## Missing or awkward controls

- Left trigger is backward while forward is left-stick only.
- Interior zoom requires touching the left thumbrest while moving the right
  stick vertically, which is not discoverable in game.

## Suggested improvements

1. Add an on-screen spell legend so the left-trigger chord is discoverable.
2. Add a Quest control preset and a first-run or in-game control legend.
3. Add comfort options: snap turn (30°/45°), turn-speed control, movement
   vignette, dominant-hand swap and haptics.
4. Make the throw grip configurable: hold-to-throw versus toggle-to-throw.
5. Add a Quest-specific remap page rather than relying on the generic
   virtual-scancode gamepad list.

## See also

- [ANDROID.md](ANDROID.md) — Android and OpenXR build details.
- [CONTROLLER.md](CONTROLLER.md) — camera input sources and gamepad axes.
