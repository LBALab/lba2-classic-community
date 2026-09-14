#ifndef LIB_SYSTEM_CRASH_CORE
#define LIB_SYSTEM_CRASH_CORE

#include <SYSTEM/ADELINE_TYPES.H>

#include <stdint.h>

/* The platform-independent half of the crash report (SYSTEM/CRASH.H): building
 * lines, the module table, the frame list and the state fields. The platform
 * backends catch the crash and call these; a host test links them alone.
 *
 * Everything here runs inside a signal handler or an exception filter. It takes
 * no lock, allocates nothing, calls nothing from stdio, and reads only memory the
 * tables below own or the state fields were registered with. */

/* The backend that catches crashes here. Elsewhere Crash_Install does nothing. */
#if defined(__APPLE__) || defined(__linux__)
#define CRASH_BACKEND_POSIX 1
#elif defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))
#define CRASH_BACKEND_WINDOWS 1
#endif

enum {
    CRASH_LINE_SIZE = 512,
    /* A windowed macOS engine has about 1050 images loaded. */
    CRASH_MAX_MODULES = 2048,
    CRASH_MODULE_NAME_SIZE = 64,
    CRASH_MODULE_ID_SIZE = 32,
    /* A deep recursion keeps its outermost frames, where the thread began, as
       well as the frames that faulted. */
    CRASH_FRAMES_HEAD = 32,
    CRASH_FRAMES_TAIL = 16,
    /* A walk's bound on frames. Every walk also stops when the stack stops climbing,
       so this is only reached by a deep recursion, whose outermost frames must still
       be reached for the tail: an 8 MB stack of small frames is a few hundred
       thousand. */
    CRASH_MAX_WALK_FRAMES = 1 << 20,
    CRASH_MAX_STATE = 32
};

/* Supplied by the backend, or by a test: writes the bytes of one finished line. */
void CrashSys_Write(const char *data, U32 size);

/* One line of the block. Text past the buffer is dropped, the newline is kept. */
struct CrashLine {
    char text[CRASH_LINE_SIZE];
    U32 length;
};

/* Starts a line with "CRASH " and the leading word. */
void CrashLine_Begin(CrashLine *line, const char *word);
void CrashLine_Str(CrashLine *line, const char *text);
void CrashLine_Hex(CrashLine *line, U64 value); /* 0x-prefixed, lower case */
void CrashLine_Dec(CrashLine *line, S64 value);
void CrashLine_Write(CrashLine *line);

/* A loaded image. Offsets in frame lines are address - base, which is what the
   symbolizer for that platform expects; start and end bound the code. */
struct CrashModule {
    uintptr_t base;
    uintptr_t start;
    uintptr_t end;
    char name[CRASH_MODULE_NAME_SIZE];
    U8 id[CRASH_MODULE_ID_SIZE];
    U8 idLength;
    U8 idIsUuid; /* printed as a UUID (LC_UUID) rather than plain hex */
};

/* Adding is two steps so a handler never reads a half-filled entry: fill the
   slot Reserve returns, then Publish it. Reserve returns NULL once the table is
   full. Additions must not run concurrently with each other. */
CrashModule *CrashModule_Reserve(void);
void CrashModule_Publish(void);
/* Copies the last component of path, after '/' or '\\'. */
void CrashModule_SetName(CrashModule *module, const char *path);
int CrashModule_Count(void);
const CrashModule *CrashModule_Get(int index);
int CrashModule_Find(uintptr_t address); /* index, or -1 */
/* Drops every module, for a backend that rebuilds the table. */
void CrashModule_Reset(void);

/* Copies the build's identity at install. */
void CrashCore_SetBuild(const char *version, const char *commit);

/* The block, in the order it is written. The backend writes the signal and
   register lines itself between Begin and Build, and the chain line before End. */
void CrashBlock_Begin(void);
void CrashBlock_Build(void);
void CrashBlock_State(void);
/* The module this code is linked into, the one a player's report is about. */
void CrashBlock_EngineModule(void);
/* Frame 0 is the faulting pc, the rest return addresses, innermost first. */
void CrashBlock_Frame(uintptr_t address);
void CrashBlock_EndFrames(void);
/* Every module a frame fell in, except the engine's, already written. */
void CrashBlock_Modules(void);
void CrashBlock_End(void);

#endif /* LIB_SYSTEM_CRASH_CORE */
