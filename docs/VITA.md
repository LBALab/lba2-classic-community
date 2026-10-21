# PlayStation Vita homebrew

The Vita target reuses the SDL2 platform and audio backends with Smacker video. It produces a homebrew VPK, not a licensed Sony SDK application. Basic boot is confirmed on a physical Vita, including executable FTP upload, companion launch and DEBUGNET output. The remaining hardware acceptance checks below are separate from cross-compilation and packaging.

## Build with Docker

```bash
bash scripts/dev/build-vita.sh
```

Run from the repository root with Docker Engine or Docker Desktop running. Bash and Git are the other host requirements; Windows contributors can use WSL with Docker integration. No host VitaSDK, SDL2, CMake or Python installation is required for ordinary builds.

The [image pin](../docker/vita-image.txt) fixes the official VitaSDK and target libraries by digest, with native amd64 and arm64 variants. The [derived SDK image](../docker/Dockerfile.vita) builds an unmodified, checksum-pinned upstream SDL2 with its native GXM renderer. This replaces the full SDK's vitaGL-enabled SDL2 so the executable does not require the optional shader compiler or taiHEN imports. The source version and SHA-256 live in that Dockerfile; ordinary builds do not upgrade SDK packages.

The [wrapper](../scripts/dev/build-vita.sh) builds that cached image, runs the [Vita preset](../CMakePresets.json), stages outputs and saves both SDK-image and game-build output under `build_logs/`. It preserves the caller's UID/GID, mounts the source at `/src` and sets `SOURCE_DATE_EPOCH` from HEAD. SDK changes require an explicit image/source-pin update. This does not promise byte-identical VPK archives: VitaSDK's archive tool controls ZIP metadata.

| Path | Contents |
|------|----------|
| `out/vita/lba2cc-vita.vpk` | Installable package, LiveArea images, README and license; no retail files |
| `out/vita/eboot.bin` | Matching SELF for incremental FTP deployment |
| `out/vita/title-id.txt` | Package title ID, generated from the packaging CMake file |
| `out/vita/debug/lba2cc.elf` | Unstripped ELF for debugging |

Build files stay in `out/build/vita/`, separate from desktop builds. [vita-package.cmake](../cmake/vita-package.cmake) owns the title ID and derives the `NN.NN` package version from the major and minor components of the root `VERSION` file: `0.13.2-dev` becomes `00.13`, and `1.0.0` becomes `01.00`. Patch numbers, prerelease suffixes and build metadata are deliberately omitted, so patch releases share a Vita package version. Each retained component must fit in two digits; larger values fail configuration rather than wrap or truncate. The engine retains its full version, and changing `VERSION` refreshes package metadata on the next build. Changing the title ID creates a different installed application. Keep it stable for upgrades.

LiveArea images in [packaging/vita/](../packaging/vita/) are generated from the existing [application icon](../packaging/lba2cc.png), not retail assets. After changing that source, run `python3 scripts/packaging/make-vita-icons.py` with Pillow installed and include the regenerated images. The generator produces PNG-8 indexed palettes, as required by the [VitaSDK image guidance](https://github.com/vitasdk/samples#notes-on-images). Normal cross-builds consume the committed images and need no Pillow.

## Install and supply game data

Transfer the VPK and install it with VitaShell. Copy files from a legally owned retail installation:

```text
ux0:/
    app/
        LBA2CC001/
            eboot.bin
            README.txt
            LICENSE
            sce_sys/
    data/
        lba2/
            LBA2.HQR
            ... other retail HQR files ...
            Music/
            VIDEO/
            VOX/
        lba2cc/
            user/
```

VitaShell creates the `app/` tree; do not substitute manual copying for the initial VPK installation. Put HQR files directly in `ux0:/data/lba2/` and preserve music, voice and movie subdirectories and their contents.

The platform entry defaults `LBA2_GAME_DIR` to `ux0:/data/lba2/` only when it is absent or empty. Explicit launcher arguments and non-empty environment overrides retain their normal precedence. Missing data uses the normal explicit-path error, not fallback discovery of another install. SDL2's native Vita message box displays setup errors.

`app0:/` is the read-only executable root. Settings, saves, persisted discovery and logs belong under `ux0:/data/lba2cc/user/`, independently of the launcher's working directory. Back up that directory before upgrading. Existing `--user-dir` and environment overrides remain available; the package does not overwrite retail data or user settings.

Directory creation checks existing parents before attempting to create missing children. Vita can refuse `mkdir("ux0:/data")` with permission denied even though `stat` confirms that parent is an existing directory; its creation permission is not a prerequisite for using it.

## Controls and presentation

The handheld has no L2/R2 triggers or stick-click buttons. The Vita preset enables `LBA2_GAMEPAD_STICK_LAYOUT`: left-stick up/down replaces trigger-based forward/backward movement, and Circle plus a direction dodges during gameplay. Circle retains its Return action in modal interfaces, including inventory and holomap. The same build option is available on desktop for regression checks; it is off by default and Switch/desktop defaults remain unchanged. Explicit cfg bindings override compiled defaults: an existing Vita profile or a copied PC cfg may need its gamepad bindings reset before Circle dodges.

| Control | Action |
|---------|--------|
| Left stick | Move forward/backward, turn |
| L / R | Behaviour / throw |
| Triangle / Square | Inventory / action |
| Cross / Circle | Always-action / dodge (Circle remains Return in modal interfaces) |
| Start / Select | Menu / holomap |
| Right stick | Existing camera controls |
| D-pad | Existing spell bindings; fixed UI navigation in menus |
| Front-screen tap | Recenter camera during gameplay |

A front-screen press produces one recenter input through the configured camera-recenter binding, including a tap completed between polls. Holding or dragging does not repeat it. Touches in modal interfaces or while unfocused do not queue an action for later, and the rear touch panel is ignored. This is a camera shortcut, not the Android touch overlay; no on-screen buttons appear.

Save creation keeps the existing automatic-name path for non-keyboard input. Custom save-name keyboard entry, touch overlays, rear-touch bindings, clock-speed overrides and a new controller-disconnect policy are not added.

SDL2 owns controller sampling, lifecycle events and audio. GXM scales the unchanged indexed software framebuffer to the handheld's 960x544 output with letterboxing, without OpenGL contexts or shader compiler plugins. The backend advertises fullscreen-only operation, so framebuffer resolution changes do not resize the physical display. The entry reserves a 128 MiB newlib heap for engine caches and registers shutdown with `atexit`, covering both normal returns and existing game `exit()` paths.

The shared SDL2 audio transport converts queued PCM in bounded playback chunks, rather than converting a whole music track on the game thread. WAV reading and ADPCM decoding use a bounded background producer after a small initial prefill, with a private reader for music in a mounted disc image. This avoids multi-second conversion and loading stalls on Vita without changing music routing or pause/resume. The pinned Vita SDL already satisfies the resampler requirement; see [AUDIO.md](AUDIO.md#sdl2-and-sdl3-output-transport).

## Upload and launch over the network

These modes mirror the FTP and companion commands in the supplied [OpenSupaplex example](https://github.com/sergiou87/open-supaplex/blob/040f18a199e72e03622c8fb0b87fe1a9d4d3fdf7/vita/CMakeLists.txt#L243-L260):

```bash
bash scripts/dev/build-vita.sh
bash scripts/dev/run-vita.sh 192.168.1.42 vpksend
# Install ux0:/data/lba2cc-vita.vpk with VitaShell once.
bash scripts/dev/run-vita.sh 192.168.1.42 send
bash scripts/dev/run-vita.sh 192.168.1.42
```

Replace the address with the Vita's IPv4 address. The [launcher](../scripts/dev/run-vita.sh) uses Python's standard-library FTP/TCP client inside the pinned SDK image; no host curl, netcat or Python is needed. It uploads an existing build and does not rebuild.

| Mode | Result |
|------|--------|
| `vpksend` | Uploads the VPK to `ux0:/data/lba2cc-vita.vpk`; installation remains a VitaShell action |
| `send` | Replaces `ux0:/app/<generated-title-id>/eboot.bin` only |
| `launch` (default) | Replaces that executable, then sends `launch <title-id>` followed by newline to vitacompanion |

Quit the game before replacing its executable. Install the VPK first; an executable upload does not create LiveArea registration. `send` and `vpksend` work with an FTP server on port 1337, including VitaShell's FTP mode. Automatic launch additionally needs [vitacompanion](https://github.com/devnoname120/vitacompanion), listening on TCP 1338. Use its own installation instructions for the version on your console. Upload and command errors fail explicitly; a failed upload never launches the application.

Both services are unauthenticated/unencrypted development endpoints. Use only a trusted LAN, never expose either port to the internet, and disable them when not developing. Docker only makes outbound connections, so no port publishing or broadcast discovery is required. A remote Docker daemon is not supported: bind mounts belong to the daemon's host.

Unlike Switch nxlink, vitacompanion's launch protocol does not stream application stdout/stderr. Read `ux0:/data/lba2cc/user/adeline.log` through FTP or VitaShell; retain the matching debug ELF for native crash diagnosis. Normal builds introduce no DEBUGNET dependency or game network service.

## Diagnose a silent startup failure

The game creates `ux0:/data/lba2cc/user/` automatically; players do not need to create it. If neither that directory nor `adeline.log` appears, startup may have failed before user-directory setup, or directory creation itself failed. Missing logs alone do not identify the cause.

Build a diagnostic VPK:

```bash
bash scripts/dev/build-vita.sh --diagnostics
```

This enables `LBA2_VITA_DIAGNOSTICS`, without changing the engine's Release optimizations. Install that VPK; it includes the DEBUGNET license notice. At the start of `main`, before SDL or data discovery, the diagnostic entry opens `ux0:/data/lba2cc-boot.log` directly. It does not need the missing user directory. The file is replaced on each launch and captures unbuffered stdout/stderr, engine entry/return and shutdown. Ordinary engine logging continues once initialized, including filesystem errors with the failing path. Diagnostic hooks stay in the Vita entry and logging module rather than tracing individual shared-backend operations.

If the boot file still does not appear, check that the diagnostic executable was installed and that `ux0:/data/` is writable. A loader, runtime or global-constructor failure before `main` cannot be captured by this trace. If it does appear, preserve it before launching again; the last completed milestone narrows the investigation without guessing from missing retail files.

### Stream diagnostic output with DEBUGNET

DEBUGNET comes from the pinned SDK; no extra console plugin is required. It is separate from vitacompanion and uses UDP port 18194. Create the ASCII file `ux0:/data/lba2cc-debugnet.txt` with your computer's LAN IPv4 address on one line, for example:

```text
192.168.1.10
```

This is the receiver computer's address, not the Vita's. Start the Python standard-library receiver on that computer before launching the game:

```bash
python3 scripts/dev/vita-log.py --bind 192.168.1.10 | tee build_logs/vita-live.log
```

Allow inbound UDP 18194 through the computer's firewall on the trusted LAN. The receiver prints only received bytes to stdout and its listening/error status to stderr. Ctrl+C stops it. Omitting `--bind` listens on all IPv4 interfaces; do not expose the port to the internet.

The diagnostic build reads the receiver file once at launch. With no receiver file, it explicitly reports that network output is disabled and continues with the local boot trace. Invalid configuration, initialization failures and send errors are recorded locally; they do not stop the game. UDP is best-effort and does not prove a receiver is listening, so keep the local file even when streaming. The DEBUGNET library emits its own initialization messages; subsequent stdout/stderr is sent without an extra prefix or truncation, in bounded datagrams.

Output streams are restored before diagnostic teardown. The SDK's DEBUGNET heap remains owned by the process until exit: its finish helper frees that heap without terminating the networking subsystem that references it. A plain `build-vita.sh` explicitly disables diagnostics again, including in a reused build directory.

## Validation

| Gate | What it proves |
|------|----------------|
| Docker Vita build | All shipped code statically cross-links, converts to SELF, and packages as a VPK |
| `test_vita_deploy.py` | Real wrapper/client against loopback FTP/TCP peers: exact uploaded bytes/paths, newline-terminated launch, upload-only modes and failure propagation; VPK identity, contents and image dimensions |
| Desktop SDL2 host suite | Shared SDL2 presentation/audio/events, Vita paths/window setup, device-root parsing and the opt-in stick binding table |
| `test_vita_diagnostics` (Linux) | Actual cookie-stream capture/restoration and long output, with native DEBUGNET/socket stubs; missing/invalid receiver config, initialization/send errors and failed trace creation |
| Docker ASM-equivalence suite | Portable original-engine code still matches x86 ASM byte-for-byte |
| Real console | Boot, interaction, persistence, lifecycle and performance actually work |

After building, run the package/deployment checks inside the SDK:

```bash
image="$(tr -d '[:space:]' < docker/vita-image.txt)"
docker run --rm --user "$(id -u):$(id -g)" \
  --volume "$PWD:/src" --workdir /src \
  "$image" python3 tests/platform/test_vita_deploy.py
```

Use the desktop SDL2 commands in [PLATFORM_ABSTRACTION.md](PLATFORM_ABSTRACTION.md#sdl2-desktop-backend), `./run_tests_docker.sh` and `make arch-check` too. [vita.yml](../.github/workflows/vita.yml) builds and checks the VPK and uploads debug/deployment files separately. [release-latest.yml](../.github/workflows/release-latest.yml) publishes the VPK on the rolling pre-release; a published build is not a hardware-support claim.

### Hardware acceptance checklist

Record console model/firmware, controller configuration, launch method and tested commit:

- [ ] Install VPK; icon, LiveArea and title appear correctly.
- [ ] Missing data produces readable setup guidance and exits without a hang.
- [x] Boot with the documented data layout (confirmed by a player on a physical Vita).
- [ ] Circle plus each left-stick direction dodges; front-screen taps recenter once; rear touch and menu touches do not recenter. Circle retains its modal Return action.
- [ ] Save, quit, relaunch and load; settings survive and retail files remain untouched.
- [ ] Effects, music, speech and Smacker video play through scene changes.
- [ ] PS/HOME, suspend and resume restore usable video/input/audio without a simulation burst.
- [ ] Quit releases audio/video and returns cleanly.
- [x] FTP executable replacement and companion launch start the installed title; local and DEBUGNET traces capture boot.
- [ ] VPK-upload-only mode transfers the package for VitaShell installation on hardware.
- [ ] Vita TV/external-controller behavior is recorded separately; handheld results do not establish it.
