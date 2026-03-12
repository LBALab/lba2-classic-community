# Copilot Instructions — LBA2 Classic Community

## ASM → C Equivalence Testing

This project maintains both the original x86 ASM implementations and their C/C++
ports in `LIB386/`.  A test suite in `tests/` validates equivalence between the
two.  **All tests are ASM-vs-CPP equivalence tests** — there is no CPP-only mode.
Tests must be run inside Docker via `run_tests_docker.sh`.

### The Golden Rule: STRICT BYTE-FOR-BYTE EQUIVALENCE

**The ASM implementation is the source of truth.**  Every equivalence test
MUST compare the ASM and CPP outputs byte-for-byte using
`ASSERT_ASM_CPP_MEM_EQ` or `ASSERT_ASM_CPP_EQ_INT`.  There are NO
exceptions:

- **NO** "no-crash" tests that just call both paths and check `ASSERT_TRUE(1)`.
- **NO** approximate comparisons ("allow up to N% difference").
- **NO** "verify both filled some pixels" checks.
- **EVERY** random stress round MUST compare the full output buffer.

If the CPP output differs from the ASM output by even a single byte,
**fix the CPP implementation** until it matches.  Do not weaken the test.

### When adding or modifying ASM equivalence tests

1. **Update `ASM_VALIDATION_PROGRESS.md`** in the project root:
   - Mark the function's status as `[x]` (tested) or `[~]` (partial).
   - Add a note if there is a known discrepancy between ASM and CPP outputs.
2. **Follow the established test pattern** (see `tests/test_harness.h`):
   - Each test file includes `test_harness.h` and the relevant LIB386 header.
   - Declare ASM-side functions as `extern "C" S32 asm_FuncName(...)`.
   - Cover at least 3 normal inputs and 2 edge cases per function.
   - **Add a randomized stress test** with a deterministic LCG (seed
     `0xDEADBEEF` or similar fixed value) that runs 20-50 rounds comparing
     ASM vs CPP outputs with random inputs.  Use this pattern:
     ```c
     static U32 rng_state;
     static void rng_seed(U32 s) { rng_state = s; }
     static U32 rng_next(void) {
         rng_state = rng_state * 1103515245u + 12345u;
         return (rng_state >> 16) & 0x7FFF;
     }
     ```
   - For functions that write globals (`X0`, `Y0`, `Z0`, `Xp`, `Yp`), read
     those globals after each call and compare ASM vs CPP.
3. **Register the test** in the appropriate `tests/<dir>/CMakeLists.txt` using
   `add_asm_cpp_test()`.

### Code conventions

- The game code uses C++98; LIB386 headers use C linkage (`extern "C"`).
- Tests may use C11/C++11 features since they are not part of the game binary.
- Types: `S32` = `int32_t`, `U32` = `uint32_t`, etc. (see
  `LIB386/H/SYSTEM/ADELINE_TYPES.H`).
- `TYPE_MAT` is a packed union with `.F` (float), `.I` (int32), `.M` (int16)
  views (see `LIB386/H/3D/DATAMAT.H`).

### Running tests (Docker only)

Tests require a 32-bit x86 toolchain with UASM and objcopy.  Use
`run_tests_docker.sh` to run the full test suite inside a Linux x86_64 Docker
container.  This works on macOS ARM64 via QEMU emulation.

```bash
./run_tests_docker.sh              # Build & run all tests
./run_tests_docker.sh --build-only # Build the Docker image without running
./run_tests_docker.sh --rebuild    # Force rebuild the Docker image
./run_tests_docker.sh test_getang2d test_lirot3df   # Run only named tests
```

Logs are saved to `build_logs/` automatically (gitignored).

### Docker image maintenance

When modifying `Dockerfile.test`, **always save the build log to a file** so
failures can be diagnosed without rebuilding:

```bash
docker build --platform linux/amd64 -t lba2-test -f Dockerfile.test . \
    2>&1 | tee build_logs/docker_build_$(date +%Y%m%d_%H%M%S).log
```

Key constraints for the Docker image:
- **UASM must be installed at container runtime**, not during `docker build`.
  On macOS ARM64, buildkit uses Rosetta which cannot run UASM (x86_64 ELF).
  QEMU at `docker run` time handles it correctly.
- **32-bit SDL3** is built with minimal backends (X11 only, no audio/GL) to
  satisfy the linker for ASM tests compiled with `-m32`.
- `.model SMALL` ASM files are automatically patched to `.model FLAT` at
  build time (see `tests/cmake/patch_asm_flat.cmake`).  This produces
  identical machine code with standard ELF relocations, enabling
  ASM-vs-CPP equivalence testing for all functions.
- Some ASM procs use **Watcom register calling convention**
  (`#pragma aux ... parm [edi] [esi] [ebx]`) instead of C stack parameters.
  These require inline ASM ABI wrappers or will SEGFAULT when called from C.
  Leaf functions use GCC inline asm wrappers; non-leaf functions that call
  other Watcom-convention functions through function pointers will SEGFAULT
  until full ABI shims are added.

## Communication Rules

- **ALWAYS use the `ask_questions` tool** to communicate with the user. The user speaks English.
- **NEVER write plain text responses** for questions, confirmations, or status updates. Route everything through `ask_questions`.
- After completing a task, ask what to do next via `ask_questions`.
- If the user's intent is ambiguous, clarify via `ask_questions` before proceeding.
- Never try to write outside of the repo, if you need to create extra files no matter if they are tools/tmp/ or documentation, do it within the repo and commit them if necessary. If not, feel free to add that folder to the .gitignore.