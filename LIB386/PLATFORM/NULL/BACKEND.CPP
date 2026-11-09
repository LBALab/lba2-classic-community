#include "PRIVATE.H"
#include "../HOST_OS.H"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
bool s_initialized = false;
bool s_fullscreen = false;
uint32_t s_width = 0, s_height = 0;
uint32_t *s_pixels = NULL;
uint8_t s_keyboard[PLATFORM_SCANCODE_COUNT] = {};

bool Fail(const char *message) { return HostOs_Fail("%s", message); }
bool Ready() { return true; }
void Noop() {}
void LogOutput(PlatformLogFn, void *) {}

const char *PlatformName() {
#ifdef _WIN32
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#else
    return "Linux";
#endif
}

bool WindowInit(const char *title) {
    if (title == NULL)
        return Fail("Null presentation requires a title");
    s_initialized = true;
    return true;
}
void Destroy() {
    free(s_pixels);
    s_pixels = NULL;
    s_width = s_height = 0;
}
void WindowEnd() {
    Destroy();
    s_initialized = false;
    s_fullscreen = false;
}
bool Initialized() { return s_initialized; }
bool Resize(uint32_t width, uint32_t height, bool) {
    if (!s_initialized || width == 0 || height == 0 ||
        width > ((uint32_t)-1) / sizeof(uint32_t) || height > INT_MAX ||
        width > ((size_t)-1) / sizeof(uint32_t) / height)
        return Fail("Invalid null presentation dimensions or lifecycle");
    uint32_t *pixels = (uint32_t *)calloc((size_t)width * height, sizeof(uint32_t));
    if (pixels == NULL)
        return Fail("Unable to allocate null presentation frame");
    free(s_pixels);
    s_pixels = pixels;
    s_width = width;
    s_height = height;
    return true;
}
bool Create(uint32_t width, uint32_t height, bool fullscreen) {
    if (s_pixels)
        return Fail("Null presentation surface already exists");
    if (!Resize(width, height, false))
        return false;
    s_fullscreen = fullscreen;
    return true;
}
bool Size(int32_t *width, int32_t *height) {
    if (!s_pixels || width == NULL || height == NULL)
        return Fail("Null presentation surface is unavailable");
    *width = (int32_t)s_width;
    *height = (int32_t)s_height;
    return true;
}
bool DisplaySize(uint32_t *, uint32_t *) {
    return Fail("Null platform has no physical display");
}
void Coordinates(int32_t x, int32_t y, int32_t *sx, int32_t *sy) {
    if (!sx || !sy) {
        Fail("Invalid null presentation coordinate outputs");
        return;
    }
    *sx = s_width ? (x < 0 ? 0 : x >= (int32_t)s_width ? (int32_t)s_width - 1
                                                       : x)
                  : x;
    *sy = s_height ? (y < 0 ? 0 : y >= (int32_t)s_height ? (int32_t)s_height - 1
                                                         : y)
                   : y;
}
void Fullscreen(bool enabled) { s_fullscreen = enabled; }
bool RequestedFullscreen() { return s_fullscreen; }
bool ActualFullscreen() { return false; }
void DeviceMode(bool enabled) {
    if (enabled)
        Fail("Null platform has no interactive input or synchronized display");
}
bool PresentFilter(PlatformPresentFilter mode) {
    /* Nothing is stretched: the frame is converted at its own size. */
    return mode == PLATFORM_FILTER_NEAREST || Fail("Null platform does not filter its presentation");
}
bool Present(const PlatformFrame *frame) {
    if (!frame || !s_pixels || frame->width != s_width || frame->height != s_height)
        return Fail("Indexed frame does not match null presentation surface");
    if (!Platform_ConvertIndexedFrame(frame, s_pixels, s_width * sizeof(uint32_t)))
        return false;
    if (frame->overlay)
        frame->overlay(s_pixels, s_width * sizeof(uint32_t), s_width, s_height, frame->palette_rgb);
    return true;
}
bool Poll(PlatformEvent *event) {
    if (!event)
        Fail("Null platform event output is missing");
    return false;
}
const uint8_t *Keyboard() { return s_keyboard; }
uint32_t Keycode(int32_t, uint16_t) {
    Fail("Null platform has no keyboard layout");
    return 0;
}
const char *KeyName(int32_t) {
    Fail("Null platform has no keyboard names");
    return "";
}
uint16_t Modifiers() { return 0; }
uint32_t *Gamepads(int32_t *count) {
    if (!count) {
        Fail("Null platform gamepad count output is missing");
        return NULL;
    }
    uint32_t *ids = (uint32_t *)calloc(1, sizeof(uint32_t));
    if (!ids) {
        Fail("Unable to allocate empty gamepad list");
        return NULL;
    }
    *count = 0;
    return ids;
}
PlatformGamepad *OpenPad(uint32_t) {
    Fail("Null platform has no gamepads");
    return NULL;
}
void ClosePad(PlatformGamepad *pad) {
    if (pad)
        Fail("Null platform cannot close a foreign gamepad");
}
const char *PadName(PlatformGamepad *) {
    Fail("Null platform has no gamepads");
    return "";
}
bool PadButton(PlatformGamepad *, PlatformGamepadButton) {
    return Fail("Null platform has no gamepads");
}
int16_t PadAxis(PlatformGamepad *, PlatformGamepadAxis) {
    Fail("Null platform has no gamepads");
    return 0;
}
const char *Home() {
#ifdef _WIN32
    const char *path = getenv("USERPROFILE");
#else
    const char *path = getenv("HOME");
#endif
    if (!path || !*path) {
        Fail("Home directory is unavailable");
        return NULL;
    }
    return path;
}
char *Preferences(const char *org, const char *app) {
    if (!org || !app || !*app || strchr(org, '/') || strchr(org, '\\') ||
        strchr(app, '/') || strchr(app, '\\') || !strcmp(org, "..") || !strcmp(app, "..")) {
        Fail("Invalid preference directory names");
        return NULL;
    }
    const char *root;
    const char *suffix = "";
#ifdef _WIN32
    root = getenv("APPDATA");
#elif defined(__APPLE__)
    root = Home();
    suffix = "/Library/Application Support";
#else
    root = getenv("XDG_DATA_HOME");
    if (!root || root[0] != '/') {
        root = Home();
        suffix = "/.local/share";
    }
#endif
    if (!root || !*root) {
        Fail("Preference directory root is unavailable");
        return NULL;
    }
    const size_t length = strlen(root) + strlen(suffix) + strlen(org) + strlen(app) + 5;
    char *path = (char *)malloc(length);
    if (!path) {
        Fail("Unable to allocate preference path");
        return NULL;
    }
    snprintf(path, length, "%s%s/%s%s%s/", root, suffix, org, *org ? "/" : "", app);
    if (!HostOs_MakeDirectory(path)) {
        free(path);
        return NULL;
    }
    return path;
}
bool Message(PlatformMessageKind, const char *, const char *) {
    return Fail("Null platform has no message-box service");
}
PlatformFolderDialog *BeginDialog(const char *) {
    Fail("Null platform has no folder-dialog service");
    return NULL;
}
PlatformDialogResult PollDialog(PlatformFolderDialog *, const char **) {
    Fail("Null platform has no folder-dialog service");
    return PLATFORM_DIALOG_ERROR;
}
void EndDialog(PlatformFolderDialog *dialog) {
    if (dialog)
        Fail("Null platform cannot release a foreign folder dialog");
}
bool SetClipboard(const char *) { return Fail("Null platform has no clipboard"); }
char *GetClipboard() {
    Fail("Null platform has no clipboard");
    return NULL;
}

PlatformInterface Interface() {
    PlatformInterface api = {};
    api.name = "null";
    api.system.init = Ready;
    api.system.shutdown = WindowEnd;
    api.system.set_headless = Ready;
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
    api.events.init = Ready;
    api.events.shutdown = Noop;
    api.events.pump = Noop;
    api.events.poll = Poll;
    api.window.init = WindowInit;
    api.window.shutdown = WindowEnd;
    api.window.initialized = Initialized;
    api.window.create = Create;
    api.window.destroy = Destroy;
    api.window.resize = Resize;
    api.window.size = Size;
    api.window.display_size = DisplaySize;
    api.window.primary_display_size = DisplaySize;
    api.window.surface_coords = Coordinates;
    api.window.set_fullscreen = Fullscreen;
    api.window.get_fullscreen = RequestedFullscreen;
    api.window.is_fullscreen = ActualFullscreen;
    api.window.set_vsync = DeviceMode;
    api.window.set_present_filter = PresentFilter;
    api.window.set_relative_mouse = DeviceMode;
    api.window.set_mouse_grab = DeviceMode;
    api.window.present = Present;
    api.input.keyboard_state = Keyboard;
    api.input.keycode = Keycode;
    api.input.scancode_name = KeyName;
    api.input.modifiers = Modifiers;
    api.input.text_input = DeviceMode;
    api.input.cursor_visible = DeviceMode;
    api.input.init_gamepads = Ready;
    api.input.shutdown_gamepads = Noop;
    api.input.gamepads = Gamepads;
    api.input.open_gamepad = OpenPad;
    api.input.close_gamepad = ClosePad;
    api.input.gamepad_name = PadName;
    api.input.gamepad_button = PadButton;
    api.input.gamepad_axis = PadAxis;
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

extern "C" const PlatformInterface g_platform = Interface();

#ifdef LBA2_NULL_TESTS
extern "C" const uint32_t *NullPlatform_Frame(uint32_t *width, uint32_t *height) {
    *width = s_width;
    *height = s_height;
    return s_pixels;
}
#endif
