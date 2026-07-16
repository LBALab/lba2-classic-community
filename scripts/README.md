# Scripts

Catalogue of the repository's developer, CI, and packaging scripts. Kept current
the same way [docs/README.md](../docs/README.md) is: when you add, move, or remove
a script under `scripts/`, update its row here in the same commit.

The **Invoked by** column tells you whether a script is load-bearing or a one-off:
`CI` / `Makefile` / `pre-commit` scripts are wired into the project and must keep
working; `manual` scripts are run by hand (usually documented in `docs/`); `spike`
scripts are one-shot investigations kept as reproducible evidence for a plan doc,
not maintained tooling.

## Formatting & lint (`ci/`)

| Script | What it does | Invoked by |
|--------|--------------|------------|
| [ci/clang-format-select.sh](ci/clang-format-select.sh) | Single source of truth for the pinned clang-format major version (`CLANG_FORMAT_MAJOR`). Sourced, not executed. | the three format scripts + pre-commit |
| [ci/check-format.sh](ci/check-format.sh) | Verify tracked C/C++ files already match the clang-format policy; non-zero on drift. | CI (`format.yml`), `make format-check` |
| [ci/apply-format.sh](ci/apply-format.sh) | Apply the clang-format policy to tracked C/C++ files (respects `.clang-format-ignore`). | manual, pre-commit fix step |
| [ci/filter-format-files.py](ci/filter-format-files.py) | Filter a NUL-delimited git file list through `.clang-format-ignore` (one shared exclusion list). | the format shell scripts |
| [ci/run-clang-tidy.sh](ci/run-clang-tidy.sh) | Run clang-tidy for memory-safety / UB checks over the project's own sources (needs `compile_commands.json`). | manual (see `.clang-tidy`) |

## Build & run (`dev/`)

| Script | What it does | Invoked by |
|--------|--------------|------------|
| [dev/build-and-run.sh](dev/build-and-run.sh) | Build and run `lba2cc` from any working directory; resolves repo root and game data. | `make run` |
| [dev/repo_root.sh](dev/repo_root.sh) | Print the absolute repository root (directory with the top-level `CMakeLists.txt`). | `Makefile` |
| [dev/build-android.sh](dev/build-android.sh) | Build the engine for Android (arm64-v8a default, armeabi-v7a via `--abi`). | manual ([ANDROID.md](../docs/ANDROID.md)) |
| [dev/build-sdl3-android.sh](dev/build-sdl3-android.sh) | Cross-build SDL3 for Android and install it to a known prefix (prerequisite for `build-android.sh`). | manual ([ANDROID.md](../docs/ANDROID.md)) |
| [dev/check-16k-align.sh](dev/check-16k-align.sh) | Verify an Android APK is safe on 16 KB memory-page devices (segment alignment + uncompressed `.so`). | CI (android), [ANDROID.md](../docs/ANDROID.md) |

## Release dry-runs (`dev/`)

Local wrappers that build a release binary and then delegate to the matching
`packaging/` bundler, so the local artifact layout can't drift from CI's.

| Script | What it does | Invoked by |
|--------|--------------|------------|
| [dev/build-linux-tarball.sh](dev/build-linux-tarball.sh) | Dry-run the Linux static-binary tarball: build, then call `bundle-linux-tarball.sh`. | manual |
| [dev/build-macos-release.sh](dev/build-macos-release.sh) | Dry-run the macOS DMG: build (host arch by default), then call `bundle-macos.sh`. | manual |
| [dev/build-windows-release.sh](dev/build-windows-release.sh) | Dry-run the Windows ZIP: build (MSYS2 native or Linux cross), then call `bundle-windows.sh`. | manual |
| [dev/verify-release.sh](dev/verify-release.sh) | Post-release smoke test: download the published Linux artifacts and run each in a clean container. | manual ([RELEASING.md](../docs/RELEASING.md)) |

## Packaging (`packaging/`)

One bundler per platform; the CI release workflows are glue around them. The
`*-readme.txt.in` files are the user-facing README templates the bundlers expand
into each artifact.

| Script | What it does | Invoked by |
|--------|--------------|------------|
| [packaging/bundle-linux-tarball.sh](packaging/bundle-linux-tarball.sh) | Bundle a built Linux binary into a portable `.tar.gz`. | CI, `build-linux-tarball.sh` |
| [packaging/bundle-macos.sh](packaging/bundle-macos.sh) | Bundle a built `.app` into a macOS DMG. | CI, `build-macos-release.sh` |
| [packaging/bundle-windows.sh](packaging/bundle-windows.sh) | Bundle a built `lba2cc.exe` into a portable ZIP. | CI, `build-windows-release.sh` |
| [packaging/bundle-android.sh](packaging/bundle-android.sh) | Bundle a built native `.so` into a debug-signed APK. | CI (android) |
| [packaging/make-appimage.sh](packaging/make-appimage.sh) | Build a Linux AppImage (installs deps, packs the SDL3 runtime). | CI (linux appimage) |

## Regression baselines & savegame corpus

| Script | What it does | Invoked by |
|--------|--------------|------------|
| [dev/regen_projrec_baselines.sh](dev/regen_projrec_baselines.sh) | Regenerate projrec baseline hashes for the save corpus and attract-mode demo (tagged by render width). | manual ([CONTROL.md](../docs/CONTROL.md)) |
| [dev/run-savegame-corpus.sh](dev/run-savegame-corpus.sh) | Build and run the savegame corpus harness from any working directory. | `make` (savegame corpus target) |
| [save_probe.py](save_probe.py) | Offline probe of `.lba` save headers, LZSS bodies, fixed-offset fields, and a 32-vs-64 ABI forward-simulator. | `run-savegame-corpus.sh`, [SAVEGAME.md](../docs/SAVEGAME.md) |
| [save_probe_lz_selftest.py](save_probe_lz_selftest.py) | Golden `ExpandLZ` vectors mirroring `tests/SYSTEM/test_lz.cpp`. | `make save-probe-lz-selftest` |

## Data-format & asset tools (`dev/`)

Decoders and inspectors for the Adeline on-disk formats. Mostly one-off tools kept
for reproducibility; cited from the format and effects docs.

| Script | What it does | Invoked by |
|--------|--------------|------------|
| [dev/hqr_inspect.py](dev/hqr_inspect.py) | List / extract / decompress HQR archive entries (LZSS + LZMIT). Also a **shared library** imported by the art and LBA1 tools below. | [ENGINE_FILE_FORMATS.md](../docs/ENGINE_FILE_FORMATS.md), imported |
| [dev/impact_disasm.py](dev/impact_disasm.py) | Disassemble and round-trip IMPACT effect bytecode (`RESS_IMPACT=47`). | [IMPACT_SCRIPTS.md](../docs/IMPACT_SCRIPTS.md) |
| [dev/flow_dump.py](dev/flow_dump.py) | Decode FLOW particle-emitter definitions (`RESS_FLOW=45`). | [IMPACT_SCRIPTS.md](../docs/IMPACT_SCRIPTS.md) |
| [dev/pof_dump.py](dev/pof_dump.py) | Decode POF 2D wireframe shapes (`RESS_POF=46`). | [IMPACT_SCRIPTS.md](../docs/IMPACT_SCRIPTS.md) |
| [dev/iso_bin.py](dev/iso_bin.py) | Read the ISO9660 filesystem out of a raw Mode1/2352 CD image on the fly. | [DISC_IMAGE_SOURCE.md](../docs/DISC_IMAGE_SOURCE.md) |
| [dev/acf_decode.py](dev/acf_decode.py) | Decode Adeline ACF/XCF cinematic frames (Time Commando tile codec). | spike |
| [dev/acf_inspect.py](dev/acf_inspect.py) | Parse the ACF/XCF cinematic container chunk layout. | spike |
| [dev/extract_lba2_gog_media.py](dev/extract_lba2_gog_media.py) | Extract FMV / VOX / music from a GOG `LBA2.GOG` BIN image into the install dir. | [GAME_DATA.md](../docs/GAME_DATA.md) |
| [dev/art_catalog_screen.py](dev/art_catalog_screen.py) | Dump every `SCREEN.HQR` bitmap to PNG (widescreen art inventory; output local-only). | manual (widescreen) |
| [dev/art_treatment_preview.py](dev/art_treatment_preview.py) | Preview widescreen art treatments (letterbox / palette-fill / edge-clone / mirror-tile) as PNGs. | manual (widescreen) |

## LBA1 feasibility spikes (`dev/`)

Read-only investigations backing [LBA1_PORT_PLAN.md](../docs/LBA1_PORT_PLAN.md); each
confirms one axis of hosting LBA1 content on this engine.

| Script | What it does | Invoked by |
|--------|--------------|------------|
| [dev/lba1_body_probe.py](dev/lba1_body_probe.py) | Transcode an LBA1 body to the lba2cc body format. | spike (LBA1_PORT_PLAN §6) |
| [dev/lba1_body_render.py](dev/lba1_body_render.py) | Render a decoded LBA1 body to PNG with a stdlib software rasteriser. | spike (LBA1_PORT_PLAN §6) |
| [dev/lba1_script_remap.py](dev/lba1_script_remap.py) | Remap LBA1 Life-script opcodes onto the LBA2 VM; flag-width divergence report. | spike (LBA1_PORT_PLAN §6) |
| [dev/lba1_bkg_repack.py](dev/lba1_bkg_repack.py) | Re-index LBA1's three background HQRs into LBA2's single merged container. | spike (LBA1_PORT_PLAN §6.5) |
| [dev/lba1_voc_probe.py](dev/lba1_voc_probe.py) | Confirm LBA1 VOC audio plays through lba2cc's existing sample path. | spike (LBA1_PORT_PLAN §6.6) |

## Git hooks (`git-hooks/`)

| Script | What it does | Invoked by |
|--------|--------------|------------|
| [git-hooks/pre-commit](git-hooks/pre-commit) | Opt-in clang-format check on staged C/C++ files. Enable with `git config core.hooksPath scripts/git-hooks`. | git (opt-in) |

## Related entrypoints (outside `scripts/`)

The test and packaging drivers that these scripts feed into, or that stand
alongside them, live next to the code they exercise:

| Path | What it does | See |
|------|--------------|-----|
| [../run_tests_docker.sh](../run_tests_docker.sh) | Build and run the full ASM↔C++ equivalence suite inside a Linux x86_64 Docker container. | [TESTING.md](../docs/TESTING.md) |
| [../tests/automation/run.sh](../tests/automation/run.sh) | Run the CLI control-harness / headless behaviour suite (`test_*.sh` + `lib.sh`). | [CONTROL.md](../docs/CONTROL.md) |
| [../tests/savegame/corpus/](../tests/savegame/corpus/) | Savegame corpus harness (Python): manifest build, native-fallback check, baseline harness. | corpus `README.md` |
| [../tests/SNAPSHOT/](../tests/SNAPSHOT/) | Polyrec render / compare / bisect helpers. | [POLYREC.md](../docs/POLYREC.md) |
