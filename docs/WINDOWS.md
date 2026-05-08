# Building on Windows

This guide covers building LBA2 on Windows using MSYS2.

## Prerequisites

1. **MSYS2** - Download and install from [msys2.org](https://www.msys2.org/)
2. After installation, update the package database:
   ```bash
   pacman -Syu
   ```
   (You may need to restart the terminal and run it again.)

## Choosing an environment

MSYS2 provides multiple environments. Choose based on your needs:

| Environment | Architecture | C Runtime | Windows Compatibility | Recommendation |
|-------------|--------------|-----------|----------------------|----------------|
| **UCRT64** | 64-bit | UCRT | Windows 10+ (built-in) | Recommended for most users |
| **MINGW64** | 64-bit | MSVCRT | All Windows versions | Use for Windows 7/8 compatibility |
| **MINGW32** | 32-bit | MSVCRT | All Windows versions | Legacy only |

**Important:** Launch the correct shell for your chosen environment:
- Start Menu → "MSYS2 UCRT64" (or MINGW64, MINGW32)

## Installing dependencies

From the appropriate MSYS2 shell:

### UCRT64 (recommended)

```bash
pacman -S mingw-w64-ucrt-x86_64-toolchain \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-sdl3
```

### MINGW64

```bash
pacman -S mingw-w64-x86_64-toolchain \
          mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-sdl3
```

### MINGW32 (legacy)

```bash
pacman -S mingw-w64-i686-toolchain \
          mingw-w64-i686-cmake \
          mingw-w64-i686-ninja \
          mingw-w64-i686-sdl3
```

> **Note:** The package name is lowercase `sdl3`, not `SDL3`.

## Building

### Using CMake presets (recommended)

```bash
# UCRT64
cmake --preset windows_ucrt64
cmake --build --preset windows_ucrt64

# MINGW64
cmake --preset windows_mingw64
cmake --build --preset windows_mingw64
```

Presets use `CMAKE_BUILD_TYPE=Release` by default (same idea as Linux/macOS presets and CI). For a Debug build (easier debugging, no optimization), override when configuring:

```bash
cmake --preset windows_ucrt64 -DCMAKE_BUILD_TYPE=Debug
cmake --build --preset windows_ucrt64
```

### Manual build

```bash
cmake -B build -G Ninja
cmake --build build
```

The executable will be at `out/build/windows_ucrt64/SOURCES/lba2.exe` (preset) or `build/SOURCES/lba2.exe` (manual).

## Running

### Game assets required

LBA2 requires the original game files to run. These are not included in this repository. You need a legitimate copy of the game from [GOG](https://www.gog.com/game/little_big_adventure_2) or [Steam](https://store.steampowered.com/app/398000/Little_Big_Adventure_2/).

### SDL3.dll

When running outside the MSYS2 environment:
- Copy `SDL3.dll` to the same directory as `lba2.exe`
- Or add the MSYS2 bin directory to your PATH

The DLL is located at:
- UCRT64: `C:\msys64\ucrt64\bin\SDL3.dll`
- MINGW64: `C:\msys64\mingw64\bin\SDL3.dll`
- MINGW32: `C:\msys64\mingw32\bin\SDL3.dll`

## Troubleshooting

### "command not found" for cmake/ninja/gcc

Make sure you're using the correct MSYS2 shell (UCRT64/MINGW64/MINGW32), not the generic MSYS shell. The compilers are only available in the MinGW-based shells.

### Package not found

Update your package database:
```bash
pacman -Syu
```

### SDL3 not found during cmake

Ensure you installed the SDL3 package for your specific environment (e.g., `mingw-w64-ucrt-x86_64-sdl3` for UCRT64).

### Build fails with ASM/UASM errors

This project no longer requires UASM. The x86 assembly has been ported to C++. If you see ASM-related errors, ensure you're using the latest code and haven't set `-DENABLE_ASM=ON`.

### Binary crashes immediately

- Verify SDL3.dll is accessible (see above)
- Make sure you have the game assets in the expected location
- Run from a command prompt to see error messages

## Legacy 32-bit build

For a 32-bit build, use the `windows` preset:

```bash
cmake --preset windows
cmake --build --preset windows
```

This requires MSYS2 MINGW32 and produces a 32-bit executable at `out/build/windows/SOURCES/lba2.exe`.
