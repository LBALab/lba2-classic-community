# Native crash reports: research and plan

**Status:** Implemented. The design below landed as planned in section 6, with the divergences the implementation measured recorded in section 7; where the two disagree, section 7 and the code win. What the engine does today is in [the logging concept](../knowledge/subsystems/logging.md), "Crash reports", and [the decision that the report hands the crash back](../knowledge/decisions/the-crash-report-hands-the-crash-back.md). Tracks issue [#680](https://github.com/LBALab/lba2-classic-community/issues/680).

**Scope:** When the engine dies on a fatal signal or an unhandled exception, write a crash block to `adeline.log` that a player can send and a maintainer can turn into function names, on Linux, macOS, Windows and Android. Keep the previous run's log so relaunching after a crash does not erase it. Everything else about the process's death stays as it is: exit status, the platform's own crash report, sanitizer reports, debuggers.

**Evidence tags:** `[measured]` was run and observed, with the platform named. `[read]` was read in the tree or in a cited external source. `[inferred]` is reasoning not yet checked. Paths and findings are against `c42ea33f`.

**Platforms measured:**

- macOS 26.6 arm64, Apple clang 21, `macos_arm64` preset, Release `-O3 -flto=thin`.
- WSL2 Ubuntu 24.04 x86-64, gcc 13.3, glibc 2.39, `linux` preset, Release `-O3 -flto=auto`, `LBA2_LINK_STATIC=ON`, Ubuntu's default `-fstack-protector-strong`.
- Android 16 (API 36) arm64 emulator, Release `-O3` thin LTO, 16 KB pages.
- Windows 11, MSYS2 UCRT64 gcc 15.2, standalone programs only.

**Read first:** [the boot concept](../knowledge/subsystems/boot.md) for the order of `main` and every way the process ends, and [the logging concept](../knowledge/subsystems/logging.md) for the log's lifecycle. This plan does not repeat them.

---

## 1. Why

The holomap crash fixed in #676 reached the maintainer as "the app closes" with an ordinary `adeline.log`. The only record of the fault was an Android tombstone the player could not read. Two things lose a crash today `[read]`:

- **Nothing catches it.** There is no signal handler or exception filter in `SOURCES` or `LIB386`. The log flushes per line, so the lines before the fault are on disk, and the fault itself adds nothing.
- **The next launch erases the log.** `CreateLog` opens `adeline.log` with `"wb"` ([LOGPRINT.CPP](../../LIB386/SYSTEM/LOGPRINT.CPP)).

## 2. Decisions taken

| # | Decision |
|---|---|
| D1 | One PR, with separate commits for each part. |
| D2 | Keep exactly one previous log, `adeline.prev.log`. |
| D3 | All four platforms. |
| D4 | Symbols: in this PR, embed the commit SHA and log module build IDs and offsets, stop stripping the AppImage, and strip the debug info the APK ships by accident. A later PR builds with `-g` and publishes split symbol files. |
| D5 | Breadcrumbs only from state the engine already holds. No new instrumentation. |
| D6 | Silent: no notice on the next launch and no dialog. |
| D7 | Always on. It exists so players can report crashes; there is no user switch. |
| D8 | Give the music decoder thread and the SDL audio callback thread their own alternate signal stacks, so a stack overflow there is reported. |
| D9 | On Android, save the system tombstone (`.pb`) next to `adeline.log` on the next launch. An Android-only option, not a model for the other platforms. |
| D10 | Prototype and experiment first, and revise this design before committing far into it. |
| D11 | No second phase that runs the full `dumpstate` after the block. Its stdio lock can deadlock a crashed process, and its watchdog would compete for the timer the macOS chaining rule uses. The block carries the scalar fields instead. |
| D12 | The in-engine Windows experiment is skipped. Its checks become required tests of the Windows commit. |

## 3. Research findings

### 3.1 POSIX: Linux, macOS, Android

- **Install** with `sigaction(SA_SIGINFO | SA_ONSTACK)` for SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT and SIGTRAP, saving each previous action `[read]`. SIGTRAP is needed: `__builtin_trap` is `brk` on arm64 and raises SIGTRAP `[measured, macOS, Android]`.
- **Chain, never reset to the default.** Resetting to `SIG_DFL` loses sanitizer reports and rewrites the platform report. How to chain differs by platform: see section 4.2. `[measured, WSL2]` the shell sees 139, 135, 132, 136, 134 and 133 for SEGV, BUS, ILL, FPE, ABRT and TRAP with the handler chained, the same as without it; macOS gives 138 for BUS.
- **Sanitizers.** ASan installs SIGSEGV, SIGBUS and SIGFPE handlers and a per-thread alternate stack before `main`. `[measured, WSL2, in the engine]` chaining prints our block and then ASan's full report, exit 1, including when the chained signal is a re-queued one. Resetting to `SIG_DFL` loses ASan's report and exits 139. So the handler is installed in sanitizer builds too. An ASan build already exits 1 on a segfault, not 139.
- **Platform reports survive chaining.** `[measured]` With the handler chained, the macOS crash report, the WSL core and the Android tombstone plus `ApplicationExitInfo` all still appear, with the same crashing stack as a run without the handler.
- **Android.** libsigchain is linked so that an app library's `sigaction` reaches it before libc; for a signal ART has claimed, it runs ART's handlers, then ours `[read, AOSP art/sigchainlib/sigchain.cc]`. `[measured]` the saved previous action is debuggerd's for every signal except SIGBUS, whose previous action is in `libandroid_runtime.so`. Bionic gives every thread, engine pthreads included, a 32 KB alternate stack; the install keeps it. Debuggerd installs with `SA_EXPOSE_TAGBITS`; without it `si_addr` loses the top-byte tag the tombstone shows.
- **Async-signal-safety.** Only calls on the POSIX list, plus `rt_tgsigqueueinfo` and `process_vm_readv` where named below: `write` to a descriptor opened with `O_APPEND` at install, `sigaction`, `raise`, `setitimer`. No stdio, no `malloc`, no `snprintf`, no `dladdr`. Numbers are formatted by hand. Module bases and build IDs are cached outside the handler `[read]`.
- **A handler that blocks turns a crash into a hang.** The harness reads timeout exit 124 as a hang, so a deadlock on a stdio lock would misreport every crash `[read]`.

### 3.2 Windows

The release is built with MSYS2 UCRT64 GCC, statically linked, with `-O3` and LTO. The toolchain floats: CI installed gcc 16.2 on its last run `[read]`. Everything below `[measured, MSYS2 UCRT64 gcc 15.2, standalone programs]`:

- `SetUnhandledExceptionFilter` and a vectored handler both catch a null write (`C0000005`), `ud2` (`C000001D`), integer divide by zero (`C0000094`), a stack overflow (`C00000FD`) and a fault on another thread.
- **`abort()` is not an exception.** UCRT ends it with `__fastfail` (`C0000409`), which no filter sees; a CRT `signal(SIGABRT, ...)` handler does run. Assertions and `std::terminate` need that handler.
- **A filter already exists before `main`:** MinGW's `_gnu_exception_handler`. `SetUnhandledExceptionFilter` returns it; chaining to it left the exit code and Windows Error Reporting unchanged.
- **Stack walk:** `RtlLookupFunctionEntry` plus `RtlVirtualUnwind` on a copy of the exception context gives clean frames from the faulting function. GCC emits `.pdata` for every function. `StackWalk64` walks too, but cannot name MinGW functions, so frames are named offline.
- **Error Reporting runs after `EXCEPTION_CONTINUE_SEARCH`** unless the inherited error mode has `SEM_NOGPFAULTERRORBOX`, which a launch from MSYS2 bash or WSL does. A player launching from Explorer gets a report.
- **Exit codes:** the parent sees the exception code with or without a handler. MSYS2 bash reports 139 and 132, but 127 for a stack overflow and for `abort()`.
- **SDL3** installs no unhandled-exception filter. It adds a vectored handler only briefly to name each thread.
- **Addresses:** the image is ASLR-relocated, so log module-relative offsets.
- **One run inside the engine.** `[measured, crash segv launched from cmd]` a prototype that saved MinGW's filter as the previous one wrote a complete block: 19 `RtlVirtualUnwind` frames and a module table with the PE `TimeDateStamp` as identity. But the process exited with code 3, not `C0000005`. The block's code and address fields are empty, frame 1 lies just past the saved previous filter's address, and ntdll's exception dispatcher frames come before the engine's. `[inferred]` The block was written from a CRT signal handler that MinGW's filter called, and the handler's return ended the process through the CRT with exit 3. Keeping the native exit code is a required test of the Windows commit.
- **Not measured inside the engine:** whether `RtlVirtualUnwind` can fault inside the filter on a smashed stack, which on every POSIX platform turned the platform report into a report on the unwinder (section 4.1). Since measured: section 7.2.

### 3.3 Threads

- Engine code runs on the main thread, the SDL audio callback (`SampleStreamCallback` in [SAMPLE.CPP](../../LIB386/AIL/SDL/SAMPLE.CPP)), and the `OGGDecode` thread the engine creates for music (`DecodeThreadFunc` in [STREAM.CPP](../../LIB386/AIL/SDL/STREAM.CPP)). The folder picker's callback may run on a backend thread `[read]`.
- `[measured, WSL2]` the SDL audio thread and `OGGDecode` have no alternate stack. A stack overflow on either produces no block; the kernel kills the process, 139. With an alternate stack installed at the start of the thread, the block is written.
- `[measured, Android]` every thread already has bionic's alternate stack, and an overflow on an engine pthread was reported on it. D8 is met on Android without code.
- macOS threads were not measured `[inferred: same as Linux]`. Since measured, with a difference: section 7.4.

### 3.4 Breadcrumbs

- **What exists:** about fifteen plain globals a handler can read without calls: `Island`, `NumCube`, `CubeMode`, `NewCube`, `Comportement`, `CinemaMode`, `FlagFade`, `NbFramePerSecond`, `TimerRefHR`, the chapter in `ListVarGame`, and the hero's position, angles, life, body and animation in `ListObjet[NUM_PERSO]`. `ListObjet` is a fixed array, so every field has a fixed address `[read]`.
- **What does not exist:** no global says a modal is open. `HoloMode` stays 1 after the holomap closes. There is no `dumpobj` command; the state commands are `status`, `dumpstate`, `flags` and `objtrace` `[read]`.
- **`dumpstate`** writes almost exactly the right fields, but through `fopen` and `fprintf`, and it walks `ListObjet` up to `NbObjets` ([CONTROL.CPP](../../SOURCES/CONTROL.CPP), `control_dump_state`). It cannot run inside a handler (D11). `fps: 0` is the nearest thing to a modal signal: a modal loop owns the frame.
- **Linkage:** LIB386 cannot include SOURCES, so the game registers a table of field names and addresses at boot ([CODESTYLE.md](../../CODESTYLE.md)).

### 3.5 Build identity and symbols

`[measured on the downloaded latest release assets]` The releases are not symbol-less:

| Artifact | Symbols today |
|---|---|
| Android APK | Full DWARF in `libmain.so`, `libSDL3.so` and `libc++_shared.so`: about 18 MB of the 24 MB APK per ABI, not intentional |
| Windows zip | COFF symbol table, no DWARF; `MainLoop` is `_Z8MainLoopv` |
| Linux tarball, macOS | Symbol table |
| Linux AppImage | Fully stripped by its packaging tool |

- **Build identity is weak.** No git hash is embedded; every rolling build reports the `VERSION` file's `0.13.0-dev` and a build date `[read]`.
- **Rebuilding to symbolize is unreliable** because the Linux, Windows and macOS CI toolchains float `[read]`.
- **A symbol table is enough for function names.** `[measured]` block offsets resolve to the right functions with the symbol table alone on macOS (`llvm-symbolizer` on a `strip -S` copy) and on the Linux tarball build (`addr2line -f -C`, which adds LTO clone suffixes). File, line and inlined frames need `-g`.
- **Module identity:** macOS logs `LC_UUID`, Linux and Android the GNU build ID. On Android the process sees `base.apk!/lib/arm64-v8a/libmain.so` while the tombstone names `base.apk!liblba2cc.so`, because the library's SONAME was never renamed; the build IDs match, and `address - dlpi_addr` equals the tombstone's `rel_pc` `[measured]`. Symbolizers key on the build ID.
- **`-g` does not change code.** `[measured, Windows CI-style build]` `.text`, `.data`, `.pdata` and `.xdata` are byte-identical with and without `-g`. `[measured, macOS and Linux]` no instruction differs.
- **Split debug info works:**
  - **Windows** `[measured]`: `objcopy --only-keep-debug` then `--strip-debug --add-gnu-debuglink`; `addr2line -f -C -i -e` resolves `HoloMap()+0x20` to `SOURCES/HOLOGLOB.CPP:1203`. The exe is 7.7 MB without `-g` and 10.9 MB with it; the split debug file is 5.4 MB.
  - **macOS** `[measured]`: with LTO, `dsymutil` produces an empty dSYM unless the link passes `-Wl,-object_path_lto,<path>`; with it, `atos -i` gives inlined frames, `SetVideoPalette (SDL.CPP:179)` inside `Dial (MESSAGE.CPP:2112)`. `atos` also finds a dSYM by UUID on its own, so a symbol-table-only check needs `llvm-symbolizer`.
- **Comparable projects** publish symbol files as release assets: PCSX2, DuckStation, OpenRCT2, OpenMW, Godot `[read]`.

### 3.6 Android recovery on the next launch

- The app has no Gradle project; `scripts/packaging/bundle-android.sh` builds it. `minSdkVersion` 24, `targetSdkVersion` 34, SDL's own activity with no subclass, and one project Java class, `PermissionHelper`, called over JNI from [ANDROID.CPP](../../LIB386/SYSTEM/ANDROID.CPP) `[read]`.
- `ActivityManager.getHistoricalProcessExitReasons` (API 30, about 87% of devices) returns the previous exits of our own package with no permission. `REASON_CRASH_NATIVE` is 5 and `getStatus` is the signal number `[read; measured on API 36: every crashing run of the prototype had one]`.
- `ApplicationExitInfo.getTraceInputStream` returns the tombstone as a protobuf from API 31 (about 79%) `[read]`. It can be null when the global tombstone buffer has rotated. `[measured]` one lba2cc crash's tombstone is 265 KB, with engine frames already symbolized on device (`MainLoop()+7580`).
- Records persist across launches, so the app must remember the timestamp of the last crash it saved `[measured]`.
- An Android app's stdin, stdout and stderr are `/dev/null`, and replacing SDL's log output function stops the engine's lines reaching logcat: on Android the engine log is `adeline.log` only `[measured]`.
- **Shape:** a `CrashHelper` Java class next to `PermissionHelper`, called from native after the log exists, writing `crash-<timestamp>.pb` into the user folder and returning a one-line summary for the log. Maintainers decode it off device with AOSP's `pbtombstone` or `protoc --decode`.

### 3.7 Log rotation

- Rename `adeline.log` to `adeline.prev.log` in `CreateLog`, after the user folder and profile are resolved. On Windows `rename` fails onto an existing file, so replace explicitly `[read]`.
- Nothing assumes the log starts empty. `tests/automation/test_cli_flag_contract.sh` excludes `adeline.log` from a snapshot and needs `adeline.prev.log` excluded too. `test_logprintf_early_boot_fallback` goes through `CreateLog` and must stay green `[read]`.

### 3.8 What the harness and CI depend on

- Scripts check 139, a negative return code and signal names (`tests/automation/test_negative.sh`, `tests/savegame/corpus/native_fallback_check.py`, `run_harness.py`), and read 124 as a hang `[read]`. The chaining rules in section 4.2 preserve all of them; `_exit(139)` would break the negative return code checks.
- CI builds `lba2cc` but never runs it, so a forced-crash test has to be a host test that launches a child process, with `CreateProcess` rather than `fork` on Windows `[read]`.
- The recorder flushes every tick and deliberately leaves a crashed session without an end state; the handler must not write one ([recording concept](../knowledge/subsystems/recording.md)) `[read]`.

## 4. Phase 0 results

Throwaway prototypes inside the engine, on local branches never pushed: a handler installed right after `atexit(Log_Shutdown)` in `main`, a raw `write` to `adeline.log`, a module cache, several frame sources side by side, and a `crash <kind>` console verb with one trigger per signal. E1 ran on macOS, E2 on WSL2, E4 on the Android emulator. E3 (Windows) was skipped (D12). E5, E6 and E7 were not run: E6 is dropped with the feature (D11), and the E5 and E7 checks become tests of their commits (section 6).

### 4.1 Frame sources

| Source | macOS arm64 | Linux x86-64 | Android arm64 |
|---|---|---|---|
| Frame-pointer walk from the context, bounds-checked to the thread stack | Never faulted. Misses a frameless leaf's caller, which `lr` holds: inserting `lr` when its function differs from frame 0's and the chain's first return address matched the crash report for every fault | Frame 0 only: at `-O3` `rbp` is a general register | Never faulted. With `lr`, matched the tombstone frame for frame |
| `backtrace()` in the handler | Drops the faulting frame | Keeps it; loses every frame on a nested fault | not measured |
| `_Unwind_Backtrace` in the handler | Drops the faulting frame; faults on a corrupted frame record | Keeps the faulting frame; faults on a corrupted frame record | Keeps the faulting frame; faults on a corrupted frame record |
| libunwind seeded from the context | Exact on clean stacks; faults on a corrupted frame record | nongnu libunwind: never faulted, stops with an error; not on player machines | Faults on a corrupted frame record |

All `[measured]`. The corrupted frame record was a synthetic trigger (a saved frame pointer overwritten with `0x10`) and, on Linux, the real holomap stack smash built without stack protection.

- **A fault inside the handler rewrites the platform report.** When an unwinder faulted, the block was cut, and the macOS crash report, the WSL core and the Android tombstone then showed the unwinder (`libunwind ... step`, `_Unwind_Backtrace` in `libgcc_s`, `stepWithDwarf` at `#00` with the real crash at `#07`) instead of the crash `[measured]`.
- **Recovery works.** With `sigsetjmp` before the unwinder, SIGSEGV and SIGBUS unblocked while it runs, and a `siglongjmp` back when the handler is re-entered, the block completes and the platform report still shows the original crash. Measured on macOS (libunwind and `_Unwind_Backtrace`) and Linux (`_Unwind_Backtrace` on the synthetic trigger and the real holomap crash). `_Unwind_Backtrace` keeps the frames it reached before the fault; `backtrace()` returns none. On glibc before 2.35, libgcc looks up unwind tables under the loader lock, which a jump out of the lookup could leave held `[inferred]`.

### 4.2 Chaining

- **The `si_code` rule** (re-raise for SIGABRT or `si_code <= 0`, otherwise return and let the fault repeat) fails in two places `[measured]`:
  - macOS synthesizes `si_code`, `si_addr` and the exception state for sent signals. `raise(SIGSEGV)` arrives with code 2, like a real fault. An outside `kill -SEGV` that lands in user code is indistinguishable too: the `pc` is anywhere, the exception state is the thread's last syscall, and during a page-fault loop even `si_addr` holds a stale fault address. The process survived.
  - Linux delivers `int3` as SIGTRAP with `si_code` `0x80`. The rule returned, and the process survived with exit 0.
- **Re-raising with `raise()` rewrites the platform report:** the crashing stack becomes `raise` or `tgkill` under our handler, the code becomes `SI_TKILL`, and an outside kill loses its sender `[measured, WSL2 and Android]`.
- **Re-queuing keeps the report as if no handler were there.** Restore the previous action, `rt_tgsigqueueinfo(getpid(), gettid(), sig, info)` with the original siginfo, and return, with the signal blocked during the handler. The queued signal is delivered after the handler returns, before a faulting instruction runs again. `[measured, WSL2]` the core matched a run without the handler in all twelve cases (segv, non-canonical address, bus, ill, fpe, abort, `raise`, int3, stack overflow, a thread segv, outside `kill -SEGV` and `kill -ABRT`), every call returned 0, and ASan's report was unchanged. `[measured, Android]` the tombstone matched for abort, `raise` and an outside kill with the narrower rule of re-queuing only when `si_code <= 0`.
- **macOS has no `rt_tgsigqueueinfo`.** The rule that held there confirms a fault by re-entry `[measured]`:
  - Re-raise at once for SIGABRT, for SIGFPE (arm64 raises none from hardware; integer divide by zero does not trap), and when the instruction before `pc` is `svc`, which catches `raise`, `abort` and a kill to an idle engine.
  - Otherwise keep the handler installed, arm a 200 ms `ITIMER_REAL`, and return. A real fault re-enters at the same `pc`; the handler cancels the timer, restores the previous action and returns, and the crash report is untouched (segv, bus, `brk`, stack overflow, the real shadow crash, and the synthetic corrupted frame record with two recovered unwinder faults).
  - A sent signal does not come back: the timer restores the previous action and raises it. An outside kill died every time, idle, busy and during a page-fault loop, exit 139.

### 4.3 Reproducers

| Fix reverted | macOS | Linux | Android |
|---|---|---|---|
| `efe606f4`, `teleport 10360 3584 <z>; ui inventory <png>` | z 58715 clean; z 400000, -60000 and 2000000 fault in `GetShadow` | z 58715, 400000 and -60000 clean; z 2000000 faults 3 of 3 | z 58715 faults |
| `1ae73d05`, holomap plan zoom and close on a Zeelich save | Faults in `MainLoop` after `HoloMap()` returns | No fault with the default `-fstack-protector-strong` and no SIGABRT; with `-fno-stack-protector`, a `#GP` at a `ret` in `GetInput` | Faults in `MainLoop` |
| `dc38d6bc`, `SCREEN.HQR` palettes re-encoded as LZSS, `ui pcx-message 72 0` | Faults in `Dial`, `addr=0xffffff0000000000` | No fault: the 12 bytes land on two 4-byte statics and padding | not run |

All `[measured]`. On macOS and Linux each fault was also confirmed absent on `c42ea33f`. `teleport 10360 3584 2000000; ui inventory <png>` is the one trigger that faulted on every platform tried. The `53736e11` ASan report was not reproduced: no scene with all ten texture effects was found.

### 4.4 Tools found along the way

- **macOS keeps at most 25 saved crash reports per process name.** ReportCrash still formulates a report for every crash (`/usr/bin/log show`, "Formulating fatal 309 report for corpse[pid]"), but `osanalyticshelper` stops saving files. A copy of the binary under another name gets its own quota.
- **WSL writes an ELF core for every crash** through its piped `core_pattern`, even with `ulimit -c 0`, into `%TEMP%\wsl-crashes`, newest ten kept. gdb on it is the ground truth.
- **`llvm-symbolizer --obj=`** reads only the file it is given; `atos` also searches for a dSYM by UUID.

## 5. Design

### 5.1 Per platform

| | Linux | Android | macOS arm64 | Windows x64 |
|---|---|---|---|---|
| Catch | `sigaction` SEGV, BUS, ILL, FPE, ABRT, TRAP; `SA_SIGINFO`, `SA_ONSTACK` | The same, plus `SA_EXPOSE_TAGBITS`; reaches libsigchain | The same | `SetUnhandledExceptionFilter`, plus a CRT `signal(SIGABRT)` handler; as built, also a vectored handler for a stack Windows cannot walk (section 7.2) |
| Chain | Re-queue the original siginfo and return, for every signal | Re-queue, for every signal | Confirm by re-entry, section 4.2 | Return `EXCEPTION_CONTINUE_SEARCH` to the previous filter |
| Frames | Registers, then `_Unwind_Backtrace` under recovery | Frame-pointer walk from the context plus `lr`, reading through `process_vm_readv`; as built, without the recovery guard (section 7.1) | Frame-pointer walk from the context plus `lr`, bounds-checked, under recovery | `RtlLookupFunctionEntry` and `RtlVirtualUnwind` on a copy of the context, bounds-checked, under a vectored-handler recovery |
| Modules | `dl_iterate_phdr` at install, `_r_debug.r_map` scan in the handler for later loads; GNU build ID | `dl_iterate_phdr` at install; GNU build ID | dyld add-image callback, plus dyld itself; `LC_UUID` | ToolHelp snapshot at install, image headers for later loads named from the export directory; link timestamp and image size |
| Alternate stacks | Main thread, `DecodeThreadFunc` entry, each `SampleStreamCallback` call (`Crash_PrepareThread`) | Keep bionic's | The same as Linux | `SetThreadStackGuarantee` of 32 KB on the main thread and through `Crash_PrepareThread` |

- **Android re-queues for every signal.** `[measured, Android]` with the walk run without the guard, the tombstone and exit record matched a run without the handler in all eleven cases of section 7.1.
- **macOS x86-64 is unverified.** The confirm rule's instruction check would look for `syscall` (`0f 05`) instead of `svc`, and frame pointers are kept by default `[inferred]`. No x86-64 Mac is available.
- **armv7 Android is unverified**: the Apple Silicon emulator cannot run it. It builds and bundles, and writes frame 0 and `lr` only.

### 5.2 Install point

- **In `main`, right after `atexit(Log_Shutdown)`**, step 5 of the [boot order](../knowledge/subsystems/boot.md): the log exists and has been rotated, and nothing heavy has run. The prototypes installed there on all three POSIX platforms.
- **The descriptor** is opened on the log path with `O_APPEND` at install. The block lands after the last line the file sink flushed `[measured]`.
- **The breadcrumb table** is registered by SOURCES at step 9, next to `atexit(TheEndInfo)`. A crash before then writes the block without state.
- **Thread stacks** are installed by the threads themselves, since `sigaltstack` is per thread.
- **Crashes before step 5** stay stderr-only, as the rest of early boot does.

### 5.3 Block format

People read it and scripts parse it, so it follows the console's rule: stable leading words, values appended rather than spliced ([console output decision](../knowledge/decisions/console-output-is-a-parsed-contract.md)). Every line starts with `CRASH `.

```
CRASH ==== fatal signal ====
CRASH signal 11 SIGSEGV code=2 addr=0x10 thread=main tid=2321310
CRASH build 0.13.0-dev <commit> <platform> <arch>
CRASH reg pc=0x... lr=0x... fp=0x... sp=0x...
CRASH state island=4 cube=90 chapter=3 behaviour=0 cinema=0 fade=0 fps=0 hero=15360,4096,24576 life=50
CRASH module lba2cc base=0x100df8000 id=260022E5-FFEE-3187-9916-33223CBDA9DF
CRASH frame 00 lba2cc+0x56d40
CRASH frame 01 lba2cc+0x82b50
CRASH module libsystem_platform.dylib base=0x18a1a2000 id=EDB83A19-EC17-32DE-9350-6145970A85D6
CRASH nested SIGSEGV recovered in unwind
CRASH chain queue
CRASH ==== end ====
```

- **Order:** header, build, registers, state and the engine module first; then frames; then the other modules the frames used; then the chain line. A block cut by anything unexpected still names the signal, the build and the engine's module identity.
- **Frames** are `module+offset`, looked up at `pc` for frame 0 and at the return address minus one for the rest by the symbolizer.
- **Recursion** keeps the first and last frames, not the first 64: a stack overflow on the decoder thread lost `DecodeThreadFunc` at a 64-frame cap `[measured, WSL2]`.
- **Size:** about 1.3 KB for a ten-frame crash `[measured, macOS]`.
- **As built:** the `chain` line names the hand-back, `queue` on Linux and Android, `confirm` or `raise` on macOS, `filter`, `search` or `return` on Windows. Windows writes `CRASH exception 0xc0000005 EXCEPTION_ACCESS_VIOLATION addr=... access=write` for an exception and `CRASH signal 22 SIGABRT` for abort. A signal with a sender appends `sender=`. A cut recursion writes `CRASH frames skipped N` between the first 32 frames and the last 16. `reg` has no `lr` on x86. The block ends before the crash is handed back.

### 5.4 Breadcrumbs

A typed table in LIB386, `{name, address, size, signedness}`, filled by SOURCES once at step 9 and read in the handler without calls. Fields: `dumpstate`'s scalars from section 3.4. No pointer is followed, so a corrupted count cannot fault the handler. As built: `Crash_AddState`, with fields added one after another under one name printed as `hero=x,y,z`, registered by `register_crash_state` in `SOURCES/PERSO.CPP`, which already includes the engine's aggregate header: a file of its own added a translation unit the build-graph ceiling on `DEFINES.H` counts. `[measured, macOS]` the fields matched `--dump-state` from the same run, and `chapter` matched `vargame 253`.

### 5.5 Trigger for tests

No crash verb ships in the engine: a player-reachable way to crash the game is a support cost for no player benefit. The forced-crash tests link the LIB386 crash core into a small host test program with its own triggers and launch it as a child process (section 3.8). The prototypes' `crash <kind>` verb stays in the prototypes.

## 6. Implementation, one PR

Each step names the commit that carries it.

1. **Keep the previous log:** rotation in `CreateLog`, a host test, and the snapshot exclusion. Landed in 361f1bed.
2. **Build identity:** commit SHA in the generated version header and the banner; the banner names Android (it uses `LOG_PLATFORM_NAME` in [INITADEL.C](../../SOURCES/INITADEL.C), which said Linux on Android). Landed in c045edbc, which also makes the version step run on every build: as a custom command it depended only on `VERSION`, so an incremental build kept the header from the last configure `[measured, macOS]`.
3. **Crash core** in `LIB386/SYSTEM`: install, the pre-opened descriptor, the module cache, signal-safe formatting, the block writer, the breadcrumb table. Landed in cb2e6776, with `test_crash_core`.
4. **Linux backend:** re-queue chaining, `_Unwind_Backtrace` under recovery, alternate stack on the main thread. Landed in a1d0b147, and e1367191 makes the smashed-record trigger reach libgcc's unwinder.
5. **macOS backend:** confirm chaining, frame-pointer walk plus `lr`. Landed in 2b894d1a, with the POSIX half shared by Linux and Android and `test_crash_report`.
6. **Android backend:** re-queue chaining, frame-pointer walk through `process_vm_readv`, `SA_EXPOSE_TAGBITS`. Test on the emulator: tombstone and exit info unchanged for a real fault, abort and an outside kill, with the always-re-queue rule. Landed in fce3cdce; see section 7.1.
7. **Windows backend:** chained filter, SIGABRT handler, `RtlVirtualUnwind` walk. Required tests (D12): the process exits with the native exception code, not the CRT's exit 3 seen in the one in-engine run (section 3.2); a fault inside the walk on a smashed stack must not replace the crash in Windows Error Reporting or the exit code; abort and `assert` write the block before `C0000409`; stack overflow on the main thread; exit codes and WER unchanged from a normal launch, not only from MSYS2; issue #526 at 320 wide, if it still reproduces. Landed in b6286cf2; the results of each required test are in section 7.2.
8. **Thread stacks** for `OGGDecode` and the audio callback on Linux, macOS and Windows (D8). Landed in c48e7b6b. `[measured, macOS engine]` under lldb, `Crash_PrepareThread` ran on the main thread, SDL's audio thread and each `OGGDecode` thread; a thread's stack is unmapped when it exits.
9. **Breadcrumbs:** the table and its registration. The table landed with the core, the registration in 1a6f73ec.
10. **Android recovery** (D9). Test on the emulator: `getTraceInputStream` non-null for a native crash, the `.pb` written next to `adeline.log`, no duplicate on the next launch, a null stream handled. Landed in 11563a6e. `[measured, Android]` the saved file was byte-identical to the system's tombstone, a relaunch saved nothing again, a new crash replaced the file, and the banner said so on a `Note:` line. A null stream was not reproduced. The last saved timestamp is kept in the app's preferences rather than the user folder.
11. **Packaging:** AppImage unstripped; APK debug info stripped with the symbol table kept, sizes recorded; SONAME set to `libmain.so` or the mismatch documented for the symbolizer. Landed in 3794dff7: `NO_STRIP=binaries` for the AppImage, not built locally; `[measured]` the arm64 APK from 24 MB to 6 MB and the armv7 APK to 4.4 MB, symbol tables kept, and a tombstone now naming `libmain.so`.
12. **Tests:** a host test that launches a child which crashes each way and checks the block, the exit status and that the process died, on Linux, macOS and Windows in CI, including the sanitizer lane. Cases: each signal, a sent signal, a corrupted frame record, and a thread stack overflow. Landed with the backends: `test_crash_report` runs each case without the handler and with it and requires both to end alike, plus `assert`, a released thread stack and a recursion tail that reaches below the recursion. `[measured]` it passed on macOS arm64 (Debug and `-O3` LTO), Linux arm64 and x86-64 (Debug, `-O3`, ASan and UBSan), and Windows x64.
13. **Knowledge:** update the logging and boot concepts; add a Decision that the handler chains so the process still dies of its signal with the platform report unchanged; fix the stale comments in `LOG.H` and `INITADEL.C`. Landed in caba5989, with a crash section in the player readmes and the saved tombstone in `ANDROID.md`. The `INITADEL.C` platform name was fixed in step 2.

**Later PR:** `-g`, split symbol files as release assets for tags and 90-day workflow artifacts for rolling builds, and a `scripts/dev` symbolizer keyed by build ID. Built as `LBA2_RELEASE_SYMBOLS`, `scripts/packaging/split-symbols.sh` and `scripts/dev/symbolize_crash.py`; see section 7.5.

## 7. As built: where the implementation diverged

### 7.1 Android

- **The walk runs without the recovery guard.** `[measured, Android]` with the guard, the SIGSEGV tombstone showed the handler above the fault (`syscall ← CrashOs_HandOn ← crash_handler ← SignalChain::Handler`), for a real fault and for `raise`, while SIGABRT and SIGTRAP stayed clean. libsigchain's `sigprocmask` leaves out the signals ART claims, so the guard's restore of the handler's mask left SIGSEGV unblocked and the re-queued signal arrived inside the handler `[inferred from the pattern, fixed by it]`. The walk reads through `process_vm_readv` and cannot fault, so it needs no guard.
- **Always re-queue holds.** `[measured, Android]` with the guard removed, and again after the review fixes, for a null write, SIGBUS, `brk`, abort, `raise`, a smashed frame record, overflows on the main and a secondary thread, a fault on another thread, and SIGSEGV and SIGABRT sent from outside, the tombstone's signal, code, sender and backtrace and the exit record's reason and status matched a run without the handler.

### 7.2 Windows

- **A smashed stack never reaches the filter.** `[measured, Windows 11, MSYS2 UCRT64 gcc 15.2]` with a saved frame pointer overwritten, the process ended with the native code and no block: Windows' own search for exception handlers found a frame outside the stack and ended the process before any filter. A vectored handler, which runs before that search, now repeats its frame check for exceptions that end a process and writes the block itself when the check fails (`chain search`). A healthy stack still reaches the filter. The check stops at the first frame with a handler of its own, which Windows calls before it looks further, since that exception may still be caught `[read]`.
- **The required tests.** `[measured]` The native exit code: access violation `0xC0000005`, `ud2` `0xC000001D`, divide by zero `0xC0000094`, stack overflow `0xC00000FD` on the main and a prepared thread, a fault on another thread, all the same as without the handler, and in the engine the shadow reproducer below exited `0xC0000005`. A fault inside the walk on a smashed stack is recovered, the block completes, and the exit code is unchanged. abort and `assert` write the block and still end in `0xC0000409`. Launched with the default error mode, the Application Error events matched a run without the handler for the access violation, stack overflow and abort; for the smashed stack both name `ntdll.dll` and `0xC0000005`, and the offset inside ntdll varied between runs once any handler was installed, including with the vectored check disabled. #526 did not reproduce in 12 boots at 320x200, 320x240, 400x300 and 480x360.
- **In the engine.** `[measured]` with `efe606f4` reverted, `teleport 10360 3584 400000; ui inventory <png>` faulted (z 2000000 did not on Windows); the block's frames resolved with the symbol table to `GetShadow ← AffScene ← MenuInventory ← cmd_ui ← Console_Execute ← control_run_commands ← Control_TickHook ← MainLoop ← MainGameMenu ← SDL_main`, and its frame 0 offset and module identity matched the Windows Error Reporting event.

### 7.3 Every platform

- **The block ends before the crash is handed back.** `[measured, qemu-user x86-64]` the re-queued signal was delivered at once and cut the block after its module lines. Writing the chain line and the end first costs nothing where the kernel waits.
- **One block per process.** `[measured, macOS ASan]` after ASan's report its own `abort` reached the still-installed SIGABRT handler and wrote a second block. Handing a crash back now restores every previous action.
- **Threads that crash together.** `[measured]` eight threads faulting at once, 40 runs each: on macOS every run began a second block that the process's death cut short, and on Windows 39 runs ended before the first block was finished. The first thread now claims the block atomically, and a thread that crashes meanwhile waits for it, up to five seconds, before it hands its own crash on. On Windows the shared walk state is taken by one thread at a time. After the change, 40 runs each on macOS, Linux and Windows wrote one whole block, and 11 runs on the Android emulator did, with the tombstone and exit record of a run without the handler.
- **A call through a null pointer.** `[measured]` the block stopped at frame 0 on Linux and wrote no frame on Windows; macOS found the caller through `lr`. Windows now reads frame 0's return address as for a leaf; Linux walks again on the context rewritten as if the call had returned and puts it back. Under gdb the handed-on signal arrived with the original pc and sp. On the Android emulator the block's frames 01 and 02 were the tombstone's, through `lr`.
- **The walk bound is 2^20 frames, not 4096.** `[measured, Windows]` a 1 MB stack overflow filled 4096 frames of the recursion and the tail never reached the thread's start. `[measured, macOS Release]` under a 64 MB stack limit the test child's recursion ran past 2^20 frames and the tail stayed in it, so the test runs the child under 8 MB. Every walk also stops when the stack stops climbing.
- **macOS module table.** `[measured]` a windowed engine has about 1050 images loaded, so the table holds 2048.
- **x86-64 Linux smashed record.** The first trigger's caller addressed its frame through the stack pointer, so libgcc never read the smashed record; an `alloca` in the caller makes it. `[measured, WSL2]` the unwinder then faults, the block records `nested SIGSEGV recovered in unwind`, and the core still shows the fault in the trigger.

### 7.4 Measured limits, kept

- **macOS, an overflow on a thread without an alternate stack:** with any handler installed the process dies of SIGILL instead of SIGBUS, because the kernel cannot build the signal frame; the crash report keeps `EXC_BAD_ACCESS` and the frames. The engine's own and audio threads are prepared; SDL's and the system's are not.
- **macOS, a signal raised at once** (abort, `raise`, or delivered after `svc`): the crash report keeps its exception and frames, but its termination record is empty. `kill` in place of `raise` did the same, and raising with the signal unblocked put the handler on the crashing stack.
- **macOS, a sent signal in running code:** dies from the confirm timer 200 ms later, and its report shows the timer's `raise`, as section 9 expected.
- **macOS, the real reproducer:** `[measured]` the Release engine's block and the crash report named the same frames at the same offsets, `GetShadow` through `main` and `dyld` `start`.

### 7.5 Symbols

- **One archive per artifact.** Each bundler's `--split-symbols` splits its staged copy, so the build tree keeps its debug info: `<exe>-<version>-<platform>-<arch>-symbols.tar.xz` holding `.debug` files for ELF and PE and a dSYM for Mach-O, the AppImage's labelled `appimage`. On Android it takes every library the APK ships, which the NDK already compiles with `-g`.
- **`-g` still changes no code.** `[measured, macOS 26.6 Apple clang, macos_arm64 Release, thin LTO with -object_path_lto]` `__text`, `__data` and `__unwind_info` identical with and without it. `[measured, Arch Linux arm64 container, GCC 16.1, Release -O3 LTO]` `.text`, `.data`, `.data.rel.ro`, `.eh_frame` and the program headers identical; `.rodata` differs in 9 bytes, the `__TIME__` stamps of the two builds.
- **A crash resolves from the archive alone.** `[measured]` with `efe606f4` reverted, the split and stripped build crashed, macOS on `teleport 10360 3584 2000000; ui inventory` and Linux arm64 on z -60000 (2000000 and 400000 did not fault there). The UUID or build ID in the block equalled the symbol file's, and frame 0 resolved to `GetShadow` at `SOURCES/GRILLE.CPP:373`, the unbounded `*ptc` read, with inlined frames below it (`cmd_ui_dispatch` inside `cmd_ui`). macOS offsets are added to `__TEXT`'s vmaddr, Linux offsets are link addresses as they stand.
- **Windows: the split changes `SizeOfImage`.** `[measured, x86_64-w64-mingw32 GCC 16.2, -O3 -flto -g, a test program]` a PE image maps its DWARF sections, so `--strip-debug` shrank `SizeOfImage` from 0x28000 to 0xd000, the end of the loaded sections plus one page for the `.gnu_debuglink` it adds, while the debug file kept 0x28000. The block's identity is the running image's, so the symbolizer derives the stripped size from the debug file's section table, which keeps every section's address and virtual size. `llvm-symbolizer` wants ImageBase plus the block's offset; `--relative-address` named the wrong function.
- **`addr2line` is not reliable here.** `[measured, GNU binutils 2.46, the Linux build above]` whether an address got its line depended on the addresses looked up before it in the same call: `main` looked up alone or first gave `??:?`, and after eleven other addresses `PERSO.CPP:3059`. `llvm-symbolizer` gave the same lines in any order, so the script prefers it, then `atos`, which prints file basenames only.
- **MinGW needs `-g` on the link.** `[measured, CI windows-latest, MSYS2 UCRT64 GCC]` with `-g` on the compile only, the release exe had no DWARF and the split stopped the build. `[measured, x86_64-w64-mingw32 GCC 16.2]` an object compiled with `-g -flto` and linked without `-g` resolved `main` to `fake:?`; linked with it, to its source line. ELF targets carried the debug info in from the objects, so `LBA2_RELEASE_SYMBOLS` passes `-g` to the link as well.
- **Every release leg, from its own artifacts.** `[measured, workflow_dispatch on the branch]` the identity of the binary inside each shipped artifact equalled its symbol file's: both Linux tarballs, the macOS app inside the DMG, the aarch64 AppImage's engine after `quick-sharun` packed it, and the Windows exe, whose stripped `SizeOfImage` (0x8ab000) was one of the two the script derives from the debug file. The aarch64 tarball binary, killed with SIGSEGV in a container, wrote a block that `symbolize_crash.py --fetch` resolved from nothing but its `build` line, through the run's artifacts, to `SpeakAnimation`, `Dial`, `DoLife` and `MainLoop` with lines. On Windows, offsets taken from the exe's symbol table resolved to their functions with lines. Archives: 0.9 MB macOS, 1.4 MB Windows, 1.5 to 1.6 MB Linux.
- **objcopy stamps a PE image with the time it runs.** `[measured, kuro, MSYS2 UCRT64 binutils 2.46, the engine exe]` `--only-keep-debug` and `--strip-debug` each wrote the current time into `TimeDateStamp`: link 1789420677, debug file 1789420717, stripped exe 1789420719. Run within one second, as on CI and in the test program above, the two agree by luck, and neither is the link's. With `SOURCE_DATE_EPOCH` set to the link timestamp both kept it, so `split-symbols.sh` pins it and requires the stripped image to still carry it. The first kuro run caught it: the two stamps were one second apart and the identity check stopped the bundle.
- **A real Windows engine crash resolves.** `[measured, kuro, Windows 11, MSYS2 UCRT64 GCC 15.2, CI's configure flags]` with `efe606f4` reverted, the exe from `bundle-windows.sh --split-symbols` faulted on `teleport 10360 3584 400000; ui inventory`, exit 139 from bash, `chain filter`. The block's identity `6aa864850078d000` matched the archive's debug file through the derived `SizeOfImage`, and the archive alone resolved frame 0 to `GetShadow` at `SOURCES/GRILLE.CPP:373`, then `AffScene`, `MenuInventory`, `cmd_ui`, `Console_Execute`, `Control_TickHook`, `MainLoop`, `SDL_main` and MinGW's `__tmainCRTStartup`, the same functions section 7.2 names from the Windows Error Reporting event.
- **Android, bundled and crashed on the emulator.** `[measured, Mac, NDK r28.2, lba2cc_pixel_api36 arm64 emulator, API 36]` `bundle-android.sh --split-symbols` split all three libraries the APK ships (`libmain.so`, `libSDL3.so`, `libc++_shared.so`) into a 4.0 MB archive, the APK came to 5.8 MB, the 16 KB alignment check passed, and each library inside the APK carried its debug file's build ID. A SIGSEGV sent to the process with `kill` was delivered to the Java UI thread idle in its looper, so its block held system frames only. Sent with `tgkill` to `SDLThread`, the block's engine frames resolved from the archive alone, `CommitPresentTarget` with `PresentFrame` and `UnlockVideoSurface` inlined, `BoxBlit`, `BoxUpdate`, `DrawGameMenu` inside `DoGameMenu`, `MainGameMenu`, `SDL_main`, and SDL's `SDL_RenderPresent` and `nativeRunMain`, each the function the system tombstone named at the same frame, four bytes apart where the block gives the return address. The emulator's APK, user folder and preferences were restored afterwards.
- **Not measured:** the Android release legs on CI, which cannot be dispatched from a branch, and armeabi-v7a, which no Apple Silicon emulator runs.

## 8. Constraints

- Chain to the saved previous handler; never `SIG_DFL`, never `_exit`, never `raise()` where re-queuing is available.
- The process dies of the original signal or exception on every platform, including a signal sent from outside.
- No stdio, allocation or lock in the POSIX handler; a hang is worse than no report.
- Nothing in the handler may fault: a nested fault replaces the crash in the platform's own report. Every read is bounds-checked, and any unwinder that reads through stack contents runs under recovery.
- Never replace an alternate stack a thread already has.
- The macOS handler owns SIGALRM for the confirm timer; nothing else in the engine may use it.
- The recorder is not flushed or finalized from the handler.
- LIB386 does not include SOURCES; platform conditionals stay in the platform layer (`make arch-check`).
- CI cannot run the game; tests are host tests.
- The Android emulator and the WSL2 and Windows machines are shared with other work: one test suite at a time.

## 9. Open risks

- **Threads without an alternate stack:** a stack overflow on any thread neither the engine nor SDL gave one stays unreported.
- **Late-loaded modules:** GPU and audio drivers loaded after install are missing from the Android module cache, which is never refreshed.
- **Confirm timer on macOS:** a sent signal leaves the process running for up to 200 ms before it dies, and an exit inside that window ends it with the exit's own status.
- **Unverified targets:** macOS x86-64 and armv7 Android, which build but were never run.
- **The AppImage's symbol table:** `NO_STRIP=binaries` was not built locally.
- **Threads without an alternate stack on macOS:** see section 7.4.
- **Recovery on old glibc:** a jump out of libgcc's unwinder may leave the loader lock held on glibc before 2.35.
- **Windows in the engine:** the filter has little stack after an overflow; `SetThreadStackGuarantee` is per thread. A thread nobody prepared keeps the default guarantee.
- **Windows, a smashed frame below a handler frame:** the vectored check stops at a frame with its own handler, such as a C++ frame with cleanups, so a stack rejected below one is left to Windows' search and writes no block. Not measured.
- **Windows reports from harness launches:** runs launched from MSYS2 or WSL get no Windows Error Reporting record, with or without the handler.
- **Android `.pb` size:** about 265 KB is awkward for players to send, and the player-facing docs will need to say how.
- **Lost last-seen timestamp:** kept in the app's preferences, so clearing the app's data brings the newest crash back once.

## 10. Sources

External material the research relied on, for whoever builds the implementation:

- POSIX: [sigaction(2)](https://man7.org/linux/man-pages/man2/sigaction.2.html), [sigaltstack(2)](https://man7.org/linux/man-pages/man2/sigaltstack.2.html), [signal-safety(7)](https://man7.org/linux/man-pages/man7/signal-safety.7.html), [backtrace(3)](https://man7.org/linux/man-pages/man3/backtrace.3.html), [rt_tgsigqueueinfo(2)](https://man7.org/linux/man-pages/man2/rt_tgsigqueueinfo.2.html).
- Reference handlers: [Breakpad's Linux exception handler](https://github.com/google/breakpad/blob/main/src/client/linux/handler/exception_handler.cc), [Crashpad's signal re-raise](https://github.com/chromium/crashpad/blob/main/util/posix/signals.cc), [Chromium's in-process stack trace](https://github.com/chromium/chromium/blob/main/base/debug/stack_trace_posix.cc), [Sentry's in-process backend](https://github.com/getsentry/sentry-native/blob/master/src/backends/sentry_backend_inproc.c), [Sentry's macOS frame walker](https://github.com/getsentry/sentry-native/blob/master/src/unwinder/sentry_unwinder_libunwind_mac.c).
- Apple: [Writing arm64 code for Apple platforms](https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms), the frame-record rule.
- Android: [ART's sigchain](https://android.googlesource.com/platform/art/+/refs/heads/main/sigchainlib/sigchain.cc), [debuggerd's handler](https://android.googlesource.com/platform/system/core/+/main/debuggerd/handler/debuggerd_handler.cpp), [ApplicationExitInfo](https://developer.android.com/reference/android/app/ApplicationExitInfo), [tombstone.proto](https://android.googlesource.com/platform/system/core/+/refs/heads/main/debuggerd/proto/tombstone.proto), [ndk-stack](https://developer.android.com/ndk/guides/ndk-stack).
- Windows: [SetUnhandledExceptionFilter](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setunhandledexceptionfilter), [SetThreadStackGuarantee](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadstackguarantee), [MinGW-w64's CRT start-up and its filter](https://github.com/mingw-w64/mingw-w64/blob/master/mingw-w64-crt/crt/crtexe.c).
- Sanitizers: [the sanitizer signal flags](https://github.com/llvm/llvm-project/blob/main/compiler-rt/lib/sanitizer_common/sanitizer_flags.inc).
- Symbols: [GitHub release limits](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases), [Actions artifact retention](https://docs.github.com/en/organizations/managing-organization-settings/configuring-the-retention-period-for-github-actions-artifacts-and-logs-in-your-organization), [the AppImage packaging tool's strip step](https://github.com/pkgforge-dev/Anylinux-AppImages/blob/main/useful-tools/quick-sharun.sh).
