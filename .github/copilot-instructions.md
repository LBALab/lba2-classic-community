# Copilot Instructions — LBA2 Classic Community

See [AGENTS.md](../AGENTS.md) for project context, principles, and golden rules. This file expands on ASM↔CPP workflow, polyrec debugging, and test patterns.

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

### Debugging mismatches must be test-driven

When investigating why the CPP implementation does not match the ASM result,
do **not** make random guesses and then hope the output improves.  Every
hypothesis must be checked with a **unit test** that runs the same specific
input through both the ASM and CPP implementations and proves the current
behavior is wrong before the fix.

Prefer a **bottom-up** testing approach whenever possible:
- Start by identifying the smallest helper, sub-step, or intermediate
  calculation that may be wrong.
- Write a focused ASM-vs-CPP unit test for that smaller function first,
  using concrete inputs and exact expected equivalence.
- Only move outward to larger functions once the smaller units are proven
  equivalent.

Tests must drive the investigation and the fix:
- A test should capture the exact broken case, not a vague approximation.
- The fix should make that test pass.
- The test must remain in place to prevent regressions.

**Polyrec replay is strongly recommended** when available.  Use it to
capture the exact inputs a function receives during a failing render or game
scenario, then turn that captured input into a dedicated ASM-vs-CPP unit test.
This is often the fastest way to isolate a mismatch without guessing.

### Workflow for finding and fixing mismatches

Follow these steps in order — do NOT skip ahead or theorize without data:

1. **Use polyrec `--bisect`** to find the first divergent draw call.
2. **Add `fprintf` debug traces** to BOTH the ASM (via `pushad/printf/popad`)
   and the CPP implementation at every key intermediate step.  Print the
   same values in the same format so output can be `diff`-ed line by line.
   Do not be shy — add traces at every `fistp`, every cross-product, every
   slope assignment.  More traces = faster convergence.
3. **Run the polyrec replay** with those traces and compare ASM vs CPP
   output to find the **exact line** where values first diverge.
4. **Extract the concrete input values** from the trace (vertex data,
   slope parameters, etc.) and write a **focused unit test** in the
   appropriate `tests/` file that calls both the ASM and CPP functions
   with those exact inputs and asserts equivalence with
   `ASSERT_ASM_CPP_EQ_INT`.
5. **Verify the test fails** — proving the bug is reproduced.
6. **Fix the CPP** to match the ASM, then confirm the test passes.
7. Re-run `--bisect` to confirm the fix and find the next divergence.

### No x86 inline assembly in library code

The CPP ports in `LIB386/` must remain **portable C/C++**.  Do not use GCC
inline `__asm__` or MSVC `__asm` blocks in `.CPP` files to "cheat" the
equivalence tests.  The purpose of the CPP port is to have readable,
maintainable C that produces the same results as the ASM — not C that
embeds the same x86 instructions.

Common pitfalls and their correct fixes:
- **x87 FPU precision**: The ASM uses 80-bit extended precision (`fild`,
  `fdivr`, `fmul`, `fistp`).  Use `long double` to match, or use `volatile long double`
  intermediates to prevent reordering.  If that is not enough, split the
  computation into explicit steps with `volatile` stores. DO NOT use compiler flags
  (`-mfpmath=387 -ffloat-store`) since we want code that is portable across different platforms and compilers.
- **Signed 32×32→64 multiply with bit extraction**: The ASM pattern
  `imul reg / shl edx,16 / shr eax,16 / or edx,eax` extracts bits
  [16..47].  In C: `(S32)(((U32)hi << 16) | ((U32)lo >> 16))` where
  `lo/hi` come from splitting the 64-bit product.
- **Sub-pixel correction must use local variables**: The sub-pixel
  adjustment to `u`, `v`, `w` must be done on **local copies** (`lineU`,
  `lineV`), not on the outer per-scanline accumulators.

### Chunking strategy for large functions

When working on a large functions, a strategy is to break it into smaller,
manageable chunks that can be tested independently.

That applies to both ASM and CPP implementations.  For example, a large function
like `Filler_TextureZFogSmooth` can be refactored to call smaller helper functions
for each major step (setup, perspective correction, scanline loop, etc).  Each
helper can be tested independently for ASM-vs-CPP equivalence, which makes it
easier to identify and fix discrepancies.

However, once the implementation of CPP matches the behavior of the ASM
implementation, the original ASM implementation must be restored and kept intact,
while the CPP implementation can be kept broken down in smaller chunks, that
is indifferent. In that case, after the right behavior is achieved, the tests
comparing the smaller chunks can be removed and just leave the main function
compared in its ASM vs CPP implementations.


### When adding or modifying ASM equivalence tests

1. **Update `docs/ASM_VALIDATION_PROGRESS.md`**:
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

When modifying `docker/Dockerfile.test`, **always save the build log to a file** so
failures can be diagnosed without rebuilding:

```bash
docker build --platform linux/amd64 -t lba2-test -f docker/Dockerfile.test . \
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