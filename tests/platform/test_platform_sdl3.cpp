#include "../test_harness.h"
#include "../../LIB386/PLATFORM/SDL3/PRIVATE.H"

#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/WINDOW.H>

#include <vector>

extern "C" {
void *Log = NULL;
void *Screen = NULL;
U32 ModeDesiredX = 32;
U32 ModeDesiredY = 24;
U32 TabOffLine[ADELINE_MAX_Y_RES];
S32 BoxCleanClip = 0;
U8 BinGphMouse[4096];
U8 Font8x8[256 * 8];
U32 SizeChar = 8;
S32 CubeMode = 0;
S32 GamepadCameraAnalog = 0;
S32 FollowCamera = 0;
void BoxStaticAdd(S32, S32, S32, S32) {}
S32 IsAndroidTVDevice() { return 0; }
}

static U32 rng = 0xDEADBEEF;
static U32 next_random() {
    rng = rng * 1103515245u + 12345u;
    return (rng >> 16) & 0x7FFF;
}

static std::vector<U32> rendered;
static void read_presentation(SDL_Renderer *renderer) {
    SDL_Surface *surface = SDL_RenderReadPixels(renderer, NULL);
    ASSERT_TRUE(surface != NULL);
    if (!surface)
        return;
    SDL_Surface *argb = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ARGB8888);
    ASSERT_TRUE(argb != NULL);
    if (argb) {
        ASSERT_EQ_INT(ModeDesiredX, argb->w);
        ASSERT_EQ_INT(ModeDesiredY, argb->h);
        rendered.resize((size_t)argb->w * argb->h);
        for (int y = 0; y < argb->h; ++y)
            memcpy(&rendered[(size_t)y * argb->w],
                   (const U8 *)argb->pixels + (size_t)y * argb->pitch,
                   (size_t)argb->w * sizeof(U32));
        SDL_DestroySurface(argb);
    }
    SDL_DestroySurface(surface);
}

static void test_complete_backend_and_key_identity() {
    ASSERT_TRUE(strcmp(g_platform.name, "sdl3") == 0);
#define REQUIRE_CALLBACK(member) ASSERT_TRUE(g_platform.member != NULL)
    REQUIRE_CALLBACK(system.init);
    REQUIRE_CALLBACK(system.shutdown);
    REQUIRE_CALLBACK(system.set_headless);
    REQUIRE_CALLBACK(system.get_error);
    REQUIRE_CALLBACK(system.clear_error);
    REQUIRE_CALLBACK(system.ticks_ms);
    REQUIRE_CALLBACK(system.delay_ms);
    REQUIRE_CALLBACK(system.performance_counter);
    REQUIRE_CALLBACK(system.performance_frequency);
    REQUIRE_CALLBACK(system.thread_id);
    REQUIRE_CALLBACK(system.cpu_count);
    REQUIRE_CALLBACK(system.ram_mb);
    REQUIRE_CALLBACK(system.platform_name);
    REQUIRE_CALLBACK(system.init_video);
    REQUIRE_CALLBACK(system.video_initialized);
    REQUIRE_CALLBACK(system.set_log_output);
    REQUIRE_CALLBACK(system.android_environment);
    REQUIRE_CALLBACK(system.android_activity);
    REQUIRE_CALLBACK(events.init);
    REQUIRE_CALLBACK(events.shutdown);
    REQUIRE_CALLBACK(events.pump);
    REQUIRE_CALLBACK(events.poll);
    REQUIRE_CALLBACK(window.init);
    REQUIRE_CALLBACK(window.shutdown);
    REQUIRE_CALLBACK(window.initialized);
    REQUIRE_CALLBACK(window.create);
    REQUIRE_CALLBACK(window.destroy);
    REQUIRE_CALLBACK(window.resize);
    REQUIRE_CALLBACK(window.size);
    REQUIRE_CALLBACK(window.display_size);
    REQUIRE_CALLBACK(window.primary_display_size);
    REQUIRE_CALLBACK(window.surface_coords);
    REQUIRE_CALLBACK(window.set_fullscreen);
    REQUIRE_CALLBACK(window.get_fullscreen);
    REQUIRE_CALLBACK(window.is_fullscreen);
    REQUIRE_CALLBACK(window.set_vsync);
    REQUIRE_CALLBACK(window.set_relative_mouse);
    REQUIRE_CALLBACK(window.handle_event);
    REQUIRE_CALLBACK(window.present);
    REQUIRE_CALLBACK(input.keyboard_state);
    REQUIRE_CALLBACK(input.keycode);
    REQUIRE_CALLBACK(input.scancode_name);
    REQUIRE_CALLBACK(input.modifiers);
    REQUIRE_CALLBACK(input.text_input);
    REQUIRE_CALLBACK(input.cursor_visible);
    REQUIRE_CALLBACK(input.init_gamepads);
    REQUIRE_CALLBACK(input.shutdown_gamepads);
    REQUIRE_CALLBACK(input.gamepads);
    REQUIRE_CALLBACK(input.open_gamepad);
    REQUIRE_CALLBACK(input.close_gamepad);
    REQUIRE_CALLBACK(input.gamepad_name);
    REQUIRE_CALLBACK(input.gamepad_button);
    REQUIRE_CALLBACK(input.gamepad_axis);
    REQUIRE_CALLBACK(host.base_path);
    REQUIRE_CALLBACK(host.current_directory);
    REQUIRE_CALLBACK(host.pref_path);
    REQUIRE_CALLBACK(host.home_path);
    REQUIRE_CALLBACK(host.create_directory);
    REQUIRE_CALLBACK(host.remove_path);
    REQUIRE_CALLBACK(host.message_box);
    REQUIRE_CALLBACK(host.begin_folder_dialog);
    REQUIRE_CALLBACK(host.poll_folder_dialog);
    REQUIRE_CALLBACK(host.end_folder_dialog);
    REQUIRE_CALLBACK(host.set_clipboard);
    REQUIRE_CALLBACK(host.get_clipboard);
    REQUIRE_CALLBACK(host.free_memory);
    REQUIRE_CALLBACK(host.strcasecmp);
    REQUIRE_CALLBACK(host.strncasecmp);
    REQUIRE_CALLBACK(host.strupr);
    REQUIRE_CALLBACK(host.strlwr);
#undef REQUIRE_CALLBACK
    ASSERT_EQ_INT(SDL_SCANCODE_A, PLATFORM_SCANCODE_A);
    ASSERT_EQ_INT(SDL_SCANCODE_Z, PLATFORM_SCANCODE_Z);
    ASSERT_EQ_INT(SDL_SCANCODE_KP_PERIOD, PLATFORM_SCANCODE_KP_PERIOD);
    ASSERT_EQ_INT(SDL_SCANCODE_KP_DECIMAL, PLATFORM_SCANCODE_KP_DECIMAL);
    ASSERT_EQ_INT(SDL_SCANCODE_RGUI, PLATFORM_SCANCODE_RGUI);
    ASSERT_EQ_UINT(SDLK_KP_ENTER, PLATFORM_KEYCODE_KP_ENTER);
    ASSERT_EQ_UINT(SDLK_DELETE, PLATFORM_KEYCODE_DELETE);
    ASSERT_EQ_UINT(SDL_KMOD_CTRL, PLATFORM_KMOD_CTRL);
    ASSERT_EQ_UINT(SDL_KMOD_SHIFT, PLATFORM_KMOD_SHIFT);
    ASSERT_EQ_UINT(SDL_KMOD_ALT, PLATFORM_KMOD_ALT);
    for (int code = 0; code < PLATFORM_SCANCODE_COUNT; ++code) {
        ASSERT_EQ_UINT(SDL_GetKeyFromScancode((SDL_Scancode)code, SDL_KMOD_NONE, true),
                       g_platform.input.keycode(code, PLATFORM_KMOD_NONE));
        ASSERT_TRUE(strcmp(SDL_GetScancodeName((SDL_Scancode)code),
                           g_platform.input.scancode_name(code)) == 0);
    }
    int count = 0;
    const bool *native = SDL_GetKeyboardState(&count);
    const U8 *state = g_platform.input.keyboard_state();
    for (int i = 0; i < PLATFORM_SCANCODE_COUNT; ++i)
        ASSERT_EQ_UINT(i < count && native[i] ? 1 : 0, state[i]);
}

static void test_event_translation_exact_fields() {
    const SDL_EventType types[] = {
        SDL_EVENT_QUIT, SDL_EVENT_WINDOW_FOCUS_GAINED,
        SDL_EVENT_WINDOW_FOCUS_LOST, SDL_EVENT_WINDOW_MOUSE_ENTER,
        SDL_EVENT_WINDOW_RESIZED, SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP,
        SDL_EVENT_TEXT_INPUT, SDL_EVENT_MOUSE_MOTION,
        SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP,
        SDL_EVENT_MOUSE_WHEEL, SDL_EVENT_GAMEPAD_ADDED,
        SDL_EVENT_GAMEPAD_REMOVED, SDL_EVENT_GAMEPAD_BUTTON_DOWN,
        SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_EVENT_FINGER_DOWN,
        SDL_EVENT_FINGER_UP, SDL_EVENT_FINGER_MOTION};
    const PlatformEventType neutral[] = {
        PLATFORM_EVENT_QUIT, PLATFORM_EVENT_WINDOW_FOCUS_GAINED,
        PLATFORM_EVENT_WINDOW_FOCUS_LOST, PLATFORM_EVENT_WINDOW_MOUSE_ENTER,
        PLATFORM_EVENT_WINDOW_RESIZED, PLATFORM_EVENT_KEY_DOWN, PLATFORM_EVENT_KEY_UP,
        PLATFORM_EVENT_TEXT_INPUT, PLATFORM_EVENT_MOUSE_MOTION,
        PLATFORM_EVENT_MOUSE_BUTTON_DOWN, PLATFORM_EVENT_MOUSE_BUTTON_UP,
        PLATFORM_EVENT_MOUSE_WHEEL, PLATFORM_EVENT_GAMEPAD_ADDED,
        PLATFORM_EVENT_GAMEPAD_REMOVED, PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN,
        PLATFORM_EVENT_GAMEPAD_AXIS_MOTION, PLATFORM_EVENT_FINGER_DOWN,
        PLATFORM_EVENT_FINGER_UP, PLATFORM_EVENT_FINGER_MOTION};
    for (int round = 0; round < 40; ++round) {
        for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
            SDL_Event source;
            PlatformEvent expected, actual;
            memset(&source, 0, sizeof(source));
            memset(&expected, 0, sizeof(expected));
            memset(&actual, 0xCD, sizeof(actual));
            source.type = types[i];
            expected.type = neutral[i];
            if (types[i] == SDL_EVENT_KEY_DOWN || types[i] == SDL_EVENT_KEY_UP) {
                source.key.scancode = SDL_SCANCODE_RSHIFT;
                source.key.mod = SDL_KMOD_SHIFT;
                source.key.repeat = (round & 1) != 0;
                expected.key.scancode = PLATFORM_SCANCODE_RSHIFT;
                expected.key.mod = PLATFORM_KMOD_SHIFT;
                expected.key.repeat = source.key.repeat;
            } else if (types[i] == SDL_EVENT_TEXT_INPUT) {
                source.text.text = "AbC";
                expected.text.text = source.text.text;
            } else if (types[i] == SDL_EVENT_MOUSE_MOTION) {
                source.motion.x = (float)next_random() / 4;
                source.motion.y = (float)next_random() / 4;
                source.motion.xrel = (float)(S32)next_random() - 16000;
                source.motion.yrel = (float)(S32)next_random() - 16000;
                expected.motion.x = source.motion.x;
                expected.motion.y = source.motion.y;
                expected.motion.xrel = source.motion.xrel;
                expected.motion.yrel = source.motion.yrel;
            } else if (types[i] == SDL_EVENT_MOUSE_BUTTON_DOWN || types[i] == SDL_EVENT_MOUSE_BUTTON_UP) {
                source.button.button = SDL_BUTTON_RIGHT;
                source.button.x = (float)next_random();
                source.button.y = (float)next_random();
                expected.button.button = PLATFORM_BUTTON_RIGHT;
                expected.button.x = source.button.x;
                expected.button.y = source.button.y;
            } else if (types[i] == SDL_EVENT_MOUSE_WHEEL) {
                source.wheel.y = (float)(S32)next_random() / 4 - 4000;
                expected.wheel.y = source.wheel.y;
            } else if (types[i] == SDL_EVENT_GAMEPAD_ADDED || types[i] == SDL_EVENT_GAMEPAD_REMOVED) {
                source.gdevice.which = next_random();
                expected.gdevice.which = source.gdevice.which;
            } else if (types[i] == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                source.gbutton.which = next_random();
                expected.gdevice.which = source.gbutton.which;
            } else if (types[i] == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
                source.gaxis.which = next_random();
                source.gaxis.axis = SDL_GAMEPAD_AXIS_RIGHTY;
                source.gaxis.value = (S16)((S32)next_random() - 16000);
                expected.gaxis.which = source.gaxis.which;
                expected.gaxis.axis = PLATFORM_GAMEPAD_AXIS_RIGHTY;
                expected.gaxis.value = source.gaxis.value;
            } else if (types[i] == SDL_EVENT_FINGER_DOWN || types[i] == SDL_EVENT_FINGER_UP ||
                       types[i] == SDL_EVENT_FINGER_MOTION) {
                source.tfinger.fingerID = UINT64_C(0x1234567800000000) + next_random();
                source.tfinger.x = (float)next_random() / 32768;
                source.tfinger.y = (float)next_random() / 32768;
                expected.tfinger.fingerID = source.tfinger.fingerID;
                expected.tfinger.x = source.tfinger.x;
                expected.tfinger.y = source.tfinger.y;
            }
            ASSERT_TRUE(SDL3_TranslateEvent(&source, &actual));
            ASSERT_MEM_EQ(&expected, &actual, sizeof(actual));
        }
    }
    SDL_Event unsupported = {};
    unsupported.type = SDL_EVENT_USER;
    PlatformEvent event;
    ASSERT_TRUE(SDL3_TranslateEvent(&unsupported, &event));
    ASSERT_EQ_INT(PLATFORM_EVENT_OTHER, event.type);
}

static void test_window_mapping_and_capabilities() {
    ASSERT_TRUE(CreateWindowSurface(1280, 720, false));
    ASSERT_TRUE(ResizeWindowSurface(640, 480, true));
    S32 w = 0, h = 0;
    ASSERT_TRUE(g_platform.window.size(&w, &h));
    ASSERT_EQ_INT(1280, w);
    ASSERT_EQ_INT(720, h);
    S32 x = 0, y = 0;
    WindowToSurfaceCoords(640, 360, &x, &y);
    ASSERT_EQ_INT(320, x);
    ASSERT_EQ_INT(240, y);
    WindowToSurfaceCoords(0, 0, &x, &y);
    ASSERT_EQ_INT(0, x);
    ASSERT_EQ_INT(0, y);
    WindowToSurfaceCoords(1279, 719, &x, &y);
    ASSERT_EQ_INT(639, x);
    ASSERT_EQ_INT(479, y);
    WindowToSurfaceCoords(-50, -50, &x, &y);
    ASSERT_EQ_INT(0, x);
    ASSERT_EQ_INT(0, y);
    U32 primaryWidth = 123, primaryHeight = 456;
    ASSERT_TRUE(!Window_GetPrimaryDisplayDimensions(&primaryWidth, &primaryHeight));
    ASSERT_EQ_UINT(123, primaryWidth);
    ASSERT_EQ_UINT(456, primaryHeight);
    ASSERT_TRUE(ResizeWindowSurface(32, 24, false));
    g_platform.events.pump();
    ASSERT_TRUE(g_platform.window.size(&w, &h));
    ASSERT_EQ_INT(32, w);
    ASSERT_EQ_INT(24, h);
}

static void update_reference_palette(SDL_Palette *palette, const U8 *source, S32 start, S32 count) {
    static SDL_Color temporary[256];
    if (start < 0)
        start = 0;
    for (S32 i = start; i < count; ++i) {
        temporary[i].r = *source++;
        temporary[i].g = *source++;
        temporary[i].b = *source++;
        temporary[i].a = 255;
    }
    SDL_SetPaletteColors(palette, temporary, start, count);
}

static void test_palette_and_actual_renderer_bytes() {
    SDL_Palette *reference = SDL_CreatePalette(256);
    ASSERT_TRUE(reference != NULL);
    if (!reference)
        return;
    ASSERT_TRUE(CreateVideoSurface(ModeDesiredX, ModeDesiredY));
    std::vector<U8> indexed((size_t)ModeDesiredX * ModeDesiredY);
    std::vector<U32> expected(indexed.size());
    Log = &indexed[0];
    SDL3_SetPresentationProbe(read_presentation);
    for (int round = 0; round < 40; ++round) {
        U8 palette[768];
        for (size_t i = 0; i < sizeof(palette); ++i)
            palette[i] = (U8)next_random();
        if (round > 0) {
            S32 start = round == 1 ? 0 : (S32)(next_random() % 64);
            S32 count = round == 1 ? 256 : (S32)(next_random() % (256 - start));
            SetVideoPalette(palette, start, count);
            update_reference_palette(reference, palette, start, count);
        }
        if (round == 3) {
            SetVideoPaletteCol(7, 3, 5, 9);
            SDL_Color color = {3, 5, 9, 255};
            ASSERT_TRUE(SDL_SetPaletteColors(reference, &color, 7, 1));
        }
        for (size_t i = 0; i < indexed.size(); ++i) {
            indexed[i] = (U8)next_random();
            SDL_Color color = reference->colors[indexed[i]];
            expected[i] = 0xFF000000 | ((U32)color.r << 16) | ((U32)color.g << 8) | color.b;
        }
        T_BOX area = {0, 0, (S16)ModeDesiredX, (S16)ModeDesiredY};
        LockVideoSurface();
        CopyVideoArea(Phys, Log, TabOffPhysLine, &area);
        UnlockVideoSurface();
        ASSERT_EQ_UINT(expected.size(), rendered.size());
        if (expected.size() == rendered.size())
            ASSERT_MEM_EQ(&expected[0], &rendered[0], expected.size() * sizeof(U32));
    }
    SDL3_SetPresentationProbe(NULL);
    ASSERT_TRUE(ResizeWindowSurface(64, 16, false));
    ASSERT_TRUE(ResizeVideoSurface(64, 16));
    ModeDesiredX = 64;
    ModeDesiredY = 16;
    indexed.assign(64 * 16, 7);
    Log = &indexed[0];
    PlatformFrame frame = {&indexed[0], 64, 16, 64, NULL, NULL};
    U8 palette[768] = {};
    frame.palette_rgb = palette;
    ASSERT_TRUE(g_platform.window.present(&frame));
    DestroyVideoSurface();
    SDL_DestroyPalette(reference);
    Log = NULL;
}

static void test_host_ownership_and_time() {
    const char *base = g_platform.host.base_path();
    ASSERT_TRUE(base != NULL);
    ASSERT_TRUE(strcmp(base, SDL_GetBasePath()) == 0);
    char *cwd = g_platform.host.current_directory();
    char *nativeCwd = SDL_GetCurrentDirectory();
    ASSERT_TRUE(cwd != NULL && nativeCwd != NULL);
    if (cwd && nativeCwd)
        ASSERT_TRUE(strcmp(cwd, nativeCwd) == 0);
    g_platform.host.free_memory(cwd);
    SDL_free(nativeCwd);
    ASSERT_TRUE(g_platform.host.set_clipboard("PAL fixture"));
    char *clipboard = g_platform.host.get_clipboard();
    ASSERT_TRUE(clipboard != NULL);
    if (clipboard)
        ASSERT_TRUE(strcmp(clipboard, "PAL fixture") == 0);
    g_platform.host.free_memory(clipboard);
    ASSERT_EQ_INT(SDL_GetNumLogicalCPUCores(), g_platform.system.cpu_count());
    ASSERT_EQ_INT(SDL_GetSystemRAM(), g_platform.system.ram_mb());
    ASSERT_EQ_UINT(SDL_GetCurrentThreadID(), g_platform.system.thread_id());
    ASSERT_EQ_UINT(SDL_GetPerformanceFrequency(), g_platform.system.performance_frequency());
    U64 start = g_platform.system.ticks_ms();
    g_platform.system.delay_ms(2);
    ASSERT_TRUE(g_platform.system.ticks_ms() >= start + 2);
    ASSERT_TRUE(g_platform.system.performance_counter() > 0);
    SDL_SetError("PAL error fixture");
    ASSERT_TRUE(strcmp(g_platform.system.get_error(), "PAL error fixture") == 0);
    g_platform.system.clear_error();
    ASSERT_TRUE(g_platform.system.get_error()[0] == '\0');
}

static void test_gamepad_button_axis_and_handle_mapping() {
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1", SDL_HINT_OVERRIDE);
    ASSERT_TRUE(g_platform.input.init_gamepads());
    SDL_VirtualJoystickDesc description;
    SDL_INIT_INTERFACE(&description);
    description.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    description.naxes = SDL_GAMEPAD_AXIS_COUNT;
    description.nbuttons = PLATFORM_GAMEPAD_BUTTON_COUNT;
    description.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    description.button_mask = (1u << PLATFORM_GAMEPAD_BUTTON_COUNT) - 1;
    description.name = "PAL virtual gamepad";
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&description);
    ASSERT_TRUE(id != 0);
    if (!id)
        return;
    SDL_Joystick *joystick = SDL_OpenJoystick(id);
    PlatformGamepad *pad = g_platform.input.open_gamepad(id);
    SDL_Gamepad *reference = SDL_OpenGamepad(id);
    ASSERT_TRUE(joystick != NULL && pad != NULL && reference != NULL);
    if (joystick && pad && reference) {
        S32 count = 0;
        U32 *ids = g_platform.input.gamepads(&count);
        bool listed = false;
        for (S32 i = 0; ids && i < count; ++i)
            listed = listed || ids[i] == id;
        ASSERT_TRUE(listed);
        g_platform.host.free_memory(ids);
        ASSERT_TRUE(strcmp(SDL_GetGamepadName(reference), g_platform.input.gamepad_name(pad)) == 0);
        for (int round = 0; round < 40; ++round) {
            for (int button = 0; button < PLATFORM_GAMEPAD_BUTTON_COUNT; ++button)
                ASSERT_TRUE(SDL_SetJoystickVirtualButton(joystick, button, (next_random() & 1) != 0));
            for (int axis = 0; axis < PLATFORM_GAMEPAD_AXIS_COUNT; ++axis)
                ASSERT_TRUE(SDL_SetJoystickVirtualAxis(joystick, axis, (S16)((S32)next_random() * 2 - 32768)));
            SDL_UpdateJoysticks();
            for (int button = 0; button < PLATFORM_GAMEPAD_BUTTON_COUNT; ++button)
                ASSERT_EQ_INT(SDL_GetGamepadButton(reference, (SDL_GamepadButton)button),
                              g_platform.input.gamepad_button(pad, (PlatformGamepadButton)button));
            for (int axis = 0; axis < PLATFORM_GAMEPAD_AXIS_COUNT; ++axis)
                ASSERT_EQ_INT(SDL_GetGamepadAxis(reference, (SDL_GamepadAxis)axis),
                              g_platform.input.gamepad_axis(pad, (PlatformGamepadAxis)axis));
        }
    }
    if (pad)
        g_platform.input.close_gamepad(pad);
    if (reference)
        SDL_CloseGamepad(reference);
    if (joystick)
        SDL_CloseJoystick(joystick);
    ASSERT_TRUE(SDL_DetachVirtualJoystick(id));
    g_platform.input.shutdown_gamepads();
}

struct DialogCompletion {
    PlatformFolderDialog *dialog;
    int result;
};
static int SDLCALL complete_dialog(void *userdata) {
    DialogCompletion *completion = (DialogCompletion *)userdata;
    if (completion->result == 0) {
        const char *files[] = {"fixture/path \n", NULL};
        SDL3_FolderChosen(completion->dialog, files, 0);
    } else if (completion->result == 1) {
        const char *files[] = {NULL};
        SDL3_FolderChosen(completion->dialog, files, 0);
    } else {
        SDL_SetError("worker-side dialog error");
        SDL3_FolderChosen(completion->dialog, NULL, 0);
    }
    return 0;
}
static void test_dialog_worker_result_and_error_ownership() {
    for (int kind = 0; kind < 3; ++kind) {
        PlatformFolderDialog *dialog = SDL3_AllocateFolderDialog();
        ASSERT_TRUE(dialog != NULL);
        if (!dialog)
            continue;
        ASSERT_EQ_INT(PLATFORM_DIALOG_PENDING, g_platform.host.poll_folder_dialog(dialog, NULL));
        DialogCompletion completion = {dialog, kind};
        SDL_Thread *worker = SDL_CreateThread(complete_dialog, "pal-dialog-fixture", &completion);
        ASSERT_TRUE(worker != NULL);
        if (!worker) {
            complete_dialog(&completion);
        } else {
            SDL_WaitThread(worker, NULL);
        }
        const char *path = NULL;
        PlatformDialogResult result = g_platform.host.poll_folder_dialog(dialog, &path);
        if (kind == 0) {
            ASSERT_EQ_INT(PLATFORM_DIALOG_SELECTED, result);
            ASSERT_TRUE(strcmp(path, "fixture/path \n") == 0);
        } else if (kind == 1) {
            ASSERT_EQ_INT(PLATFORM_DIALOG_CANCELLED, result);
            ASSERT_TRUE(path == NULL);
        } else {
            ASSERT_EQ_INT(PLATFORM_DIALOG_ERROR, result);
            ASSERT_TRUE(strcmp(g_platform.system.get_error(), "worker-side dialog error") == 0);
        }
        g_platform.host.end_folder_dialog(dialog);
    }
}

int main() {
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    if (!g_platform.system.set_headless() || !g_platform.system.init() ||
        !g_platform.events.init() || !InitWindow("SDL3 PAL fixture")) {
        fprintf(stderr, "Platform init failed: %s\n", g_platform.system.get_error());
        return 1;
    }
    RUN_TEST(test_complete_backend_and_key_identity);
    RUN_TEST(test_event_translation_exact_fields);
    RUN_TEST(test_window_mapping_and_capabilities);
    RUN_TEST(test_palette_and_actual_renderer_bytes);
    RUN_TEST(test_host_ownership_and_time);
    RUN_TEST(test_gamepad_button_axis_and_handle_mapping);
    RUN_TEST(test_dialog_worker_result_and_error_ownership);
    EndWindow();
    g_platform.events.shutdown();
    g_platform.system.shutdown();
    TEST_SUMMARY();
    return test_failures != 0;
}
