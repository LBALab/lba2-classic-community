# Platform abstraction layer

The platform abstraction layer (PAL) separates live engine and game code from SDL. SDL3 is the default interactive implementation; SDL2 is an optional desktop implementation with audio; the null implementation provides SDK-free offscreen presentation for desktop automation. The boundary covers window/presentation, events, keyboard/gamepad/touch input, clocks, host paths, dialogs, clipboard, and diagnostics. Android requires SDL3, and native Android context stays private to that backend. Audio keeps its existing, independent AIL backend contract.

This implements the platform-decoupling and headless-backend parts of [discussion #120](https://github.com/LBALab/lba2-classic-community/discussions/120), without the proposed directory-wide restructure, hardware renderer, or main-loop inversion. The preceding audit remains in [the PAL extraction plan](plan/PLATFORM_PAL_PLAN.md); this document describes shipped code.

## Contract and selection

[PLATFORM.H](../LIB386/H/PLATFORM/PLATFORM.H) defines the C-compatible contract: fixed-width types, plain event/frame structs, opaque device/dialog handles, and one immutable `PlatformInterface` named `g_platform`. Implementation code stays C++98; the public headers also compile as C11. No SDL header or SDK handle appears in the contract.

| Group | Responsibility |
|---|---|
| `system` | Initialisation, wall-clock milliseconds, delay, performance counter/frequency, thread identity, CPU/RAM probes, errors, diagnostic callback |
| `events` | Initialisation, pumping, and polling backend-neutral `PlatformEvent` values |
| `window` | Surface lifetime, logical/window/display dimensions, coordinate mapping, fullscreen/vsync/present filter/relative mouse/mouse grab, indexed-frame presentation |
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

## SDK-free null backend

The [null backend](../LIB386/PLATFORM/NULL/BACKEND.CPP) links the whole game without SDL. It has no physical window, display, keyboard, gamepad, clipboard, or dialogs. Presentation allocates an owned ARGB buffer, converts indexed frames, and runs the same overlay callback; clocks, host paths and directory operations use desktop OS facilities. Unavailable services report errors rather than fabricate results. Requested fullscreen state is retained for compatibility, but actual fullscreen is always false. Keyboard snapshots and controller lists are empty; the control harness and recorder still inject input through the engine hooks.

```bash
cmake -S . -B build-null -DPLATFORM_BACKEND=null -DSOUND_BACKEND=null \
  -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON \
  -DLBA2_BUILD_TESTS=ON -DLBA2_BUILD_ASM_EQUIV_TESTS=OFF
cmake --build build-null --target lba2cc host_tests
ctest --test-dir build-null -L host_quick --output-on-failure
```

SDL package discovery is disabled in the Linux SDK-free CI job, which builds the game and host tests, rejects SDL dynamic linkage, and boots the executable through help and an isolated missing-data failure. The latter initializes host services and writes its own log without retail assets. With retail data, use the normal harness flags including `--headless`, `--no-autosave` and an owned `--user-dir`; this backend does not invert the blocking game loop.

`PLATFORM_BACKEND=null` requires `SOUND_BACKEND=null`. The SDL audio implementation uses the selected SDL2/SDL3 platform's SDK target, so choosing it with a non-SDL platform is an explicit configuration error. Null builds retain Smacker video decoding by default, with silent audio. Android's Activity entry point is not supported by the desktop null backend.

## SDL2 desktop backend

The [SDL2 backend](../LIB386/PLATFORM/SDL2/BACKEND.CPP) implements the same contract without linking SDL3. Its minimum SDK version is specified by `find_package` in [the platform CMake file](../LIB386/PLATFORM/CMakeLists.txt); driver hints require that floor. SDL3 remains the default and the only Android implementation.

```bash
cmake -S . -B build-sdl2 -DPLATFORM_BACKEND=sdl2 -DSOUND_BACKEND=sdl \
  -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON \
  -DLBA2_BUILD_TESTS=ON -DLBA2_BUILD_ASM_EQUIV_TESTS=OFF
cmake --build build-sdl2 --target lba2cc host_tests
ctest --test-dir build-sdl2 -L host_quick --output-on-failure
```

SDL2 translates controller device indices on add events to stable instance IDs, and translates those IDs back to indices when opening a controller. Text events carry embedded storage rather than SDL3's pointer payload: polling retains the native event until the next poll. Physical key IDs and controller units retain the same engine vocabulary.

Presentation uses a streaming texture and explicit letterboxing. Nearest-neighbour filtering remains the default; linear filtering is supported and retained across texture/window recreation. SDL2 refuses pixelart filtering with an explanatory error without changing the active mode, allowing the engine's configuration policy to select its fallback. SDL2's logical-size feature rewrites mouse and finger events, so the backend computes the presentation rectangle and window-to-frame coordinates itself, leaving input events in their contract-defined units.

SDL2 has no native folder-dialog API. Those callbacks fail with an explanatory error; use `--game-dir`, `LBA2_GAME_DIR`, or normal resource discovery instead. No shell command or extra dialog dependency is introduced. Message boxes and clipboard remain native SDL2 services. `SDL2::SDL2-static` is selected when `LBA2_LINK_STATIC=ON`.

Directory creation makes missing parents while preserving existing filesystem roots. On Windows, a UNC path such as `\\server\share\profile` creates only descendants of the existing server/share root; it never tries to create the server or share. Root-only requests verify that the root exists and is a directory.

Some distributions package `sdl2-compat`, which implements the SDL2 API on SDL3. Such a package can satisfy CMake but is not evidence of SDL3-independent operation. Backend validation should use genuine SDL2.

The independent Linux SDL2 CI job builds genuine upstream SDL2 at the supported API floor read from CMake, disables SDL3 package discovery, builds the whole game and host suite, rejects SDL3 dynamic linkage, and runs dummy-driver conformance plus an isolated missing-data boot.

### Nintendo Switch SDL2 target

The Switch homebrew preset reuses SDL2 with a native C bridge for SD-card roots, output dimensions and libnx application-error dialogs. It selects static portlibs and a Switch entry point that registers teardown for `exit()` as well as normal returns. The backend advertises fullscreen-only operation; GLES2 scales the unchanged software framebuffer to handheld/docked output. Device roots such as `sdmc:/` are preserved during parent-directory creation. SDL's desktop filesystem helpers are not used on Switch because the pinned portlibs disable them.

See [SWITCH.md](SWITCH.md) for the Docker build, fixed installation and writable paths, supported scope and required hardware checks. SDL3 remains the desktop default and the Android backend.

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

Game code should use an existing engine facade when it covers the operation: for example, `InitWindow` and `Window_SetVSync` in [WINDOW.CPP](../LIB386/SYSTEM/WINDOW.CPP), and framebuffer operations in [SDL.CPP](../LIB386/SVGA/SDL.CPP). These facades retain engine-owned policy and compatibility. Direct `g_platform` calls are appropriate for backend-neutral infrastructure and host services without a facade; they must not bypass input, timing, or framebuffer policy. Neither route permits SDK calls in game or engine code.

`make arch-check` enforces SDK containment and the platform's include boundary, including new, unstaged files. The only preserved SDL exception outside the SDL2/SDL3 and AIL backends is the unbuilt Win32 keyboard driver, `LIB386/SYSTEM/KEYBWIN.CPP`.

## Input and compatibility

[INPUT.H](../LIB386/H/PLATFORM/INPUT.H) owns physical-key IDs, keycode masks, modifiers, mouse buttons, and canonical gamepad buttons/axes. Physical IDs preserve existing cfg bindings, recordings, key-table packing, and the virtual gamepad range beginning at 1024. A backend whose native key IDs differ translates them at its boundary; it must not rewrite stored bindings.

Keyboard state contains 512 byte-valued entries, each 0 or 1. The engine still chooses the lowest held physical ID as `Key`, then runs the touch, harness, and recorder hooks in their established order. Keycodes and display names come from the backend's keyboard layout, not an ASCII guess.

Gamepad sticks retain signed 16-bit units with positive Y down; triggers are 0–32767. The game still owns deadzones, cardinal snap, hysteresis, bindings, first-press detection, and the analog right-stick camera. The PAL does not digitise axes or change controller feel.

Event payloads are plain data. Mouse coordinates are window pixels; finger coordinates are normalised over the full display. Text is borrowed until the next poll and must be copied if retained. Native cursor hiding and resize re-presentation happen inside backend polling, before delivery to the engine; the game event filter cannot suppress this host housekeeping. The engine retains its event-filter and handler ordering, including timer/video handling before focus state changes.

## Presentation and overlays

The software renderer still owns its 8-bit framebuffer, palette, dirty markers, nested lock count, and timing/overlay hooks. The historical `LIB386/SVGA/SDL.CPP` filename now contains an SDK-free framebuffer facade.

`CopyVideoArea` copies dirty rectangles from the render buffer into the persistent indexed presentation surface, `Phys`. `PresentFrame` submits that surface, not `Log`: the renderer may prepare a full destination image offscreen while a zoom reveals only part of it. Pixels outside the updated rectangles retain the previous visible frame. Creation and resizing initialise the presentation surface to colour index zero.

The backend receives a `PlatformFrame`: indexed pixels, width/height/byte pitch, 256 RGB triples, and an optional ARGB overlay callback. There is no global-framebuffer lookup or mandatory GPU/texture object in this boundary. Pixels and palette are borrowed for the duration of the present call.

Palette updates retain the existing API's source ordering, including partial updates. Each present snapshots its palette before callbacks run: a console command may change the next frame's palette, but must not change the table describing pixels already converted for the current frame.

The existing console, screenshot and touch compositors use `0xAARRGGBB` pixels and byte pitches. A native indexed/RGB565 backend may present its preferred format directly when no overlay is requested; when an overlay is requested, it must provide the compositor's ARGB view and convert that result to its output format. The shared `Platform_ConvertIndexedFrame` helper supplies exact indexed-to-ARGB conversion without touching row padding. This preserves existing overlays rather than introducing a new render interface.

The SDL3 implementation in [WINDOW.CPP](../LIB386/PLATFORM/SDL3/WINDOW.CPP) defaults to nearest-neighbour scaling (`set_present_filter` can choose linear or, on SDL 3.4 or newer, pixelart), and preserves SDL's letterboxed logical presentation, coordinate clamping, fullscreen state, and resolution changes that retain the window. Desktop presents write into a locked streaming texture. Android presents use a persistent staging buffer and texture upload, keeping the MTE-safe path inside the backend.

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

Path removal succeeds if the path is already absent. Directory removal requires an empty directory; it never recursively deletes contents.

`window.init` is idempotent and prepares display discovery before framebuffer sizing; the later `create` call creates the actual surface. Message-box and folder-dialog services initialise their own native prerequisites, so callers do not manage video subsystems.

`--headless` selects the SDL2/SDL3 dummy video driver through the PAL and the matching dummy audio driver through `AIL_SetHeadless` before initialisation. The engine continues to own fixed-dt simulation time independently of the backend's wall clock.

## Adding another backend

1. Add a backend directory under `LIB386/PLATFORM/` and implement every callback using only the public platform data contracts, its SDK, and portable helpers. Keep SDK types private. Populate the callback table by named fields, as in [the SDL3 table builder](../LIB386/PLATFORM/SDL3/BACKEND.CPP), so callbacks with identical signatures cannot silently shift when the contract changes.
2. Extend the backend selection in `LIB386/PLATFORM/CMakeLists.txt`, producing `lba2_platform` and the appropriate `lba2_platform_entry`. Discover/link the SDK there, not in game targets.
3. Translate native events, physical-key IDs, controller buttons and axis ranges into the fixed engine vocabulary. Supply capabilities appropriate to the host, including permanently fullscreen devices.
4. Present indexed pixels and handle overlays, scaling, and coordinate mapping consistently. Do not depend on engine framebuffer globals.
5. Supply host paths and meaningful failure results for unavailable clipboard/dialog services. Preserve allocation pairing and borrowed-data lifetimes.
6. Select or add an AIL audio backend separately; `SOUND_BACKEND=null` is the existing silent choice. The `sdl` AIL implementation uses the selected SDL2/SDL3 SDK, sharing decoding and voice policy through a private output transport.
7. Add a behavioral check for every callback, including ownership, error results, and observable state where applicable; a non-null function pointer alone does not prove correct wiring. Run the SDK-free contract fixture, host suite, and existing ASM equivalence suite.

SDL1 can implement the same contract without exposing its different event/renderer APIs. KallistiOS can present the software framebuffer through native facilities and map controller input into the same game policy. Neither implementation is shipped yet.

libretro additionally requires frontend-owned frame stepping and resumable modal/menu loops. The PAL removes SDL coupling; it does not make the existing blocking game loop a libretro core. Main-loop conversion is intentionally separate work.

## Boundaries not changed

Audio mixing remains AIL-owned. The software rasteriser and ASM-equivalence math are unchanged. File/archive IO keeps its existing portable C and system/file-source seams; a frontend needing a virtual filesystem must adapt those seams as well. Endianness, floating-point differences, game-state global ownership, and existing process-exit semantics remain the assumptions documented in [PLATFORM.md](PLATFORM.md).

## Checks

`tests/platform` registers the shared fixture and the selected backend's `host_quick` targets:

| Target | Coverage |
|---|---|
| `test_platform_contract` | SDL-free link of real engine facades and input policy; C11 and C++98 header compilation (including absence of C99 integer-constant macros); event order, focus/audio/timer coupling, fixed-dt/wrap, input hooks, pad edges/hotplug, touch wake/reapply/release, full-buffer conversion, palette snapshots, zoom rectangles preserving the visible background and nested present/overlay ordering |
| `test_platform_sdl3` | Complete production table; physical-ID/keymap/modifier compatibility; exact event payloads and native polling housekeeping; fullscreen requests versus actual state, vsync, present filter read back from stretched pixels, relative mouse, cursor and text-input behavior; native virtual-gamepad sampling; paths/directory/allocator/error ownership; modal dispatch and worker-thread folder results; diagnostic severity/userdata; and byte-exact pixels read from the actual SDL renderer |
| `test_platform_sdl3_staging` | The same conformance checks compiled with the persistent-buffer/upload path, including frame resizing |
| `test_platform_sdl2` | Desktop SDL2 event payloads and borrowed text, unmodified mouse/finger coordinates, controller instance IDs and sampling, fullscreen rollback, host services/errors, diagnostics, lifecycle and byte-exact real-renderer frames |
| `test_switch_host` | Switch C bridge against a native-service stub: fixed SD-card paths, handheld/docked dimensions, exact dialog contents and propagation of create/show/oversize failures; not a native applet or GPU test |
| `test_platform_callbacks` | Every callback on the selected production backend is non-null; named-field checks are generated from the public contract, and each deliberately omitted callback is detected |
| `test_sdl_audio` | Simultaneous sample/music/video transports, queued source bytes, gain, pause and WAV loading on either SDK; SDL2 additionally compares full converted output against an independent native converter over deterministic random inputs and checks mixing, EOF, independent stream lifetime and recursive callback locking |
| `test_sdl_audio_device` | SDL2 production callback/device consumption with dummy audio, without deterministic-read probes |
| `test_platform_null` | SDK-free production callbacks, byte-exact offscreen frames and overlays, resize/lifecycle, empty input, host paths/clocks and explicit unavailable-service errors; selected with the null backend |
| `test_platform_null_game` | Whole SDK-free executable help and isolated missing-data boot/log initialization; selected with the null backend |

The staging, renderer-readback, modal-launch, and text-focus probes exist only in test builds. Modal probes check native flags, arguments, result ownership, and errors without opening OS dialogs; the focus probe selects the real dummy-driver window while SDL still starts and stops text input. These checks do not validate native dialog UI, physical keyboard focus, or controller hardware. The staging checks exercise the Android presentation mechanism on desktop hosts; they do not substitute for Android JNI/window lifecycle or on-device MTE validation.

```bash
cmake -S . -B build -DLBA2_BUILD_TESTS=ON -DLBA2_BUILD_ASM_EQUIV_TESTS=OFF
cmake --build build --target host_tests
ctest --test-dir build -R '^test_platform_' --output-on-failure
make arch-check
./run_tests_docker.sh
```
