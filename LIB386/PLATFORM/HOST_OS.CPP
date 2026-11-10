#include "HOST_OS.H"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
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
char s_basePath[4096] = "";

bool ErrnoFail(const char *operation, const char *path) {
    return path ? HostOs_Fail("%s %s: %s", operation, path, strerror(errno))
                : HostOs_Fail("%s: %s", operation, strerror(errno));
}

bool IsSeparator(char ch) {
#ifdef _WIN32
    return ch == '/' || ch == '\\';
#else
    return ch == '/';
#endif
}

int MakeOne(const char *path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0770);
#endif
}

/* An existing folder is fine; anything else in the way is not. */
bool MakeLevel(const char *path) {
    if (MakeOne(path) == 0)
        return true;
    if (errno == EEXIST && HostOs_IsDirectory(path))
        return true;
    if (errno == EEXIST)
        return HostOs_Fail("mkdir %s: a file is in the way", path);
    return ErrnoFail("mkdir", path);
}
} // namespace

bool HostOs_Fail(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(s_error, sizeof s_error, format, args);
    va_end(args);
    Platform_Log(PLATFORM_LOG_ERROR, "%s", s_error);
    return false;
}

const char *HostOs_Error(void) { return s_error; }
void HostOs_ClearError(void) { s_error[0] = '\0'; }

uint64_t HostOs_Counter(void) {
#ifdef _WIN32
    LARGE_INTEGER value;
    if (!QueryPerformanceCounter(&value)) {
        HostOs_Fail("Unable to read host performance counter");
        return 0;
    }
    return (uint64_t)value.QuadPart;
#else
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        ErrnoFail("clock_gettime", NULL);
        return 0;
    }
    return (uint64_t)value.tv_sec * 1000000000 + (uint64_t)value.tv_nsec;
#endif
}

uint64_t HostOs_Frequency(void) {
#ifdef _WIN32
    LARGE_INTEGER value;
    if (!QueryPerformanceFrequency(&value)) {
        HostOs_Fail("Unable to read host performance frequency");
        return 0;
    }
    return (uint64_t)value.QuadPart;
#else
    return 1000000000;
#endif
}

uint64_t HostOs_Ticks(void) {
    const uint64_t frequency = HostOs_Frequency();
    const uint64_t counter = HostOs_Counter();
    return frequency ? (counter / frequency) * 1000 + (counter % frequency) * 1000 / frequency : 0;
}

void HostOs_Delay(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec remaining = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000};
    while (nanosleep(&remaining, &remaining) != 0)
        if (errno != EINTR) {
            ErrnoFail("nanosleep", NULL);
            break;
        }
#endif
}

uint64_t HostOs_ThreadID(void) {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return (uint64_t)(uintptr_t)pthread_self();
#endif
}

int32_t HostOs_CPUCount(void) {
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (int32_t)info.dwNumberOfProcessors;
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count < 1) {
        ErrnoFail("sysconf CPU count", NULL);
        return 0;
    }
    return (int32_t)count;
#endif
}

int32_t HostOs_RAM(void) {
    uint64_t bytes;
#ifdef _WIN32
    MEMORYSTATUSEX info;
    memset(&info, 0, sizeof info);
    info.dwLength = sizeof(info);
    if (!GlobalMemoryStatusEx(&info)) {
        HostOs_Fail("Unable to read host memory size");
        return 0;
    }
    bytes = info.ullTotalPhys;
#elif defined(__APPLE__)
    size_t size = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &size, NULL, 0) != 0) {
        ErrnoFail("sysctl memory size", NULL);
        return 0;
    }
#else
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        ErrnoFail("sysinfo", NULL);
        return 0;
    }
    bytes = (uint64_t)info.totalram * info.mem_unit;
#endif
    const uint64_t mb = bytes / (1024 * 1024);
    return mb > INT_MAX ? INT_MAX : (int32_t)mb;
}

char *HostOs_Copy(const char *text) {
    char *copy = (char *)malloc(strlen(text) + 1);
    if (!copy) {
        HostOs_Fail("Unable to allocate host path");
        return NULL;
    }
    strcpy(copy, text);
    return copy;
}

char *HostOs_CurrentDirectory(void) {
    char path[4096];
#ifdef _WIN32
    if (!_getcwd(path, sizeof(path))) {
#else
    if (!getcwd(path, sizeof(path))) {
#endif
        ErrnoFail("getcwd", NULL);
        return NULL;
    }
    return HostOs_Copy(path);
}

const char *HostOs_BasePath(void) {
    if (s_basePath[0])
        return s_basePath;
#ifdef _WIN32
    DWORD length = GetModuleFileNameA(NULL, s_basePath, sizeof(s_basePath));
    if (!length || length >= sizeof(s_basePath)) {
        s_basePath[0] = '\0';
        HostOs_Fail("Unable to locate the executable");
        return NULL;
    }
#elif defined(__APPLE__)
    char path[4096];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0 || !realpath(path, s_basePath)) {
        s_basePath[0] = '\0';
        ErrnoFail("executable path", NULL);
        return NULL;
    }
#else
    ssize_t length = readlink("/proc/self/exe", s_basePath, sizeof(s_basePath) - 1);
    if (length < 0 || (size_t)length >= sizeof(s_basePath) - 1) {
        s_basePath[0] = '\0';
        ErrnoFail("executable path", NULL);
        return NULL;
    }
    s_basePath[length] = '\0';
#endif
    char *last = NULL;
    for (char *p = s_basePath; *p; ++p)
        if (IsSeparator(*p))
            last = p;
    if (!last) {
        s_basePath[0] = '\0';
        HostOs_Fail("Executable path has no parent directory");
        return NULL;
    }
    last[1] = '\0';
    return s_basePath;
}

bool HostOs_IsDirectory(const char *path) {
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

bool HostOs_MakeDirectory(const char *path) {
    if (!path || !*path)
        return HostOs_Fail("Host directory path is empty");
    char *copy = HostOs_Copy(path);
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
    while (!HostOs_IsDirectory(copy)) {
        char *cut = NULL;
        for (char *p = copy + 1; *p; ++p)
            if (IsSeparator(*p))
                cut = p;
        if (!cut)
            break; /* nothing above exists: the top is created too */
        if (depth == MAX_DEPTH) {
            ok = HostOs_Fail("Directory path is too deep: %s", path);
            break;
        }
        cuts[depth] = (size_t)(cut - copy);
        separators[depth++] = *cut;
        *cut = '\0';
    }
    /* Then create each missing level on the way back down. */
    if (ok && !HostOs_IsDirectory(copy))
        ok = MakeLevel(copy);
    while (ok && depth > 0) {
        --depth;
        copy[cuts[depth]] = separators[depth];
        ok = MakeLevel(copy);
    }
    free(copy);
    return ok;
}

bool HostOs_Remove(const char *path) {
    if (!path || !*path)
        return HostOs_Fail("Host removal path is empty");
#ifdef _WIN32
    /* The CRT's remove takes files only. */
    if (HostOs_IsDirectory(path))
        return _rmdir(path) == 0 || ErrnoFail("rmdir", path);
#endif
    if (remove(path) == 0 || errno == ENOENT)
        return true;
    return ErrnoFail("remove", path);
}
