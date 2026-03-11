# Copilot Instructions — LBA2 Classic Community

## ASM → C Equivalence Testing

This project maintains both the original x86 ASM implementations and their C/C++
ports in `LIB386/`.  A test suite in `tests/` validates equivalence between the
two.

### When adding or modifying ASM equivalence tests

1. **Update `ASM_VALIDATION_PROGRESS.md`** in the project root:
   - Mark the function's status as `[x]` (tested) or `[~]` (partial).
   - Add a note if there is a known discrepancy between ASM and CPP outputs.
2. **Follow the established test pattern** (see `tests/test_harness.h`):
   - Each test file includes `test_harness.h` and the relevant LIB386 header.
   - Use `#ifdef LBA2_ASM_TESTS` for ASM-vs-CPP comparison sections.
   - Declare ASM-side functions as `extern S32 asm_FuncName(...)`.
   - Cover at least 3 normal inputs and 2 edge cases per function.
   - For functions that write globals (`X0`, `Y0`, `Z0`, `Xp`, `Yp`), read
     those globals after each call and compare ASM vs CPP.
3. **Register the test** in the appropriate `tests/<dir>/CMakeLists.txt` using
   `add_asm_cpp_test()` or `add_cpp_test()`.

### Code conventions

- The game code uses C++98; LIB386 headers use C linkage (`extern "C"`).
- Tests may use C11/C++11 features since they are not part of the game binary.
- Types: `S32` = `int32_t`, `U32` = `uint32_t`, etc. (see
  `LIB386/H/SYSTEM/ADELINE_TYPES.H`).
- `TYPE_MAT` is a packed union with `.F` (float), `.I` (int32), `.M` (int16)
  views (see `LIB386/H/3D/DATAMAT.H`).

### Building tests

```bash
# CPP-only correctness tests (works everywhere)
cmake -S . -B build -DLBA2_BUILD_TESTS=ON --preset linux
cmake --build build
ctest --test-dir build --output-on-failure

# ASM equivalence tests (requires 32-bit toolchain + uasm + objcopy)
cmake -S . -B build --preset linux_test_asm
cmake --build build
ctest --test-dir build --output-on-failure
```

### Running tests in Docker (macOS ARM64 / any platform)

Use `run_tests_docker.sh` to run the full test suite inside a Linux x86_64
Docker container.  This works on macOS ARM64 via QEMU emulation.

```bash
./run_tests_docker.sh          # CPP-only tests (64-bit)
./run_tests_docker.sh --asm    # CPP + ASM equivalence tests (32-bit)
./run_tests_docker.sh --build-only  # Build the Docker image without running
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
  These cannot be called directly from C test code and SEGFAULT in `--asm`
  mode.  They remain registered as `add_asm_cpp_test()` but only run the
  CPP-only portion until ABI wrappers are added.
