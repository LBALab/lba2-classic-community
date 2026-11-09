#include "PRIVATE.H"
#include "../HOST_OS.H"

#include <windows.storage.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace WS = ABI::Windows::Storage;

namespace {
char s_localState[MAX_PATH * 3] = "";
bool s_localStateRead = false;

/* LocalState is the one folder a UWP app can write besides temp: settings,
   saves, logs, and on a console the game data copied in through the Device
   Portal. The engine opens files with the narrow CRT calls, which read the
   ANSI code page, so the path is converted to that; a path it cannot spell
   is refused rather than handed over mangled. */
const char *LocalState() {
    if (s_localStateRead)
        return s_localState;
    s_localStateRead = true;
    WS::IApplicationDataStatics *statics = NULL;
    WS::IApplicationData *data = NULL;
    WS::IStorageFolder *folder = NULL;
    WS::IStorageItem *item = NULL;
    HSTRING path = NULL;
    if (SUCCEEDED(Uwp_Factory(RuntimeClass_Windows_Storage_ApplicationData,
                              __uuidof(WS::IApplicationDataStatics), (void **)&statics)) &&
        SUCCEEDED(statics->get_Current(&data)) && SUCCEEDED(data->get_LocalFolder(&folder)) &&
        SUCCEEDED(folder->QueryInterface(__uuidof(WS::IStorageItem), (void **)&item)) &&
        SUCCEEDED(item->get_Path(&path))) {
        BOOL lossy = FALSE;
        const int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, WindowsGetStringRawBuffer(path, NULL), -1,
                                          s_localState, (int)sizeof(s_localState) - 1, NULL, &lossy);
        if (n > 1 && !lossy) {
            strcat(s_localState, "\\");
        } else {
            s_localState[0] = '\0';
            Platform_Log(PLATFORM_LOG_ERROR, "UWP: the LocalState path cannot be spelled in the ANSI code page");
        }
    }
    if (path)
        WindowsDeleteString(path);
    Uwp_Release(item);
    Uwp_Release(folder);
    Uwp_Release(data);
    Uwp_Release(statics);
    return s_localState;
}

bool Ready() { return true; }
void LogOutput(PlatformLogFn, void *) {}

/* Every app has its own LocalState, so the org and app names add nothing. */
char *Preferences(const char *org, const char *app) {
    if (!org || !app || !*app) {
        Uwp_Fail("Invalid preference directory names");
        return NULL;
    }
    const char *path = LocalState();
    if (!*path) {
        Uwp_Fail("UWP: the app's LocalState folder is unavailable");
        return NULL;
    }
    return HostOs_Copy(path);
}

const char *Home() {
    const char *path = LocalState();
    if (!*path) {
        Uwp_Fail("UWP: the app's LocalState folder is unavailable");
        return NULL;
    }
    return path;
}

/* An app always has its window, which stays a present-capable target, so a
   headless run (tests/automation through tools/uwp/xbox-lba2cc.py) runs as
   usual and shows on the TV. The engine skips the audio device itself. */
bool s_headless = false;

bool Headless() {
    s_headless = true;
    Platform_Log(PLATFORM_LOG_INFO, "UWP: --headless keeps the window; an app always has one");
    return true;
}

/* Headless, the display is not measured, as with SDL's dummy driver and the
   null backend, so what a run does never depends on the screen it ran on
   (the boot resolution's auto-detect falls through to the default). */
bool PrimaryDisplaySize(uint32_t *width, uint32_t *height) {
    if (s_headless)
        return Uwp_Fail("UWP: a headless run does not measure the display");
    return Uwp_DisplaySize(width, height);
}

/* No mouse on a console, and no pointer support yet anywhere. */
void PointerMode(bool enabled) {
    if (enabled)
        Uwp_Fail("UWP has no pointer support yet");
}

void Fullscreen(bool) {}
bool AlwaysFullscreen() { return true; }

const char *PlatformName() { return "UWP"; }

void Shutdown() { Uwp_WindowShutdown(); }

bool Message(PlatformMessageKind, const char *, const char *) {
    return Uwp_Fail("UWP has no message-box service");
}
PlatformFolderDialog *BeginDialog(const char *) {
    Uwp_Fail("UWP has no folder-dialog service");
    return NULL;
}
PlatformDialogResult PollDialog(PlatformFolderDialog *, const char **) {
    Uwp_Fail("UWP has no folder-dialog service");
    return PLATFORM_DIALOG_ERROR;
}
void EndDialog(PlatformFolderDialog *dialog) {
    if (dialog)
        Uwp_Fail("UWP cannot release a foreign folder dialog");
}
bool SetClipboard(const char *) { return Uwp_Fail("UWP has no clipboard"); }
char *GetClipboard() {
    Uwp_Fail("UWP has no clipboard");
    return NULL;
}

PlatformInterface Interface() {
    PlatformInterface api;
    memset(&api, 0, sizeof api);
    api.name = "uwp";
    api.capabilities.windowed = false;
    api.capabilities.transient_window_loss = false;
    api.capabilities.touch_overlay = false;
    api.capabilities.touch_recenter = false;
    api.system.init = Ready;
    api.system.shutdown = Shutdown;
    api.system.set_headless = Headless;
    api.system.get_error = HostOs_Error;
    api.system.clear_error = HostOs_ClearError;
    api.system.ticks_ms = HostOs_Ticks;
    api.system.delay_ms = HostOs_Delay;
    api.system.performance_counter = HostOs_Counter;
    api.system.performance_frequency = HostOs_Frequency;
    api.system.thread_id = HostOs_ThreadID;
    api.system.cpu_count = HostOs_CPUCount;
    api.system.ram_mb = HostOs_RAM;
    api.system.platform_name = PlatformName;
    api.system.set_log_output = LogOutput;
    api.events.init = Uwp_EventsInit;
    api.events.shutdown = Uwp_EventsShutdown;
    api.events.pump = Uwp_Pump;
    api.events.poll = Uwp_Poll;
    api.window.init = Uwp_WindowInit;
    api.window.shutdown = Uwp_WindowShutdown;
    api.window.initialized = Uwp_WindowInitialized;
    api.window.create = Uwp_WindowCreate;
    api.window.destroy = Uwp_WindowDestroy;
    api.window.resize = Uwp_WindowResize;
    api.window.size = Uwp_WindowSize;
    api.window.display_size = Uwp_DisplaySize;
    api.window.primary_display_size = PrimaryDisplaySize;
    api.window.surface_coords = Uwp_SurfaceCoords;
    api.window.set_fullscreen = Fullscreen;
    api.window.get_fullscreen = AlwaysFullscreen;
    api.window.is_fullscreen = AlwaysFullscreen;
    api.window.set_vsync = Uwp_SetVSync;
    api.window.set_present_filter = Uwp_SetPresentFilter;
    api.window.set_relative_mouse = PointerMode;
    api.window.set_mouse_grab = PointerMode;
    api.window.present = Uwp_Present;
    api.input.keyboard_state = Uwp_KeyboardState;
    api.input.keycode = Uwp_Keycode;
    api.input.scancode_name = Uwp_ScancodeName;
    api.input.modifiers = Uwp_Modifiers;
    api.input.text_input = Uwp_TextInput;
    api.input.cursor_visible = PointerMode;
    api.input.init_gamepads = Uwp_InitGamepads;
    api.input.shutdown_gamepads = Uwp_ShutdownGamepads;
    api.input.gamepads = Uwp_Gamepads;
    api.input.open_gamepad = Uwp_OpenGamepad;
    api.input.close_gamepad = Uwp_CloseGamepad;
    api.input.gamepad_name = Uwp_GamepadName;
    api.input.gamepad_button = Uwp_GamepadButton;
    api.input.gamepad_axis = Uwp_GamepadAxis;
    api.host.base_path = HostOs_BasePath;
    api.host.current_directory = HostOs_CurrentDirectory;
    api.host.pref_path = Preferences;
    api.host.home_path = Home;
    api.host.create_directory = HostOs_MakeDirectory;
    api.host.remove_path = HostOs_Remove;
    api.host.message_box = Message;
    api.host.begin_folder_dialog = BeginDialog;
    api.host.poll_folder_dialog = PollDialog;
    api.host.end_folder_dialog = EndDialog;
    api.host.set_clipboard = SetClipboard;
    api.host.get_clipboard = GetClipboard;
    api.host.free_memory = free;
    return api;
}
} // namespace

bool Uwp_Fail(const char *format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof message, format, args);
    va_end(args);
    return HostOs_Fail("%s", message);
}

HRESULT Uwp_Factory(const WCHAR *className, REFIID iid, void **out) {
    HSTRING_HEADER header;
    HSTRING name;
    HRESULT hr = WindowsCreateStringReference(className, (UINT32)wcslen(className), &header, &name);
    if (FAILED(hr))
        return hr;
    return RoGetActivationFactory(name, iid, out);
}

const char *Uwp_LocalState(void) { return LocalState(); }

extern "C" const PlatformInterface g_platform = Interface();
