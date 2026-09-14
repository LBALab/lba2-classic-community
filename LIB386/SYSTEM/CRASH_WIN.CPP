#include <SYSTEM/CRASH_CORE.H>

#if defined(CRASH_BACKEND_WINDOWS)

#include <SYSTEM/CRASH.H>

#include <windows.h>
#include <tlhelp32.h>

#include <signal.h>
#include <string.h>

/* An unhandled exception reaches the filter set with SetUnhandledExceptionFilter,
 * which writes the block and then returns what the filter installed before it
 * returns: MinGW's, which hands the exception on, so the process ends with the
 * exception's code and Windows Error Reporting sees it as if the filter were not
 * there. The filter installs no CRT signal handler for the fault signals, since
 * MinGW's filter would call one and end the process through the C runtime.
 *
 * abort() is not an exception: the C runtime calls its SIGABRT handler, then
 * ends the process with __fastfail, which no filter sees. A SIGABRT handler writes
 * the block and returns, and abort carries on as it would without one.
 *
 * Frames come from RtlVirtualUnwind on a copy of the exception's context, which
 * reads saved registers through stack memory. A vectored handler catches a fault
 * inside that walk and resumes at a snapshot taken before it, so the fault cannot
 * become the crash Windows reports.
 *
 * On a stack smashed badly enough, the filter never runs: before any filter,
 * Windows walks the stack for exception handlers with the same unwind data, and a
 * frame outside the stack ends the search and the process with it. The vectored
 * handler runs before that search, so for an exception that ends a process it
 * repeats the walk's frame check first, and writes the block itself when the check
 * fails. An exception on a stack that walks cleanly is left to the handlers and
 * the filter, since code may still catch it, and so is one below a frame with a
 * handler of its own: Windows calls that handler before it looks further.
 *
 * One thread writes the block. Another that crashes meanwhile waits for the block
 * to be done before it hands its exception on, which would end the process under
 * the writer.
 *
 * The walk state is one per process. A thread takes it before a walk; the
 * vectored handler, which also sees exceptions that code goes on to catch, skips its
 * check while another thread holds it, and the block waits for it. */

enum {
    /* Room kept below a thread's stack for the filter after a stack overflow. */
    STACK_GUARANTEE = 32 * 1024
};

static HANDLE s_log = INVALID_HANDLE_VALUE;
static DWORD s_mainThread = 0;
static DWORD s_prepareSlot = TLS_OUT_OF_INDEXES;
static LPTOP_LEVEL_EXCEPTION_FILTER s_previousFilter = NULL;
static volatile LONG s_reporter = 0; /* id of the thread writing the block, or 0 */
static volatile LONG s_reportDone = 0;

static CONTEXT s_walkContext;
static CONTEXT s_resumeContext;
static volatile LONG s_walking = 0;
static volatile LONG s_walkFaulted = 0;
static volatile LONG s_walkOwner = 0; /* id of the thread using the walk state, or 0 */
static volatile LONG s_stackInvalid = 0;

//----------------------------------------------------------------------------
void CrashSys_Write(const char *data, U32 size) {
    HANDLE handle = s_log != INVALID_HANDLE_VALUE ? s_log : GetStdHandle(STD_ERROR_HANDLE);
    while (size > 0) {
        DWORD written = 0;
        if (!WriteFile(handle, data, size, &written, NULL) || written == 0)
            return;
        data += written;
        size -= written;
    }
}

//----------------------------------------------------------------------------
// Modules.

/* An image is filed by a symbol server under its link timestamp and size; the id
   is the two, in that order. The name falls back to the one in the image's export
   directory, which is readable without the loader's lock. */
static void add_image(uintptr_t base, const char *path) {
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const IMAGE_NT_HEADERS *nt;
    CrashModule *module;
    DWORD stamp;
    DWORD size;
    int i;

    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return;
    nt = (const IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return;
    module = CrashModule_Reserve();
    if (!module)
        return;
    stamp = nt->FileHeader.TimeDateStamp;
    size = nt->OptionalHeader.SizeOfImage;
    module->base = base;
    module->start = base;
    module->end = base + size;
    for (i = 0; i < 4; i++) {
        module->id[i] = (U8)(stamp >> (24 - 8 * i));
        module->id[4 + i] = (U8)(size >> (24 - 8 * i));
    }
    module->idLength = 8;

    if (!path) {
        const IMAGE_DATA_DIRECTORY *exports = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        path = "?";
        if (exports->VirtualAddress && exports->VirtualAddress < size) {
            const IMAGE_EXPORT_DIRECTORY *directory = (const IMAGE_EXPORT_DIRECTORY *)(base + exports->VirtualAddress);
            if (directory->Name && directory->Name < size)
                path = (const char *)(base + directory->Name);
        }
    }
    CrashModule_SetName(module, path);
    CrashModule_Publish();
}

static void add_loaded_images(void) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    MODULEENTRY32 entry;
    if (snapshot == INVALID_HANDLE_VALUE)
        return;
    entry.dwSize = sizeof(entry);
    if (Module32First(snapshot, &entry)) {
        do {
            add_image((uintptr_t)entry.modBaseAddr, entry.szExePath);
        } while (Module32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

//----------------------------------------------------------------------------
// Frames.

__attribute__((noinline)) static void walk_unwind(void) {
    const NT_TIB *tib = (const NT_TIB *)NtCurrentTeb();
    DWORD64 low = (DWORD64)(uintptr_t)tib->StackLimit;
    DWORD64 high = (DWORD64)(uintptr_t)tib->StackBase;
    int frames;

    for (frames = 0; frames < CRASH_MAX_WALK_FRAMES; frames++) {
        DWORD64 pc = s_walkContext.Rip;
        DWORD64 imageBase = 0;
        DWORD64 previousSp = s_walkContext.Rsp;
        PRUNTIME_FUNCTION function;
        PVOID handlerData;
        DWORD64 establisherFrame;

        /* A pc of 0 in frame 0 is a call through a null pointer, and the return
           address it left is found as for a leaf. */
        if (pc == 0 && frames > 0)
            return;
        /* A return address can be the first byte after a call to a function that
           does not return, so it is looked up one byte back. */
        function = RtlLookupFunctionEntry(frames == 0 ? pc : pc - 1, &imageBase, NULL);
        if (imageBase && CrashModule_Find((uintptr_t)pc) < 0)
            add_image((uintptr_t)imageBase, NULL);
        CrashBlock_Frame((uintptr_t)pc);

        if (function) {
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, pc, function, &s_walkContext, &handlerData,
                             &establisherFrame, NULL);
        } else {
            /* A leaf without unwind data: the return address is on top. */
            if (s_walkContext.Rsp < low || s_walkContext.Rsp + 8 > high)
                return;
            s_walkContext.Rip = *(const DWORD64 *)(uintptr_t)s_walkContext.Rsp;
            s_walkContext.Rsp += 8;
        }
        if (s_walkContext.Rsp <= previousSp || s_walkContext.Rsp < low || s_walkContext.Rsp > high)
            return;
    }
}

/* The frame check of Windows' own search for exception handlers: each frame's
   establisher frame must be aligned and inside the thread's stack. Sets
   s_stackInvalid when one is not before a frame with a handler. */
__attribute__((noinline)) static void walk_dispatch_check(void) {
    const NT_TIB *tib = (const NT_TIB *)NtCurrentTeb();
    DWORD64 low = (DWORD64)(uintptr_t)tib->StackLimit;
    DWORD64 high = (DWORD64)(uintptr_t)tib->StackBase;
    int frames;

    for (frames = 0; frames < CRASH_MAX_WALK_FRAMES; frames++) {
        DWORD64 pc = s_walkContext.Rip;
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION function;
        PEXCEPTION_ROUTINE handler = NULL;
        PVOID handlerData;
        DWORD64 establisherFrame = 0;

        if ((pc == 0 && frames > 0) || s_walkContext.Rsp >= high)
            return;
        function = RtlLookupFunctionEntry(frames == 0 ? pc : pc - 1, &imageBase, NULL);
        if (function) {
            handler = RtlVirtualUnwind(UNW_FLAG_EHANDLER, imageBase, pc, function, &s_walkContext, &handlerData,
                                       &establisherFrame, NULL);
        } else {
            if (s_walkContext.Rsp < low || s_walkContext.Rsp + 8 > high) {
                s_stackInvalid = 1;
                return;
            }
            establisherFrame = s_walkContext.Rsp;
            s_walkContext.Rip = *(const DWORD64 *)(uintptr_t)s_walkContext.Rsp;
            s_walkContext.Rsp += 8;
        }
        if ((establisherFrame & 7) || establisherFrame < low || establisherFrame > high) {
            s_stackInvalid = 1;
            return;
        }
        if (handler)
            return;
    }
}

enum { WALK_FAULTED = 1,
       WALK_STACK_INVALID = 2,
       WALK_BUSY = 4 };

/* Runs walk on s_walkContext, a copy of context, holding the walk state. With wait,
   waits up to a second for another thread to release it. Returns WALK_ flags. */
__attribute__((noinline)) static int run_guarded(const CONTEXT *context, void (*walk)(void), int wait) {
    LONG self = (LONG)GetCurrentThreadId();
    ULONGLONG deadline = GetTickCount64() + 1000;
    int result;
    for (;;) {
        LONG owner = InterlockedCompareExchange(&s_walkOwner, self, 0);
        if (owner == 0)
            break;
        /* This thread holds it when its own walk raised an exception the filter saw. */
        if (!wait || owner == self || GetTickCount64() >= deadline)
            return WALK_BUSY;
        Sleep(1);
    }
    s_walkContext = *context;
    s_walkFaulted = 0;
    s_stackInvalid = 0;
    /* A fault in the walk resumes here with s_walkFaulted set. Nothing after this
       line may rely on a local variable set before it. */
    RtlCaptureContext(&s_resumeContext);
    if (!s_walkFaulted) {
        s_walking = 1;
        walk();
        s_walking = 0;
    }
    result = (s_walkFaulted ? WALK_FAULTED : 0) | (s_stackInvalid ? WALK_STACK_INVALID : 0);
    InterlockedExchange(&s_walkOwner, 0);
    return result;
}

static void write_frames(const CONTEXT *context) {
    int result = run_guarded(context, walk_unwind, 1);
    CrashLine line;
    if (result & WALK_BUSY) {
        CrashBlock_Frame((uintptr_t)context->Rip);
        CrashLine_Begin(&line, "unwind not run, another thread held it");
        CrashLine_Write(&line);
    } else if (result & WALK_FAULTED) {
        CrashLine_Begin(&line, "nested EXCEPTION_ACCESS_VIOLATION recovered in unwind");
        CrashLine_Write(&line);
    }
}

//----------------------------------------------------------------------------
// The block.

static const char *exception_name(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return "EXCEPTION_ACCESS_VIOLATION";
    case EXCEPTION_IN_PAGE_ERROR:
        return "EXCEPTION_IN_PAGE_ERROR";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_PRIV_INSTRUCTION:
        return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:
        return "EXCEPTION_INT_OVERFLOW";
    case EXCEPTION_STACK_OVERFLOW:
        return "EXCEPTION_STACK_OVERFLOW";
    case EXCEPTION_BREAKPOINT:
        return "EXCEPTION_BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    default:
        return "?";
    }
}

static void write_thread(CrashLine *line) {
    DWORD tid = GetCurrentThreadId();
    CrashLine_Str(line, tid == s_mainThread ? " thread=main tid=" : " thread=other tid=");
    CrashLine_Dec(line, tid);
}

static void write_registers(const CONTEXT *context) {
    CrashLine line;
    CrashLine_Begin(&line, "reg pc=");
    CrashLine_Hex(&line, context->Rip);
    CrashLine_Str(&line, " fp=");
    CrashLine_Hex(&line, context->Rbp);
    CrashLine_Str(&line, " sp=");
    CrashLine_Hex(&line, context->Rsp);
    CrashLine_Write(&line);
}

static void write_rest_of_block(const CONTEXT *context, const char *chain) {
    CrashLine line;
    CrashBlock_Build();
    write_registers(context);
    CrashBlock_State();
    CrashBlock_EngineModule();
    write_frames(context);
    CrashBlock_EndFrames();
    CrashBlock_Modules();
    CrashLine_Begin(&line, "chain ");
    CrashLine_Str(&line, chain);
    CrashLine_Write(&line);
    CrashBlock_End();
    s_reportDone = 1;
}

static void write_exception_block(EXCEPTION_POINTERS *pointers, const char *chain) {
    const EXCEPTION_RECORD *record = pointers->ExceptionRecord;
    CrashLine line;
    CrashBlock_Begin();
    CrashLine_Begin(&line, "exception ");
    CrashLine_Hex(&line, record->ExceptionCode);
    CrashLine_Str(&line, " ");
    CrashLine_Str(&line, exception_name(record->ExceptionCode));
    if ((record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
        record->NumberParameters >= 2) {
        ULONG_PTR access = record->ExceptionInformation[0];
        CrashLine_Str(&line, " addr=");
        CrashLine_Hex(&line, record->ExceptionInformation[1]);
        CrashLine_Str(&line, access == 0 ? " access=read" : access == 1 ? " access=write"
                                                                        : " access=execute");
    } else {
        CrashLine_Str(&line, " addr=");
        CrashLine_Hex(&line, (uintptr_t)record->ExceptionAddress);
    }
    write_thread(&line);
    CrashLine_Write(&line);
    write_rest_of_block(pointers->ContextRecord, chain);
}

/* Exceptions that end a process unless code catches them. Breakpoints, C++
   exceptions and the thread-naming exception are not among them. */
static int is_fatal_exception(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return 1;
    default:
        return 0;
    }
}

/* Non-zero when this thread is to write the block. Waits up to five seconds for the
   block when another thread writes it. */
static int claim_report(void) {
    LONG self = (LONG)GetCurrentThreadId();
    LONG owner = InterlockedCompareExchange(&s_reporter, self, 0);
    if (owner == 0)
        return 1;
    if (owner != self) {
        ULONGLONG deadline = GetTickCount64() + 5000;
        while (!s_reportDone && GetTickCount64() < deadline)
            Sleep(1);
    }
    return 0;
}

static LONG WINAPI on_vectored_exception(EXCEPTION_POINTERS *pointers) {
    DWORD code = pointers->ExceptionRecord->ExceptionCode;

    if (s_walking && (DWORD)s_walkOwner == GetCurrentThreadId()) {
        if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_IN_PAGE_ERROR &&
            code != EXCEPTION_DATATYPE_MISALIGNMENT)
            return EXCEPTION_CONTINUE_SEARCH;
        s_walking = 0;
        s_walkFaulted = 1;
        /* Carries on right after the snapshot in run_guarded. */
        *pointers->ContextRecord = s_resumeContext;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (!is_fatal_exception(code))
        return EXCEPTION_CONTINUE_SEARCH;
    if (s_reporter) {
        claim_report();
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if ((run_guarded(pointers->ContextRecord, walk_dispatch_check, 0) & (WALK_FAULTED | WALK_STACK_INVALID)) &&
        claim_report())
        write_exception_block(pointers, "search");
    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG WINAPI on_unhandled_exception(EXCEPTION_POINTERS *pointers) {
    if (claim_report())
        write_exception_block(pointers, "filter");
    return s_previousFilter ? s_previousFilter(pointers) : EXCEPTION_CONTINUE_SEARCH;
}

static void on_abort_signal(int sig) {
    static CONTEXT context;
    (void)sig;
    if (claim_report()) {
        CrashLine line;
        RtlCaptureContext(&context);
        CrashBlock_Begin();
        CrashLine_Begin(&line, "signal 22 SIGABRT");
        write_thread(&line);
        CrashLine_Write(&line);
        write_rest_of_block(&context, "return");
    }
    /* Returning lets abort end the process as it does with no handler. */
}

//----------------------------------------------------------------------------
void Crash_PrepareThread(void) {
    ULONG guarantee = STACK_GUARANTEE;
    if (s_prepareSlot == TLS_OUT_OF_INDEXES || TlsGetValue(s_prepareSlot))
        return;
    SetThreadStackGuarantee(&guarantee);
    TlsSetValue(s_prepareSlot, (LPVOID)1);
}

void Crash_Install(const char *logPath, const char *version, const char *commit) {
    static int installed = 0;
    if (installed)
        return;
    installed = 1;

    CrashCore_SetBuild(version, commit);
    if (logPath && logPath[0])
        s_log = CreateFileA(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    s_mainThread = GetCurrentThreadId();
    add_loaded_images();

    s_prepareSlot = TlsAlloc();
    Crash_PrepareThread();

    AddVectoredExceptionHandler(1, on_vectored_exception);
    s_previousFilter = SetUnhandledExceptionFilter(on_unhandled_exception);
    signal(SIGABRT, on_abort_signal);
}

#endif
