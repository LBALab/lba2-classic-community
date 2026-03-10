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
