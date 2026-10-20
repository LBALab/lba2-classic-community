#include "../test_harness.h"
#include "../../LIB386/PLATFORM/NULL/PRIVATE.H"

#include <SYSTEM/ANDROID.H>
#include <vector>

static void quiet_log(void *userdata, PlatformLogLevel level, const char *message) {
    if (userdata) {
        ++*(int *)userdata;
        ASSERT_EQ_INT(PLATFORM_LOG_ERROR, level);
        ASSERT_TRUE(message[0] != '\0');
    }
}
static void overlay(uint32_t *pixels, uint32_t, uint32_t, uint32_t, const uint8_t *) {
    pixels[0] = 0xFF123456;
}
static void test_offscreen_frames_and_lifecycle() {
    ASSERT_TRUE(!g_platform.window.initialized());
    ASSERT_TRUE(g_platform.window.init("Null conformance"));
    ASSERT_TRUE(g_platform.window.init("Null conformance"));
    ASSERT_TRUE(g_platform.window.create(8, 6, false));
    uint32_t rng = 0xDEADBEEF;
    for (int round = 0; round < 40; ++round) {
        uint32_t width = round % 2 ? 8 : 16, height = round % 3 ? 6 : 4;
        ASSERT_TRUE(g_platform.window.resize(width, height, round % 2 != 0));
        std::vector<uint8_t> source((width + 3) * height);
        uint8_t palette[768];
        for (size_t i = 0; i < source.size(); ++i) {
            rng = rng * 1103515245u + 12345u;
            source[i] = (uint8_t)(rng >> 16);
        }
        for (size_t i = 0; i < sizeof(palette); ++i) {
            rng = rng * 1103515245u + 12345u;
            palette[i] = (uint8_t)(rng >> 16);
        }
        PlatformFrame frame = {&source[0], width, height, width + 3, palette, round % 2 ? overlay : NULL};
        ASSERT_TRUE(g_platform.window.present(&frame));
        std::vector<uint32_t> expected(width * height);
        for (uint32_t y = 0; y < height; ++y)
            for (uint32_t x = 0; x < width; ++x) {
                const uint8_t *rgb = palette + source[y * (width + 3) + x] * 3;
                expected[y * width + x] = 0xFF000000u | ((uint32_t)rgb[0] << 16) |
                                          ((uint32_t)rgb[1] << 8) | rgb[2];
            }
        if (frame.overlay)
            expected[0] = 0xFF123456;
        uint32_t actualWidth = 0, actualHeight = 0;
        const uint32_t *actual = NullPlatform_Frame(&actualWidth, &actualHeight);
        ASSERT_EQ_UINT(width, actualWidth);
        ASSERT_EQ_UINT(height, actualHeight);
        ASSERT_MEM_EQ(&expected[0], actual, expected.size() * sizeof(uint32_t));
        int32_t w = 0, h = 0, x = -1, y = -1;
        ASSERT_TRUE(g_platform.window.size(&w, &h));
        ASSERT_EQ_INT(width, w);
        ASSERT_EQ_INT(height, h);
        g_platform.window.surface_coords(-10, 1000, &x, &y);
        ASSERT_EQ_INT(0, x);
        ASSERT_EQ_INT(height - 1, y);
    }
    ASSERT_TRUE(!g_platform.window.resize(0, 1, false));
    g_platform.window.set_fullscreen(true);
    ASSERT_TRUE(g_platform.window.get_fullscreen());
    ASSERT_TRUE(!g_platform.window.is_fullscreen());
    g_platform.window.set_fullscreen(false);
    ASSERT_TRUE(!g_platform.window.get_fullscreen());
    ASSERT_TRUE(g_platform.window.set_present_filter(PLATFORM_FILTER_NEAREST));
    g_platform.system.clear_error();
    ASSERT_TRUE(!g_platform.window.set_present_filter(PLATFORM_FILTER_LINEAR));
    ASSERT_TRUE(g_platform.system.get_error()[0] != '\0');
    g_platform.system.clear_error();
    g_platform.window.destroy();
    g_platform.window.shutdown();
    ASSERT_TRUE(!g_platform.window.initialized());
}
static void test_absent_devices_and_explicit_errors() {
    ASSERT_TRUE(!g_platform.capabilities.windowed);
    ASSERT_TRUE(!g_platform.capabilities.touch_overlay);
    ASSERT_TRUE(!g_platform.capabilities.transient_window_loss);
    PlatformEvent event = {};
    ASSERT_TRUE(g_platform.events.init());
    g_platform.events.pump();
    ASSERT_TRUE(!g_platform.events.poll(&event));
    g_platform.events.shutdown();
    const uint8_t *keys = g_platform.input.keyboard_state();
    for (int i = 0; i < PLATFORM_SCANCODE_COUNT; ++i)
        ASSERT_EQ_UINT(0, keys[i]);
    ASSERT_EQ_UINT(0, g_platform.input.modifiers());
    ASSERT_TRUE(g_platform.input.init_gamepads());
    int32_t count = -1;
    uint32_t *pads = g_platform.input.gamepads(&count);
    ASSERT_TRUE(pads != NULL);
    ASSERT_EQ_INT(0, count);
    g_platform.host.free_memory(pads);
    ASSERT_TRUE(g_platform.input.open_gamepad(42) == NULL);
    g_platform.input.close_gamepad(NULL);
    g_platform.input.shutdown_gamepads();
    uint32_t w = 123, h = 456;
    ASSERT_TRUE(!g_platform.window.display_size(&w, &h));
    ASSERT_TRUE(!g_platform.window.primary_display_size(&w, &h));
    ASSERT_EQ_UINT(123, w);
    ASSERT_EQ_UINT(456, h);
    ASSERT_TRUE(!g_platform.host.message_box(PLATFORM_MESSAGE_INFORMATION, "Test", "Test"));
    ASSERT_TRUE(g_platform.host.begin_folder_dialog(NULL) == NULL);
    ASSERT_EQ_INT(PLATFORM_DIALOG_ERROR, g_platform.host.poll_folder_dialog(NULL, NULL));
    g_platform.host.end_folder_dialog(NULL);
    ASSERT_TRUE(!g_platform.host.set_clipboard("Test"));
    ASSERT_TRUE(g_platform.host.get_clipboard() == NULL);
    ASSERT_TRUE(strstr(g_platform.system.get_error(), "clipboard") != NULL);
    g_platform.system.clear_error();
    ASSERT_TRUE(g_platform.system.get_error()[0] == '\0');
    ASSERT_EQ_INT(0, IsAndroidTVDevice());
    ASSERT_TRUE(Android_EnsureExternalStoragePermission());
    ASSERT_TRUE(Android_HasExternalStoragePermission());
    ASSERT_EQ_INT(0, Android_GetExternalFilesDir(NULL, 0));
    Android_SavePreviousCrashReport(NULL);
    ASSERT_TRUE(Android_GetPreviousCrashNote()[0] == '\0');
    int records = 0;
    Platform_SetLogOutput(quiet_log, &records);
    void (*modes[])(bool) = {
        g_platform.window.set_vsync, g_platform.window.set_relative_mouse, g_platform.window.set_mouse_grab,
        g_platform.input.text_input, g_platform.input.cursor_visible};
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        g_platform.system.clear_error();
        modes[i](true);
        ASSERT_TRUE(g_platform.system.get_error()[0] != '\0');
        ASSERT_EQ_INT(i + 1, records);
        g_platform.system.clear_error();
        modes[i](false);
        ASSERT_TRUE(g_platform.system.get_error()[0] == '\0');
    }
    ASSERT_EQ_UINT(0, g_platform.input.keycode(PLATFORM_SCANCODE_A, PLATFORM_KMOD_NONE));
    ASSERT_TRUE(g_platform.input.scancode_name(PLATFORM_SCANCODE_A)[0] == '\0');
    ASSERT_TRUE(g_platform.input.gamepad_name(NULL)[0] == '\0');
    ASSERT_TRUE(!g_platform.input.gamepad_button(NULL, PLATFORM_GAMEPAD_BUTTON_SOUTH));
    ASSERT_EQ_INT(0, g_platform.input.gamepad_axis(NULL, PLATFORM_GAMEPAD_AXIS_LEFTX));
    ASSERT_EQ_INT(10, records);
    Platform_SetLogOutput(quiet_log, NULL);
}
static void test_host_services_and_clock() {
    ASSERT_TRUE(g_platform.system.thread_id() != 0);
    ASSERT_TRUE(g_platform.system.cpu_count() > 0);
    ASSERT_TRUE(g_platform.system.ram_mb() > 0);
    ASSERT_TRUE(g_platform.system.platform_name()[0] != '\0');
    const uint64_t ticks = g_platform.system.ticks_ms();
    const uint64_t counter = g_platform.system.performance_counter();
    ASSERT_TRUE(g_platform.system.performance_frequency() > 0);
    g_platform.system.delay_ms(3);
    ASSERT_TRUE(g_platform.system.ticks_ms() >= ticks + 3);
    ASSERT_TRUE(g_platform.system.performance_counter() > counter);
    const char *base = g_platform.host.base_path();
    ASSERT_TRUE(base && base[0]);
    ASSERT_TRUE(base == g_platform.host.base_path());
    char *cwd = g_platform.host.current_directory();
    ASSERT_TRUE(cwd && cwd[0]);
    g_platform.host.free_memory(cwd);
    ASSERT_TRUE(g_platform.host.home_path() != NULL);
    ASSERT_TRUE(g_platform.host.pref_path("PAL fixture", NULL) == NULL);
    char folder[128];
    snprintf(folder, sizeof(folder), "pal-null-%llu", (unsigned long long)counter);
    ASSERT_TRUE(g_platform.host.create_directory(folder));
    ASSERT_TRUE(g_platform.host.create_directory(folder));
    char child[160];
    snprintf(child, sizeof(child), "%s/child", folder);
    ASSERT_TRUE(g_platform.host.create_directory(child));
    ASSERT_TRUE(!g_platform.host.remove_path(folder));
    ASSERT_TRUE(g_platform.host.remove_path(child));
    ASSERT_TRUE(g_platform.host.remove_path(folder));
    ASSERT_TRUE(g_platform.host.remove_path(folder));
}
int main() {
    Platform_SetLogOutput(quiet_log, NULL);
    ASSERT_TRUE(strcmp(g_platform.name, "null") == 0);
    ASSERT_TRUE(g_platform.system.set_headless());
    ASSERT_TRUE(g_platform.system.init());
    RUN_TEST(test_offscreen_frames_and_lifecycle);
    RUN_TEST(test_absent_devices_and_explicit_errors);
    RUN_TEST(test_host_services_and_clock);
    g_platform.system.shutdown();
    Platform_SetLogOutput(NULL, NULL);
    TEST_SUMMARY();
    return test_failures != 0;
}
