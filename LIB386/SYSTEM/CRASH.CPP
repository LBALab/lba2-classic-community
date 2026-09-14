#include <SYSTEM/CRASH.H>
#include <SYSTEM/CRASH_CORE.H>

#include <string.h>

#if defined(_WIN32)
#define CRASH_PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
#define CRASH_PLATFORM_NAME "macOS"
#elif defined(__ANDROID__)
#define CRASH_PLATFORM_NAME "Android"
#elif defined(__linux__)
#define CRASH_PLATFORM_NAME "Linux"
#else
#define CRASH_PLATFORM_NAME "unknown"
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define CRASH_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#define CRASH_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define CRASH_ARCH_NAME "arm64"
#elif defined(__arm__)
#define CRASH_ARCH_NAME "arm"
#else
#define CRASH_ARCH_NAME "unknown"
#endif

#if !defined(CRASH_BACKEND_POSIX) && !defined(CRASH_BACKEND_WINDOWS)
void Crash_Install(const char *logPath, const char *version, const char *commit) {
    (void)logPath;
    (void)version;
    (void)commit;
}

void Crash_PrepareThread(void) {
}
#endif

//----------------------------------------------------------------------------
// Lines.

void CrashLine_Str(CrashLine *line, const char *text) {
    while (*text && line->length < CRASH_LINE_SIZE - 1)
        line->text[line->length++] = *text++;
}

void CrashLine_Begin(CrashLine *line, const char *word) {
    line->length = 0;
    CrashLine_Str(line, "CRASH ");
    CrashLine_Str(line, word);
}

void CrashLine_Hex(CrashLine *line, U64 value) {
    static const char digits[] = "0123456789abcdef";
    char reversed[16];
    int count = 0;
    do {
        reversed[count++] = digits[value & 0xf];
        value >>= 4;
    } while (value);
    CrashLine_Str(line, "0x");
    while (count > 0 && line->length < CRASH_LINE_SIZE - 1)
        line->text[line->length++] = reversed[--count];
}

void CrashLine_Dec(CrashLine *line, S64 value) {
    char reversed[20];
    int count = 0;
    /* Negated as unsigned, so the most negative value has a magnitude. */
    U64 magnitude = value < 0 ? (U64)0 - (U64)value : (U64)value;
    do {
        reversed[count++] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude);
    if (value < 0)
        CrashLine_Str(line, "-");
    while (count > 0 && line->length < CRASH_LINE_SIZE - 1)
        line->text[line->length++] = reversed[--count];
}

void CrashLine_Write(CrashLine *line) {
    line->text[line->length++] = '\n';
    CrashSys_Write(line->text, line->length);
}

static void line_bytes_hex(CrashLine *line, const U8 *bytes, int count, int asUuid) {
    static const char lower[] = "0123456789abcdef";
    static const char upper[] = "0123456789ABCDEF";
    const char *digits = asUuid ? upper : lower;
    int i;
    for (i = 0; i < count; i++) {
        char pair[3];
        pair[0] = digits[bytes[i] >> 4];
        pair[1] = digits[bytes[i] & 0xf];
        pair[2] = '\0';
        CrashLine_Str(line, pair);
        if (asUuid && (i == 3 || i == 5 || i == 7 || i == 9))
            CrashLine_Str(line, "-");
    }
}

//----------------------------------------------------------------------------
// Modules.

static CrashModule s_modules[CRASH_MAX_MODULES];
/* Written only after the entry below it is complete. */
static volatile int s_moduleCount = 0;
static U8 s_moduleUsed[CRASH_MAX_MODULES];

CrashModule *CrashModule_Reserve(void) {
    int index = s_moduleCount;
    if (index >= CRASH_MAX_MODULES)
        return NULL;
    memset(&s_modules[index], 0, sizeof(s_modules[index]));
    return &s_modules[index];
}

void CrashModule_Publish(void) {
    if (s_moduleCount < CRASH_MAX_MODULES)
        s_moduleCount = s_moduleCount + 1;
}

void CrashModule_SetName(CrashModule *module, const char *path) {
    const char *base = path;
    const char *p;
    U32 i = 0;
    for (p = path; *p; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    while (base[i] && i < CRASH_MODULE_NAME_SIZE - 1) {
        module->name[i] = base[i];
        i++;
    }
    module->name[i] = '\0';
}

int CrashModule_Count(void) {
    return s_moduleCount;
}

const CrashModule *CrashModule_Get(int index) {
    return (index >= 0 && index < s_moduleCount) ? &s_modules[index] : NULL;
}

int CrashModule_Find(uintptr_t address) {
    int count = s_moduleCount;
    int i;
    for (i = 0; i < count; i++) {
        if (address >= s_modules[i].start && address < s_modules[i].end)
            return i;
    }
    return -1;
}

void CrashModule_Reset(void) {
    s_moduleCount = 0;
}

static void line_module(CrashLine *line, int index) {
    const CrashModule *module = &s_modules[index];
    CrashLine_Begin(line, "module ");
    CrashLine_Str(line, module->name[0] ? module->name : "?");
    CrashLine_Str(line, " base=");
    CrashLine_Hex(line, module->base);
    CrashLine_Str(line, " id=");
    if (module->idLength)
        line_bytes_hex(line, module->id, module->idLength, module->idIsUuid);
    else
        CrashLine_Str(line, "none");
}

//----------------------------------------------------------------------------
// Build and state.

static char s_version[64] = "unknown";
static char s_commit[64] = "unknown";

static void copy_bounded(char *dst, U32 size, const char *src) {
    U32 i = 0;
    while (src && src[i] && i < size - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void CrashCore_SetBuild(const char *version, const char *commit) {
    copy_bounded(s_version, sizeof(s_version), version);
    copy_bounded(s_commit, sizeof(s_commit), commit);
}

struct CrashState {
    const char *name;
    const void *address;
    U8 size;
    U8 isSigned;
    U8 continues; /* same name as the field before: printed after a comma */
};

static CrashState s_state[CRASH_MAX_STATE];
static int s_stateCount = 0;

void Crash_AddState(const char *name, const void *address, U32 size, S32 isSigned) {
    CrashState *field;
    if (s_stateCount >= CRASH_MAX_STATE || !name || !address)
        return;
    if (size != 1 && size != 2 && size != 4 && size != 8)
        return;
    field = &s_state[s_stateCount];
    field->name = name;
    field->address = address;
    field->size = (U8)size;
    field->isSigned = isSigned ? 1 : 0;
    field->continues = s_stateCount > 0 && !strcmp(s_state[s_stateCount - 1].name, name);
    s_stateCount++;
}

static void line_state_value(CrashLine *line, const CrashState *field) {
    const U8 *bytes = (const U8 *)field->address;
    switch (field->size) {
    case 1: {
        U8 v = bytes[0];
        CrashLine_Dec(line, field->isSigned ? (S64)(S8)v : (S64)v);
        break;
    }
    case 2: {
        U16 v;
        memcpy(&v, bytes, sizeof(v));
        CrashLine_Dec(line, field->isSigned ? (S64)(S16)v : (S64)v);
        break;
    }
    case 4: {
        U32 v;
        memcpy(&v, bytes, sizeof(v));
        CrashLine_Dec(line, field->isSigned ? (S64)(S32)v : (S64)v);
        break;
    }
    default: {
        U64 v;
        memcpy(&v, bytes, sizeof(v));
        if (field->isSigned)
            CrashLine_Dec(line, (S64)v);
        else
            CrashLine_Hex(line, v);
        break;
    }
    }
}

//----------------------------------------------------------------------------
// The block.

static U32 s_frameCount = 0;
static uintptr_t s_frameTail[CRASH_FRAMES_TAIL];

static void line_frame(CrashLine *line, U32 index, uintptr_t address) {
    int module = CrashModule_Find(address);
    CrashLine_Begin(line, "frame ");
    if (index < 10)
        CrashLine_Str(line, "0");
    CrashLine_Dec(line, index);
    CrashLine_Str(line, " ");
    if (module >= 0) {
        s_moduleUsed[module] = 1;
        CrashLine_Str(line, s_modules[module].name);
        CrashLine_Str(line, "+");
        CrashLine_Hex(line, address - s_modules[module].base);
    } else {
        CrashLine_Hex(line, address);
    }
}

void CrashBlock_Begin(void) {
    CrashLine line;
    memset(s_moduleUsed, 0, sizeof(s_moduleUsed));
    s_frameCount = 0;
    CrashLine_Begin(&line, "==== fatal signal ====");
    CrashLine_Write(&line);
}

void CrashBlock_Build(void) {
    CrashLine line;
    CrashLine_Begin(&line, "build ");
    CrashLine_Str(&line, s_version);
    CrashLine_Str(&line, " ");
    CrashLine_Str(&line, s_commit);
    CrashLine_Str(&line, " " CRASH_PLATFORM_NAME " " CRASH_ARCH_NAME);
    CrashLine_Write(&line);
}

void CrashBlock_State(void) {
    CrashLine line;
    int i;
    if (s_stateCount == 0)
        return;
    CrashLine_Begin(&line, "state");
    for (i = 0; i < s_stateCount; i++) {
        const CrashState *field = &s_state[i];
        if (field->continues) {
            CrashLine_Str(&line, ",");
        } else {
            /* A field never splits across lines; a long table continues on
               another state line. */
            if (line.length > CRASH_LINE_SIZE - 96) {
                CrashLine_Write(&line);
                CrashLine_Begin(&line, "state");
            }
            CrashLine_Str(&line, " ");
            CrashLine_Str(&line, field->name);
            CrashLine_Str(&line, "=");
        }
        line_state_value(&line, field);
    }
    CrashLine_Write(&line);
}

void CrashBlock_EngineModule(void) {
    CrashLine line;
    int engine = CrashModule_Find((uintptr_t)&CrashBlock_EngineModule);
    if (engine < 0)
        return;
    s_moduleUsed[engine] = 1;
    line_module(&line, engine);
    CrashLine_Write(&line);
}

void CrashBlock_Frame(uintptr_t address) {
    if (s_frameCount < CRASH_FRAMES_HEAD) {
        CrashLine line;
        line_frame(&line, s_frameCount, address);
        CrashLine_Write(&line);
    } else {
        s_frameTail[(s_frameCount - CRASH_FRAMES_HEAD) % CRASH_FRAMES_TAIL] = address;
    }
    s_frameCount++;
}

void CrashBlock_EndFrames(void) {
    CrashLine line;
    U32 kept;
    U32 first;
    U32 i;
    if (s_frameCount <= CRASH_FRAMES_HEAD)
        return;
    kept = s_frameCount - CRASH_FRAMES_HEAD;
    if (kept > CRASH_FRAMES_TAIL) {
        CrashLine_Begin(&line, "frames skipped ");
        CrashLine_Dec(&line, kept - CRASH_FRAMES_TAIL);
        CrashLine_Write(&line);
        kept = CRASH_FRAMES_TAIL;
    }
    first = s_frameCount - kept;
    for (i = 0; i < kept; i++) {
        U32 index = first + i;
        line_frame(&line, index, s_frameTail[(index - CRASH_FRAMES_HEAD) % CRASH_FRAMES_TAIL]);
        CrashLine_Write(&line);
    }
}

void CrashBlock_Modules(void) {
    CrashLine line;
    int engine = CrashModule_Find((uintptr_t)&CrashBlock_EngineModule);
    int count = s_moduleCount;
    int i;
    for (i = 0; i < count; i++) {
        if (!s_moduleUsed[i] || i == engine)
            continue;
        line_module(&line, i);
        CrashLine_Write(&line);
    }
}

void CrashBlock_End(void) {
    CrashLine line;
    CrashLine_Begin(&line, "==== end ====");
    CrashLine_Write(&line);
}
