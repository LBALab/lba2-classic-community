# Xbox (UWP Dev Mode): first build probe

Status: experiment. The game runs on an Xbox Series S in Developer Mode through its own UWP platform backend, silent, with the gamepad.

The target is a UWP app sideloaded onto an Xbox in Developer Mode, the same homebrew posture as the [Switch port](../SWITCH.md). The licensed GDK route needs an NDA and publisher rights, so it is out of scope for this repository.

## What was tried

[llvm-mingw](https://github.com/mstorsjo/llvm-mingw) ships `x86_64-w64-mingw32uwp-*` compilers. They define `WINAPI_FAMILY=WINAPI_FAMILY_APP`, which hides desktop-only Windows APIs, and link against the UWP C runtime. [`cmake/llvm-mingw-x86_64-uwp.cmake`](../../cmake/llvm-mingw-x86_64-uwp.cmake) wraps them:

```sh
export LLVM_MINGW_ROOT=/path/to/llvm-mingw-<version>-ucrt-macos-universal
cmake -S . -B out/build/uwp-null -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw-x86_64-uwp.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DPLATFORM_BACKEND=null -DSOUND_BACKEND=null -DMVIDEO_BACKEND=null
ninja -C out/build/uwp-null
```

`-DLLVM_MINGW_TRIPLE=x86_64-w64-mingw32` builds the same tree as a desktop Windows control.

## Results

With the null backends, the engine compiles and links for UWP after three small changes:

| File | Change |
|---|---|
| `LIB386/H/SYSTEM/CRASH_CORE.H` | A UWP build takes the null crash backend. The Windows one needs `RtlCaptureContext`, `AddVectoredExceptionHandler`, `SetThreadStackGuarantee` and Toolhelp, none of which a UWP app may call. |
| `LIB386/SYSTEM/FILES.CPP` | `GetCurrentProcessId` instead of `_getpid`, which the UWP CRT does not declare. Same value on desktop. |
| `LIB386/SYSTEM/LOG.CPP` | No console colour setup in a UWP build. A UWP app has no console. |

The desktop control build still imports the crash handler and console APIs; the UWP build imports neither.

CMake must not link the desktop libraries. `SOURCES/CMakeLists.txt` adds `gdi32 user32 ddraw winmm kernel32` to every Windows build, and those bind the imports to desktop DLL names ahead of the toolchain's `windowsapp` library. The toolchain file sets `LBA2_WINDOWS_APP`, which skips that line and CMake's default Windows library list. With both gone, every engine import goes through an `api-ms-win-*` API set, as UWP expects:

- From our code: `GlobalMemoryStatusEx` (`PLATFORM/NULL/BACKEND.CPP`), `MoveFileExA` (`SYSTEM/FILES.CPP`, from `api-ms-win-core-kernel32-legacy`).
- From the mingw-w64 runtime: `GetFileAttributesA` (`dirent`, `access`), `VirtualProtect` and `VirtualQuery` (pseudo-relocations), `SetUnhandledExceptionFilter` (startup), `GetModuleFileNameA`.

`windowsapp` lists the APIs a UWP app may call, so these should be allowed. The Windows App Certification Kit or a console has the final word; `MoveFileExA` is the one to watch, since UWP also has `MoveFileFromApp`. `libc++.dll` and `libunwind.dll` would ship in the package, or link statically.

## SDL cannot be used

SDL2's UWP backend (`src/core/winrt`, `src/video/winrt`) is written in C++/CX (`ref class`, `^` handles, built with `-ZW`). Only MSVC compiles C++/CX, so llvm-mingw cannot build SDL2 for UWP. SDL3 dropped UWP and supports only the GDK.

## Our own UWP backend

The other route is a `PLATFORM_BACKEND=uwp` of our own. llvm-mingw ships the WinRT interface headers (`windows.applicationmodel.core.h`, `windows.ui.core.h`, `windows.gaming.input.h`, `windows.storage.h`), Direct3D 11, DXGI, XAudio2 and WASAPI, with their import libraries. The WinRT headers compile as C++98 and as C11, so a backend can call WinRT as plain COM interfaces, with no C++/CX and no C++/WinRT.

### Spike

[`tools/uwp_spike/uwp_spike.cpp`](../../tools/uwp_spike/uwp_spike.cpp) tries the parts a backend needs, in C++98:

- the app entry: hand-written `IFrameworkViewSource` and `IFrameworkView` objects passed to `CoreApplication::Run`, a loop in `Run` that pumps the window's dispatcher, and two hand-written event delegates (`Activated`, `GamepadAdded`),
- presentation: a D3D11 swap chain on the CoreWindow whose back buffer is the game's size, written with `UpdateSubresource` and scaled by DXGI with `DXGI_SCALING_ASPECT_RATIO_STRETCH`, so no shader is needed,
- input: the first `Windows.Gaming.Input` gamepad's reading,
- paths: the `LocalState` folder from `ApplicationData`.

```sh
x86_64-w64-mingw32uwp-clang++ -std=gnu++98 -Wall -Wextra -O2 -municode -mwindows \
  tools/uwp_spike/uwp_spike.cpp -o uwp_spike.exe -ld3d11 -ldxgi -lruntimeobject -static
```

It builds with no warnings and imports only API sets and `d3d11.dll`.

To run it on a Windows PC in Developer Mode, copy `uwp_spike.exe` next to [`AppxManifest.xml`](../../tools/uwp_spike/AppxManifest.xml), `Assets` and [`run.ps1`](../../tools/uwp_spike/run.ps1), and run the script. It registers the folder as an app without signing, starts it, and prints the log the app writes to `LocalState`.

Run on Windows 11 Home 22H2 (build 22622), 2026-11-09: the app started from the package, activated its window (1200×900), created the swap chain and presented about 105 frames a second, every `Present` returning `S_OK`. The screen was locked, so the picture itself was not seen, and no gamepad was connected, so the gamepad path did not run.

### On the Xbox

The spike runs on an Xbox Series S in Developer Mode (OS 26100), 2026-11-09: the square draws on the 4:3 frame with black bars left and right, the left stick moves it, B quits. Its log shows a 1920×1080 window and the gamepad reading.

[`xbox-run.sh`](../../tools/uwp_spike/xbox-run.sh) is the loop, about 17 seconds end to end: build with llvm-mingw, stop the running copy, deploy the folder as a loose app (no packaging, no signing), launch, wait, save a screenshot of the TV picture and the app's `LocalState`, stop it. It drives the console through a Device Portal helper script (`XBOX_SH`) that keeps the login and pins the portal's certificate.

- The console needs a signed-in user. Without one, a launch fails with `0x8004090A` and a sign-in prompt waits on the TV.
- A deploy over a running copy fails with HTTP 500, since the app holds its `.exe` open; the loop stops it first.
- A POST without a body needs `Content-Length: 0`, or the portal answers HTTP 411.

### Packaging without the Windows SDK

[`pack_appx.py`](../../tools/uwp_spike/pack_appx.py) packs a folder into an `.appx`: a ZIP64 file with stored entries, the block map (one SHA-256 per 64 KiB block) and `[Content_Types].xml`. `osslsigncode` (Homebrew) signs it with a self-signed certificate whose subject matches the manifest's `Publisher`:

```sh
openssl req -x509 -newkey rsa:2048 -nodes -keyout key.pem -out cert.pem -days 365 -subj "/CN=lba2cc-dev" \
  -addext extendedKeyUsage=codeSigning -addext keyUsage=digitalSignature -addext basicConstraints=critical,CA:FALSE
python3 tools/uwp_spike/pack_appx.py FOLDER uwp_spike-unsigned.appx
osslsigncode sign -certs cert.pem -key key.pem -in uwp_spike-unsigned.appx -out uwp_spike.appx
```

- `osslsigncode` 2.15 rewrites `[Content_Types].xml` compressed but keeps a stored entry's method, which corrupts it; the packer writes that entry deflated.
- Windows 11 opens the package and refuses it only because the certificate is not trusted. The Xbox in Developer Mode installs it through the Device Portal as it is.
- The manifest has a `uap:SplashScreen`, which the schema requires; desktop registration from a folder did not insist.

### What SDL2 does

SDL2 2.32.10's UWP code was read for its shape (`src/core/winrt`, `src/video/winrt`, `src/render/direct3d11`, `src/joystick/windows/SDL_windows_gaming_input.c`, `src/audio/wasapi`, `src/filesystem/winrt`). About 5,000 lines are UWP-specific, but most cover XAML hosting, phones, rotation, pointers and OpenGL ES, which lba2cc does not need. Its gamepad code is plain C over the same WinRT COM interfaces, so the C++/CX is a convenience in SDL, not a requirement.

Same as the spike:

- `main` runs on the CoreWindow thread, inside `IFrameworkView::Run`, with no second thread. `SDL_PumpEvents` calls `ProcessEvents(ProcessAllIfPresent)`. The engine's blocking loop fits the same way.
- The app quits by returning from `Run`; SDL never calls `CoreApplication::Exit`.
- Paths: the install folder from `Package::Current->InstalledLocation`, the writable one from `ApplicationData::Current->LocalFolder`. Plain CRT file I/O works in both.

Taken into the spike:

- The window is activated from `CoreApplicationView::Activated`.
- Something must subscribe to `Gamepad.GamepadAdded`, or an Xbox reports no gamepads ("HACK ALERT" in `SDL_winrtapp_direct3d.cpp`). The handler does nothing.

To take into the backend:

- Suspend: take a deferral in `Suspending`, call `IDXGIDevice3::Trim`, complete within about 5 seconds. `ProcessEvents` does not return while the app is suspended, so the game loop just stops inside the pump. Anything that must be saved is saved from the handler.
- While the window is hidden, pump with `ProcessOneAndAllPending`, which blocks, so the game does not spin.
- `Closed` does not mean quit by itself. After it, the pump must stop calling `ProcessEvents` and the engine must be told to leave.
- B on an Xbox raises `SystemNavigationManager.BackRequested`. SDL turns it into a key and marks it handled only when asked to. The mingw-w64 headers do not declare `SystemNavigationManager`, so its two interfaces would be declared by hand.
- `InputPane.Visible` is wrong on Xbox; the on-screen keyboard is tracked with its Showing and Hiding events. lba2cc only needs it for save names, which already fall back to automatic names.
- Gamepad readings are all zero when the app loses focus. SDL treats that as centred sticks.
- Keyboard: `KeyDown`/`KeyUp` give a virtual key and a scan code. SDL maps them through a virtual-key table and `src/events/scancodes_windows.h` to SDL scancodes, which are the engine's physical key IDs. Left and right Shift come from the scan code.
- WACK has failed when DXGI reports no display outputs; display queries need a fallback to the window bounds.
- Device loss: `DXGI_ERROR_DEVICE_REMOVED` from `Present` or `ResizeBuffers` means recreating the device and swap chain.
- Window size in pixels is the CoreWindow bounds times `LogicalDpi / 96`.

Where the spike differs:

- Presentation. SDL makes the swap chain the window's size with `DXGI_SCALING_NONE` and draws a textured quad with precompiled shaders (`ps_4_0_level_9_1` bytecode in C arrays). It never writes into the back buffer with `UpdateSubresource` and never relies on DXGI scaling, so the spike's shortcut is unproven. It also cannot honour `set_present_filter`, since DXGI picks the filter. The backend should draw a quad. The shader bytecode needs `fxc`, which is Windows-only; SDL's own arrays are zlib-licensed and could be reused, or the shaders compiled once on Windows and checked in.
- Audio. SDL uses WASAPI, activated with `ActivateAudioInterfaceAsync` on the default render device, then an ordinary event-driven WASAPI loop. It has no XAudio2 code. The activation needs one more hand-written COM object (`IActivateAudioInterfaceCompletionHandler`, agile). `AUDCLNT_E_DEVICE_INVALIDATED` means activating again.

### Shape of the backend

The contract has 63 callbacks. The null backend already does clocks, threads, CPU and memory probes, paths and directories with OS calls, and it builds for UWP unchanged, so the UWP backend can start from it:

| Group | UWP |
|---|---|
| `system` | As the null backend |
| `events` | `pump` calls `CoreDispatcher::ProcessEvents`; key, focus, size and suspend events come from CoreWindow handlers |
| `window` | A window-sized swap chain and a textured quad, as SDL draws; fullscreen only, as on Switch |
| `input` | `Windows.Gaming.Input.Gamepad` for pads; CoreWindow key events, mapped to the engine's physical key IDs, for a keyboard |
| `host` | Paths under `LocalState`; message boxes, folder dialogs and clipboard report unsupported, as the null backend does |
| entry | `wWinMain` runs `CoreApplication::Run`; the view's `Run` calls `Lba2Main`. The engine's blocking loop fits, because the event poll pumps the dispatcher |

Audio is separate. The engine's SDL audio module reaches SDL only through its output transport (`LIB386/AIL/SDL/AUDIO_DEVICE.H`), but that header uses `SDL_AudioSpec` and SDL's WAV loader. A UWP build needs that transport on WASAPI, as SDL does, and those two types replaced with neutral ones.

### Testing

- Host tests, on any OS: keep the translations SDK-free, as `DIRECTORY_PATH.H` is for SDL2: virtual key to physical key ID, gamepad reading to engine buttons and axes, letterbox coordinate mapping, path building. Plus the contract fixture for every callback that does not need WinRT.
- CI build: the UWP executable built with llvm-mingw in Linux Docker, with an import check that fails on any DLL outside the API sets, `d3d11`, `dxgi` and `xaudio2_9`. This catches desktop-only calls without Windows.
- Run: a sideloaded package on a Windows PC runs the same code as an Xbox, without a console. The console is the last check.

## The backend

`PLATFORM_BACKEND=uwp` ([`LIB386/PLATFORM/UWP`](../../LIB386/PLATFORM/UWP)) runs the game on an Xbox Series S, 2026-11-09: it starts a new game, A answers the dialogue, the left stick walks, B no longer closes the app. Silent (`SOUND_BACKEND=null`) and without video (`MVIDEO_BACKEND=null`).

```sh
export LLVM_MINGW_ROOT=/path/to/llvm-mingw-<version>-ucrt-macos-universal
cmake -S . -B out/build/uwp -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw-x86_64-uwp.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DPLATFORM_BACKEND=uwp -DSOUND_BACKEND=null -DMVIDEO_BACKEND=null
tools/uwp/xbox-run.sh 20                                   # build, run, screenshot, fetch LocalState
tools/uwp/xbox-push-data.sh /path/to/LBA2/Common VIDEO     # once, after the first run
```

The exe imports only API sets, `d3d11.dll` and `d2d1.dll`; libc++ is linked in.

| Part | How |
|---|---|
| Base | Clocks, CPU and memory probes, the error slot, the install folder, directory creation and removal come from `HOST_OS.CPP`, which the null backend uses too. The UWP table is its own, by named fields. |
| Entry | `wWinMain` runs `CoreApplication::Run`; the view's `Run` calls `Lba2Main` on the window's thread. `Suspending` takes a deferral and trims the device. |
| Events | CoreWindow key, character, activation, visibility, close and size events go into a queue that `poll` drains. `pump` calls `ProcessEvents`, blocking while the window is hidden and never after `Closed`. |
| Present | A swap chain the window's size (`DXGI_SCALING_NONE`); Direct2D draws the frame into a letterbox with nearest or linear filtering, so no shader is compiled. Pixelart is refused. A lost device is recreated. |
| Pads | `Windows.Gaming.Input` is listed and read on every pump; arrivals, departures, presses and stick moves become events. The `GamepadAdded` subscription is made at startup. |
| Keys | Scan codes (E0 prefix included) to physical key IDs, with virtual keys as fallback; the Xbox's gamepad and navigation keys are dropped, since the pad is read directly. |
| Back | `SystemNavigationManager.BackRequested` is marked handled; unhandled, B closes the app. The interfaces are declared by hand in `NAVIGATION.H`, their IDs from the Windows metadata as the `windows` crate prints them; the handler's ID is computed by the WinRT rule, checked by reproducing the headers' `EventHandler<Gamepad>` ID. |
| Paths | Saves, config and log in `LocalState`, whose path is converted to the ANSI code page the engine's file calls read (refused if it cannot be spelled there). Game data in `LocalState\Game`, passed as `--game-dir` when that folder exists; otherwise the game's own search, which starts in the install folder. `LocalState\args.txt` adds arguments, one per line, each logged. |

The parts that need no Windows header are host-tested: translations (keys, pad buttons and axes, the events a pad reading adds, letterbox) in [`TRANSLATE.H`](../../LIB386/PLATFORM/UWP/TRANSLATE.H), and the event queue in [`QUEUE.H`](../../LIB386/PLATFORM/UWP/QUEUE.H), which never drops a quit: after `Closed` the window is not pumped again, so a lost quit would leave the game running. `test_platform_uwp_translate` checks them on every host against documented set-1 scan codes and virtual keys and the `GamepadButtons` values, and, when SDL3 is in the build, every key name and unshifted key code against SDL3's. Shifted codes are checked against the US layout directly: SDL3 without video has no Shift layer.

On the console:

- Registering a loose folder again wipes the app's `LocalState`, game data and saves included. `xbox-run.sh` replaces only `lba2cc.exe` in the registered folder, and registers only on the first run or with `--register`.
- After registering, `LocalState` does not exist until the app first runs, and the portal answers 200 to uploads into it while dropping them. `xbox-push-data.sh` checks first.
- An app cannot look at the folders above its own (`Q:\Users` reads as not found), so creating a directory walks up to the deepest folder that exists instead of down from the drive.
- `VIDEO.HQR` is required even without a video backend; voices and music are optional.
- The pad is listed a moment after start, as a hot-plug; the first boot logs "Joystick not detected", then "Gamepad connected".
- As a sideloaded "App", the game sees 1 GB of RAM, which is enough.

## Also needed

- Audio: the AIL output transport on WASAPI (see above).
- Lifecycle: the game is not told it is being suspended; a save from the `Suspending` handler would keep progress if the console ends the app.
- Pointer: no mouse or touch; the backend reports pointer modes as unsupported.
- Tests: a CI build of the UWP exe with an import check.
- Packaging: an `AppxManifest.xml`, icons and a signed MSIX. Microsoft's cross-platform `makemsix` may build one without Windows; not tried.
