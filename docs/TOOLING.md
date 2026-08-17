# Tooling

Index of the external tools this repository expects, and which of them you
actually need. Companion to [scripts/README.md](../scripts/README.md), which
catalogues the scripts *we* ship; this doc covers the tools *they* assume you
have.

Run the check instead of reading the tables:

```bash
./scripts/dev/check-tooling.sh     # or: make check-tooling
```

It probes every tool below, reports what is missing, and exits non-zero only
when the clone genuinely cannot build. `--tier 1` narrows it, `--quiet` shows
only rows needing attention, `--strict` also fails on tier 2.

## How the tiers work

Tools are grouped by what breaks without them, not by how useful they are.
"Optional" would put the Android NDK (a hard requirement for one lane) in the
same bucket as `actionlint` (never required by anything), so it isn't used here.

| Tier | Test | Consequence |
|------|------|-------------|
| **1 — Build & run** | no binary without it | `check-tooling.sh` exits non-zero |
| **2 — Pass review** | CI fails you without it | warning; `--strict` exits non-zero |
| **3 — Per lane** | required only if you work on that lane | reported |
| **4 — Faster, not required** | nothing breaks | reported |

Two rules keep the tables from going stale, and both matter more than the
tables themselves:

- **No version literals here.** Every floor and pin lives in the file that owns
  it, named in the Version owner column. `check-tooling.sh` parses those same
  files, so the doc, the script, and the build cannot disagree about a version —
  there is only ever one copy.
- **No probe commands here.** How a tool is detected lives in
  `check-tooling.sh`. A second copy in prose is a second thing to forget.

## Tier 1 — build & run

Without all five there is no `lba2cc` binary. The root
[README](../README.md#prerequisites) states the same set for players building
from source.

| Tool | Needed for | Version owner | Install |
|------|-----------|---------------|---------|
| CMake | every build path | [../CMakeLists.txt](../CMakeLists.txt#L1) (`cmake_minimum_required`) | distro package; the `ubuntu-latest` runner already satisfies the floor |
| Ninja | all [CMakePresets.json](../CMakePresets.json) presets and `make build` use the Ninja generator | — | `apt install ninja-build`, `brew install ninja`, `pacman -S mingw-w64-ucrt-x86_64-ninja` |
| C/C++ compiler | C++98 dialect; see [CODESTYLE.md](../CODESTYLE.md) | — | GCC or Clang. MSVC is not supported |
| SDL3 | `find_package(SDL3 CONFIG)`; shared by default, static for release packaging | see note below | `brew install sdl3`, `pacman -S mingw-w64-ucrt-x86_64-SDL3`; most distros need a source build |
| git | the format and clang-tidy scripts enumerate files with `git ls-files` | — | distro package |

**SDL3 has no single version owner.** The release-build pin is duplicated across
four code sites — [docker/Dockerfile.test](../docker/Dockerfile.test#L32),
[reusable-build-android.yml](../.github/workflows/reusable-build-android.yml#L126),
[build-linux-tarball.sh](../scripts/dev/build-linux-tarball.sh#L91), and
[build-sdl3-android.sh](../scripts/dev/build-sdl3-android.sh#L39) — and the
Dockerfile comment asks you to keep the four in sync when bumping. Local
development builds against whatever SDL3 your system provides and does not care
about the pin; only bundled and containerised builds do.

GNU Make is not in the table: it only drives the [Makefile](../Makefile)
shortcuts, and plain `cmake` works without it.

**On Windows all five hang off the MSYS2 environment.** In the plain `MSYS`
shell, cmake, ninja and gcc are simply not on `PATH`, so every row above fails at
once and the cause is invisible. `check-tooling.sh` reports the active `MSYSTEM`
first for exactly that reason — it should say `UCRT64`. See
[WINDOWS.md](WINDOWS.md).

## Tier 2 — pass review

CI enforces these, so a gap here means a red PR rather than a broken clone. Most
rows are conditional on what your change touches — C/C++, shell, Python,
workflows, or `LIB386/`. Lychee is the exception: `docs-links.yml` is
deliberately not path-filtered, so it gates every PR including a docs-only one.

| Tool | Needed for | Version owner | Install |
|------|-----------|---------------|---------|
| clang-format | C/C++ — `format.yml`, `make format-check`, the optional pre-commit hook | [clang-format-select.sh](../scripts/ci/clang-format-select.sh#L18) (`CLANG_FORMAT_MAJOR`) | the format scripts print the exact package name for the pinned major when they refuse to run — [check-format.sh](../scripts/ci/check-format.sh#L13) |
| lychee | any change — `docs-links.yml` checks every relative link and `#anchor` in tracked markdown, via `make docs-links` | [docs-links.yml](../.github/workflows/docs-links.yml#L25) (`LYCHEE_VERSION`) | `cargo install lychee`, or the release binary CI downloads |
| shellcheck | shell — the `shellcheck` job over every tracked `*.sh` | [lint.yml](../.github/workflows/lint.yml#L31) (`SHELLCHECK_VERSION`) | distro package, or the release tarball CI uses |
| ruff | Python — the `ruff` job over every tracked `*.py` | [lint.yml](../.github/workflows/lint.yml#L33) (`RUFF_VERSION`) | `pipx install ruff` |
| actionlint | workflows — the `actionlint` job, which also shells out to shellcheck for every `run:` body | [lint.yml](../.github/workflows/lint.yml#L32) (`ACTIONLINT_VERSION`) | release binary |
| Python 3 | [filter-format-files.py](../scripts/ci/filter-format-files.py) gates the format check; also the save probes and corpus harness | — | distro package |
| Docker | `LIB386/` — [run_tests_docker.sh](../run_tests_docker.sh), the ASM equivalence suite | — | Docker Engine or Docker Desktop |

The linters take their rule selection from checked-in config —
[.shellcheckrc](../.shellcheckrc), [ruff.toml](../ruff.toml) — so a local run
matches CI. The one exception is shellcheck severity, which it only accepts on
the command line: CI passes `-S warning`, so add that locally or you will see
info and style findings the gate ignores.

Without lychee, `make docs-links` still runs its second half — a grep for
`docs/<name>.md` paths named by bare path in source, tests and CMake, which a
link checker cannot see — and skips the link half with a warning rather than
failing. Easy to mistake for a pass.

The format scripts refuse to run a non-matching major rather than silently
disagreeing with CI, so install the exact version the owner file names. Only
stdlib Python is needed here; [Pillow](#tier-4--faster-not-required) is tier 4.

The package name is not guessable on MSYS2, and the scripts' own error message
covers Debian and macOS only: there it is
`pacman -S mingw-w64-ucrt-x86_64-clang-tools-extra`, and the binary is unversioned
(`clang-format`, never `clang-format-N`). `check-tooling.sh` prints the right
one for your platform.

Two things about the container:

- **Presence is not the question.** A Docker Desktop install can leave a `docker`
  on `PATH` in WSL that resolves fine and then fails at exec, which
  [verify-release.sh](../scripts/dev/verify-release.sh#L73) documents and
  `check-tooling.sh` mirrors by calling `docker info`.
- **Podman is not a drop-in.** Both scripts invoke `docker` by name, so podman
  needs a shim or alias on `PATH`. The commands themselves are compatible —
  [run_tests_docker.sh](../run_tests_docker.sh) only uses `images -q`,
  `build --platform` and `run --rm --platform`, all of which podman accepts. The
  harder case is [verify-release.sh](../scripts/dev/verify-release.sh#L141),
  which runs `--privileged tonistiigi/binfmt` to register arm64 emulation;
  rootless podman cannot do that, and the script already falls back to skipping
  the arm64 artifact. Neither path is verified against podman, so treat it as
  unsupported-but-probably-fine rather than tested.

Everything the suite needs *inside* the image — UASM, 32-bit multilib, both SDL3
builds — is pinned and fetched by
[docker/Dockerfile.test](../docker/Dockerfile.test), not by you. That is the
point of running it in a container.

## Tier 3 — per lane

Hard requirements with a narrow audience. Nothing here matters until you work on
that specific lane.

### ASM equivalence on the host

The container path above is the supported one. Building the suite natively needs
all three, and `check-tooling.sh` expects most hosts to lack the last two.

| Tool | Needed for | Version owner | Install |
|------|-----------|---------------|---------|
| `objcopy` | `LBA2_BUILD_ASM_EQUIV_TESTS=ON` (default ON when tests are enabled) | — | binutils |
| 32-bit runtime | linking the 32-bit equivalence targets | — | `gcc-multilib g++-multilib` |
| UASM | `ENABLE_ASM=ON` only | [docker/Dockerfile.test](../docker/Dockerfile.test) (`ARG UASM_VERSION`) | the image fetches it; see [TESTING.md](TESTING.md) |

`make test` sets `LBA2_BUILD_ASM_EQUIV_TESTS=OFF`, so none of this is needed for
the host-only pass.

### Releasing

Maintainer lane; see [RELEASING.md](RELEASING.md).

| Tool | Needed for | Version owner | Install |
|------|-----------|---------------|---------|
| `gh` (authenticated) | release upload and edit, [verify-release.sh](../scripts/dev/verify-release.sh) | — | [cli.github.com](https://cli.github.com) |
| `git-cliff` | `git cliff --prepend` for CHANGELOG; not needed for a first release | — | `cargo install git-cliff` or a release binary |
| `tar` | Linux tarball bundling, artifact verification | — | distro package |

### Platform artifacts

One bundler per platform, each with its own host requirement.

| Tool | Needed for | Version owner | Install |
|------|-----------|---------------|---------|
| `zip` or Python 3 | [bundle-windows.sh](../scripts/packaging/bundle-windows.sh#L104) prefers `zip`, falls back to the stdlib `zipfile` | — | distro package |
| mingw-w64 | the `cross_linux2win` preset and [cmake/mingw-w64-i686.cmake](../cmake/mingw-w64-i686.cmake); Unix hosts only, so the probe skips it on Windows | — | `apt install mingw-w64`, `pacman -S mingw-w64-gcc` |
| MSYS2 UCRT64 | native Windows builds — the recommended local path, bit-for-bit CI's toolchain. Probed via `MSYSTEM`, see the tier 1 note | — | [WINDOWS.md](WINDOWS.md) |
| `hdiutil`, `xcrun` | DMG creation; [bundle-macos.sh](../scripts/packaging/bundle-macos.sh#L64) hard-requires a macOS host | — | Xcode command-line tools |

The AppImage is the exception: [make-appimage.sh](../scripts/packaging/make-appimage.sh)
calls `pacman`, `get-debloated-pkgs`, and `quick-sharun`, and runs inside the
`ghcr.io/pkgforge-dev/archlinux` container in CI. It has no local dry-run on a
non-Arch host, unlike the other three bundlers.

### Android

See [ANDROID.md](ANDROID.md).

| Tool | Needed for | Version owner | Install |
|------|-----------|---------------|---------|
| Android NDK | native build; set `ANDROID_NDK` | [build-android.sh](../scripts/dev/build-android.sh#L8) (header) and its default path at [line 28](../scripts/dev/build-android.sh#L28) | Android Studio SDK manager or the NDK zip |
| SDK build-tools | `aapt2`, `zipalign`, `apksigner` — [bundle-android.sh](../scripts/packaging/bundle-android.sh#L77) picks the highest installed | — | SDK command-line tools |
| JDK (`javac`, `keytool`) | compiles the SDL3 Java activity to `classes.dex`; generates the debug keystore | [reusable-build-android.yml](../.github/workflows/reusable-build-android.yml#L55) (`java-version`) | Temurin, or any distro JDK at that version |
| `adb` | installing and reading logs on a device | — | SDK platform-tools |

### Game-data folder picker (Linux runtime)

Not a build dependency — it's what the shipped binary calls when it needs to ask
where your retail data is. `zenity` or an `xdg-desktop-portal` backend; the
per-distro table with the reasoning is in
[GAME_DATA.md](GAME_DATA.md#picker-backends-per-environment).

## Tier 4 — faster, not required

Nothing in this section is wired into CI or any script's happy path. Each entry
either degrades gracefully or is purely for your own loop.

| Tool | What it buys you | Notes |
|------|------------------|-------|
| clang-tidy + `run-clang-tidy` | memory-safety and UB checks via [run-clang-tidy.sh](../scripts/ci/run-clang-tidy.sh) | scope in [.clang-tidy](../.clang-tidy). The only linter with no CI job, so nothing enforces it. Needs `compile_commands.json` |
| Pillow | screenshot comparison in the automation suite | [lib.sh](../tests/automation/lib.sh#L189) skips image asserts when it is absent, so the suite passes either way |
| ImageMagick | PPM to PNG in [render_polyrec.sh](../tests/SNAPSHOT/render_polyrec.sh#L81) | without it the PPMs are kept and conversion is skipped |
| `act` | runs CI jobs locally | Linux jobs only; the macOS and Windows runners cannot be emulated |
| `gdb` / `lldb` | debugging | the `linux_clang` and `linux_sanitize` presets pair with it |

## Engine-internal tooling

Everything above is a binary you install. The engine also ships a lot of its own
instrumentation — polyrec, the control harness, the console, the trace commands —
and that is what you actually reach for when debugging the game rather than the
build.

It is deliberately **not** tiered here, for two reasons. Nothing needs
installing, so "what breaks without it" has no meaning. And it changes on a
different clock: a PR that adds a console command would age this doc, while the
external tool list moves once a year. So this section routes; it never lists
individual commands.

Prefer the live enumerators over any doc — they are generated from the code and
cannot drift:

```bash
lba2cc --help                                   # player-facing flags
lba2cc --help-all                               # every flag, grouped
lba2cc --headless --exec "cmdlist" --tick 2 --exit   # every console command
lba2cc --headless --exec "varlist" --tick 2 --exit   # every cvar
```

| Capability | What it is for | Where it is documented |
|-----------|----------------|------------------------|
| Control harness | boot, restore a save, run commands, advance N ticks, dump state or a screenshot, exit — all in one invocation. The backbone of headless verification | [CONTROL.md](CONTROL.md) |
| Debug console | ~40 commands and a set of cvars, always available (no build flag) | [CONSOLE.md](CONSOLE.md) |
| Polyrec | polygon record/replay; byte-compares ASM against C++ draw calls, with a bisect driver for first divergence | [POLYREC.md](POLYREC.md) |
| Trace commands | per-subsystem logging you toggle at runtime rather than rebuilding | [CONSOLE.md](CONSOLE.md), [TIMING.md](TIMING.md) |
| Perftrace | per-frame timing ring buffer for frame-pacing work | [PERFTRACE.md](PERFTRACE.md) |
| Adeline debug tools | the original 1997 developer tools, behind `DEBUG_TOOLS=ON` | [DEBUG.md](DEBUG.md) |
| Automation suite | the shell suite that drives the harness for regression | [CONTROL.md](CONTROL.md), [TESTING.md](TESTING.md) |
| Savegame corpus | replays a corpus of real saves through the load path | [SAVEGAME.md](SAVEGAME.md) |

The one place the two axes meet is tier 4: Pillow and ImageMagick are external
packages that only exist to make the *internal* harnesses more useful, and both
degrade to a skip when absent.

## By task

| I want to… | Tiers | Extra |
|-----------|-------|-------|
| Build and play | 1 | retail game data — [GAME_DATA.md](GAME_DATA.md) |
| Fix a bug in `SOURCES/` | 1 + clang-format | `make test` for the host pass |
| Touch `LIB386/` | 1 + 2 | the container runs the ASM suite for you |
| Edit a script or workflow | 1 + shellcheck / ruff / actionlint | run `shellcheck -S warning` to match the gate |
| Edit docs only | lychee | `make docs-links`; the [docs-only CI gate](CI.md) covers `build` and `test`, but the link check still runs |
| Run the control harness | 1 | retail data; Pillow for image asserts — [CONTROL.md](CONTROL.md) |
| Build for Android | 1 + Android lane | — |
| Cut a release | 1 + releasing + the target platform's bundler | [RELEASING.md](RELEASING.md) |
| Poke at disc images or HQR data | 1 + Python 3 | stdlib only; the two art scripts and the ACF decoder need Pillow |

## Deliberately not required

Named here so nobody adds them by accident:

- **A dependency manager** (Nix, devenv, mise, asdf, vcpkg, Conan). Contributors
  are on Linux, macOS, Windows/MSYS2, and WSL, and the tool set is small enough
  that per-platform package managers stay cheaper than a lockfile everyone has
  to adopt.
- **A pinned compiler.** The engine targets C++98 across GCC and Clang on four
  platforms; pinning one compiler would hide exactly the portability breaks CI
  exists to catch. See [COMPILER_NOTES.md](COMPILER_NOTES.md) and
  [PLATFORM.md](PLATFORM.md).
- **Node, Rust, or Go toolchains.** `git-cliff` ships prebuilt binaries; nothing
  else needs them.
- **Third-party Python packages**, with one exception. Pillow is the only one,
  and it is needed by exactly three asset scripts — `art_catalog_screen.py`,
  `art_treatment_preview.py`, `acf_decode.py` — plus the automation suite's
  optional image asserts. Everything else, including the save probes and the
  corpus harness, runs on a bare `python3`. Keep it that way: a new script that
  needs numpy needs a conversation first.

## Keeping this current

Same contract as [scripts/README.md](../scripts/README.md) and
[docs/README.md](README.md): when you make a script or workflow depend on a new
external tool, add its row here and a probe to
[check-tooling.sh](../scripts/dev/check-tooling.sh) in the same commit. There is
a row for this in the [AGENTS.md](../AGENTS.md#when-modifying-x-do-y) table.

What actually rots in a doc like this is versions and per-distro install lines,
not the tool list. So:

- **Never write a version number in this file.** Add it to the Version owner
  column as a link to the file that pins it, and teach `check-tooling.sh` to
  parse that file. A version stated in two places is already drifting.
- **Only write an install command where the obvious one is wrong.** `apt install
  cmake` earns nothing. The four-way portal-backend split in
  [GAME_DATA.md](GAME_DATA.md#picker-backends-per-environment) earns its space
  because guessing there breaks D-Bus.
- **If a tool has no probe, it does not belong in tiers 1–3.** Those tiers are
  claims about whether work is possible, and an unprobed claim is the one that
  goes stale silently.
