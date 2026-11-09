#include "PRIVATE.H"

#include <windows.storage.h>

#include <direct.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace WS = ABI::Windows::Storage;

namespace {
char s_localState[MAX_PATH * 3] = "";

/* LocalState is the one folder a UWP app can write besides temp: settings,
   saves, logs, and on a console the game data copied in through the Device
   Portal. */
const char *LocalState() {
    if (s_localState[0])
        return s_localState;
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
        const int n = WideCharToMultiByte(CP_UTF8, 0, WindowsGetStringRawBuffer(path, NULL), -1, s_localState,
                                          (int)sizeof(s_localState) - 1, NULL, NULL);
        if (n > 1)
            strcat(s_localState, "\\");
        else
            s_localState[0] = '\0';
    }
    if (path)
        WindowsDeleteString(path);
    Uwp_Release(item);
    Uwp_Release(folder);
    Uwp_Release(data);
    Uwp_Release(statics);
    return s_localState;
}

char *Copy(const char *text) {
    char *copy = (char *)malloc(strlen(text) + 1);
    if (copy)
        strcpy(copy, text);
    else
        Uwp_Fail("Unable to allocate a host path");
    return copy;
}

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
    return Copy(path);
}

const char *Home() {
    const char *path = LocalState();
    if (!*path) {
        Uwp_Fail("UWP: the app's LocalState folder is unavailable");
        return NULL;
    }
    return path;
}

bool IsSeparator(char ch) { return ch == '\\' || ch == '/'; }

bool IsDirectory(const char *path) {
    const DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

/* An app cannot look at the folders above its own (Q:\Users answers "not
   found"), so creating every parent from the drive down fails. Find the
   deepest folder that exists and create only what is missing below it. */
bool MakeDirectory(const char *path) {
    if (!path || !*path)
        return Uwp_Fail("Host directory path is empty");
    char *copy = Copy(path);
    if (!copy)
        return false;
    size_t len = strlen(copy);
    while (len > 1 && IsSeparator(copy[len - 1]))
        copy[--len] = '\0';
    /* Cut the path back one level at a time until what is left exists. */
    enum { MAX_DEPTH = 64 };
    size_t cuts[MAX_DEPTH];
    char separators[MAX_DEPTH];
    int depth = 0;
    bool ok = true;
    while (!IsDirectory(copy)) {
        char *cut = NULL;
        for (char *p = copy + 1; *p; ++p)
            if (IsSeparator(*p))
                cut = p;
        if (!cut)
            break; /* nothing above exists: the remaining top is created too */
        if (depth == MAX_DEPTH) {
            ok = Uwp_Fail("Directory path is too deep: %s", path);
            break;
        }
        cuts[depth] = (size_t)(cut - copy);
        separators[depth++] = *cut;
        *cut = '\0';
    }
    /* Then create each missing level on the way back down. */
    if (ok && !IsDirectory(copy) && _mkdir(copy) != 0 && errno != EEXIST)
        ok = Uwp_Fail("mkdir %s: %s", copy, strerror(errno));
    while (ok && depth > 0) {
        --depth;
        copy[cuts[depth]] = separators[depth];
        if (_mkdir(copy) != 0 && errno != EEXIST)
            ok = Uwp_Fail("mkdir %s: %s", copy, strerror(errno));
    }
    free(copy);
    return ok;
}

bool NoHeadless() { return Uwp_Fail("UWP has no headless mode"); }

/* No mouse on a console, and no pointer support yet anywhere. */
void PointerMode(bool enabled) {
    if (enabled)
        Uwp_Fail("UWP has no pointer support yet");
}

void Fullscreen(bool) {}
bool AlwaysFullscreen() { return true; }

const char *PlatformName() { return "UWP"; }

void Shutdown() {
    Uwp_WindowShutdown();
}

PlatformInterface Interface() {
    /* Clocks, CPU and memory probes, the working directory, the install
       folder, directory creation and removal: the null backend's OS calls,
       which a UWP app may make too. Dialogs, message boxes and the
       clipboard stay unsupported, as there. */
    PlatformInterface api = NullPlatform_Interface();
    api.name = "uwp";
    api.capabilities.windowed = false;
    api.capabilities.transient_window_loss = false;
    api.capabilities.touch_overlay = false;
    api.capabilities.touch_recenter = false;
    api.system.shutdown = Shutdown;
    api.system.set_headless = NoHeadless;
    api.system.platform_name = PlatformName;
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
    api.window.primary_display_size = Uwp_DisplaySize;
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
    api.host.pref_path = Preferences;
    api.host.home_path = Home;
    api.host.create_directory = MakeDirectory;
    return api;
}
} // namespace

bool Uwp_Fail(const char *format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof message, format, args);
    va_end(args);
    return NullPlatform_Fail(message);
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
