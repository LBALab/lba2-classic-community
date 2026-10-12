# Platform abstraction layer

The platform abstraction layer (PAL) separates live engine and game code from SDL. SDL3 is the only shipped implementation and remains the default. The boundary covers window/presentation, events, keyboard/gamepad/touch input, clocks, host paths, dialogs, clipboard, and diagnostics. Native Android context stays private to the SDL3 backend. Audio keeps its existing, independent AIL backend contract.

This implements the platform-decoupling part of [discussion #120](https://github.com/LBALab/lba2-classic-community/discussions/120), without the proposed directory-wide restructure, hardware renderer, separate headless backend, or main-loop inversion. The preceding audit remains in [the PAL extraction plan](plan/PLATFORM_PAL_PLAN.md); this document describes shipped code.

## Contract and selection

[PLATFORM.H](../LIB386/H/PLATFORM/PLATFORM.H) defines the C-compatible contract: fixed-width types, plain event/frame structs, opaque device/dialog handles, and one immutable `PlatformInterface` named `g_platform`. Implementation code stays C++98; the public headers also compile as C11. No SDL header or SDK handle appears in the contract.

| Group | Responsibility |
|---|---|
| `system` | Initialisation, wall-clock milliseconds, delay, performance counter/frequency, thread identity, CPU/RAM probes, errors, diagnostic callback |
| `events` | Initialisation, pumping, and polling backend-neutral `PlatformEvent` values |
| `window` | Surface lifetime, logical/window/display dimensions, coordinate mapping, fullscreen/vsync/relative mouse, indexed-frame presentation |
| `input` | Physical-key snapshot, layout-aware keycodes/names/modifiers, text input/cursor, gamepad lifetime and sampling |
| `host` | Executable/current/preference/home paths, directory operations, message boxes, asynchronous folder requests, clipboard, allocator pairing |
| `capabilities` | Meaningful windowed mode, temporary loss of a native window, touch-overlay support |

[The platform CMake file](../LIB386/PLATFORM/CMakeLists.txt) selects exactly one implementation at build time:

```bash
cmake -S . -B build -DPLATFORM_BACKEND=sdl3
cmake --build build
```

An unknown backend is a configuration error, not a fallback to SDL3. No runtime backend switching or partially populated production table is supported.

`lba2_platform` owns the implementation and its private SDK dependency. `lba2_platform_entry` owns the host entry-point convention: the SDL3 entry point calls the SDK-free game entry, `Lba2Main`, in [PERSO.CPP](../SOURCES/PERSO.CPP). This keeps SDL_main and Android's loader convention out of game logic.

## Dependency direction

```text
game / engine policy / software rasteriser
                    |
              PLATFORM.H
                    |
              lba2_platform
                    |
               selected SDK

game audio policy -> AIL headers -> selected AIL backend
```

The platform implementation does not link `sys`, `svg`, `console`, or game code. It receives frame/event data rather than reading `Log`, `TabKeys`, game globals, or renderer dirty boxes. Backend diagnostics use `Platform_Log` in [PLATFORM.CPP](../LIB386/PLATFORM/PLATFORM.CPP); the engine registers its logger through `Platform_SetLogOutput`, so there is no upward logging dependency.

The preserved Android service declarations in [ANDROID.H](../LIB386/H/SYSTEM/ANDROID.H) and their primitive type header are the only kernel-header exceptions. Their implementation belongs to the platform library, not `sys`.

`make arch-check` enforces SDK containment and the platform's include boundary, including new, unstaged files. The only preserved SDL exception outside the SDL3 and AIL backends is the unbuilt Win32 keyboard driver, `LIB386/SYSTEM/KEYBWIN.CPP`.

## Input and compatibility

[INPUT.H](../LIB386/H/PLATFORM/INPUT.H) owns physical-key IDs, keycode masks, modifiers, mouse buttons, and canonical gamepad buttons/axes. Physical IDs preserve existing cfg bindings, recordings, key-table packing, and the virtual gamepad range beginning at 1024. A backend whose native key IDs differ translates them at its boundary; it must not rewrite stored bindings.

Keyboard state contains 512 byte-valued entries, each 0 or 1. The engine still chooses the lowest held physical ID as `Key`, then runs the touch, harness, and recorder hooks in their established order. Keycodes and display names come from the backend's keyboard layout, not an ASCII guess.

Gamepad sticks retain signed 16-bit units with positive Y down; triggers are 0–32767. The game still owns deadzones, cardinal snap, hysteresis, bindings, first-press detection, and the analog right-stick camera. The PAL does not digitise axes or change controller feel.

Event payloads are plain data. Mouse coordinates are window pixels; finger coordinates are normalised over the full display. Text is borrowed until the next poll and must be copied if retained. Native cursor hiding and resize re-presentation happen inside backend polling, before delivery to the engine; the game event filter cannot suppress this host housekeeping. The engine retains its event-filter and handler ordering, including timer/video handling before focus state changes.

## Presentation and overlays

The software renderer still owns its 8-bit framebuffer, palette, dirty markers, nested lock count, and timing/overlay hooks. The historical `LIB386/SVGA/SDL.CPP` filename now contains an SDK-free framebuffer facade.

The backend receives a `PlatformFrame`: indexed pixels, width/height/byte pitch, 256 RGB triples, and an optional ARGB overlay callback. There is no global-framebuffer lookup or mandatory GPU/texture object in this boundary. Pixels and palette are borrowed for the duration of the present call.

Palette updates retain the existing API's source ordering, including partial updates. Each present snapshots its palette before callbacks run: a console command may change the next frame's palette, but must not change the table describing pixels already converted for the current frame.

The existing console, screenshot and touch compositors use `0xAARRGGBB` pixels and byte pitches. A native indexed/RGB565 backend may present its preferred format directly when no overlay is requested; when an overlay is requested, it must provide the compositor's ARGB view and convert that result to its output format. The shared `Platform_ConvertIndexedFrame` helper supplies exact indexed-to-ARGB conversion without touching row padding. This preserves existing overlays rather than introducing a new render interface.

The SDL3 implementation in [WINDOW.CPP](../LIB386/PLATFORM/SDL3/WINDOW.CPP) preserves nearest-neighbour scaling, SDL's letterboxed logical presentation, coordinate clamping, fullscreen state, and resolution changes that retain the window. Desktop presents write into a locked streaming texture. Android presents use a persistent staging buffer and texture upload, keeping the MTE-safe path inside the backend.

## Host services, ownership and errors

| Result | Ownership |
|---|---|
| Base/home path, scancode name | Borrowed from the backend |
| Current directory, preference path, clipboard text, gamepad-ID list | Owned; release through `g_platform.host.free_memory` |
| Open gamepad | Release through `close_gamepad`; its name is borrowed while open |
| Folder request | Poll to completion, consume/copy the selected path, then release through `end_folder_dialog` |
| Frame pixels/palette | Caller-owned; backend consumes or copies before returning |

Folder results distinguish pending, selected, cancelled, and error. SDL's worker callback publishes its result atomically; polling transfers any worker-thread error to the calling thread's error channel. Resource validation and path normalisation remain game-side.

Unsupported host services report failure with an explanatory error. Missing display dimensions are a normal failed query with outputs untouched; they are not invented display sizes. Backend diagnostics flow to the engine logger, while the engine's own structural log records dispatch directly and retain their severity/formatting rules.

`--headless` still selects SDL3 dummy video/audio drivers before initialisation; it is not a second PAL implementation. The engine continues to own fixed-dt simulation time independently of the backend's wall clock.

## Adding another backend

1. Add a backend directory under `LIB386/PLATFORM/` and implement every callback using only the public platform data contracts, its SDK, and portable helpers. Keep SDK types private.
2. Extend the backend selection in `LIB386/PLATFORM/CMakeLists.txt`, producing `lba2_platform` and the appropriate `lba2_platform_entry`. Discover/link the SDK there, not in game targets.
3. Translate native events, physical-key IDs, controller buttons and axis ranges into the fixed engine vocabulary. Supply capabilities appropriate to the host, including permanently fullscreen devices.
4. Present indexed pixels and handle overlays, scaling, and coordinate mapping consistently. Do not depend on engine framebuffer globals.
5. Supply host paths and meaningful failure results for unavailable clipboard/dialog services. Preserve allocation pairing and borrowed-data lifetimes.
6. Select or add an AIL audio backend separately; `SOUND_BACKEND=null` is the existing silent choice. The current `sdl` AIL implementation uses SDL3.
7. Add backend conformance coverage and run the SDK-free contract fixture, host suite, and existing ASM equivalence suite.

SDL2/SDL1 can implement the same contract without exposing their different event/renderer APIs. KallistiOS can present the software framebuffer through native facilities and map controller input into the same game policy. Neither implementation is shipped yet.

libretro additionally requires frontend-owned frame stepping and resumable modal/menu loops. The PAL removes SDL coupling; it does not make the existing blocking game loop a libretro core. Main-loop conversion is intentionally separate work.

## Boundaries not changed

Audio mixing remains AIL-owned. The software rasteriser and ASM-equivalence math are unchanged. File/archive IO keeps its existing portable C and system/file-source seams; a frontend needing a virtual filesystem must adapt those seams as well. Endianness, floating-point differences, game-state global ownership, and existing process-exit semantics remain the assumptions documented in [PLATFORM.md](PLATFORM.md).

## Checks

`tests/platform` registers three `host_quick` targets:

| Target | Coverage |
|---|---|
| `test_platform_contract` | SDL-free link of real engine facades and input policy; C11 and C++98 header compilation (including absence of C99 integer-constant macros); event order, focus/audio/timer coupling, fixed-dt/wrap, input hooks, pad edges/hotplug, touch wake/reapply/release, full-buffer conversion, palette snapshots and nested present/overlay ordering |
| `test_platform_sdl3` | Complete production table, physical-ID/keymap compatibility, exact event payloads, native virtual-gamepad sampling, paths/allocator/error ownership, worker-thread folder results, and byte-exact pixels read from the actual SDL renderer |
| `test_platform_sdl3_staging` | The same conformance checks compiled with the persistent-buffer/upload path, including frame resizing |

The staging and renderer-readback switches exist only in test builds. They exercise the Android presentation mechanism on desktop hosts; they do not substitute for Android JNI/window lifecycle or on-device MTE validation.

```bash
cmake -S . -B build -DLBA2_BUILD_TESTS=ON -DLBA2_BUILD_ASM_EQUIV_TESTS=OFF
cmake --build build --target host_tests
ctest --test-dir build -R '^test_platform_' --output-on-failure
make arch-check
./run_tests_docker.sh
```
