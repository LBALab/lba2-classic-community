#include "PRIVATE.H"

namespace {
PlatformInterface Interface() {
    PlatformInterface api = {};
    api.name = "sdl3";
#ifdef __ANDROID__
    api.capabilities.transient_window_loss = true;
    api.capabilities.touch_overlay = true;
#else
    api.capabilities.windowed = true;
#endif
    api.system.init = SDL3_Init;
    api.system.shutdown = SDL_Quit;
    api.system.set_headless = SDL3_SetHeadless;
    api.system.get_error = SDL_GetError;
    api.system.clear_error = SDL3_ClearError;
    api.system.ticks_ms = SDL3_Ticks;
    api.system.delay_ms = SDL_Delay;
    api.system.performance_counter = SDL3_Counter;
    api.system.performance_frequency = SDL3_Frequency;
    api.system.thread_id = SDL3_ThreadID;
    api.system.cpu_count = SDL3_CPUCount;
    api.system.ram_mb = SDL3_RAM;
    api.system.platform_name = SDL_GetPlatform;
    api.system.set_log_output = SDL3_SetLogOutput;
    api.events.init = SDL3_InitEvents;
    api.events.shutdown = SDL3_EndEvents;
    api.events.pump = SDL_PumpEvents;
    api.events.poll = SDL3_PollEvent;
    api.window.init = SDL3_InitWindow;
    api.window.shutdown = SDL3_EndWindow;
    api.window.initialized = SDL3_IsWindowInitialized;
    api.window.create = SDL3_CreateWindow;
    api.window.destroy = SDL3_DestroyWindow;
    api.window.resize = SDL3_ResizeWindow;
    api.window.size = SDL3_WindowSize;
    api.window.display_size = SDL3_DisplaySize;
    api.window.primary_display_size = SDL3_PrimaryDisplaySize;
    api.window.surface_coords = SDL3_SurfaceCoords;
    api.window.set_fullscreen = SDL3_SetFullscreen;
    api.window.get_fullscreen = SDL3_GetFullscreen;
    api.window.is_fullscreen = SDL3_IsFullscreen;
    api.window.set_vsync = SDL3_SetVSync;
    api.window.set_relative_mouse = SDL3_SetRelativeMouse;
    api.window.present = SDL3_Present;
    api.input.keyboard_state = SDL3_KeyboardState;
    api.input.keycode = SDL3_Keycode;
    api.input.scancode_name = SDL3_ScancodeName;
    api.input.modifiers = SDL3_Modifiers;
    api.input.text_input = SDL3_TextInput;
    api.input.cursor_visible = SDL3_CursorVisible;
    api.input.init_gamepads = SDL3_InitGamepads;
    api.input.shutdown_gamepads = SDL3_EndGamepads;
    api.input.gamepads = SDL3_Gamepads;
    api.input.open_gamepad = SDL3_OpenGamepad;
    api.input.close_gamepad = SDL3_CloseGamepad;
    api.input.gamepad_name = SDL3_GamepadName;
    api.input.gamepad_button = SDL3_GamepadButton;
    api.input.gamepad_axis = SDL3_GamepadAxis;
    api.host.base_path = SDL_GetBasePath;
    api.host.current_directory = SDL_GetCurrentDirectory;
    api.host.pref_path = SDL_GetPrefPath;
    api.host.home_path = SDL3_HomePath;
    api.host.create_directory = SDL_CreateDirectory;
    api.host.remove_path = SDL_RemovePath;
    api.host.message_box = SDL3_MessageBox;
    api.host.begin_folder_dialog = SDL3_BeginFolderDialog;
    api.host.poll_folder_dialog = SDL3_PollFolderDialog;
    api.host.end_folder_dialog = SDL3_EndFolderDialog;
    api.host.set_clipboard = SDL_SetClipboardText;
    api.host.get_clipboard = SDL_GetClipboardText;
    api.host.free_memory = SDL_free;
    return api;
}
} // namespace

extern "C" const PlatformInterface g_platform = Interface();
