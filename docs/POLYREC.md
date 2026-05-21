# Polyrec — polygon call recording and replay

Polyrec is the harness for catching divergence between the original ASM polygon
fillers and their C++ ports. It records the real polygon draw stream of a single
game frame into a `.lba2polyrec` file, then replays that exact stream through
both the ASM and C++ renderers and compares the resulting framebuffers.

This document describes what the harness captures, how capture and replay work,
what the comparison actually checks, where it stops, and what would have to
change to extend it. It is grounded in the code under
[`LIB386/SNAPSHOT/`](../LIB386/SNAPSHOT/) and [`tests/SNAPSHOT/`](../tests/SNAPSHOT/).
For the operator-facing command reference (`run_tests_docker.sh` flags, render
and bisect workflows), see [TESTING.md](TESTING.md).

## What it is

### Purpose

Filler/rasterizer mismatches between ASM and C++ are hard to reproduce with
small synthetic fixtures: a single wrong pixel can depend on a specific
combination of polygon type, texture, fog state, clip rectangle, and vertex
attributes that only occurs in a real scene. Polyrec captures the exact
sequence of draw calls and render-state changes the engine made during one
frame, so that sequence can be replayed deterministically through either filler
path and compared byte for byte.

Because every polygon — terrain bricks, 3D objects, sprites drawn as polys —
reaches the screen through `Fill_Poly`, `Fill_Sphere`, or `Line_A`, capturing at
those three entry points records the whole frame's rasterization work without
needing the scene graph, bodies, animations, or projection matrices. The header
comment on [`POLY_RECORDING.H`](../LIB386/H/SNAPSHOT/POLY_RECORDING.H) states this
explicitly.

### The record/replay model

- **Capture** lives in the engine, in
  [`LIB386/SNAPSHOT/POLY_RECORDING.CPP`](../LIB386/SNAPSHOT/POLY_RECORDING.CPP),
  and is compiled in only when the build is configured with
  `-DENABLE_POLY_RECORDING` (option defaults `OFF`, [`CMakeLists.txt:48`](../CMakeLists.txt)).
  Thin hooks at the top of the polygon primitives forward each call to a
  `polyrec_record_*` function when recording is active.
- **Replay** lives in the tests, in [`tests/SNAPSHOT/`](../tests/SNAPSHOT/). The
  same replay source is built into two executables — `replay_polyrec_cpp` (links
  the C++ fillers) and `replay_polyrec_asm` (links the original ASM fillers) —
  that load a recording, set up identical render state, run the recorded calls,
  and write out the framebuffer.
- **Comparison** is `cmp` over the two raw framebuffers, driven by
  [`compare_polyrec.sh`](../tests/SNAPSHOT/compare_polyrec.sh). CTest registers
  one test per fixture.

The ASM result is treated as the source of truth; the goal is for the C++ path
to produce a byte-identical frame.

### What gets captured

One `.lba2polyrec` file is a structured binary (magic `LBA2PREC`, version `1`)
with these sections, defined in
[`POLY_RECORDING.H`](../LIB386/H/SNAPSHOT/POLY_RECORDING.H):

| Section | Contents |
|---|---|
| File header | Magic, version, screen width/height/pitch, event count, initial clip rectangle |
| `CLUT` | `Fill_Logical_Palette` (256 bytes), an RGB palette (768 bytes, used only for PPM output), the initial CLUT line offset, and the two 64 KB colour-lookup tables `PtrCLUTFog` and `PtrCLUTGouraud` |
| `TEXT` | Up to `POLYREC_MAX_TEXTURES` (64) texture blobs, each with its `rep_mask` and size |
| `INIT` | Initial filler bank, fog near/far, `Fill_Patch`, active texture index, `RepMask`, clip rectangle |
| `EVNT` | The delta-compressed event stream (state changes + draw calls) |
| `REFF` | The game's own framebuffer (`Log`) after the frame rendered — `width × height` bytes |
| `END!` | Terminator |

The event stream interleaves state-change events and draw events:

| Event | Hex | Payload |
|---|---|---|
| `SWITCH_FILLERS` | `0x02` | bank (u32) |
| `SET_FOG` | `0x03` | z_near, z_far (s32) |
| `SET_CLUT` | `0x04` | line (u32) |
| `SET_TEXTURE` | `0x05` | texture index (u32) |
| `SET_CLIP` | `0x06` | xmin, ymin, xmax, ymax (s32) |
| `SET_REPMASK` | `0x07` | rep_mask (u32) |
| `SET_FILL_PATCH` | `0x08` | fill_patch (u32) |
| `SET_CLUT_OFFSET` | `0x09` | byte offset of `PtrCLUTGouraud` into the fog table (u32) |
| `FILL_POLY` | `0x10` | type, color, nb_points (s32), then `nb_points × 16` bytes of `Struc_Point` |
| `FILL_SPHERE` | `0x11` | type, color, centre_x, centre_y, radius, zbuf_value (s32) |
| `LINE_A` | `0x12` | x0, y0, x1, y1, col, z1, z2 (s32) |
| `EOF` | `0xFF` | — |

A `FILL_POLY` event stores each vertex as the engine's 16-byte
[`Struc_Point`](../LIB386/H/POLYGON/POLY.H) verbatim: screen X/Y (`Pt_XE`,
`Pt_YE`, both `S16`), texture U/V, light, normalized Z, and `Pt_W`. These are
**already-projected screen-space coordinates** — projection has happened
upstream by the time `Fill_Poly` is called. This is the single most important
fact about the harness's scope (see [Current scope and limitations](#current-scope-and-limitations)).

### What gets compared

The compared artifact is the **8-bit indexed framebuffer**: `width × height`
bytes, one palette index per pixel, row-major, pitch equal to width.
`compare_polyrec.sh` runs both replays and `cmp -s` the two `.raw` files. A pass
means every pixel index is identical. The Z-buffer, the colour PPM, and the
captured game reference framebuffer are produced for inspection but are **not**
part of the pass/fail assertion.

## How it works

### The capture pipeline

Recording is driven from the scene renderer and gated by `ENABLE_POLY_RECORDING`
throughout:

1. **Trigger.** During gameplay, `Alt+F9` sets `SnapshotRequested = 1`
   ([`PERSO.CPP:754`](../SOURCES/PERSO.CPP)).
2. **Start.** At the top of `AffScene()`
   ([`OBJECT.CPP:5324`](../SOURCES/OBJECT.CPP)), if `SnapshotRequested` is set,
   the engine builds a filename `polyrec_%04d.lba2polyrec`, calls
   `polyrec_start()`, hands it the current RGB palette for PPM output, and clears
   the flag. It then forces a full redraw (`flagflip = AFF_ALL_NO_FLIP`,
   [`OBJECT.CPP:5346`](../SOURCES/OBJECT.CPP)) so that both terrain and objects
   re-emit their draw calls into the recording — incremental redraw modes would
   otherwise skip terrain that was drawn in an earlier frame.
3. **Snapshot of initial state.** `polyrec_start()`
   ([`POLY_RECORDING.CPP:138`](../LIB386/SNAPSHOT/POLY_RECORDING.CPP)) allocates a
   1 MB growable event buffer, records screen dimensions from `ModeDesiredX/Y`
   and `ScreenPitch`, copies the palette and both 64 KB CLUT tables, derives the
   initial CLUT line from `(PtrTruePal - PtrCLUTFog) >> 8`, captures the initial
   filler/fog/patch/RepMask/clip state, seeds the delta-tracking baselines, and
   sets `g_polyrec_recording = 1`.
4. **Per-call recording.** Each primitive checks the fast global
   `g_polyrec_recording` at its entry and, if set, forwards its arguments:
   `Fill_Poly` ([`POLY.CPP:146`](../LIB386/pol_work/POLY.CPP)), `Fill_Sphere`
   ([`POLYDISC.CPP:142`](../LIB386/pol_work/POLYDISC.CPP)), `Line_A`
   ([`POLYLINE.CPP:904`](../LIB386/pol_work/POLYLINE.CPP)), plus the state setters
   `Switch_Fillers`, `SetCLUT`, and `SetFog`
   ([`POLY.CPP:221`, `:274`, `:299`](../LIB386/pol_work/POLY.CPP)).
5. **Delta compression.** State setters record an event only when the value
   actually changed since the last one. Before every draw call,
   `check_state_changes()`
   ([`POLY_RECORDING.CPP:279`](../LIB386/SNAPSHOT/POLY_RECORDING.CPP)) polls
   `PtrMap`, `RepMask`, the clip rectangle, `Fill_Patch`, and `PtrCLUTGouraud`
   and emits the corresponding `SET_*` event only on change. Textures are
   deduplicated by pointer, size, and content (`find_or_add_texture`), with the
   size derived from the low 16 bits of `RepMask`.
6. **Stop.** At the end of `AffScene()`
   ([`OBJECT.CPP:6159`](../SOURCES/OBJECT.CPP)), `polyrec_stop()` clears the flag,
   copies the now-rendered `Log` buffer as the reference framebuffer, appends the
   `EOF` event, writes all sections to disk, and frees the recording state.

New recordings land in the working directory as `polyrec_0000.lba2polyrec`,
`polyrec_0001…`; promoting one into the regression corpus means moving it into
[`tests/SNAPSHOT/fixtures/`](../tests/SNAPSHOT/fixtures/), where CMake globs them
([`tests/SNAPSHOT/CMakeLists.txt:260`](../tests/SNAPSHOT/CMakeLists.txt)).

### The replay pipeline

Both replay binaries share
[`polyrec_replay.cpp`](../tests/SNAPSHOT/polyrec_replay.cpp) and
[`replay_polyrec_main.cpp`](../tests/SNAPSHOT/replay_polyrec_main.cpp). The flow
in `polyrec_replay_run()`:

1. `polyrec_load()` reads the file into a `T_POLYREC_LOADED`.
2. `allocate_buffers()` mallocs the framebuffer (`width × height`, 8-bit) and a
   Z-buffer (`width × height × 2`), fills the Z-buffer with `0xFF` (far plane),
   and points the engine globals `Log` and `PtrZBuffer` at them.
3. It builds `TabOffLine[y] = y * width` (for `y < 1024`), sets `PTR_TabOffLine`,
   `ScreenPitch`, and `ModeDesiredX/Y` from the recorded header.
4. It applies the recorded CLUT pointers, palette, `PtrTruePal`
   (`= PtrCLUTFog + (initial_clut_line << 8)`), clip rectangle, `Fill_Patch`, and
   `RepMask`, then calls `Switch_Fillers(bank)` and `SetFog(...)` and installs the
   initial texture if one was set.
5. It walks the event stream, dispatching each event to the real engine
   functions — `Switch_Fillers`, `SetFog`, `SetCLUT`, `PtrMap`/`RepMask`
   assignment, clip assignment, `Fill_Patch`, `PtrCLUTGouraud`, and the three
   draw calls. For `FILL_POLY` it copies the recorded vertices into a local
   stack buffer (because `Fill_Poly` may clip them in place) and calls
   `Fill_Poly`.
6. It writes the raw framebuffer to the output path, plus a derived
   `…_zbuf.raw`, and optionally a PPM and a reference PPM.

`--start-after N` / `--stop-after N` gate which draw calls actually execute while
still applying every non-draw state change, so a window of the frame can be
rendered in isolation. These power the bisect and per-call render workflows.

The **only** difference between the two binaries is which library is linked
([`tests/SNAPSHOT/CMakeLists.txt`](../tests/SNAPSHOT/CMakeLists.txt)):

- `replay_polyrec_cpp` links the C++ `poly` library directly.
- `replay_polyrec_asm` links the original ASM objects assembled from `LIB386/`,
  with the symbols `Fill_Sphere` and `Line_A` renamed to `asm_*` via
  `objcopy --redefine-sym`, plus
  [`polyrec_asm_wrappers.cpp`](../tests/SNAPSHOT/polyrec_asm_wrappers.cpp). Those
  wrappers provide C-callable `Fill_Poly`, `Fill_Sphere`, `Line_A`, and
  `Switch_Fillers` that marshal arguments into the Watcom register calling
  convention (`EAX`/`EBX`/`ECX`/`ESI`…) and jump to the ASM. Engine globals
  (`Log`, `PtrZBuffer`, `TabOffLine`, the CLUT pointers, …) keep their original
  names so both paths read and write the same data. Both executables are linked
  `-no-pie` because the generated 32-bit objects use text relocations; the ASM
  build additionally uses `-Wl,--allow-multiple-definition` so the wrappers
  override the C++ symbols.

`replay_polyrec_main.cpp` also has a `--dump` mode that prints the decoded event
stream (and a poly-type histogram) without rendering.

### What "byte-identical" means here

The assertion is **`replay_polyrec_asm` framebuffer == `replay_polyrec_cpp`
framebuffer**, compared with `cmp` over `width × height` bytes of palette
indices. On mismatch, `compare_polyrec.sh` reports the size of each output and,
when sizes match, the count of differing bytes and the first 20 offsets from
`cmp -l`.

Two consequences follow from how the input is captured:

- Both fillers receive **identical integer input**. Vertices are recorded as
  `Struc_Point` with `S16` screen coordinates, so there is no floating-point
  projection step inside replay to diverge — the comparison isolates the
  rasterizer/filler stage and the state machine around it.
- The comparison does **not** involve the live game. The captured reference
  framebuffer (`REFF`) is written to PPM by `render_polyrec.sh` for eyeballing,
  but no script asserts either replay against it. Polyrec answers "do ASM and
  C++ fillers agree on this frame?", not "does either match the retail binary?".

## Current scope and limitations

### What it covers

- The polygon fillers reached through `Fill_Poly` (all poly types — flat,
  gouraud, textured, textured-Z, fog and env-mapping variants), plus
  `Fill_Sphere` ([POLYDISC](../LIB386/pol_work/POLYDISC.CPP)) and `Line_A`
  ([POLYLINE](../LIB386/pol_work/POLYLINE.CPP)).
- The render-state machinery those fillers depend on: filler bank, fog, CLUT
  line and Gouraud-CLUT offset, texture selection and `RepMask`, clip rectangle,
  and `Fill_Patch`.
- Both the painter's-algorithm path (fog, no Z-buffer) and the Z-buffer path,
  since replay always allocates and wires up a Z-buffer.
- Both exterior terrain and 3D objects, because both go through the same three
  primitives.
- The committed corpus is 18 fixtures (`polyrec_0000`–`polyrec_0017`), all
  **640 × 480**, pitch 640.

### What it does not cover

- **Projection and rotation.** Vertices are recorded post-projection as integer
  screen coordinates, so [`LPROJ3DF`](../LIB386/3D/LPROJ3DF.CPP),
  [`LROT3DF`](../LIB386/3D/LROT3DF.CPP), and the `long double`/`lrintl` precision
  questions are entirely upstream of the capture point. Those have their own
  tests under [`tests/3D/`](../tests/3D/) and
  [`tests/fpu_precision/`](../tests/fpu_precision/).
- **Draw ordering.** The sort tree in [`SORT.CPP`](../SOURCES/SORT.CPP) decides
  the *order* of draw calls; polyrec records the already-ordered stream and
  replays it verbatim. It cannot surface an ordering bug — only filler output
  given a fixed order.
- **2D/sprite/blit paths** (`CopyMask`, `ScaleSprite`, font, box fills, etc.),
  which are covered by [`tests/SVGA/`](../tests/SVGA/).
- **Agreement with the retail game.** The reference framebuffer is captured but
  never asserted.

### Known fragility and baked-in assumptions

- **64-texture cap.** `POLYREC_MAX_TEXTURES` is 64. Once exceeded,
  `find_or_add_texture` returns `-1`; the `SET_TEXTURE` event is simply not
  emitted, so replay keeps using whatever texture was last bound. A frame with
  more than 64 distinct textures records silently wrong.
- **64-vertex polygons.** Replay copies recorded vertices into a fixed
  `Struc_Point local_points[64]`
  ([`polyrec_replay.cpp:308`](../tests/SNAPSHOT/polyrec_replay.cpp)) but still
  passes the original `nb_points` to `Fill_Poly`; a polygon with more than 64
  vertices would over-read the stack buffer. In practice poly vertex counts are
  small.
- **Hardcoded Z-buffer size in `--zbuf`.** The explicit `--zbuf` dump in
  [`replay_polyrec_main.cpp:270`](../tests/SNAPSHOT/replay_polyrec_main.cpp)
  writes exactly `640 * 480` `U16`s regardless of the recorded resolution. (The
  automatic `…_zbuf.raw` written from `polyrec_replay.cpp` uses the correct
  `fb_size`.)
- **`TabOffLine` height cap.** The per-line offset table is built only for
  `y < 1024`, fine for 480 but a ceiling for taller buffers.
- **Init-state placeholders.** `init_state.active_texture` is always written as
  `0xFFFFFFFF` (none) — the first real texture arrives as a `SET_TEXTURE` event;
  `init_state.filler_bank` is seeded with `FILL_POLY_TEXTURES` and overwritten by
  the first `Switch_Fillers`.
- **Delta compression trusts the hooks.** State changed by any route that does
  not pass through the hooked setters or is not one of the values polled in
  `check_state_changes()` would not be reflected in the stream.
- **Fixed format/endianness.** The loader rejects any magic or version other than
  `LBA2PREC` / `1`. Structs are `#pragma pack(1)` and serialized by raw `memcpy`,
  so fixtures are little-endian x86 artifacts.
- **Whole-file comparison.** `compare_polyrec.sh` assumes both outputs have the
  same dimensions (true by construction — both replays use the recorded
  `width`/`height`). It has no concept of a sub-rectangle.

## Extension points

### Capturing projection setup

Today the recording begins at `Fill_Poly`/`Fill_Sphere`/`Line_A`, i.e. *after*
projection. Vertices are already in screen space, centred and scaled by the
projection routines (which use `XCentre`/`YCentre` from
[`CAMERA.H`](../LIB386/H/3D/CAMERA.H), focal distance, etc.). The file captures
none of those projection inputs — only screen dimensions, pitch, and the clip
rectangle.

To make projection part of what polyrec exercises, two things would change
together:

1. **Move the capture point upstream of projection.** Record pre-projection 3D
   vertices instead of the post-projection `Struc_Point`, and run the projection
   inside replay. As long as vertices are frozen at their projected screen
   coordinates, projection changes cannot be A/B-tested through polyrec at all —
   the result is baked into the fixture.
2. **Add the projection parameters to the captured state.** Extend
   `T_POLYREC_INIT_STATE` ([`POLY_RECORDING.H:135`](../LIB386/H/SNAPSHOT/POLY_RECORDING.H))
   with the projection globals (`XCentre`, `YCentre`, focal/optic distance, any
   aspect terms), snapshot them in `polyrec_start()` alongside the existing init
   state ([`POLY_RECORDING.CPP:180`](../LIB386/SNAPSHOT/POLY_RECORDING.CPP)),
   bump `POLYREC_VERSION`, write and read them in the `INIT` section, and apply
   them in the replay setup
   ([`polyrec_replay.cpp:204`–`225`](../tests/SNAPSHOT/polyrec_replay.cpp)).

### Centre-crop comparison: 640×480 baseline vs 854×480 wide

This is not supported today and would need new code in two places.

What blocks it now:

- The comparison is whole-file `cmp`. An 854×480 frame is 409,920 bytes against
  the baseline's 307,200, so `cmp` reports a size mismatch and never aligns rows.
  Nothing in the harness understands width or a sub-rectangle.
- A fixture bakes in `screen_width` (from `ModeDesiredX`) **and** the projected
  vertices. A 640-wide fixture cannot simply be replayed at 854, because its
  vertices are already projected for 640. A wide frame therefore requires either
  a recording captured at 854 wide, or the upstream-projection change described
  above so replay can re-project at a different width.

What would be needed:

1. **Two frames to compare.** Produce the 640-wide baseline and the 854-wide
   frame, by whichever of the two routes above is chosen.
2. **A crop-aware comparator** to replace `cmp`. The framebuffer is row-major,
   one byte per pixel, with pitch equal to width (`ScreenPitch = recorded width`,
   `TabOffLine[y] = y * width`,
   [`polyrec_replay.cpp:179`–`184`](../tests/SNAPSHOT/polyrec_replay.cpp)). To
   compare the central 640 columns of the 854-wide frame against the 640-wide
   baseline, for each row `y` in `[0, 480)`:
   - wide offset = `y * 854 + (854 - 640) / 2` = `y * 854 + 107`
   - baseline offset = `y * 640`
   - compare 640 bytes.

   The 107 columns trimmed from each side are the wide frame's new content and
   are excluded from the equality check. This per-row crop has to be added as a
   new mode or script; [`compare_polyrec.sh`](../tests/SNAPSHOT/compare_polyrec.sh)
   has no per-row logic to build on.
3. **A defined expectation for the central region.** Whether the central 640
   columns *should* be byte-identical depends on how widescreen projection is
   defined (horizontal extension of the field of view versus a rescale). The
   code neither implements nor assumes either; the crop comparison is the tool
   you would build to answer that question, not an answer in itself.

## Related files

- [`LIB386/H/SNAPSHOT/POLY_RECORDING.H`](../LIB386/H/SNAPSHOT/POLY_RECORDING.H) — file format, event/section constants, in-memory structs, API.
- [`LIB386/SNAPSHOT/POLY_RECORDING.CPP`](../LIB386/SNAPSHOT/POLY_RECORDING.CPP) — capture, write, and load implementation.
- [`tests/SNAPSHOT/polyrec_replay.cpp`](../tests/SNAPSHOT/polyrec_replay.cpp) / [`.h`](../tests/SNAPSHOT/polyrec_replay.h) — shared replay logic.
- [`tests/SNAPSHOT/replay_polyrec_main.cpp`](../tests/SNAPSHOT/replay_polyrec_main.cpp) — CLI entry point, argument parsing, `--dump` mode.
- [`tests/SNAPSHOT/polyrec_asm_wrappers.cpp`](../tests/SNAPSHOT/polyrec_asm_wrappers.cpp) — Watcom-convention wrappers for the ASM replay binary.
- [`tests/SNAPSHOT/CMakeLists.txt`](../tests/SNAPSHOT/CMakeLists.txt) — builds both replay binaries and registers one CTest per fixture.
- [`tests/SNAPSHOT/compare_polyrec.sh`](../tests/SNAPSHOT/compare_polyrec.sh), [`render_polyrec.sh`](../tests/SNAPSHOT/render_polyrec.sh), [`bisect_polyrec.sh`](../tests/SNAPSHOT/bisect_polyrec.sh) — comparison, rendering, and bisection scripts.
- [TESTING.md](TESTING.md) — operator-facing command and workflow reference.
