# Perftrace

Ring-buffer per-frame timing capture for diagnosing perf cliffs that need real
user input or scene state to reproduce — high-resolution stalls,
platform-specific frame-pacing, slow modal loops. Console-driven, opt-in, zero
cost when inactive.

Companion to [TIMING.md](TIMING.md) (timing semantics) and
[CONSOLE.md](CONSOLE.md) (verb registration).

## When to reach for it

- A user reports "the game is slow on my machine at native resolution" and you
  can't reproduce locally.
- A platform-specific symptom (Mac M-series, Steam Deck, Wayland fullscreen,
  WSLg) is suspected to be in the present pipeline rather than the engine.
- You want a quick baseline of where per-frame time is going before reaching
  for `perf` / Instruments / VTune.

What it does **not** do: profile inside functions, attribute cycles to call
sites, or capture call-graph data. For those use `perf record` / dtrace /
VTune. Perftrace measures the engine's own per-frame timeline.

## What it captures

Per frame (loop iteration in `MainLoop`):

| Field | Source | Meaning |
|---|---|---|
| `frame_us` | `Perftrace_FrameBegin` → `Perftrace_FrameEnd` | Total per-iteration wall time. |
| `scene_us` | `Perftrace_PhaseBegin/End(SCENE)` around `AffScene` | Scene render including any presents fired from inside it. |
| `present_us` | `PresentTimingFn` around `PresentFrame` | All `PresentFrame()` invocations during the frame (palette LUT scan + texture upload + GL submit + vsync wait). |
| `scene_tag` | `CubeMode` at FrameEnd | `iso` (interior cube), `ext` (exterior), or `?` (pre-load). |

Plus aggregate counters that survive ring rotation:

- `capture_window` — wall-clock ms from first `FrameEnd` to most recent.
- `total_frames` — every `FrameEnd` since the last `Reset`/`Start` (not just the ring).
- `FrameEnd/s` — derived rate; tells you the true loop rate even if the ring has rotated past the start.

## Console verb

```
perftrace [start | stop | reset | status | dump [path] | autodump [path]]
```

| Subcommand | Effect |
|---|---|
| (no args) | Print status + last-60-frame summary. |
| `start` | Reset the ring buffer and enable capture. |
| `stop` | Halt capture without dumping. |
| `reset` | Clear the ring buffer (keeps `active` state). |
| `status` | Print a one-line summary to the console scrollback. |
| `dump [path]` | Write a detailed dump to `path` (default `/tmp/perftrace.txt`). |
| `autodump [path]` | Arm a dump at process exit. Useful for headless `--exec`-driven captures where you can't type `dump` between the tick loop and exit. |

The dump is plain text with three sections:

```
# perftrace dump
# active=1  ring_count=256  ring_capacity=256
# capture_window=4287 ms  total_frames=299  rate=69.7 FrameEnd/s

## All frames  (n=256)
  frame_us  min=10620  avg=12700  p50=12529  p95=14484  p99=17036  max=18223  (~78.7 fps)
  scene avg=12623 us  (99.3% of frame)
  present avg=12112 us  (95.3% of frame)

## Interior / iso  (n=256)
  ... same stats filtered to iso frames ...

## Exterior  (n=0)
  ... same stats filtered to exterior frames ...

## Per-frame samples (oldest first, microseconds)
# idx  tag    frame   scene  present
    0  iso    12969   12904   12425
    1  iso    15268   15188   14696
    ...
```

The per-frame trace lets you spot transition spikes (cube changes, modal
entry/exit) that the aggregate stats hide.

## Typical workflows

### Diagnosing a high-res stall (interactive)

```
perftrace reset
perftrace start
... play normally; reach the suspect scene ...
perftrace dump /tmp/native-stall.txt
```

Compare `present` vs `scene` shares. If `present > scene`, the cost is in
the SDL → GL → compositor chain (texture upload, llvmpipe upscale, vsync).
If `scene > present`, the cost is in the engine's draw code (`AffScene`,
`TreeInsert`, the projection pipeline).

### A/B at different resolutions (interactive)

```
perftrace reset; perftrace start
... play 5-10 s at the current resolution, dump ...
perftrace dump /tmp/a-640.txt

resolution 1920x1024
perftrace reset; perftrace start
... play 5-10 s at the new resolution, dump ...
perftrace dump /tmp/b-1920.txt
```

The `dump_section` per-frame stats let you see whether `present_us` scales
linearly with pixel count (CPU palette LUT + GL upload) or is dominated by a
fixed cost (vsync wait blocking on the compositor).

### Headless capture (CI / automation)

```
./build/SOURCES/lba2cc \
  --game-dir /path/to/data \
  --load <save> --resolution 1920x1024 \
  --exec "perftrace autodump /tmp/run.txt; perftrace start" \
  --tick 600 --no-audio --exit
```

`autodump` arms an `atexit` handler so the dump is written when the engine
returns from `MainLoop`. No interactive console needed.

## Cost

| State | Cost |
|---|---|
| Inactive (`!Perftrace_IsActive()`) | One branch test per `FrameBegin`/`FrameEnd`/`PhaseBegin`/`PhaseEnd` call (~5 ns each). |
| Active | `SDL_GetPerformanceCounter` + 32-byte ring write per frame (~1 µs total). |
| At-rest binary size | ~10 KB (one BSS ring of 256 × 32 B + the module code). |

Hooks are unconditionally compiled in — the LIB386_TRACE precedent. No `-D`
flag to set or build to recreate. Type the verb when you need it.

## API surface

For engine code that wants its own capture hooks (e.g., a future
`Perftrace_PhaseBegin(PHASE_SORT)` around `TreeInsert`):

```c
#include "PERFTRACE.H"

void Perftrace_Init(void);
void Perftrace_FrameBegin(void);
void Perftrace_FrameEnd(void);
void Perftrace_PhaseBegin(int phase);
void Perftrace_PhaseEnd(int phase);
void Perftrace_Start(void);
void Perftrace_Stop(void);
int  Perftrace_IsActive(void);
void Perftrace_Reset(void);
int  Perftrace_Dump(const char *path, int max_samples);
void Perftrace_SetAutoDumpPath(const char *path);

/* No-arg trampolines for SDL.CPP SetPresentTimingCallbacks(). */
void Perftrace_PresentBegin(void);
void Perftrace_PresentEnd(void);
```

Adding a new phase: bump `PERFTRACE_PHASE_COUNT` in `PERFTRACE.H`, add the
enum entry, and the dump format auto-includes the new column.

## What perftrace is not

- **Not a profiler.** It tells you which *phase* of the frame is heavy, not which
  function inside that phase. For that, run `perf record` / Instruments / VTune
  with the capture active and dump as a pointer.
- **Not a long-running telemetry stream.** The ring is 256 frames (≈ 4 s at 60
  fps). Long-running aggregate counters (`total_frames`, `capture_window`,
  `FrameEnd/s`) extend the visible window but the per-frame trace is bounded.
- **Not deterministic.** Timing varies run-to-run. For repeatable timing
  invariants use the projection corpus
  ([tests/automation/test_projection_corpus.sh](../tests/automation/test_projection_corpus.sh))
  under `--fixed-dt`.

## Related

- [TIMING.md](TIMING.md) — clocks/locks/ManageTime semantics; perftrace was
  the diagnostic that pinned the 1997 `ManageTime` bug. The fix landed in
  `LIB386/SYSTEM/TIMER.CPP`; the techniques used to find it landed here.
- [CONSOLE.md](CONSOLE.md) — verb registration mechanics.
- [CONTROL.md](CONTROL.md) — `--exec` for headless captures.
- `SOURCES/PERFTRACE.{H,CPP}` — source of truth.
