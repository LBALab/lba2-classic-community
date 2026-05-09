# Crash investigation runbook

How to investigate a native crash in this engine — the asan + gdb workflow, with the commands that have actually worked. Companion to [PLATFORM.md](PLATFORM.md), which catalogues the hazard classes; this doc is the process for finding which class a crash belongs to.

## Quick path

Most crashes are localised. The fast loop is:

1. Reproduce under gdb to get a stack frame.
2. If the stack alone doesn't tell you what's wrong, rebuild (or preload) with AddressSanitizer and rerun.
3. Inspect frame state at the fault to identify what's malformed.

Don't reach for ASan on a known null deref or a clean assertion failure — gdb alone is enough. Reach for ASan when the crash is intermittent, or the stack ends inside a tight inner loop where the actual cause is upstream.

## Building with AddressSanitizer

There are two paths. The preload trick gets you running without rebuilding the world; the static-link build is cleaner for repeat sessions.

### Option A — preload at run time (no rebuild)

If you have a debug build but not an ASan build, this is the quick path. Inside gdb:

```
(gdb) set exec-wrapper env LD_PRELOAD=$(gcc -print-file-name=libasan.so)
(gdb) set environment LBA2_GAME_DIR=/path/to/data
(gdb) run
```

Without the preload you'll see `ASan runtime does not come first in initial library list` and the process exits immediately — that error is the signal you forgot the wrapper. Use `clang -print-file-name=libclang_rt.asan-*.so` instead of `gcc` if you're on a clang build.

### Option B — link statically (recommended for repeated runs)

```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -static-libasan -static-libubsan" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -static-libasan -static-libubsan" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -static-libasan -static-libubsan"
cmake --build build-asan -j
```

Run via `gdb --args ./build-asan/SOURCES/lba2 --game-dir /path/to/data`.

### ASan options worth setting

```
(gdb) set environment ASAN_OPTIONS=abort_on_error=1:halt_on_error=1
(gdb) set environment UBSAN_OPTIONS=print_stacktrace=1:abort_on_error=1
```

Without `abort_on_error=1`, ASan prints its report and calls `exit()`, which gdb passes through — you lose the live stack. With it, gdb stops at the moment of the bad access so you can interrogate state in place.

## Inspecting state at the crash

When SIGSEGV hits, the cheap interrogation:

```
(gdb) bt
(gdb) frame N             # walk to the frame you care about
(gdb) info locals
(gdb) print var
(gdb) print (long)var     # if var is unsigned, see the actual value as signed
(gdb) print (void*)Log    # printable form of an opaque pointer
(gdb) x/16xb maskData-16  # peek at bytes around a buffer cursor
(gdb) info registers
```

Compute the offending pointer by hand and check it matches the faulted address — this is how you go from "crashed somewhere in a loop" to "the offset was constructed wrong before the loop ran":

```
(gdb) print (void*)(Log + (long)initialOffset)
```

If that matches `screen` (or is within a few bytes — the inner loop will have advanced it), the offset was constructed wrong upstream. Walk back up the call stack until you find where the bad value was introduced.

## Reading ASan output

The header line tells you what kind of bad access it was:

| Header | What it usually means | Where to look |
|---|---|---|
| `heap-buffer-overflow` | Walked off a `malloc`'d buffer | `freed by` and `allocated at` traces show the buffer's lifetime |
| `heap-use-after-free` | Read/wrote freed memory | The `freed by` stack tells you who freed it |
| `stack-buffer-overflow` | Walked off a stack array | The function holding the array |
| `global-buffer-overflow` | Walked off a static/global array | The `0x… is located N bytes (after) the variable …` line names the symbol |
| `stack-use-after-return` | Returned a pointer to a local | The frame where the local was declared |
| `SEGV` (ASan ran but didn't catch) | Wild pointer not in any tracked allocation | Probably bad pointer arithmetic; gdb is the better tool from here |

The last row is the one that means "the bug isn't a normal overrun." That's the signal to pivot from ASan to gdb-and-arithmetic.

## When the bug is a pointer-arithmetic trap

If the faulted target pointer (`screen`, `dst`, etc.) lands far outside any expected buffer, and the local offset variables include a `U32` that may have come from a signed source, suspect a 32→64-bit wraparound. See [PLATFORM.md §1 — Renderer-side wraparound](PLATFORM.md#renderer-side-wraparound) for the class. Worked example with full transcript: [PR #84](https://github.com/LBALab/lba2-classic-community/pull/84).

The mechanical proof for a wrap bug: at the fault site, `print (long)<offset_var>` shows a value near `0xFFFFFFE8` (or any large near-`UINT32_MAX` figure), `print (void*)Log` shows the buffer base, and `Log + that_offset` matches the faulted pointer to within a few bytes. That's the signature; once you see it, fix is to switch the geometry locals to `S32`.

## After the fix: pin it

Per [project policy](FEATURE_WORKFLOW.md), bug fixes land with a regression test. For host-only repros (no Docker, no retail data), the pattern is:

- New `tests/<name>/` directory with `test_<name>.cpp` and `CMakeLists.txt`.
- `register_host_test(test_<name>)` in the `CMakeLists.txt` adds it under the `host_quick` ctest label.
- `add_subdirectory(<name>)` in `tests/CMakeLists.txt`.

Working example: `tests/copymask_negx/`. Verify the test catches the regression by reverting the fix, rebuilding, and confirming the test fails — only then is it pinning anything useful.

## Things that look like crashes but aren't

- **`ASan runtime does not come first…`** — preload not set. See Option A above.
- **`AddressSanitizer:DEADLYSIGNAL`** with no detail — usually means ASan caught a SEGV but couldn't symbolize. Build with `-fno-omit-frame-pointer` (or attach gdb to the running process) and re-run.
- **`stack-overflow on …`** with a deep stack — usually unbounded recursion, not a corruption bug.
