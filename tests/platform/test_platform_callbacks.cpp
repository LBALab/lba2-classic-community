#include "../test_harness.h"
#include <PLATFORM/PLATFORM.H>

static void test_selected_backend_callbacks() {
#define PLATFORM_CALLBACK(member) ASSERT_TRUE(g_platform.member != NULL);
#include "platform_callback_checks.inc"
#undef PLATFORM_CALLBACK
}

static int missing_callbacks(const PlatformInterface &api) {
    int missing = 0;
#define PLATFORM_CALLBACK(member) missing += api.member == NULL;
#include "platform_callback_checks.inc"
#undef PLATFORM_CALLBACK
    return missing;
}

static void test_each_missing_callback_is_detected() {
#define PLATFORM_CALLBACK(member)                        \
    {                                                    \
        PlatformInterface incomplete = g_platform;       \
        incomplete.member = NULL;                        \
        ASSERT_EQ_INT(1, missing_callbacks(incomplete)); \
    }
#include "platform_callback_checks.inc"
#undef PLATFORM_CALLBACK
}

int main() {
    RUN_TEST(test_selected_backend_callbacks);
    RUN_TEST(test_each_missing_callback_is_detected);
    TEST_SUMMARY();
    return test_failures != 0;
}
