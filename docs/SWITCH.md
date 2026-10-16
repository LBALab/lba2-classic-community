# Nintendo Switch homebrew

The Switch target uses devkitPro/libnx and the SDL2 backend, with SDL audio and Smacker video. It produces a Homebrew Menu NRO, not a licensed Nintendo SDK application. Desktop defaults and gameplay bindings are unchanged. Hardware validation is required before treating the port as supported.

## Build with Docker

From the repository root:

```bash
bash scripts/dev/build-switch.sh
```

Docker Engine or Docker Desktop must be running. No host compiler, CMake, SDL or devkitPro installation is needed. The [image pin](../docker/switch-image.txt) selects an immutable official devkitPro SDK image, including its portlibs; ordinary builds never upgrade packages or download an unpinned SDK. The image has native amd64 and arm64 variants, so Apple Silicon does not require x86 emulation for this build.

The [wrapper](../scripts/dev/build-switch.sh) runs the [Switch preset](../CMakePresets.json) with devkitPro's official toolchain, preserves the caller's file ownership and mounts the clone at a stable container path. Build files live in `out/build/switch/`, staging in `out/switch/`, and logs in `build_logs/`. Keep this build directory separate from host builds. Git for the source revision and Bash are the other host requirements; Windows contributors can run the wrapper from WSL with Docker integration enabled.

Outputs:

| Path | Contents |
|------|----------|
| `out/lba2cc-switch.zip` | SD-card installation tree, README and license; no retail assets |
| `out/switch/switch/lba2cc/lba2cc.nro` | Homebrew executable with NACP metadata and devkitPro's default icon |
| `out/switch/debug/lba2cc.elf` | Unstripped, matching executable for debugging; not needed on the SD card |

The wrapper sets `SOURCE_DATE_EPOCH` from the HEAD commit time, fixing compiler date/time macros and ZIP timestamps. ZIP packaging includes only the three installation files, omits host UID/access-time metadata and uses UTC. Reproducibility means the same source revision, working-tree contents and SDK architecture; native amd64 and arm64 compiler distributions are not assumed to emit identical binaries. The engine still labels modified sources as dirty. A different image digest is an explicit SDK upgrade, and should be verified with a clean build.

## Install and supply game data

Extract the ZIP onto the SD-card root, then supply files from a legally owned retail installation:

```text
sdmc:/switch/lba2cc/
    lba2cc.nro
    README.txt
    LICENSE
    data/
        LBA2.HQR
        ... other retail HQR files ...
        Music/
        VIDEO/
        VOX/
    user/
```

The HQR files belong directly in `data/`; preserve the installation's music, movie and voice subdirectories and their contents. They are not included in the package. Use application mode/title takeover to launch the Homebrew Menu, rather than the memory-constrained Album applet. Use the game's Quit option to return cleanly.

The backend reports `sdmc:/switch/lba2cc/` as its installation root regardless of the launcher's working directory. Existing discovery finds its `data/` subdirectory without a Switch branch in game code. Writable settings, saves, persisted resource discovery and logs live under `sdmc:/switch/lba2cc/user/`; back up this directory before upgrading. Existing `--game-dir`, `--user-dir` and environment overrides retain their normal precedence if supplied by a launcher. Retail configuration remains an underlay, not a file to overwrite.

Missing-data and other message boxes use libnx's application-error applet and include the SD-card setup paths. Clipboard and folder-picker support are not added; the installation layout avoids needing a desktop picker.

## Platform boundary

Switch-specific code stays in the [SDL2 backend](../LIB386/PLATFORM/SDL2/). The native bridge is C because current libnx headers require newer C++ features, while the game and C++ backend remain C++98. No libnx SDK type enters the public PAL contract.

Presentation uses SDL2's GLES2 renderer to scale the existing indexed software framebuffer with letterboxing. The logical render resolution stays unchanged; the native window starts at handheld or docked dimensions, and the SDL2 Switch driver handles dock transitions. The backend advertises fullscreen-only operation and does not resize the physical output when changing framebuffer resolution.

SDL2 owns controller enumeration, native event pumping and audio output. The platform entry registers shutdown with `atexit`, covering both returns and the game's existing `exit()` paths. There is no new controller mapping, touchscreen overlay, custom-name keyboard, controller-disconnect pause policy or clock-speed override. Save creation retains the existing non-keyboard automatic-name path.

## Validation

Cross-compilation and successful NRO packaging do not prove runtime support. These are separate gates:

| Gate | What it proves |
|------|----------------|
| Docker Switch build | All shipped code compiles and statically links against the pinned target SDK; NACP and NRO packaging complete |
| Desktop SDL2 host suite | Shared presentation, audio, event and host-service behavior, including Switch device-root directory parsing |
| Docker ASM-equivalence suite | Original x86 behavior remains byte-for-byte matched by the portable engine implementations |
| Real console | The installation, lifecycle and controller-only experience actually work |

Run the desktop SDL2 commands in [PLATFORM_ABSTRACTION.md](PLATFORM_ABSTRACTION.md#sdl2-desktop-backend), plus `./run_tests_docker.sh` and `make arch-check`. The [Switch workflow](../.github/workflows/switch.yml) calls the same Docker wrapper and uploads the SD-card ZIP and separate debug ELF. It does not publish a release or claim hardware validation.

### Hardware acceptance checklist

Record console/system version, launch mode, controller configuration and tested commit with each result. None of the following is implied by a successful CI build:

- [ ] Missing data produces readable setup guidance, then exits without a hang.
- [ ] Data in the documented SD-card layout is discovered without launcher arguments.
- [ ] Boot logos and main menu render; the framebuffer keeps its aspect ratio.
- [ ] Start a new game with attached Joy-Cons, a detached Joy-Con pair and a Pro Controller; all required gameplay/menu actions work without a keyboard.
- [ ] Save, quit, relaunch and load; settings and saves survive in `user/`, with retail data untouched.
- [ ] Sound effects, music, speech and Smacker movies play; pause/resume and scene transitions do not lose audio.
- [ ] Controller disconnect/reconnect does not leave held movement or stale input; another supported controller can regain control.
- [ ] Dock/undock and HOME/sleep/wake restore usable video, input and audio without a simulation-time burst.
- [ ] Quit returns to the Homebrew Menu, writes settings and leaves no truncated save.

Single sideways Joy-Con play and custom save-name entry are not first-milestone requirements. Existing desktop crash capture does not install a native Switch exception handler; retain the matching debug ELF and collect the system's report for native crashes.
