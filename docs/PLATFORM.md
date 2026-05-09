# Platform assumptions

This is a high-level map of the host assumptions the engine makes — pointer widths, endianness, floating-point precision, OS boundaries, and similar. It exists so contributors stepping into platform work can orient quickly and find the deep-dive doc for whatever they're touching.

This doc does not duplicate the existing deep dives. For pointer-ABI patterns and the fat-type catalogue, see [ABI.md](ABI.md). For the savegame load path specifically, see [SAVEGAME.md](SAVEGAME.md). For calling conventions, see [COMPILER_NOTES.md](COMPILER_NOTES.md). What lives here is the cross-cutting map and the assumptions that don't yet have their own doc.

It is a baseline. It covers the hot zones with representative file:line citations and grows as platform work surfaces more cases.

Status legend:

- **✓ contained** — assumption is documented and behind a clear boundary (SDL, wire32, build-time backend select, etc.).
- **⚠ partial** — boundary exists but leaks; or a workaround is in place while the root cause is unfixed.
- **✗ exposed** — no boundary; engine code knows the assumption directly.

Truth hierarchy: code > this document > external sources. If a citation drifts, fix the doc.

---

## 1. Pointer width and ABI &nbsp;&nbsp;&nbsp;&nbsp; ⚠ partial

Several writeable structs contain native pointer fields and are serialized as raw bytes. On 32-bit retail those pointers are 4 bytes; on 64-bit hosts they are 8 + alignment padding. Today this is handled by per-struct mirror types and (for legacy saves where the writer's ABI is unknown) auto-retry stride at load time.

Canonical sites:

- `T_PTR_NUM` union — `LIB386/H/OBJECT/AFF_OBJ.H:21-25`
- `T_OBJ_3D` (3× `T_PTR_NUM`, 2× `void *`, 2× `PTR_U32`) — `LIB386/H/OBJECT/AFF_OBJ.H:28-86`
- `T_EXTRA::PtrBody` — `SOURCES/COMMON.H:704`
- `S_PART_FLOW::PtrListDot` — `SOURCES/FLOW.H:65`
- `S_CRED_OBJ_2` (credits HQR records) — `SOURCES/CREDITS.H` (paired `*_DISK` type lives there)
- Wire32 mirror structs for legacy save format — `SOURCES/SAVEGAME.CPP:201-342`

**Deep dive:** [ABI.md](ABI.md) documents the three patterns (paired on-disk type, field-by-field serialization, tolerant read with stride retry), the full fat-type catalogue, the compile-time guards, and the review checklist for new file-load sites.

**Next:** A canonical `NUM_VERSION 37+` save format that does not memcpy pointer-bearing structs would remove this category for new saves entirely; legacy loaders stay in place. Tracked at [issue #64](https://github.com/LBALab/lba2-classic-community/issues/64).

### Renderer-side wraparound

A second, independent face of the 32→64-bit pointer transition, distinct from the disk-layout side above: pointer arithmetic with a `U32` offset that may legitimately be negative. On 32-bit, `pointer + (U32)-24` wraps in the address space and resolves to `pointer - 24`. On 64-bit it zero-extends to a +4 GiB byte offset, so the pointer lands in unmapped memory — or, worse, a mapped page belonging to something else, which is silent corruption.

Coupled defect that hides the first one: clip tests written in `U32` (`if (xMin < ClipXMin)`) silently fail for negative `xMin`, so the existing margin/clip branch never fires and the corrupted offset reaches the inner loop unnoticed.

Worked example — `LIB386/SVGA/COPYMASK.CPP`, called from `DrawOverBrick3` (interior brick recover pass) with `x = -24` for column 0. The school scene with the grand wizard and foreground candles triggers it on every entry. Fixed in [PR #84](https://github.com/LBALab/lba2-classic-community/pull/84) by switching the geometry locals to `S32`. Pinned by `tests/copymask_negx/`. Investigation runbook in [CRASH_INVESTIGATION.md](CRASH_INVESTIGATION.md).

The smell: a renderer-adjacent function takes signed coordinates but stores them in `U32` locals before doing pointer math against `Log` / `Screen`. Crashes are intermittent because whether `pointer + 4 GiB` lands in a mapped page is allocator/ASLR luck — easy to mistake for "flaky" rather than a real bug.

**Audit log:** per-file sweep results live in [PLATFORM_AUDITS.md § "U32 wrap"](PLATFORM_AUDITS.md#u32-wrap--renderer-side-address-space-wraparound). SVGA and pol_work groups swept (#84, #86, #87); 3D + 3DEXT and GRILLE + INTEXT remain.

---

## 2. Endianness &nbsp;&nbsp;&nbsp;&nbsp; ✗ exposed

The engine assumes little-endian throughout. HQR readers cast raw bytes through `*(U32 *)ptr`. Save reads and writes use `memcpy` with no byteswap. There is no byteswap helper layer in the tree. Every supported target today is little-endian, so this has not bitten anyone, but the assumption is wide and the call sites do not name it.

- HQR offset reads as raw casts — `LIB386/SYSTEM/HQRMEM.CPP:17,22`
- Save read/write macros (no byteswap) — `SOURCES/SAVEGAME.CPP:60-90` (`LbaRead` / `LbaWrite`)
- No byteswap helpers anywhere in the tree (notable absence).

**Next:** Document as immutable for now. Introduce a byteswap layer only if a big-endian or mixed-endian target enters scope.

---

## 3. Floating-point precision and FPU semantics &nbsp;&nbsp;&nbsp;&nbsp; ⚠ partial

Projection and rotation paths use `long double` + `lrintl()` to match the original x87 FPU's round-to-nearest `fistp` behavior. `long double` is 80-bit on Linux x86_64, 64-bit on macOS arm64 and Windows MSYS2. The result is small but real per-platform divergence in screen coordinates and Z values. Mitigated where it matters (projection, rotation, polygon slopes); tested in `tests/fpu_precision/`.

- `LongProjectPoint3D` — `LIB386/3D/LPROJ3DF.CPP:25-28`
- `LongRotatePointF` — `LIB386/3D/LROT3DF.CPP:13-15`
- POLY.CPP slope `volatile long double` — `LIB386/pol_work/POLY.CPP:263, 668, 686, 859-863, 921-936`
- FPU control word reference (test) — `tests/fpu_precision/fpu_ops.asm:15-16`
- `volatile` barrier rationale (forces FPU stack → memory round-trip) — `tests/fpu_precision/test_fpu_precision.cpp:109,125`

**Next:** Consolidate the `lrintl(long double)` callers behind a single helper TU (`lba_round_to_int`) so the platform divergence has one place to live. Quantify the per-platform delta in a regression test.

---

## 4. x86 ASM fastpath &nbsp;&nbsp;&nbsp;&nbsp; ✓ contained

x86 assembly is opt-in via the `ENABLE_ASM` CMake option (default OFF). Every ASM function has a CPP equivalent, and ASM↔CPP equivalence is tested per function. Watcom-style register-passing in the legacy ASM is bridged by naked-function shims in the test harness. Non-ASM builds run on any architecture supported by the rest of the toolchain; ASM builds require x86 plus UASM.

- `ENABLE_ASM` option — `CMakeLists.txt:6-20`
- ASM sources — `LIB386/3D/*.ASM` (9 files), `LIB386/pol_work/*.ASM` (12+ files)
- Watcom calling convention example — `LIB386/3D/LROT3DF.ASM:5,40-42`
- ASM↔CPP equivalence tests — `tests/3D/`, `tests/pol_work/`, `tests/fpu_precision/`
- Naked-function shims (Watcom→cdecl bridge) — `tests/ANIM/test_intanim.cpp:29-49`

**Deep dive:** [COMPILER_NOTES.md](COMPILER_NOTES.md) for calling-convention background; [ASM_TO_CPP_REFERENCE.md](ASM_TO_CPP_REFERENCE.md) for which modules have CPP equivalents.

**Next:** None required. Could be retired if profiling ever shows the CPP path is good enough on all targets, but no urgency.

---

## 5. OS boundary (filesystem, conditionals, SDL3) &nbsp;&nbsp;&nbsp;&nbsp; ✓ contained

Operating-system contact is concentrated in a thin layer. SDL3 wraps window, events, audio, video, and timer. Path manipulation is gated by `_WIN32` for the separator character and `mkdir` signature. Asset and save discovery uses `SDL_GetBasePath` plus `LBA2_GAME_DIR` plus a candidate walk. Case-insensitivity on macOS APFS bit us once (`<version>` vs the build's `VERSION` text file) and is now sidestepped by writing `VERSION.txt` instead.

- `_WIN32` mkdir signature + path separator — `LIB386/H/SYSTEM/ADELINE.H:38-44`
- `ADELINE_PATH_SEP` usage — `SOURCES/RES_DISCOVERY.CPP`
- macOS case-insensitivity hazard (with explanation) — `cmake/git_version.cmake:15-20`
- SDL3 window — `LIB386/SYSTEM/WINDOW.CPP:5,89`
- SDL3 events — `LIB386/SYSTEM/EVENTS.CPP:10,35,64,67`
- SDL3 timer — `LIB386/SYSTEM/TIMER.CPP:5-6,29,73`
- Asset discovery — `SOURCES/RES_DISCOVERY.CPP:238-315`

**Deep dive:** [GAME_DATA.md](GAME_DATA.md) for the discovery rules and override knobs.

**Next:** Audit `#ifdef _WIN32` / `__linux__` / `__APPLE__` for vestigial branches. Add the case-insensitivity hazard to a per-PR checklist for files that get added to the build dir on a header search path.

---

## 6. Backend abstractions (audio, video) &nbsp;&nbsp;&nbsp;&nbsp; ✓ contained

Both audio and video are pluggable at build time through CMake options. Engine code sees only the common interface; the chosen backend's TU is compiled in. Good prior art for any future "swap an implementation per platform" work.

- `SOUND_BACKEND` (`null` / `miles` / `sdl`) and `MVIDEO_BACKEND` (`null` / `smacker`) — `CMakeLists.txt:25-39`
- AIL backend implementations — `LIB386/AIL/{NULL,MILES,SDL}/`
- Backend selection in CMake — `LIB386/AIL/CMakeLists.txt:3-9`
- Common interface — `LIB386/H/AIL/VIDEO_AUDIO.H:17-29`

**Deep dive:** [AUDIO.md](AUDIO.md).

**Next:** None structural.

---

## 7. Build matrix &nbsp;&nbsp;&nbsp;&nbsp; ✓ contained

CMake presets cover the supported targets and toolchains. Cross-compile to Windows from Linux is supported via mingw-w64.

- Presets (`linux`, `linux_clang`, `linux_test`, `macos_arm64`, `macos_x86_64`, `windows_ucrt64`, `windows_mingw64`, `cross_linux2win`) — `CMakePresets.json`
- Cross-toolchain — `cmake/mingw-w64-i686.cmake`
- `LBA2_NATIVE_ARCH` (per-arch tuning flags) — `CMakeLists.txt`

**Deep dive:** [WINDOWS.md](WINDOWS.md) for the MSYS2 path; root [README](../README.md) for the broader build instructions.

**Next:** None immediate. Add a one-line comment to each preset explaining its role if not obvious from the name.

---

## Open questions / deeper-dive candidates

These are not assumptions the doc has fully audited — they are areas worth checking when the work surfaces.

- **Char signedness.** Default `char` signedness differs across compilers and platforms. No known site has bitten us, but the codebase is C++98 with heavy x86 heritage, so it is worth a sweep when someone has time.
- **Threading model.** Single-threaded today. `SDL_GetTicks` polled per frame; no `pthread`, `std::thread`, or async primitives in tree. If multithreaded work ever lands, the contract for shared state needs to be written down before the first race lands.
- **HQR file format.** The on-disk format is implicit in `LIB386/SYSTEM/HQRMEM.CPP` and `LIB386/SYSTEM/HQR.CPP`. A written spec would help future format-touching work the same way [SAVEGAME.md](SAVEGAME.md) helps savegame work.
- **Save format canonicalization.** A `NUM_VERSION 37+` format that does not memcpy pointer-bearing structs would remove the entire pointer-ABI workaround chain for new saves. Tracked at [issue #64](https://github.com/LBALab/lba2-classic-community/issues/64).

---

## Future directions

Architectural changes that would shift the readiness picture above. Listed here to keep the surface visible; each becomes its own design doc or issue when it matures. Entries are holding-pen items, not commitments.

- **GPU-backed rendering.** The polygon path today is software (`LIB386/pol_work/`, CPU-side rasterization, 8-bit indexed framebuffer). A GPU renderer — `SDL_GPU` or a thin Vulkan/Metal/D3D backend with cross-compiled shaders — would change the FP-precision picture (matches GPU float semantics, not x87), open the door to higher resolutions and HDR, and deprecate the ASM fillers. Big enough that the software path likely stays as a parallel implementation, with the polyrec replay harness as the equivalence test.
- **Directory restructure.** `LIB386/` ("32-bit Intel") encodes a platform assumption in its name. A renaming sweep (`engine/`, `lib/`, or similar) plus splitting platform-specific files into a `sys/` subtree would surface the boundaries the way Doom 3's `sys/win32/` / `sys/linux/` does. No code change; pure rename + `#include` updates. Worth doing once the team agrees on preferred layout.
- **Canonical save format (`NUM_VERSION 37+`).** Tracked at [issue #64](https://github.com/LBALab/lba2-classic-community/issues/64). Removes the wire32 / auto-retry chain for new saves; legacy loaders stay.
- **ASM retirement.** Once profiling confirms the CPP path is fast enough on all targets, the `ENABLE_ASM` option could be removed and the `*.ASM` files retired. The CPP equivalents are already canonical; equivalence is already tested.

---

## Iterating on this doc

When you touch a file referenced here, check that the line numbers still match. When you find a new platform assumption that is not listed, add a row in the relevant section with a citation. When a status changes (a workaround lands or a root-cause fix removes one), update the badge.
