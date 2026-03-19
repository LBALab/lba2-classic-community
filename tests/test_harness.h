/*
 * Minimal test harness for LBA2 ASM ↔ C equivalence testing.
 * No external dependencies — plain C89 compatible.
 *
 * Output follows TAP (Test Anything Protocol) version 14.
 * See https://testanything.org/tap-version-14-specification.html
 *
 * Usage:
 *   #include "test_harness.h"
 *
 *   void test_something(void) {
 *       ASSERT_EQ_INT(expected, actual);
 *   }
 *
 *   int main(void) {
 *       RUN_TEST(test_something);
 *       TEST_SUMMARY();
 *       return test_failures != 0;
 *   }
 */
#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <string.h>
#include <math.h>

/* ── counters ─────────────────────────────────────────────────────────── */
static int test_count = 0;
static int test_failures = 0;
static int test_passed = 0;
static int assert_count = 0;

static const char *current_test_name = "";

/* ── path stripping ───────────────────────────────────────────────────
 * TEST_SOURCE_ROOT is set by CMake to the project root (e.g. "/tmp/lba2/").
 * We strip it from __FILE__ so output shows "tests/3D/foo.cpp" instead.   */
#ifndef TEST_SOURCE_ROOT
#define TEST_SOURCE_ROOT ""
#endif

static inline const char *_strip_root(const char *path) {
    const char *root = TEST_SOURCE_ROOT;
    size_t len = strlen(root);
    if (len > 0 && strncmp(path, root, len) == 0)
        return path + len;
    return path;
}

/* ── colour helpers (disabled when NO_COLOR is set) ───────────────────── */
#ifdef NO_COLOR
#define CLR_RED ""
#define CLR_GREEN ""
#define CLR_RESET ""
#else
#define CLR_RED "\033[1;31m"
#define CLR_GREEN "\033[1;32m"
#define CLR_RESET "\033[0m"
#endif

/* ── internal failure reporter ────────────────────────────────────────── */
#define FAIL_MSG(fmt, ...)                                                   \
    do {                                                                     \
        fprintf(stderr, CLR_RED "  FAIL" CLR_RESET " %s:%d (%s): " fmt "\n", \
                _strip_root(__FILE__), __LINE__, current_test_name,          \
                __VA_ARGS__);                                                \
        test_failures++;                                                     \
    } while (0)

/* ── assertions ───────────────────────────────────────────────────────── */

/* Integer equality (S32/U32/int) */
#define ASSERT_EQ_INT(expected, actual)                  \
    do {                                                 \
        assert_count++;                                  \
        long long _e = (long long)(expected);            \
        long long _a = (long long)(actual);              \
        if (_e != _a)                                    \
            FAIL_MSG("expected %lld, got %lld", _e, _a); \
    } while (0)

/* Unsigned equality */
#define ASSERT_EQ_UINT(expected, actual)                        \
    do {                                                        \
        assert_count++;                                         \
        unsigned long long _e = (unsigned long long)(expected); \
        unsigned long long _a = (unsigned long long)(actual);   \
        if (_e != _a)                                           \
            FAIL_MSG("expected %llu, got %llu", _e, _a);        \
    } while (0)

/* Float approximate equality */
#define ASSERT_NEAR_F(expected, actual, eps)                               \
    do {                                                                   \
        assert_count++;                                                    \
        double _e = (double)(expected);                                    \
        double _a = (double)(actual);                                      \
        double _d = fabs(_e - _a);                                         \
        if (_d > (double)(eps))                                            \
            FAIL_MSG("expected %.10f, got %.10f (diff %.10f > eps %.10f)", \
                     _e, _a, _d, (double)(eps));                           \
    } while (0)

/* Memory block equality */
#define ASSERT_MEM_EQ(expected, actual, size)                         \
    do {                                                              \
        assert_count++;                                               \
        if (memcmp((expected), (actual), (size)) != 0)                \
            FAIL_MSG("memory blocks differ (%d bytes)", (int)(size)); \
    } while (0)

/* Boolean truth */
#define ASSERT_TRUE(cond)                               \
    do {                                                \
        assert_count++;                                 \
        if (!(cond))                                    \
            FAIL_MSG("condition '%s' is false", #cond); \
    } while (0)

/* ── ASM vs CPP comparison helpers ────────────────────────────────────── */

#define ASSERT_ASM_CPP_EQ_INT(asm_val, cpp_val, label)              \
    do {                                                            \
        assert_count++;                                             \
        long long _a = (long long)(asm_val);                        \
        long long _c = (long long)(cpp_val);                        \
        if (_a != _c)                                               \
            FAIL_MSG("[%s] ASM=%lld vs CPP=%lld", (label), _a, _c); \
    } while (0)

#define ASSERT_ASM_CPP_NEAR_F(asm_val, cpp_val, eps, label)      \
    do {                                                         \
        assert_count++;                                          \
        double _a = (double)(asm_val);                           \
        double _c = (double)(cpp_val);                           \
        double _d = fabs(_a - _c);                               \
        if (_d > (double)(eps))                                  \
            FAIL_MSG("[%s] ASM=%.10f vs CPP=%.10f (diff %.10f)", \
                     (label), _a, _c, _d);                       \
    } while (0)

#define ASSERT_ASM_CPP_MEM_EQ(asm_ptr, cpp_ptr, size, label)      \
    do {                                                          \
        assert_count++;                                           \
        if (memcmp((asm_ptr), (cpp_ptr), (size)) != 0)            \
            FAIL_MSG("[%s] ASM vs CPP memory differs (%d bytes)", \
                     (label), (int)(size));                       \
    } while (0)

/* ── test runner ──────────────────────────────────────────────────────── */
#define RUN_TEST(fn)                                           \
    do {                                                       \
        int _prev_failures = test_failures;                    \
        current_test_name = #fn;                               \
        test_count++;                                          \
        fn();                                                  \
        if (test_failures == _prev_failures) {                 \
            test_passed++;                                     \
            printf(CLR_GREEN "  PASS" CLR_RESET " %s\n", #fn); \
        }                                                      \
    } while (0)

/* ── summary ──────────────────────────────────────────────────────────── */
#define TEST_SUMMARY()                                                   \
    do {                                                                 \
        printf("\n──────────────────────────────────────────\n");        \
        printf("Tests: %d | Passed: %d | Failed: %d | Assertions: %d\n", \
               test_count, test_passed, test_failures, assert_count);    \
        if (test_failures == 0)                                          \
            printf(CLR_GREEN "ALL TESTS PASSED" CLR_RESET "\n");         \
        else                                                             \
            printf(CLR_RED "%d TEST(S) FAILED" CLR_RESET "\n",           \
                   test_failures);                                       \
    } while (0)

#endif /* TEST_HARNESS_H */
