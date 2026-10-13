#include "PRIVATE.H"

namespace {
char *s_basePath = NULL;
PlatformLogFn s_log = NULL;
void *s_logUserdata = NULL;
void SDLCALL LogOutput(void *, int, SDL_LogPriority priority, const char *message) {
    PlatformLogLevel level = PLATFORM_LOG_DEBUG;
    if (priority == SDL_LOG_PRIORITY_INFO)
        level = PLATFORM_LOG_INFO;
    else if (priority == SDL_LOG_PRIORITY_WARN)
        level = PLATFORM_LOG_WARN;
    else if (priority >= SDL_LOG_PRIORITY_ERROR)
        level = PLATFORM_LOG_ERROR;
    if (s_log)
        s_log(s_logUserdata, level, message);
}
bool Init() {
    SDL_SetMainReady();
    return SDL_Init(0) == 0;
}
void Shutdown() {
    SDL2_EndWindow();
    SDL_free(s_basePath);
    s_basePath = NULL;
    SDL_Quit();
}
bool Headless() {
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    const char *driver = SDL_GetHint(SDL_HINT_VIDEODRIVER);
    if (driver && SDL_strcmp(driver, "dummy") == 0)
        return true;
    SDL_SetError("Unable to select dummy video driver");
    return false;
}
uint64_t ThreadID() { return (uint64_t)SDL_ThreadID(); }
uint64_t Ticks() { return SDL_GetTicks64(); }
uint64_t Counter() { return SDL_GetPerformanceCounter(); }
uint64_t Frequency() { return SDL_GetPerformanceFrequency(); }
int32_t CPUCount() { return SDL_GetCPUCount(); }
int32_t RAM() { return SDL_GetSystemRAM(); }
void LogOutputSet(PlatformLogFn callback, void *userdata) {
    s_log = callback;
    s_logUserdata = userdata;
    SDL_LogSetOutputFunction(callback ? LogOutput : NULL, NULL);
}
bool InitEvents() { return SDL_InitSubSystem(SDL_INIT_EVENTS) == 0; }
void EndEvents() { SDL_QuitSubSystem(SDL_INIT_EVENTS); }
const uint8_t *Keyboard() {
    static uint8_t state[PLATFORM_SCANCODE_COUNT];
    int count = 0;
    const Uint8 *native = SDL_GetKeyboardState(&count);
    SDL_memset(state, 0, sizeof(state));
    for (int i = 0; i < count && i < PLATFORM_SCANCODE_COUNT; ++i)
        state[i] = native[i] ? 1 : 0;
    return state;
}
uint32_t Keycode(int32_t scancode, uint16_t) {
    /* SDL2 keycodes are layout-aware, unmodified key identities. Text input
       carries shifted characters; the engine applies its own ASCII case policy. */
    if (!SDL2_InitVideo()) {
        Platform_Log(PLATFORM_LOG_ERROR, "Unable to initialize SDL2 keyboard layout: %s", SDL_GetError());
        return 0;
    }
    return (uint32_t)SDL_GetKeyFromScancode((SDL_Scancode)scancode);
}
const char *ScancodeName(int32_t scancode) { return SDL_GetScancodeName((SDL_Scancode)scancode); }
uint16_t Modifiers() { return (uint16_t)SDL_GetModState(); }
void TextInput(bool enabled) {
    if (enabled)
        SDL_StartTextInput();
    else
        SDL_StopTextInput();
}
void Cursor(bool visible) {
    if (SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE) < 0)
        Platform_Log(PLATFORM_LOG_WARN, "Unable to change SDL2 cursor: %s", SDL_GetError());
}
bool InitGamepads() { return SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0; }
void EndGamepads() { SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER); }
uint32_t *Gamepads(int32_t *count) {
    const int devices = SDL_NumJoysticks();
    if (devices < 0) {
        Platform_Log(PLATFORM_LOG_ERROR, "Unable to enumerate SDL2 controllers: %s", SDL_GetError());
        return NULL;
    }
    uint32_t *ids = (uint32_t *)SDL_malloc(((size_t)devices + 1) * sizeof(uint32_t));
    if (!ids) {
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return NULL;
    }
    int32_t n = 0;
    for (int i = 0; i < devices; ++i) {
        if (!SDL_IsGameController(i))
            continue;
        const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(i);
        if (id >= 0)
            ids[n++] = (uint32_t)id;
    }
    ids[n] = 0;
    if (count)
        *count = n;
    return ids;
}
PlatformGamepad *OpenGamepad(uint32_t id) {
    const int count = SDL_NumJoysticks();
    for (int i = 0; i < count; ++i)
        if ((uint32_t)SDL_JoystickGetDeviceInstanceID(i) == id)
            return (PlatformGamepad *)SDL_GameControllerOpen(i);
    SDL_SetError("SDL2 controller instance %u is unavailable", id);
    return NULL;
}
void CloseGamepad(PlatformGamepad *pad) { SDL_GameControllerClose((SDL_GameController *)pad); }
const char *GamepadName(PlatformGamepad *pad) { return SDL_GameControllerName((SDL_GameController *)pad); }
bool Button(PlatformGamepad *pad, PlatformGamepadButton button) {
    if (button < 0 || button >= PLATFORM_GAMEPAD_BUTTON_COUNT) {
        SDL_SetError("Invalid gamepad button %d", (int)button);
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return false;
    }
    return SDL_GameControllerGetButton((SDL_GameController *)pad, (SDL_GameControllerButton)button) != 0;
}
int16_t Axis(PlatformGamepad *pad, PlatformGamepadAxis axis) {
    if (axis < 0 || axis >= PLATFORM_GAMEPAD_AXIS_COUNT) {
        SDL_SetError("Invalid gamepad axis %d", (int)axis);
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return 0;
    }
    return SDL_GameControllerGetAxis((SDL_GameController *)pad, (SDL_GameControllerAxis)axis);
}
const char *BasePath() {
    if (!s_basePath)
        s_basePath = SDL_GetBasePath();
    return s_basePath;
}
bool Clipboard(const char *text) { return SDL_SetClipboardText(text) == 0; }
char *Preferences(const char *org, const char *app) {
    if (!org || !app || !*app) {
        SDL_SetError("Invalid preference directory names");
        Platform_Log(PLATFORM_LOG_ERROR, "%s", SDL_GetError());
        return NULL;
    }
    return SDL_GetPrefPath(org, app);
}

PlatformInterface Interface() {
    PlatformInterface api = {};
    api.name = "sdl2";
    api.capabilities.windowed = true;
    api.system.init = Init;
    api.system.shutdown = Shutdown;
    api.system.set_headless = Headless;
    api.system.get_error = SDL_GetError;
    api.system.clear_error = SDL_ClearError;
    api.system.ticks_ms = Ticks;
    api.system.delay_ms = SDL_Delay;
    api.system.performance_counter = Counter;
    api.system.performance_frequency = Frequency;
    api.system.thread_id = ThreadID;
    api.system.cpu_count = CPUCount;
    api.system.ram_mb = RAM;
    api.system.platform_name = SDL_GetPlatform;
    api.system.set_log_output = LogOutputSet;
    api.events.init = InitEvents;
    api.events.shutdown = EndEvents;
    api.events.pump = SDL_PumpEvents;
    api.events.poll = SDL2_PollEvent;
    api.window.init = SDL2_InitWindow;
    api.window.shutdown = SDL2_EndWindow;
    api.window.initialized = SDL2_WindowInitialized;
    api.window.create = SDL2_CreateWindow;
    api.window.destroy = SDL2_DestroyWindow;
    api.window.resize = SDL2_ResizeWindow;
    api.window.size = SDL2_WindowSize;
    api.window.display_size = SDL2_DisplaySize;
    api.window.primary_display_size = SDL2_PrimaryDisplaySize;
    api.window.surface_coords = SDL2_SurfaceCoords;
    api.window.set_fullscreen = SDL2_SetFullscreen;
    api.window.get_fullscreen = SDL2_GetFullscreen;
    api.window.is_fullscreen = SDL2_IsFullscreen;
    api.window.set_vsync = SDL2_SetVSync;
    api.window.set_relative_mouse = SDL2_SetRelativeMouse;
    api.window.present = SDL2_Present;
    api.input.keyboard_state = Keyboard;
    api.input.keycode = Keycode;
    api.input.scancode_name = ScancodeName;
    api.input.modifiers = Modifiers;
    api.input.text_input = TextInput;
    api.input.cursor_visible = Cursor;
    api.input.init_gamepads = InitGamepads;
    api.input.shutdown_gamepads = EndGamepads;
    api.input.gamepads = Gamepads;
    api.input.open_gamepad = OpenGamepad;
    api.input.close_gamepad = CloseGamepad;
    api.input.gamepad_name = GamepadName;
    api.input.gamepad_button = Button;
    api.input.gamepad_axis = Axis;
    api.host.base_path = BasePath;
    api.host.current_directory = SDL2_CurrentDirectory;
    api.host.pref_path = Preferences;
    api.host.home_path = SDL2_HomePath;
    api.host.create_directory = SDL2_CreateDirectory;
    api.host.remove_path = SDL2_RemovePath;
    api.host.message_box = SDL2_MessageBox;
    api.host.begin_folder_dialog = SDL2_BeginFolderDialog;
    api.host.poll_folder_dialog = SDL2_PollFolderDialog;
    api.host.end_folder_dialog = SDL2_EndFolderDialog;
    api.host.set_clipboard = Clipboard;
    api.host.get_clipboard = SDL_GetClipboardText;
    api.host.free_memory = SDL_free;
    return api;
}
} // namespace

bool SDL2_InitVideo() {
    return SDL_WasInit(SDL_INIT_VIDEO) != 0 || (Init() && SDL_InitSubSystem(SDL_INIT_VIDEO) == 0);
}
extern "C" const PlatformInterface g_platform = Interface();
