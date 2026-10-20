#include "../test_harness.h"
#include "../../LIB386/PLATFORM/SDL2/PRIVATE.H"
#include "../../LIB386/PLATFORM/SDL2/DIRECTORY_PATH.H"

#include <string>
#include <vector>

static std::vector<uint32_t> rendered;
static int presents = 0;
static void read_pixels(SDL_Renderer *renderer) {
    ++presents;
    int w, h;
    ASSERT_EQ_INT(0, SDL_GetRendererOutputSize(renderer, &w, &h));
    rendered.resize((size_t)w * h);
    ASSERT_EQ_INT(0, SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                                          &rendered[0], w * sizeof(uint32_t)));
}
static void overlay(uint32_t *pixels, uint32_t, uint32_t, uint32_t, const uint8_t *) {
    pixels[0] = 0xFF123456u;
}
static int fail_fullscreen(SDL_Window *, Uint32) { return SDL_SetError("fullscreen fixture"); }

static int blended_pixels() {
    int count = 0;
    for (size_t i = 0; i < rendered.size(); ++i) {
        const uint32_t rgb = rendered[i] & 0x00FFFFFF;
        if (rgb != 0 && rgb != 0x00FFFFFF)
            ++count;
    }
    return count;
}

static void test_present_filter_and_surface_recreation() {
    ASSERT_TRUE(g_platform.window.set_present_filter != NULL);
    if (!g_platform.window.set_present_filter)
        return;
    ASSERT_TRUE(g_platform.window.init("SDL2 filter"));
    ASSERT_TRUE(g_platform.window.create(120, 90, false));
    ASSERT_TRUE(g_platform.window.resize(16, 12, true));
    uint8_t pixels[16 * 12], palette[768] = {};
    palette[3] = palette[4] = palette[5] = 255;
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 16; ++x)
            pixels[y * 16 + x] = (uint8_t)((x + y) & 1);
    PlatformFrame frame = {pixels, 16, 12, 16, palette, NULL};
    SDL2_SetPresentationProbe(read_pixels);
    ASSERT_TRUE(g_platform.window.present(&frame));
    ASSERT_EQ_INT(0, blended_pixels());
    const int before = presents;
    ASSERT_TRUE(g_platform.window.set_present_filter(PLATFORM_FILTER_LINEAR));
    ASSERT_EQ_INT(before + 1, presents);
    ASSERT_TRUE(blended_pixels() > 0);
    const std::vector<uint32_t> linear = rendered;
    ASSERT_TRUE(g_platform.window.resize(16, 12, true));
    ASSERT_TRUE(g_platform.window.present(&frame));
    ASSERT_MEM_EQ(&linear[0], &rendered[0], linear.size() * sizeof(uint32_t));
    ASSERT_TRUE(!g_platform.window.set_present_filter(PLATFORM_FILTER_PIXELART));
    ASSERT_TRUE(strstr(SDL_GetError(), "pixelart") != NULL);
    ASSERT_TRUE(!g_platform.window.set_present_filter((PlatformPresentFilter)3));
    ASSERT_TRUE(SDL_GetError()[0] != '\0');
    ASSERT_TRUE(g_platform.window.present(&frame));
    ASSERT_MEM_EQ(&linear[0], &rendered[0], linear.size() * sizeof(uint32_t));
    g_platform.window.destroy();
    ASSERT_TRUE(g_platform.window.create(120, 90, false));
    ASSERT_TRUE(g_platform.window.resize(16, 12, true));
    ASSERT_TRUE(g_platform.window.present(&frame));
    ASSERT_MEM_EQ(&linear[0], &rendered[0], linear.size() * sizeof(uint32_t));
    ASSERT_TRUE(g_platform.window.set_present_filter(PLATFORM_FILTER_NEAREST));
    ASSERT_EQ_INT(0, blended_pixels());
    SDL2_SetPresentationProbe(NULL);
    g_platform.window.shutdown();
}

static void test_window_frames_modes_and_lifetime() {
    ASSERT_TRUE(g_platform.capabilities.windowed);
    ASSERT_TRUE(!g_platform.capabilities.transient_window_loss && !g_platform.capabilities.touch_overlay);
    ASSERT_TRUE(!g_platform.window.initialized());
    ASSERT_TRUE(g_platform.window.init("SDL2 conformance"));
    ASSERT_TRUE(g_platform.window.init("SDL2 conformance"));
    uint32_t dw = 123, dh = 456;
    ASSERT_TRUE(!g_platform.window.primary_display_size(&dw, &dh));
    ASSERT_EQ_UINT(123, dw);
    ASSERT_EQ_UINT(456, dh);
    ASSERT_TRUE(g_platform.window.create(1280, 720, false));
    ASSERT_TRUE(g_platform.window.resize(640, 480, true));
    int32_t w, h, x, y;
    ASSERT_TRUE(g_platform.window.size(&w, &h));
    ASSERT_EQ_INT(1280, w);
    ASSERT_EQ_INT(720, h);
    g_platform.window.surface_coords(640, 360, &x, &y);
    ASSERT_EQ_INT(320, x);
    ASSERT_EQ_INT(240, y);
    g_platform.window.surface_coords(0, -1, &x, &y);
    ASSERT_EQ_INT(0, x);
    ASSERT_EQ_INT(0, y);
    g_platform.window.surface_coords(1280, 720, &x, &y);
    ASSERT_EQ_INT(639, x);
    ASSERT_EQ_INT(479, y);
    SDL_SetWindowSize(SDL2_TestWindow(), 3, 3);
    g_platform.window.surface_coords(INT32_MAX, INT32_MAX, &x, &y);
    ASSERT_EQ_INT(639, x);
    ASSERT_EQ_INT(479, y);
    g_platform.window.surface_coords(INT32_MIN, INT32_MIN, &x, &y);
    ASSERT_EQ_INT(0, x);
    ASSERT_EQ_INT(0, y);
    SDL_SetWindowSize(SDL2_TestWindow(), 1280, 720);
    ASSERT_TRUE(g_platform.window.display_size(&dw, &dh));
    SDL_DisplayMode mode;
    ASSERT_EQ_INT(0, SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(SDL2_TestWindow()), &mode));
    ASSERT_EQ_UINT(mode.w, dw);
    ASSERT_EQ_UINT(mode.h, dh);
    SDL2_SetPresentationProbe(read_pixels);
    uint32_t rng = 0xDEADBEEF;
    for (int round = 0; round < 40; ++round) {
        const uint32_t width = round % 2 ? 32 : 16, height = round % 3 ? 24 : 8;
        ASSERT_TRUE(g_platform.window.resize(width, height, false));
        std::vector<uint8_t> input((width + 3) * height);
        std::vector<uint32_t> expected(width * height);
        uint8_t palette[768];
        for (size_t i = 0; i < input.size(); ++i) {
            rng = rng * 1103515245u + 12345u;
            input[i] = (uint8_t)(rng >> 16);
        }
        for (size_t i = 0; i < sizeof(palette); ++i) {
            rng = rng * 1103515245u + 12345u;
            palette[i] = (uint8_t)(rng >> 16);
        }
        for (uint32_t row = 0; row < height; ++row)
            for (uint32_t col = 0; col < width; ++col) {
                const uint8_t *rgb = palette + input[row * (width + 3) + col] * 3;
                expected[row * width + col] = 0xFF000000u | ((uint32_t)rgb[0] << 16) |
                                              ((uint32_t)rgb[1] << 8) | rgb[2];
            }
        PlatformFrame frame = {&input[0], width, height, width + 3, palette, round % 2 ? overlay : NULL};
        if (frame.overlay)
            expected[0] = 0xFF123456u;
        ASSERT_TRUE(g_platform.window.present(&frame));
        ASSERT_EQ_UINT(expected.size(), rendered.size());
        if (expected.size() == rendered.size())
            ASSERT_MEM_EQ(&expected[0], &rendered[0], expected.size() * sizeof(uint32_t));
    }
    PlatformEvent event;
    while (g_platform.events.poll(&event)) {
    }
    const std::vector<uint32_t> previous = rendered;
    const int previousPresents = presents;
    SDL_Event resize = {};
    resize.type = SDL_WINDOWEVENT;
    resize.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    ASSERT_EQ_INT(1, SDL_PushEvent(&resize));
    while (g_platform.events.poll(&event)) {
    }
    ASSERT_EQ_INT(previousPresents + 1, presents);
    ASSERT_MEM_EQ(&previous[0], &rendered[0], previous.size() * sizeof(uint32_t));
    SDL2_SetPresentationProbe(NULL);
    for (int state = 0; state < 2; ++state) {
        g_platform.window.set_fullscreen(state != 0);
        ASSERT_EQ_INT(state, g_platform.window.get_fullscreen());
        const bool actual = g_platform.window.is_fullscreen();
        SDL2_SetFullscreenProbe(fail_fullscreen);
        for (int request = 0; request < 2; ++request) {
            g_platform.window.set_fullscreen(request != 0);
            ASSERT_EQ_INT(state, g_platform.window.get_fullscreen());
            ASSERT_EQ_INT(actual, g_platform.window.is_fullscreen());
            ASSERT_TRUE(strcmp(g_platform.system.get_error(), "fullscreen fixture") == 0);
        }
        SDL2_SetFullscreenProbe(NULL);
    }
    g_platform.window.set_fullscreen(false);
    for (int state = 0; state < 2; ++state) {
        SDL_Renderer *renderer = SDL_GetRenderer(SDL2_TestWindow());
        const int nativeVSyncResult = SDL_RenderSetVSync(renderer, state);
        char *nativeVSyncError = SDL_strdup(SDL_GetError());
        SDL_RendererInfo expectedInfo;
        ASSERT_EQ_INT(0, SDL_GetRendererInfo(renderer, &expectedInfo));
        if (nativeVSyncResult == 0)
            SDL_RenderSetVSync(renderer, !state);
        g_platform.window.set_vsync(state != 0);
        if (nativeVSyncResult != 0)
            ASSERT_TRUE(strcmp(SDL_GetError(), nativeVSyncError) == 0);
        SDL_free(nativeVSyncError);
        SDL_RendererInfo info;
        ASSERT_EQ_INT(0, SDL_GetRendererInfo(renderer, &info));
        ASSERT_EQ_UINT(expectedInfo.flags, info.flags);
        const int nativeResult = SDL_SetRelativeMouseMode(state ? SDL_TRUE : SDL_FALSE);
        const bool expectedRelative = SDL_GetRelativeMouseMode() != SDL_FALSE;
        if (nativeResult == 0)
            SDL_SetRelativeMouseMode(state ? SDL_FALSE : SDL_TRUE);
        g_platform.window.set_relative_mouse(state != 0);
        ASSERT_EQ_INT(expectedRelative, SDL_GetRelativeMouseMode());
        g_platform.input.cursor_visible(state != 0);
        ASSERT_EQ_INT(state, SDL_ShowCursor(SDL_QUERY));
        g_platform.input.text_input(state != 0);
        ASSERT_EQ_INT(state, SDL_IsTextInputActive());
        ASSERT_EQ_INT(state, SDL_ShowCursor(SDL_QUERY));
    }
    g_platform.window.destroy();
    w = 123;
    h = 456;
    ASSERT_TRUE(!g_platform.window.size(&w, &h));
    ASSERT_EQ_INT(123, w);
    ASSERT_EQ_INT(456, h);
    g_platform.window.set_fullscreen(true);
    ASSERT_TRUE(g_platform.window.get_fullscreen());
    ASSERT_TRUE(!g_platform.window.is_fullscreen());
    g_platform.window.set_fullscreen(false);
    g_platform.window.shutdown();
    ASSERT_TRUE(!g_platform.window.initialized());
    ASSERT_EQ_UINT(0, SDL_WasInit(SDL_INIT_VIDEO));
}

static void test_event_payloads_and_borrowed_text() {
    ASSERT_TRUE(g_platform.window.init("Event coordinates"));
    ASSERT_TRUE(g_platform.window.create(1280, 720, false));
    ASSERT_TRUE(g_platform.window.resize(640, 480, true));
    PlatformEvent event;
    while (g_platform.events.poll(&event)) {
    }
    SDL_Event motion = {};
    motion.type = SDL_MOUSEMOTION;
    motion.motion.x = 640;
    motion.motion.y = 360;
    motion.motion.xrel = -13;
    motion.motion.yrel = 7;
    ASSERT_EQ_INT(1, SDL_PushEvent(&motion));
    ASSERT_TRUE(g_platform.events.poll(&event));
    ASSERT_EQ_INT(PLATFORM_EVENT_MOUSE_MOTION, event.type);
    ASSERT_TRUE(event.motion.x == 640 && event.motion.y == 360 &&
                event.motion.xrel == -13 && event.motion.yrel == 7);
    while (g_platform.events.poll(&event)) {
    }
    SDL_Event text = {};
    g_platform.input.text_input(true);
    text.type = SDL_TEXTINPUT;
    strcpy(text.text.text, "PAL text");
    ASSERT_EQ_INT(1, SDL_PushEvent(&text));
    const bool receivedText = g_platform.events.poll(&event);
    ASSERT_TRUE(receivedText);
    ASSERT_EQ_INT(PLATFORM_EVENT_TEXT_INPUT, event.type);
    if (receivedText && event.type == PLATFORM_EVENT_TEXT_INPUT)
        ASSERT_TRUE(strcmp(event.text.text, "PAL text") == 0);
    char noise[1024];
    memset(noise, 0xCC, sizeof(noise));
    if (receivedText && event.type == PLATFORM_EVENT_TEXT_INPUT)
        ASSERT_TRUE(strcmp(event.text.text, "PAL text") == 0);
    SDL_Event key = {};
    key.type = SDL_KEYDOWN;
    key.key.keysym.scancode = SDL_SCANCODE_RSHIFT;
    key.key.keysym.mod = KMOD_RSHIFT;
    key.key.repeat = 1;
    PlatformEvent expected = {};
    expected.type = PLATFORM_EVENT_KEY_DOWN;
    expected.key.scancode = PLATFORM_SCANCODE_RSHIFT;
    expected.key.mod = PLATFORM_KMOD_RSHIFT;
    expected.key.repeat = true;
    ASSERT_TRUE(SDL2_TranslateEvent(&key, &event));
    ASSERT_MEM_EQ(&expected, &event, sizeof(event));
    SDL_Event wheel = {};
    wheel.type = SDL_MOUSEWHEEL;
    wheel.wheel.preciseY = -0.75f;
    ASSERT_TRUE(SDL2_TranslateEvent(&wheel, &event));
    ASSERT_TRUE(event.wheel.y == -0.75f);
    SDL_Event finger = {};
    finger.type = SDL_FINGERMOTION;
    finger.tfinger.fingerId = (SDL_FingerID)0x1234567801234567ULL;
    finger.tfinger.x = 0.25f;
    finger.tfinger.y = 0.75f;
    while (g_platform.events.poll(&event)) {
    }
    ASSERT_EQ_INT(1, SDL_PushEvent(&finger));
    bool receivedFinger = false;
    while (g_platform.events.poll(&event))
        if (event.type == PLATFORM_EVENT_FINGER_MOTION) {
            receivedFinger = true;
            ASSERT_EQ_UINT(finger.tfinger.fingerId, event.tfinger.fingerID);
            ASSERT_TRUE(event.tfinger.x == 0.25f && event.tfinger.y == 0.75f);
        }
    ASSERT_TRUE(receivedFinger);
    g_platform.input.cursor_visible(true);
    SDL_Event focus = {};
    focus.type = SDL_WINDOWEVENT;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    ASSERT_EQ_INT(1, SDL_PushEvent(&focus));
    while (g_platform.events.poll(&event)) {
    }
    ASSERT_EQ_INT(SDL_DISABLE, SDL_ShowCursor(SDL_QUERY));
    g_platform.window.shutdown();
}

static void test_keyboard_and_gamepad_instance_ids() {
    for (int i = 0; i < PLATFORM_SCANCODE_COUNT; ++i) {
        ASSERT_EQ_UINT(SDL_GetKeyFromScancode((SDL_Scancode)i), g_platform.input.keycode(i, 0));
        ASSERT_TRUE(strcmp(SDL_GetScancodeName((SDL_Scancode)i), g_platform.input.scancode_name(i)) == 0);
    }
    int count;
    const Uint8 *native = SDL_GetKeyboardState(&count);
    const uint8_t *keys = g_platform.input.keyboard_state();
    for (int i = 0; i < PLATFORM_SCANCODE_COUNT; ++i)
        ASSERT_EQ_UINT(i < count && native[i] ? 1 : 0, keys[i]);
    SDL_SetModState((SDL_Keymod)(KMOD_RSHIFT | KMOD_LCTRL));
    ASSERT_EQ_UINT(PLATFORM_KMOD_RSHIFT | PLATFORM_KMOD_LCTRL, g_platform.input.modifiers());
    SDL_SetModState(KMOD_NONE);
    ASSERT_TRUE(g_platform.input.init_gamepads());
    const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                                SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    ASSERT_TRUE(index >= 0);
    if (index < 0)
        return;
    SDL_Joystick *joystick = SDL_JoystickOpen(index);
    const uint32_t id = (uint32_t)SDL_JoystickGetDeviceInstanceID(index);
    int32_t n = 0;
    uint32_t *ids = g_platform.input.gamepads(&n);
    ASSERT_TRUE(ids != NULL && n > 0);
    bool found = false;
    for (int32_t i = 0; ids && i < n; ++i)
        found = found || ids[i] == id;
    ASSERT_TRUE(found);
    g_platform.host.free_memory(ids);
    PlatformGamepad *pad = g_platform.input.open_gamepad(id);
    ASSERT_TRUE(pad != NULL);
    SDL_Event added = {};
    added.type = SDL_CONTROLLERDEVICEADDED;
    added.cdevice.which = index;
    PlatformEvent event;
    ASSERT_TRUE(SDL2_TranslateEvent(&added, &event));
    ASSERT_EQ_UINT(id, event.gdevice.which);
    ASSERT_TRUE(strcmp(g_platform.input.gamepad_name(pad), SDL_GameControllerName((SDL_GameController *)pad)) == 0);
    uint32_t rng = 0xDEADBEEF;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    for (int round = 0; pad && round < 40; ++round) {
        for (int axis = 0; axis < PLATFORM_GAMEPAD_AXIS_COUNT; ++axis) {
            rng = rng * 1103515245u + 12345u;
            ASSERT_EQ_INT(0, SDL_JoystickSetVirtualAxis(joystick, axis, (Sint16)(rng >> 16)));
        }
        for (int button = 0; button < PLATFORM_GAMEPAD_BUTTON_COUNT; ++button)
            ASSERT_EQ_INT(0, SDL_JoystickSetVirtualButton(joystick, button, (Uint8)((round + button) % 2)));
        SDL_JoystickUpdate();
        for (int axis = 0; axis < PLATFORM_GAMEPAD_AXIS_COUNT; ++axis)
            ASSERT_EQ_INT(SDL_GameControllerGetAxis((SDL_GameController *)pad, (SDL_GameControllerAxis)axis),
                          g_platform.input.gamepad_axis(pad, (PlatformGamepadAxis)axis));
        for (int button = 0; button < PLATFORM_GAMEPAD_BUTTON_COUNT; ++button)
            ASSERT_EQ_INT(SDL_GameControllerGetButton((SDL_GameController *)pad, (SDL_GameControllerButton)button),
                          g_platform.input.gamepad_button(pad, (PlatformGamepadButton)button));
    }
    g_platform.input.close_gamepad(pad);
    SDL_JoystickClose(joystick);
    ASSERT_EQ_INT(0, SDL_JoystickDetachVirtual(index));
    ASSERT_TRUE(g_platform.input.open_gamepad(id) == NULL);
    g_platform.input.shutdown_gamepads();
}

static void test_translated_event_union_bytes() {
    const Uint32 types[] = {SDL_QUIT, SDL_KEYDOWN, SDL_KEYUP, SDL_MOUSEMOTION,
                            SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP, SDL_MOUSEWHEEL, SDL_CONTROLLERDEVICEREMOVED,
                            SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERAXISMOTION, SDL_FINGERDOWN, SDL_FINGERUP,
                            SDL_FINGERMOTION, SDL_TEXTINPUT, SDL_USEREVENT};
    const PlatformEventType neutral[] = {PLATFORM_EVENT_QUIT, PLATFORM_EVENT_KEY_DOWN,
                                         PLATFORM_EVENT_KEY_UP, PLATFORM_EVENT_MOUSE_MOTION, PLATFORM_EVENT_MOUSE_BUTTON_DOWN,
                                         PLATFORM_EVENT_MOUSE_BUTTON_UP, PLATFORM_EVENT_MOUSE_WHEEL, PLATFORM_EVENT_GAMEPAD_REMOVED,
                                         PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN, PLATFORM_EVENT_GAMEPAD_AXIS_MOTION,
                                         PLATFORM_EVENT_FINGER_DOWN, PLATFORM_EVENT_FINGER_UP, PLATFORM_EVENT_FINGER_MOTION,
                                         PLATFORM_EVENT_TEXT_INPUT, PLATFORM_EVENT_OTHER};
    Uint32 rng = 0xDEADBEEF;
    for (int round = 0; round < 40; ++round) {
        rng = rng * 1103515245u + 12345u;
        for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
            SDL_Event source = {};
            source.type = types[i];
            PlatformEvent expected = {}, actual;
            expected.type = neutral[i];
            if (source.type == SDL_KEYDOWN || source.type == SDL_KEYUP) {
                source.key.keysym.scancode = SDL_SCANCODE_A;
                source.key.keysym.mod = KMOD_CTRL;
                source.key.repeat = round % 2;
                expected.key.scancode = PLATFORM_SCANCODE_A;
                expected.key.mod = PLATFORM_KMOD_CTRL;
                expected.key.repeat = source.key.repeat != 0;
            } else if (source.type == SDL_MOUSEMOTION) {
                source.motion.x = (Sint32)(rng & 0xFFFF);
                source.motion.y = (Sint32)(rng >> 16);
                source.motion.xrel = -round;
                source.motion.yrel = round;
                expected.motion.x = source.motion.x;
                expected.motion.y = source.motion.y;
                expected.motion.xrel = source.motion.xrel;
                expected.motion.yrel = source.motion.yrel;
                // Alternate a finger-made event with a real mouse one.
                source.motion.which = (round & 1) ? SDL_TOUCH_MOUSEID : 1;
                expected.motion.touch = (round & 1) != 0;
            } else if (source.type == SDL_MOUSEBUTTONDOWN || source.type == SDL_MOUSEBUTTONUP) {
                source.button.button = SDL_BUTTON_RIGHT;
                source.button.x = round;
                source.button.y = -round;
                expected.button.button = PLATFORM_BUTTON_RIGHT;
                expected.button.x = source.button.x;
                expected.button.y = source.button.y;
                source.button.which = (round & 1) ? SDL_TOUCH_MOUSEID : 1;
                expected.button.touch = (round & 1) != 0;
            } else if (source.type == SDL_MOUSEWHEEL) {
                source.wheel.preciseY = (float)round / 4;
                expected.wheel.y = source.wheel.preciseY;
            } else if (source.type == SDL_CONTROLLERDEVICEREMOVED) {
                source.cdevice.which = (Sint32)(rng & 0x7FFFFFFF);
                expected.gdevice.which = source.cdevice.which;
            } else if (source.type == SDL_CONTROLLERBUTTONDOWN) {
                source.cbutton.which = (Sint32)(rng & 0x7FFFFFFF);
                expected.gdevice.which = source.cbutton.which;
            } else if (source.type == SDL_CONTROLLERAXISMOTION) {
                source.caxis.which = 123;
                source.caxis.axis = SDL_CONTROLLER_AXIS_RIGHTY;
                source.caxis.value = (Sint16)(rng >> 16);
                expected.gaxis.which = 123;
                expected.gaxis.axis = PLATFORM_GAMEPAD_AXIS_RIGHTY;
                expected.gaxis.value = source.caxis.value;
            } else if (source.type == SDL_FINGERDOWN || source.type == SDL_FINGERUP || source.type == SDL_FINGERMOTION) {
                source.tfinger.fingerId = (SDL_FingerID)rng;
                source.tfinger.x = (float)round / 40;
                source.tfinger.y = 0.75f;
                expected.tfinger.fingerID = (uint64_t)source.tfinger.fingerId;
                expected.tfinger.x = source.tfinger.x;
                expected.tfinger.y = source.tfinger.y;
            } else if (source.type == SDL_TEXTINPUT) {
                strcpy(source.text.text, "native text");
                expected.text.text = source.text.text;
            }
            memset(&actual, 0xCC, sizeof(actual));
            ASSERT_TRUE(SDL2_TranslateEvent(&source, &actual));
            ASSERT_MEM_EQ(&expected, &actual, sizeof(actual));
        }
    }
    const Uint8 events[] = {SDL_WINDOWEVENT_FOCUS_GAINED, SDL_WINDOWEVENT_FOCUS_LOST,
                            SDL_WINDOWEVENT_ENTER, SDL_WINDOWEVENT_SIZE_CHANGED};
    const PlatformEventType expected[] = {PLATFORM_EVENT_WINDOW_FOCUS_GAINED,
                                          PLATFORM_EVENT_WINDOW_FOCUS_LOST, PLATFORM_EVENT_WINDOW_MOUSE_ENTER, PLATFORM_EVENT_WINDOW_RESIZED};
    for (int i = 0; i < 4; ++i) {
        SDL_Event source = {};
        source.type = SDL_WINDOWEVENT;
        source.window.event = events[i];
        PlatformEvent actual;
        ASSERT_TRUE(SDL2_TranslateEvent(&source, &actual));
        ASSERT_EQ_INT(expected[i], actual.type);
    }
}

struct LogCapture {
    int count;
    PlatformLogLevel level;
};
static Uint32 messageFlags;
static bool failMessage;
static int message_probe(Uint32 flags, const char *title, const char *message, SDL_Window *) {
    messageFlags = flags;
    ASSERT_TRUE(strcmp(title, "PAL title") == 0);
    ASSERT_TRUE(strcmp(message, "PAL message") == 0);
    return failMessage ? SDL_SetError("message fixture") : 0;
}
static void capture(void *userdata, PlatformLogLevel level, const char *message) {
    LogCapture *log = (LogCapture *)userdata;
    ++log->count;
    log->level = level;
    ASSERT_TRUE(strcmp(message, "log fixture") == 0);
}
static void test_directory_parent_prefixes() {
    struct Case {
        const char *path;
        bool windows;
        const char *root;
        const char *first;
        const char *second;
        bool allowDevicePrefix;
    };
    const Case cases[] = {
        {"\\\\server\\share\\profile\\saves", true, "\\\\server\\share", "\\\\server\\share\\profile", "\\\\server\\share\\profile\\saves"},
        {"//server/share/profile/saves", true, "//server/share", "//server/share/profile", "//server/share/profile/saves"},
        {"\\\\server/share\\profile/saves", true, "\\\\server/share", "\\\\server/share\\profile", "\\\\server/share\\profile/saves"},
        {"\\\\server\\share", true, "\\\\server\\share", NULL, NULL},
        {"\\\\server\\share\\", true, "\\\\server\\share", NULL, NULL},
        {"C:\\profile\\saves", true, "C:\\", "C:\\profile", "C:\\profile\\saves"},
        {"C:/profile/saves", true, "C:/", "C:/profile", "C:/profile/saves"},
        {"C:profile\\saves", true, "C:", "C:profile", "C:profile\\saves"},
        {"profile\\saves", true, "", "profile", "profile\\saves"},
        {"\\profile\\saves", true, "\\", "\\profile", "\\profile\\saves"},
        {"/profile/saves", false, "/", "/profile", "/profile/saves"},
        {"//profile/saves", false, "/", "//profile", "//profile/saves"},
        {"profile/saves/", false, "", "profile", "profile/saves"},
        {"profile\\saves", false, "", "profile\\saves", NULL},
        {"/", false, "/", NULL, NULL},
        {"C:\\", true, "C:\\", NULL, NULL},
        {"profile//saves", false, "", "profile", "profile//saves"},
        {"\\\\server\\share\\\\profile\\saves\\", true, "\\\\server\\share", "\\\\server\\share\\\\profile", "\\\\server\\share\\\\profile\\saves"},
        {"sdmc:/switch/lba2cc", false, "sdmc:/", "sdmc:/switch", "sdmc:/switch/lba2cc", true},
        {"sdmc:/", false, "sdmc:/", NULL, NULL, true},
        {"sdmc://switch//lba2cc/", false, "sdmc:/", "sdmc://switch", "sdmc://switch//lba2cc", true},
        {"romfs:/data", false, "romfs:/", "romfs:/data", NULL, true},
        {"/switch/lba2cc", false, "/", "/switch", "/switch/lba2cc", true},
        {"switch/lba2cc", false, "", "switch", "switch/lba2cc", true},
        {"sdmc:/switch", false, "", "sdmc:", "sdmc:/switch", false},
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
        const Case &fixture = cases[c];
        size_t cursor = SDL2_DirectoryRootLength(fixture.path, fixture.windows, fixture.allowDevicePrefix);
        ASSERT_TRUE(std::string(fixture.path, cursor) == fixture.root);
        std::vector<std::string> prefixes;
        size_t end;
        while ((end = SDL2_NextDirectoryPrefix(fixture.path, &cursor, fixture.windows)) != 0)
            prefixes.push_back(std::string(fixture.path, end));
        const size_t expected = (fixture.first ? 1 : 0) + (fixture.second ? 1 : 0);
        ASSERT_EQ_UINT(expected, prefixes.size());
        if (prefixes.size() == expected) {
            if (fixture.first)
                ASSERT_TRUE(prefixes[0] == fixture.first);
            if (fixture.second)
                ASSERT_TRUE(prefixes[1] == fixture.second);
        }
    }
}

static void test_host_services_and_diagnostics() {
    ASSERT_TRUE(g_platform.host.base_path() != NULL);
    ASSERT_TRUE(g_platform.host.base_path() == g_platform.host.base_path());
    char *cwd = g_platform.host.current_directory();
    ASSERT_TRUE(cwd && cwd[0]);
    g_platform.host.free_memory(cwd);
    ASSERT_TRUE(g_platform.host.home_path() && g_platform.host.home_path()[0]);
    ASSERT_TRUE(g_platform.host.pref_path("PAL", NULL) == NULL);
    SDL2_SetMessageProbe(message_probe);
    const PlatformMessageKind kinds[] = {PLATFORM_MESSAGE_INFORMATION, PLATFORM_MESSAGE_WARNING, PLATFORM_MESSAGE_ERROR};
    const Uint32 flags[] = {SDL_MESSAGEBOX_INFORMATION, SDL_MESSAGEBOX_WARNING, SDL_MESSAGEBOX_ERROR};
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(g_platform.host.message_box(kinds[i], "PAL title", "PAL message"));
        ASSERT_EQ_UINT(flags[i], messageFlags);
    }
    failMessage = true;
    ASSERT_TRUE(!g_platform.host.message_box(PLATFORM_MESSAGE_ERROR, "PAL title", "PAL message"));
    ASSERT_TRUE(strcmp(g_platform.system.get_error(), "message fixture") == 0);
    SDL2_SetMessageProbe(NULL);
    char folder[128], child[160];
    snprintf(folder, sizeof(folder), "sdl2-host-%llu", (unsigned long long)SDL_GetPerformanceCounter());
    snprintf(child, sizeof(child), "%s/child", folder);
    ASSERT_TRUE(g_platform.host.create_directory(child));
    ASSERT_TRUE(g_platform.host.create_directory(child));
    ASSERT_TRUE(g_platform.host.create_directory("."));
    char *base = g_platform.host.current_directory();
    ASSERT_TRUE(base != NULL);
    if (base) {
        char *root = SDL_strdup(base);
        ASSERT_TRUE(root != NULL);
        if (root) {
            const bool windows = strcmp(SDL_GetPlatform(), "Windows") == 0;
            root[SDL2_DirectoryRootLength(root, windows)] = '\0';
            if (*root)
                ASSERT_TRUE(g_platform.host.create_directory(root));
            g_platform.host.free_memory(root);
        }
        g_platform.host.free_memory(base);
    }
    ASSERT_TRUE(!g_platform.host.remove_path(folder));
    ASSERT_TRUE(g_platform.host.remove_path(child));
    ASSERT_TRUE(g_platform.host.remove_path(folder));
    ASSERT_TRUE(g_platform.host.remove_path(folder));
    ASSERT_TRUE(g_platform.host.begin_folder_dialog(NULL) == NULL);
    ASSERT_TRUE(strstr(g_platform.system.get_error(), "folder-dialog") != NULL);
    ASSERT_EQ_INT(PLATFORM_DIALOG_ERROR, g_platform.host.poll_folder_dialog(NULL, NULL));
    g_platform.host.end_folder_dialog(NULL);
    ASSERT_TRUE(g_platform.window.init("Clipboard fixture"));
    ASSERT_TRUE(g_platform.host.set_clipboard("SDL2 clipboard"));
    char *text = g_platform.host.get_clipboard();
    ASSERT_TRUE(text && strcmp(text, "SDL2 clipboard") == 0);
    g_platform.host.free_memory(text);
    g_platform.window.shutdown();
    ASSERT_TRUE(strcmp(SDL_GetPlatform(), g_platform.system.platform_name()) == 0);
    ASSERT_EQ_INT(SDL_GetCPUCount(), g_platform.system.cpu_count());
    ASSERT_EQ_INT(SDL_GetSystemRAM(), g_platform.system.ram_mb());
    ASSERT_EQ_UINT(SDL_ThreadID(), g_platform.system.thread_id());
    ASSERT_EQ_UINT(SDL_GetPerformanceFrequency(), g_platform.system.performance_frequency());
    uint64_t ticks = g_platform.system.ticks_ms(), counter = g_platform.system.performance_counter();
    g_platform.system.delay_ms(3);
    ASSERT_TRUE(g_platform.system.ticks_ms() >= ticks + 3);
    ASSERT_TRUE(g_platform.system.performance_counter() > counter);
    g_platform.system.clear_error();
    ASSERT_TRUE(g_platform.system.get_error()[0] == '\0');
    LogCapture log = {};
    Platform_SetLogOutput(capture, &log);
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
    const SDL_LogPriority priorities[] = {SDL_LOG_PRIORITY_DEBUG, SDL_LOG_PRIORITY_INFO, SDL_LOG_PRIORITY_WARN, SDL_LOG_PRIORITY_ERROR};
    for (int i = 0; i < 4; ++i) {
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priorities[i], "log fixture");
        ASSERT_EQ_INT(i + 1, log.count);
        ASSERT_EQ_INT(i, log.level);
    }
    Platform_SetLogOutput(NULL, NULL);
}

int main() {
    SDL_SetMainReady();
    SDL_SetHintWithPriority(SDL_HINT_VIDEODRIVER, "dummy", SDL_HINT_OVERRIDE);
    ASSERT_TRUE(strcmp(g_platform.name, "sdl2") == 0);
    ASSERT_TRUE(g_platform.system.set_headless());
    ASSERT_TRUE(g_platform.system.init());
    ASSERT_TRUE(g_platform.events.init());
    RUN_TEST(test_window_frames_modes_and_lifetime);
    RUN_TEST(test_present_filter_and_surface_recreation);
    RUN_TEST(test_event_payloads_and_borrowed_text);
    RUN_TEST(test_keyboard_and_gamepad_instance_ids);
    RUN_TEST(test_translated_event_union_bytes);
    RUN_TEST(test_directory_parent_prefixes);
    RUN_TEST(test_host_services_and_diagnostics);
    g_platform.events.shutdown();
    ASSERT_EQ_UINT(0, SDL_WasInit(SDL_INIT_EVENTS));
    g_platform.system.shutdown();
    ASSERT_EQ_UINT(0, SDL_WasInit(0));
    TEST_SUMMARY();
    return test_failures != 0;
}
