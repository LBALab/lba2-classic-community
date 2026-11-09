# Xbox (UWP Dev Mode): first build probe

Status: experiment. Nothing here has run on Windows or a console yet.

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

- the app entry: hand-written `IFrameworkViewSource` and `IFrameworkView` objects passed to `CoreApplication::Run`, a loop in `Run` that pumps the window's dispatcher,
- presentation: a D3D11 swap chain on the CoreWindow whose back buffer is the game's size, written with `UpdateSubresource` and scaled by DXGI with `DXGI_SCALING_ASPECT_RATIO_STRETCH`, so no shader is needed,
- input: the first `Windows.Gaming.Input` gamepad's reading,
- paths: the `LocalState` folder from `ApplicationData`.

```sh
x86_64-w64-mingw32uwp-clang++ -std=gnu++98 -Wall -Wextra -O2 -municode -mwindows \
  tools/uwp_spike/uwp_spike.cpp -o uwp_spike.exe -ld3d11 -ldxgi -lruntimeobject -static
```

It builds with no warnings and imports only API sets and `d3d11.dll`. It has not run: that needs an app package and a Windows PC or an Xbox in Developer Mode.

### Shape of the backend

The contract has 63 callbacks. The null backend already does clocks, threads, CPU and memory probes, paths and directories with OS calls, and it builds for UWP unchanged, so the UWP backend can start from it:

| Group | UWP |
|---|---|
| `system` | As the null backend |
| `events` | `pump` calls `CoreDispatcher::ProcessEvents`; key, focus, size and suspend events come from CoreWindow handlers |
| `window` | The spike's swap chain; fullscreen only, as on Switch |
| `input` | `Windows.Gaming.Input.Gamepad` for pads; CoreWindow key events, mapped to the engine's physical key IDs, for a keyboard |
| `host` | Paths under `LocalState`; message boxes, folder dialogs and clipboard report unsupported, as the null backend does |
| entry | `wWinMain` runs `CoreApplication::Run`; the view's `Run` calls `Lba2Main`. The engine's blocking loop fits, because the event poll pumps the dispatcher |

Audio is separate. The SDL audio module reaches SDL only through its output transport (`LIB386/AIL/SDL/AUDIO_DEVICE.H`), but that header uses `SDL_AudioSpec` and SDL's WAV loader. A UWP build needs that transport on XAudio2 or WASAPI and those two types replaced with neutral ones.

### Testing

- Host tests, on any OS: keep the translations SDK-free, as `DIRECTORY_PATH.H` is for SDL2: virtual key to physical key ID, gamepad reading to engine buttons and axes, letterbox coordinate mapping, path building. Plus the contract fixture for every callback that does not need WinRT.
- CI build: the UWP executable built with llvm-mingw in Linux Docker, with an import check that fails on any DLL outside the API sets, `d3d11`, `dxgi` and `xaudio2_9`. This catches desktop-only calls without Windows.
- Run: a sideloaded package on a Windows PC runs the same code as an Xbox, without a console. The console is the last check.

## Also needed

- Game data: copied to `LocalState` through the Dev Mode Device Portal.
- Lifecycle: UWP suspends and resumes apps instead of letting them exit.
- Packaging: an `AppxManifest.xml`, icons and a signed MSIX. Microsoft's cross-platform `makemsix` may build one without Windows; not tried.
