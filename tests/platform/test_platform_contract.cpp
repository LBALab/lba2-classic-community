/* An SDL-free link test for the real engine facades and input policy.
   The table below is a test fixture, not an additional selectable backend. */
#include "../test_harness.h"

#include <PLATFORM/PLATFORM.H>
#include <SVGA/DIRTYBOX.H>
#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/EVENTS.H>
#include <SYSTEM/INPUT.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/MOUSE.H>
#include <SYSTEM/TIMER.H>
#include <SYSTEM/WINDOW.H>

#include "JOYSTICK.H"
#include "TOUCH_INPUT.H"

#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" int Platform_HeaderProbe(void);
extern "C" S32 LastInputWasKeyboard;

struct PlatformGamepad {
    U32 id;
};

extern "C" {
void *Log = NULL;
void *Screen = NULL;
U32 ModeDesiredX = 8;
U32 ModeDesiredY = 6;
U32 TabOffLine[ADELINE_MAX_Y_RES];
S32 BoxCleanClip = 0;
U8 BinGphMouse[4096];
U8 Font8x8[256 * 8];
U32 SizeChar = 8;
S32 CubeMode = 0;
S32 GamepadCameraAnalog = 0;
S32 FollowCamera = 0;
}

namespace {
U64 s_ticks = 1000;
U32 s_lastDelay = 0;
U8 s_keyboard[PLATFORM_SCANCODE_COUNT];
U32 s_harnessKey = 0;
U32 s_recordKey = 0;
std::vector<PlatformEvent> s_events;
size_t s_eventIndex = 0;
int s_pumps = 0;
int s_dirtyBoxes = 0;
int s_pauseCalls = 0;
int s_resumeCalls = 0;
bool s_windowInitialized = false;
bool s_fullscreen = false;
bool s_vsync = true;
int s_presentFilter = PLATFORM_FILTER_NEAREST;
bool s_relativeMouse = false;
bool s_keepWindowSize = false;
U32 s_width = 0;
U32 s_height = 0;
int s_presentCalls = 0;
std::vector<U32> s_presented;
U8 s_presentedPalette[256 * 3];
char s_presentOrder[32];
int s_orderLength = 0;
PlatformGamepad s_pad = {42};
bool s_padPresent = true;
bool s_buttons[PLATFORM_GAMEPAD_BUTTON_COUNT];
S16 s_axes[PLATFORM_GAMEPAD_AXIS_COUNT];
U32 s_rng = 0xDEADBEEF;

U32 NextRandom() {
    s_rng = s_rng * 1103515245u + 12345u;
    return (s_rng >> 16) & 0x7FFF;
}
void Noop() {}
bool Success() { return true; }
U64 Ticks() { return s_ticks; }
void Delay(U32 ms) {
    s_lastDelay = ms;
    s_ticks += ms;
}
const char *Error() { return "fixture failure"; }
void LogOutput(PlatformLogFn, void *) {}
void Pump() { s_pumps++; }
bool Poll(PlatformEvent *event) {
    if (s_eventIndex == s_events.size())
        return false;
    *event = s_events[s_eventIndex++];
    return true;
}
void Queue(PlatformEventType type) {
    PlatformEvent event = {};
    event.type = type;
    s_events.push_back(event);
}
void ResetEvents() {
    s_events.clear();
    s_eventIndex = 0;
}
const U8 *Keyboard() { return s_keyboard; }
U32 Keycode(S32 scancode, U16) {
    if (scancode == K_A)
        return 'q'; /* The backend, not the engine, owns the layout. */
    if (scancode >= K_A && scancode <= K_Z)
        return 'a' + scancode - K_A;
    if (scancode == K_UP)
        return A_UP;
    return 0;
}
const char *KeyName(S32) { return "Fixture key"; }
bool FixtureWindowInit(const char *title) {
    ASSERT_TRUE(strcmp(title, "Contract fixture") == 0);
    s_windowInitialized = true;
    return true;
}
bool WindowInitialized() { return s_windowInitialized; }
void FixtureWindowEnd() { s_windowInitialized = false; }
bool CreateWindow(U32 w, U32 h, bool fullscreen) {
    s_width = w;
    s_height = h;
    s_fullscreen = fullscreen;
    return true;
}
void DestroyWindow() {
    s_width = s_height = 0;
}
bool ResizeWindow(U32 w, U32 h, bool keep) {
    s_width = w;
    s_height = h;
    s_keepWindowSize = keep;
    return true;
}
bool WindowSize(S32 *w, S32 *h) {
    *w = (S32)s_width * 2;
    *h = (S32)s_height * 2;
    return true;
}
bool DisplaySize(U32 *w, U32 *h) {
    *w = 1280;
    *h = 720;
    return true;
}
void SurfaceCoords(S32 x, S32 y, S32 *sx, S32 *sy) {
    *sx = x / 2;
    *sy = y / 2;
}
void Fullscreen(bool enabled) { s_fullscreen = enabled; }
bool GetFullscreen() { return s_fullscreen; }
void VSync(bool enabled) { s_vsync = enabled; }
bool PresentFilter(PlatformPresentFilter mode) {
    if (mode == PLATFORM_FILTER_PIXELART)
        return false;
    s_presentFilter = mode;
    return true;
}
void RelativeMouse(bool enabled) { s_relativeMouse = enabled; }
void NotePresent(char stage) {
    s_presentOrder[s_orderLength++] = stage;
    s_presentOrder[s_orderLength] = '\0';
}
bool Present(const PlatformFrame *frame) {
    s_presentCalls++;
    ASSERT_EQ_UINT(s_width, frame->width);
    ASSERT_EQ_UINT(s_height, frame->height);
    s_presented.resize((size_t)frame->width * frame->height);
    memcpy(s_presentedPalette, frame->palette_rgb, sizeof(s_presentedPalette));
    bool converted = Platform_ConvertIndexedFrame(frame, &s_presented[0], frame->width * 4);
    if (converted && frame->overlay)
        frame->overlay(&s_presented[0], frame->width * 4, frame->width, frame->height, frame->palette_rgb);
    NotePresent('P');
    return converted;
}
U32 *Gamepads(S32 *count) {
    *count = s_padPresent ? 1 : 0;
    U32 *ids = (U32 *)malloc(2 * sizeof(U32));
    ids[0] = s_pad.id;
    ids[1] = 0;
    return ids;
}
PlatformGamepad *OpenPad(U32 id) { return s_padPresent && id == s_pad.id ? &s_pad : NULL; }
void ClosePad(PlatformGamepad *) {}
const char *PadName(PlatformGamepad *) { return "Fixture pad"; }
bool Button(PlatformGamepad *, PlatformGamepadButton button) { return s_buttons[button]; }
S16 Axis(PlatformGamepad *, PlatformGamepadAxis axis) { return s_axes[axis]; }
void PauseAudio() { s_pauseCalls++; }
void ResumeAudio() { s_resumeCalls++; }
void BeginPresent() { NotePresent('B'); }
void EndPresent() { NotePresent('E'); }
void MainOverlay(U32 *pixels, U32, U32, U32, const U8 *) {
    NotePresent('M');
    pixels[0] = 0xFF123456;
}
void TopOverlay(U32 *pixels, U32, U32, U32, const U8 *) {
    ASSERT_EQ_UINT(0xFF123456, pixels[0]);
    NotePresent('O');
    pixels[0] = 0xFFABCDEF;
}
void PaletteChangingOverlay(U32 *, U32, U32, U32, const U8 *palette) {
    U8 original[768];
    memcpy(original, palette, sizeof(original));
    SetVideoPaletteCol(7, 91, 92, 93);
    ASSERT_MEM_EQ(original, palette, sizeof(original));
}
void SetExpectedKey(U8 *keys, U32 key) {
    if (key < 256)
        keys[key] |= 0x80;
    else
        keys[256 - 32 + (key >> 3)] |= (U8)(1u << (key & 7));
}
PlatformInterface Fixture() {
    PlatformInterface api = {};
    api.name = "contract-fixture";
    api.capabilities.windowed = true;
    api.capabilities.touch_overlay = true;
    api.system.init = Success;
    api.system.shutdown = Noop;
    api.system.get_error = Error;
    api.system.ticks_ms = Ticks;
    api.system.delay_ms = Delay;
    api.system.set_log_output = LogOutput;
    api.events.init = Success;
    api.events.shutdown = Noop;
    api.events.pump = Pump;
    api.events.poll = Poll;
    api.window.init = FixtureWindowInit;
    api.window.shutdown = FixtureWindowEnd;
    api.window.initialized = WindowInitialized;
    api.window.create = CreateWindow;
    api.window.destroy = DestroyWindow;
    api.window.resize = ResizeWindow;
    api.window.size = WindowSize;
    api.window.display_size = DisplaySize;
    api.window.primary_display_size = DisplaySize;
    api.window.surface_coords = SurfaceCoords;
    api.window.set_fullscreen = Fullscreen;
    api.window.get_fullscreen = GetFullscreen;
    api.window.is_fullscreen = GetFullscreen;
    api.window.set_vsync = VSync;
    api.window.set_present_filter = PresentFilter;
    api.window.set_relative_mouse = RelativeMouse;
    api.window.present = Present;
    api.input.keyboard_state = Keyboard;
    api.input.keycode = Keycode;
    api.input.scancode_name = KeyName;
    api.input.init_gamepads = Success;
    api.input.shutdown_gamepads = Noop;
    api.input.gamepads = Gamepads;
    api.input.open_gamepad = OpenPad;
    api.input.close_gamepad = ClosePad;
    api.input.gamepad_name = PadName;
    api.input.gamepad_button = Button;
    api.input.gamepad_axis = Axis;
    api.host.free_memory = free;
    return api;
}
} // namespace

extern "C" const PlatformInterface g_platform = Fixture();

extern "C" void BoxStaticAdd(S32, S32, S32, S32) { s_dirtyBoxes++; }
extern "C" S32 IsAndroidTVDevice() { return 0; }
extern "C" void ApplyHarnessKeys() {
    if (s_harnessKey) {
        SetExpectedKey(TabKeys, s_harnessKey);
        if (!Key)
            Key = s_harnessKey;
    }
}
extern "C" void Record_PollHook() {
    if (s_recordKey) {
        memset(TabKeys, 0, sizeof(TabKeys));
        SetExpectedKey(TabKeys, s_recordKey);
        Key = s_recordKey;
    }
}

static void test_c_header_and_window_dispatch() {
    ASSERT_TRUE(Platform_HeaderProbe());
    ASSERT_TRUE(Window_SupportsWindowedMode());
    ASSERT_TRUE(!Window_CanLoseNativeWindow());
    U32 width = 0, height = 0;
    ASSERT_TRUE(Window_GetDisplayDimensions(&width, &height));
    ASSERT_EQ_UINT(1280, width);
    ASSERT_EQ_UINT(720, height);
    ASSERT_TRUE(Window_GetPrimaryDisplayDimensions(&width, &height));
    SetWindowFullscreen(true);
    ASSERT_TRUE(GetWindowFullscreen());
    ASSERT_TRUE(Window_IsFullscreen());
    SetWindowFullscreen(false);
    Window_SetVSync(0);
    ASSERT_TRUE(!s_vsync);
    ASSERT_TRUE(Window_SetPresentFilter(PLATFORM_FILTER_LINEAR));
    ASSERT_EQ_INT(PLATFORM_FILTER_LINEAR, s_presentFilter);
    ASSERT_TRUE(!Window_SetPresentFilter(PLATFORM_FILTER_PIXELART));
    ASSERT_EQ_INT(PLATFORM_FILTER_LINEAR, s_presentFilter);
    SetWindowRelativeMouse(true);
    ASSERT_TRUE(s_relativeMouse);
    ASSERT_TRUE(ResizeWindowSurface(8, 6, true));
    ASSERT_TRUE(s_keepWindowSize);
    S32 x = 0, y = 0;
    WindowToSurfaceCoords(12, 8, &x, &y);
    ASSERT_EQ_INT(6, x);
    ASSERT_EQ_INT(4, y);
}

static int ConsumeKey(const PlatformEvent *event) {
    return event->type == PLATFORM_EVENT_KEY_DOWN;
}

static void test_keyboard_layout_filters_and_replay_order() {
    memset(s_keyboard, 0, sizeof(s_keyboard));
    memset(TabKeys, 0, sizeof(TabKeys));
    s_keyboard[K_A] = 1;
    s_keyboard[K_Z] = 1;
    ManageKeyboard();
    ASSERT_EQ_INT(K_A, Key);
    ASSERT_EQ_INT('q', GetAscii());
    ASSERT_EQ_INT(0, GetAscii());
    ASSERT_TRUE(CheckKey(K_Z));
    memset(s_keyboard, 0, sizeof(s_keyboard));
    ManageKeyboard();
    ASSERT_EQ_INT(0, GetAscii());

    s_harnessKey = K_A;
    ManageKeyboard();
    ASSERT_EQ_INT('q', GetAscii());
    s_recordKey = K_B;
    ManageKeyboard();
    ASSERT_EQ_INT(K_B, Key);
    ASSERT_TRUE(!CheckKey(K_A));
    ASSERT_TRUE(CheckKey(K_B));
    s_recordKey = s_harnessKey = 0;

    ResetEvents();
    LastInputWasKeyboard = 0;
    Queue(PLATFORM_EVENT_KEY_DOWN);
    SetEventFilter(ConsumeKey);
    ManageEvents();
    ASSERT_EQ_INT(0, LastInputWasKeyboard);
    SetEventFilter(NULL);
    Queue(PLATFORM_EVENT_KEY_DOWN);
    ManageEvents();
    ASSERT_EQ_INT(1, LastInputWasKeyboard);
    ResetEvents();

    for (int round = 0; round < 40; ++round) {
        U8 expected[TABKEYS_NUM_KEYS] = {};
        memset(TabKeys, 0, sizeof(TabKeys));
        memset(s_keyboard, 0, sizeof(s_keyboard));
        for (int i = 0; i < 8; ++i) {
            U32 key = K_A + NextRandom() % 26;
            s_keyboard[key] = 1;
            SetExpectedKey(expected, key);
        }
        ManageKeyboard();
        ASSERT_MEM_EQ(expected, TabKeys, sizeof(expected));
    }
    memset(s_keyboard, 0, sizeof(s_keyboard));
    memset(TabKeys, 0, sizeof(TabKeys));
}

static void test_mouse_and_focus_event_order() {
    InitMouse();
    ResetEvents();
    PlatformEvent motion = {};
    motion.type = PLATFORM_EVENT_MOUSE_MOTION;
    motion.motion.x = 12;
    motion.motion.y = 8;
    motion.motion.xrel = 4;
    motion.motion.yrel = 4;
    s_events.push_back(motion);
    LastInputWasTouch = 1;
    ManageEvents();
    ManageMouse();
    ASSERT_EQ_INT(2, MouseXDep);
    ASSERT_EQ_INT(2, MouseYDep);
    ASSERT_EQ_UINT(6, MouseX);
    ASSERT_EQ_UINT(4, MouseY);
    ASSERT_EQ_INT(0, LastInputWasKeyboard);
    ASSERT_EQ_INT(1, LastInputWasTouch);
    ResetEvents();
    PlatformEvent button = {};
    button.type = PLATFORM_EVENT_MOUSE_BUTTON_DOWN;
    button.button.button = PLATFORM_BUTTON_RIGHT;
    s_events.push_back(button);
    PlatformEvent wheel = {};
    wheel.type = PLATFORM_EVENT_MOUSE_WHEEL;
    wheel.wheel.y = 30;
    s_events.push_back(wheel);
    ManageEvents();
    ASSERT_EQ_INT(2, Click);
    ASSERT_EQ_INT(8, ConsumeMouseWheelY());
    ResetEvents();
    button.type = PLATFORM_EVENT_MOUSE_BUTTON_UP;
    s_events.push_back(button);
    ManageEvents();
    ASSERT_EQ_INT(0, Click);

    ResetEvents();
    s_ticks = 1000;
    InitTimer();
    SetTimerHR(0);
    AppActive = true;
    SetAppFocusAudioHooks(PauseAudio, ResumeAudio);
    Queue(PLATFORM_EVENT_WINDOW_FOCUS_LOST);
    Queue(PLATFORM_EVENT_WINDOW_FOCUS_LOST);
    ManageEvents();
    s_ticks += 50;
    ManageTime();
    ASSERT_EQ_UINT(0, TimerRefHR);
    ASSERT_EQ_INT(1, s_pauseCalls);
    ASSERT_TRUE(!AppActive);
    s_dirtyBoxes = 0;
    Queue(PLATFORM_EVENT_WINDOW_FOCUS_GAINED);
    Queue(PLATFORM_EVENT_WINDOW_FOCUS_GAINED);
    ManageEvents();
    ManageTime();
    ASSERT_EQ_UINT(50, TimerRefHR);
    ASSERT_EQ_INT(1, s_resumeCalls);
    ASSERT_EQ_INT(1, s_dirtyBoxes);
    ASSERT_TRUE(AppActive);
    ResetEvents();
}

static void test_wall_clock_wrap_and_fixed_dt() {
    s_ticks = UINT32_C(0xFFFFFFF0);
    ASSERT_EQ_INT(32, Timer_SleepUntil(0x10));
    ASSERT_EQ_UINT(32, s_lastDelay);
    ASSERT_EQ_INT(-1, Timer_SleepUntil(0x0F));
    s_ticks = 1000;
    InitTimer();
    Timer_EnableFixedDt(16);
    Timer_FixedDtAdvance();
    ManageTime();
    ASSERT_EQ_UINT(16, TimerRefHR);
    s_ticks += 5000;
    ManageTime();
    ASSERT_EQ_UINT(16, TimerRefHR);
    Timer_FixedDtAdvance();
    ManageTime();
    ASSERT_EQ_UINT(32, TimerRefHR);
    Timer_DisableFixedDt();
}

static void test_gamepad_state_edges_and_hotplug() {
    memset(TabKeys, 0, sizeof(TabKeys));
    InitJoystick();
    ASSERT_TRUE(JoystickIsPresent());
    ASSERT_TRUE(strcmp(JoystickGetName(), "Fixture pad") == 0);
    UpdateJoystick();
    ASSERT_EQ_UINT(0, JoystickFirstPressedScancode());
    s_buttons[PLATFORM_GAMEPAD_BUTTON_SOUTH] = true;
    s_axes[PLATFORM_GAMEPAD_AXIS_LEFTX] = 24000;
    s_axes[PLATFORM_GAMEPAD_AXIS_RIGHTY] = -24000;
    s_axes[PLATFORM_GAMEPAD_AXIS_LEFT_TRIGGER] = 24000;
    UpdateJoystick();
    ASSERT_TRUE(CheckKey(K_GAMEPAD_A));
    ASSERT_TRUE(CheckKey(K_GAMEPAD_LSTICK_RIGHT));
    ASSERT_TRUE(CheckKey(K_GAMEPAD_RSTICK_UP));
    ASSERT_TRUE(CheckKey(K_GAMEPAD_LTRIGGER));
    ASSERT_EQ_UINT(K_GAMEPAD_A, JoystickFirstPressedScancode());
    UpdateJoystick();
    ASSERT_EQ_UINT(0, JoystickFirstPressedScancode());
    S16 x = 0, y = 0;
    JoystickGetRightStick(&x, &y);
    ASSERT_EQ_INT(0, x);
    ASSERT_EQ_INT(-24000, y);

    memset(s_buttons, 0, sizeof(s_buttons));
    memset(s_axes, 0, sizeof(s_axes));
    s_axes[PLATFORM_GAMEPAD_AXIS_LEFTX] = 7999;
    UpdateJoystick();
    ASSERT_TRUE(!CheckKey(K_GAMEPAD_LSTICK_RIGHT));
    s_axes[PLATFORM_GAMEPAD_AXIS_LEFTX] = 0;

    const U32 keys[] = {
        K_GAMEPAD_A, K_GAMEPAD_B, K_GAMEPAD_X, K_GAMEPAD_Y,
        K_GAMEPAD_BACK, K_GAMEPAD_GUIDE, K_GAMEPAD_START,
        K_GAMEPAD_LSTICK_BTN, K_GAMEPAD_RSTICK_BTN,
        K_GAMEPAD_LSHOULDER, K_GAMEPAD_RSHOULDER,
        K_GAMEPAD_DPAD_UP, K_GAMEPAD_DPAD_DOWN,
        K_GAMEPAD_DPAD_LEFT, K_GAMEPAD_DPAD_RIGHT};
    for (int round = 0; round < 40; ++round) {
        U8 expected[TABKEYS_NUM_KEYS] = {};
        memset(TabKeys, 0, sizeof(TabKeys));
        for (int i = 0; i < PLATFORM_GAMEPAD_BUTTON_COUNT; ++i) {
            s_buttons[i] = (NextRandom() & 1) != 0;
            if (s_buttons[i])
                SetExpectedKey(expected, keys[i]);
        }
        UpdateJoystick();
        ASSERT_MEM_EQ(expected, TabKeys, sizeof(expected));
    }

    ResetEvents();
    PlatformEvent removed = {};
    removed.type = PLATFORM_EVENT_GAMEPAD_REMOVED;
    removed.gdevice.which = s_pad.id;
    s_padPresent = false;
    s_events.push_back(removed);
    ManageEvents();
    ASSERT_TRUE(!JoystickIsPresent());
    U8 empty[TABKEYS_NUM_KEYS] = {};
    ASSERT_MEM_EQ(empty, TabKeys, sizeof(empty));
    Joystick_SetVirtualPad(1);
    ASSERT_TRUE(JoystickIsPresent());
    Joystick_SetVirtualPad(0);
    EndJoystick();
    ResetEvents();
}

static void test_indexed_conversion_exact_bytes() {
    U8 palette[768];
    for (int round = 0; round < 40; ++round) {
        U32 width = 1 + NextRandom() % 37;
        U32 height = 1 + NextRandom() % 19;
        U32 sourcePitch = width + 3;
        U32 targetPitch = width + 5;
        std::vector<U8> source((size_t)sourcePitch * height);
        for (size_t i = 0; i < source.size(); ++i)
            source[i] = (U8)NextRandom();
        for (size_t i = 0; i < sizeof(palette); ++i)
            palette[i] = (U8)NextRandom();
        std::vector<U8> original = source;
        std::vector<U32> expected((size_t)targetPitch * height + 2, 0x89ABCDEF);
        std::vector<U32> actual = expected;
        for (U32 y = 0; y < height; ++y) {
            for (U32 x = 0; x < width; ++x) {
                U32 index = source[y * sourcePitch + x];
                expected[1 + y * targetPitch + x] = 0xFF000000 |
                                                    ((U32)palette[index * 3] << 16) |
                                                    ((U32)palette[index * 3 + 1] << 8) | palette[index * 3 + 2];
            }
        }
        PlatformFrame frame = {&source[0], width, height, sourcePitch, palette, NULL};
        ASSERT_TRUE(Platform_ConvertIndexedFrame(&frame, &actual[1], targetPitch * 4));
        ASSERT_MEM_EQ(&expected[0], &actual[0], actual.size() * sizeof(U32));
        ASSERT_MEM_EQ(&original[0], &source[0], source.size());
    }
    U8 source[8] = {};
    U32 target[8];
    memset(target, 0xCD, sizeof(target));
    U32 original[8];
    memcpy(original, target, sizeof(target));
    PlatformFrame invalid = {source, 4, 2, 4, palette, NULL};
    ASSERT_TRUE(!Platform_ConvertIndexedFrame(&invalid, target, 15));
    ASSERT_MEM_EQ(original, target, sizeof(target));
    invalid.pitch = 3;
    ASSERT_TRUE(!Platform_ConvertIndexedFrame(&invalid, target, 16));
    ASSERT_MEM_EQ(original, target, sizeof(target));
}

static void test_present_lock_palette_and_overlay_order() {
    std::vector<U8> indexed(8 * 6);
    for (size_t i = 0; i < indexed.size(); ++i)
        indexed[i] = (U8)i;
    Log = &indexed[0];
    ASSERT_TRUE(InitVideo());
    ASSERT_TRUE(CreateVideoSurface(8, 6));
    T_BOX area = {0, 0, 8, 6};
    LockVideoSurface();
    CopyVideoArea(Phys, Log, TabOffPhysLine, &area);
    UnlockVideoSurface();
    ASSERT_EQ_INT(1, s_presentCalls);
    for (size_t i = 0; i < s_presented.size(); ++i)
        ASSERT_EQ_UINT(0xFFFFFFFF, s_presented[i]);

    U8 palette[768];
    for (size_t i = 0; i < sizeof(palette); ++i)
        palette[i] = (U8)i;
    SetVideoPaletteSync(palette);
    SetVideoPaletteCol(7, 1, 2, 3);
    palette[21] = 1;
    palette[22] = 2;
    palette[23] = 3;
    SetPrePresentCallback(MainOverlay);
    SetOverlayPrePresentCallback(TopOverlay);
    SetPresentTimingCallbacks(BeginPresent, EndPresent);
    s_orderLength = 0;
    s_presentOrder[0] = '\0';
    LockVideoSurface();
    LockVideoSurface();
    CopyVideoArea(Phys, Log, TabOffPhysLine, &area);
    UnlockVideoSurface();
    ASSERT_EQ_INT(1, s_presentCalls);
    UnlockVideoSurface();
    ASSERT_EQ_INT(2, s_presentCalls);
    ASSERT_TRUE(strcmp(s_presentOrder, "BMOPE") == 0);
    ASSERT_MEM_EQ(palette, s_presentedPalette, sizeof(palette));
    ASSERT_EQ_UINT(0xFFABCDEF, s_presented[0]);
    for (size_t i = 1; i < indexed.size(); ++i) {
        U32 index = indexed[i] * 3;
        U32 color = 0xFF000000 | ((U32)palette[index] << 16) |
                    ((U32)palette[index + 1] << 8) | palette[index + 2];
        ASSERT_EQ_UINT(color, s_presented[i]);
    }
    UnlockVideoSurface();
    ASSERT_EQ_INT(2, s_presentCalls);
    SetPrePresentCallback(NULL);
    SetOverlayPrePresentCallback(NULL);
    SetPresentTimingCallbacks(NULL, NULL);
    SetPrePresentCallback(PaletteChangingOverlay);
    CopyVideoArea(Phys, Log, TabOffPhysLine, &area);
    UnlockVideoSurface();
    SetPrePresentCallback(NULL);
    DestroyVideoSurface();
    Log = NULL;
}

static void test_touch_wake_hold_release_and_reapply() {
    ModeDesiredX = ModeResX = 640;
    ModeDesiredY = ModeResY = 480;
    ASSERT_TRUE(ResizeWindowSurface(640, 480, false));
    memset(TabKeys, 0, sizeof(TabKeys));
    TouchOverlay_Init();
    ASSERT_TRUE(TouchOverlay_IsSupported());
    LastInputWasTouch = 0;
    PlatformEvent touch = {};
    touch.type = PLATFORM_EVENT_FINGER_DOWN;
    touch.tfinger.fingerID = 42;
    touch.tfinger.x = 0.1f;
    touch.tfinger.y = 0.65f;
    TouchOverlay_HandleEvent(&touch);
    ASSERT_EQ_INT(1, LastInputWasTouch);
    ASSERT_TRUE(!CheckKey(K_UP));
    touch.type = PLATFORM_EVENT_FINGER_UP;
    TouchOverlay_HandleEvent(&touch);
    touch.type = PLATFORM_EVENT_FINGER_DOWN;
    ASSERT_EQ_INT(1, TouchOverlay_HandleEvent(&touch));
    ASSERT_TRUE(CheckKey(K_UP));
    ManageKeyboard();
    ASSERT_TRUE(CheckKey(K_UP));
    ASSERT_EQ_INT(0, Key);
    touch.type = PLATFORM_EVENT_FINGER_MOTION;
    touch.tfinger.y = 0.85f;
    TouchOverlay_HandleEvent(&touch);
    ASSERT_TRUE(!CheckKey(K_UP));
    ASSERT_TRUE(CheckKey(K_DOWN));
    touch.type = PLATFORM_EVENT_FINGER_UP;
    TouchOverlay_HandleEvent(&touch);
    ASSERT_TRUE(!CheckKey(K_DOWN));
    TouchOverlay_Shutdown();
    ASSERT_TRUE(!TouchOverlay_IsSupported());
    ASSERT_EQ_INT(0, TouchOverlay_HandleEvent(&touch));
}

int main() {
    if (!InitEvents() || !InitWindow("Contract fixture") ||
        !CreateWindowSurface(8, 6, false))
        return 1;
    RUN_TEST(test_c_header_and_window_dispatch);
    RUN_TEST(test_keyboard_layout_filters_and_replay_order);
    RUN_TEST(test_mouse_and_focus_event_order);
    RUN_TEST(test_wall_clock_wrap_and_fixed_dt);
    RUN_TEST(test_gamepad_state_edges_and_hotplug);
    RUN_TEST(test_indexed_conversion_exact_bytes);
    RUN_TEST(test_present_lock_palette_and_overlay_order);
    RUN_TEST(test_touch_wake_hold_release_and_reapply);
    EndWindow();
    EndEvents();
    TEST_SUMMARY();
    return test_failures != 0;
}
