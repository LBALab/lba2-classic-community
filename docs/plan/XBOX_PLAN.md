# Xbox (UWP Dev Mode): first build probe

Status: experiment. Nothing here runs on a console yet.

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

Linking proves less than it seems: the UWP import libraries still resolve some desktop-only functions. The UWP executable imports these from `KERNEL32.dll`:

- From our code: `GlobalMemoryStatusEx` (`PLATFORM/NULL/BACKEND.CPP`), `MoveFileExA` (`SYSTEM/FILES.CPP`).
- From the mingw-w64 runtime: `GetFileAttributesA` (`dirent`, `access`), `VirtualProtect` and `VirtualQuery` (pseudo-relocations), `SetUnhandledExceptionFilter` (startup), `GetModuleFileNameA`.

Whether each is allowed has to be checked with the Windows App Certification Kit or on a console. `MoveFileExA` and `VirtualProtect` are the likely problems; UWP has `MoveFileFromApp` and `VirtualProtectFromApp`.

## The blocker: SDL

SDL2's UWP backend (`src/core/winrt`, `src/video/winrt`) is written in C++/CX (`ref class`, `^` handles, built with `-ZW`). Only MSVC compiles C++/CX, so llvm-mingw cannot build SDL2 for UWP. SDL3 dropped UWP and supports only the GDK.

Options, roughly cheapest first:

1. Build SDL2 for UWP with MSVC as a DLL and link the llvm-mingw engine against it. SDL's API is plain C and both sides use the UCRT, so this can work, but it needs a Windows machine with Visual Studio and the UWP workload.
2. Build the whole tree with MSVC or clang-cl. MSVC is not supported today (see [TOOLING.md](../TOOLING.md)).
3. Write a small UWP platform backend in C++/WinRT, which clang can compile. This means a new presentation, input and audio layer.

## Also needed, whichever option

- Folders: a UWP app sees only its package (read-only) and its `LocalState` folder. `SDL2_GetHomeDirectory` reads `USERPROFILE`, which a UWP app does not have, and the current directory is the package. On UWP, SDL2 offers `SDL_WinRTGetFSPathUTF8` and `SDL_GetPrefPath`. Game data would be copied to `LocalState` through the Dev Mode Device Portal.
- Lifecycle: UWP suspends and resumes apps instead of letting them exit.
- Packaging: an `AppxManifest.xml`, icons, and a signed MSIX. CI can build a package; only a console can run it.
