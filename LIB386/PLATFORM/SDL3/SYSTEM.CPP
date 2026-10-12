#include "PRIVATE.H"

namespace {
PlatformLogFn s_logCallback = NULL;
void *s_logUserdata = NULL;

void SDLCALL LogOutput(void *, int, SDL_LogPriority priority, const char *message) {
    PlatformLogLevel level = PLATFORM_LOG_DEBUG;
    switch (priority) {
    case SDL_LOG_PRIORITY_INFO:
        level = PLATFORM_LOG_INFO;
        break;
    case SDL_LOG_PRIORITY_WARN:
        level = PLATFORM_LOG_WARN;
        break;
    case SDL_LOG_PRIORITY_ERROR:
    case SDL_LOG_PRIORITY_CRITICAL:
        level = PLATFORM_LOG_ERROR;
        break;
    default:
        break;
    }
    if (s_logCallback)
        s_logCallback(s_logUserdata, level, message);
}
} // namespace

extern "C" {

bool SDL3_Init(void) { return SDL_Init(0); }
void SDL3_ClearError(void) { SDL_ClearError(); }

bool SDL3_SetHeadless(void) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    const char *video = SDL_GetHint(SDL_HINT_VIDEO_DRIVER);
    const char *audio = SDL_GetHint(SDL_HINT_AUDIO_DRIVER);
    if (video == NULL || audio == NULL ||
        SDL_strcmp(video, "dummy") != 0 || SDL_strcmp(audio, "dummy") != 0)
        return SDL_SetError("Unable to select dummy video/audio drivers");
    return true;
}

uint64_t SDL3_Ticks(void) { return SDL_GetTicks(); }
uint64_t SDL3_Counter(void) { return SDL_GetPerformanceCounter(); }
uint64_t SDL3_Frequency(void) { return SDL_GetPerformanceFrequency(); }
uint64_t SDL3_ThreadID(void) { return (uint64_t)SDL_GetCurrentThreadID(); }
int32_t SDL3_CPUCount(void) { return SDL_GetNumLogicalCPUCores(); }
int32_t SDL3_RAM(void) { return SDL_GetSystemRAM(); }
bool SDL3_InitVideo(void) { return SDL_InitSubSystem(SDL_INIT_VIDEO); }
bool SDL3_VideoInitialized(void) { return SDL_WasInit(SDL_INIT_VIDEO) != 0; }

void SDL3_SetLogOutput(PlatformLogFn callback, void *userdata) {
    s_logCallback = callback;
    s_logUserdata = userdata;
    SDL_SetLogOutputFunction(callback ? LogOutput : NULL, NULL);
}
}
