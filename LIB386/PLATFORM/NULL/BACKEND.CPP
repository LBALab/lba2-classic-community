#include "PRIVATE.H"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#else
#include <sys/sysinfo.h>
#endif
#endif

namespace {
#ifdef _MSC_VER
__declspec(thread) char s_error[1024];
#else
__thread char s_error[1024];
#endif
bool s_initialized = false;
bool s_fullscreen = false;
uint32_t s_width = 0, s_height = 0;
uint32_t *s_pixels = NULL;
uint8_t s_keyboard[PLATFORM_SCANCODE_COUNT] = {};
char s_basePath[4096] = "";

bool Fail(const char *message) {
    snprintf(s_error, sizeof(s_error), "%s", message);
    Platform_Log(PLATFORM_LOG_ERROR, "%s", s_error);
    return false;
}
bool HostError(const char *operation) {
    char message[1024];
    snprintf(message, sizeof(message), "%s: %s", operation, strerror(errno));
    return Fail(message);
}
const char *Error() { return s_error; }
void ClearError() { s_error[0] = '\0'; }
bool Ready() { return true; }
void Noop() {}
void LogOutput(PlatformLogFn, void *) {}

uint64_t Counter() {
#ifdef _WIN32
    LARGE_INTEGER value;
    if (!QueryPerformanceCounter(&value)) {
        Fail("Unable to read host performance counter");
        return 0;
    }
    return (uint64_t)value.QuadPart;
#else
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        HostError("clock_gettime");
        return 0;
    }
    return (uint64_t)value.tv_sec * 1000000000 + (uint64_t)value.tv_nsec;
#endif
}
uint64_t Frequency() {
#ifdef _WIN32
    LARGE_INTEGER value;
    if (!QueryPerformanceFrequency(&value)) {
        Fail("Unable to read host performance frequency");
        return 0;
    }
    return (uint64_t)value.QuadPart;
#else
    return 1000000000;
#endif
}
uint64_t Ticks() {
    const uint64_t frequency = Frequency();
    const uint64_t counter = Counter();
    return frequency ? (counter / frequency) * 1000 + (counter % frequency) * 1000 / frequency : 0;
}
void Delay(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec remaining = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000};
    while (nanosleep(&remaining, &remaining) != 0)
        if (errno != EINTR) {
            HostError("nanosleep");
            break;
        }
#endif
}
uint64_t ThreadID() {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return (uint64_t)(uintptr_t)pthread_self();
#endif
}
int32_t CPUCount() {
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (int32_t)info.dwNumberOfProcessors;
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count < 1) {
        HostError("sysconf CPU count");
        return 0;
    }
    return (int32_t)count;
#endif
}
int32_t RAM() {
    uint64_t bytes;
#ifdef _WIN32
    MEMORYSTATUSEX info = {};
    info.dwLength = sizeof(info);
    if (!GlobalMemoryStatusEx(&info)) {
        Fail("Unable to read host memory size");
        return 0;
    }
    bytes = info.ullTotalPhys;
#elif defined(__APPLE__)
    size_t size = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &size, NULL, 0) != 0) {
        HostError("sysctl memory size");
        return 0;
    }
#else
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        HostError("sysinfo");
        return 0;
    }
    bytes = (uint64_t)info.totalram * info.mem_unit;
#endif
    const uint64_t mb = bytes / (1024 * 1024);
    return mb > INT_MAX ? INT_MAX : (int32_t)mb;
}
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
char *Copy(const char *text) {
    char *copy = (char *)malloc(strlen(text) + 1);
    if (!copy) {
        Fail("Unable to allocate host path");
        return NULL;
    }
    strcpy(copy, text);
    return copy;
}
char *CurrentDirectory() {
    char path[4096];
#ifdef _WIN32
    if (!_getcwd(path, sizeof(path))) {
#else
    if (!getcwd(path, sizeof(path))) {
#endif
        HostError("getcwd");
        return NULL;
    }
    return Copy(path);
}
const char *BasePath() {
    if (s_basePath[0])
        return s_basePath;
#ifdef _WIN32
    DWORD length = GetModuleFileNameA(NULL, s_basePath, sizeof(s_basePath));
    if (!length || length >= sizeof(s_basePath)) {
        s_basePath[0] = '\0';
        Fail("Unable to locate null platform executable");
        return NULL;
    }
#elif defined(__APPLE__)
    char path[4096];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0 || !realpath(path, s_basePath)) {
        s_basePath[0] = '\0';
        HostError("executable path");
        return NULL;
    }
#else
    ssize_t length = readlink("/proc/self/exe", s_basePath, sizeof(s_basePath) - 1);
    if (length < 0 || (size_t)length >= sizeof(s_basePath) - 1) {
        s_basePath[0] = '\0';
        HostError("executable path");
        return NULL;
    }
    s_basePath[length] = '\0';
#endif
    char *last = strrchr(s_basePath, '/');
#ifdef _WIN32
    char *backslash = strrchr(s_basePath, '\\');
    if (!last || (backslash && backslash > last))
        last = backslash;
#endif
    if (!last) {
        s_basePath[0] = '\0';
        Fail("Executable path has no parent directory");
        return NULL;
    }
    last[1] = '\0';
    return s_basePath;
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
bool Separator(char ch) {
#ifdef _WIN32
    return ch == '/' || ch == '\\';
#else
    return ch == '/';
#endif
}
bool MakeDirectory(const char *path) {
    if (!path || !*path)
        return Fail("Host directory path is empty");
    char *copy = Copy(path);
    if (!copy)
        return false;
    bool ok = true;
    for (size_t i = 1; ok; ++i) {
        if (copy[i] != '\0' && !Separator(copy[i]))
            continue;
        const char saved = copy[i];
        copy[i] = '\0';
#ifdef _WIN32
        int result = (i == 2 && copy[1] == ':') ? 0 : _mkdir(copy);
#else
        int result = mkdir(copy, 0770);
#endif
        if (result != 0) {
            struct stat info;
            if (errno != EEXIST || stat(copy, &info) != 0 || !S_ISDIR(info.st_mode))
                ok = HostError("mkdir");
        }
        copy[i] = saved;
        if (!saved)
            break;
    }
    free(copy);
    return ok;
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
    if (!MakeDirectory(path)) {
        free(path);
        return NULL;
    }
    return path;
}
bool Remove(const char *path) {
    if (!path || !*path)
        return Fail("Host removal path is empty");
    if (remove(path) == 0 || errno == ENOENT)
        return true;
    return HostError("remove");
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
    api.system.get_error = Error;
    api.system.clear_error = ClearError;
    api.system.ticks_ms = Ticks;
    api.system.delay_ms = Delay;
    api.system.performance_counter = Counter;
    api.system.performance_frequency = Frequency;
    api.system.thread_id = ThreadID;
    api.system.cpu_count = CPUCount;
    api.system.ram_mb = RAM;
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
    api.host.base_path = BasePath;
    api.host.current_directory = CurrentDirectory;
    api.host.pref_path = Preferences;
    api.host.home_path = Home;
    api.host.create_directory = MakeDirectory;
    api.host.remove_path = Remove;
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
