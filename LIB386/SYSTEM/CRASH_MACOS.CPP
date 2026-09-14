#if defined(__APPLE__)

#include <SYSTEM/CRASH_CORE.H>
#include <SYSTEM/CRASH_POSIX.H>

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <unistd.h>

/* macOS has no rt_tgsigqueueinfo, and it synthesizes si_code and si_addr for a
 * signal another process sends, so nothing in the handler tells a real fault from
 * a sent SIGSEGV. The fault confirms itself instead: the handler stays installed
 * and returns, a real fault runs the same instruction again and comes straight
 * back, and that second delivery goes to the previous action. A signal that does
 * not come back within the confirm time was sent, and is raised again.
 *
 * The confirm timer is SIGALRM on ITIMER_REAL. Nothing else in the process may
 * use either. */

enum { CONFIRM_MICROSECONDS = 200 * 1000 };

#if defined(__aarch64__)
/* Return addresses saved by arm64e system libraries carry a pointer
   authentication code in their high bits. */
#define CODE_ADDRESS_MASK 0x00007fffffffffffULL
#else
#define CODE_ADDRESS_MASK 0xffffffffffffffffULL
#endif

//----------------------------------------------------------------------------
// Modules.

static void add_image(const struct mach_header *header, const char *path) {
    const struct mach_header_64 *header64 = (const struct mach_header_64 *)header;
    const struct load_command *command;
    CrashModule *module;
    U32 i;

    if (header64->magic != MH_MAGIC_64)
        return;
    module = CrashModule_Reserve();
    if (!module)
        return;
    module->base = (uintptr_t)header;
    module->start = (uintptr_t)header;
    command = (const struct load_command *)(header64 + 1);
    for (i = 0; i < header64->ncmds; i++) {
        if (command->cmd == LC_UUID) {
            memcpy(module->id, ((const struct uuid_command *)command)->uuid, 16);
            module->idLength = 16;
            module->idIsUuid = 1;
        } else if (command->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *segment = (const struct segment_command_64 *)command;
            if (!strcmp(segment->segname, SEG_TEXT))
                module->end = module->start + (uintptr_t)segment->vmsize;
        }
        command = (const struct load_command *)((const char *)command + command->cmdsize);
    }
    CrashModule_SetName(module, path ? path : "?");
    CrashModule_Publish();
}

static void on_add_image(const struct mach_header *header, intptr_t slide) {
    Dl_info info;
    (void)slide;
    add_image(header, (dladdr(header, &info) && info.dli_fname) ? info.dli_fname : NULL);
}

/* dyld is not in the image list, and every main thread's outermost frame is its
   start. */
static void add_dyld(void) {
    struct task_dyld_info dyldInfo;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    const struct dyld_all_image_infos *images;
    if (task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&dyldInfo, &count) != KERN_SUCCESS)
        return;
    images = (const struct dyld_all_image_infos *)(uintptr_t)dyldInfo.all_image_info_addr;
    if (images && images->dyldImageLoadAddress)
        add_image(images->dyldImageLoadAddress, images->dyldPath ? images->dyldPath : "dyld");
}

//----------------------------------------------------------------------------
// Confirmation.

static volatile sig_atomic_t s_pendingSignal = 0;

static void stop_confirm_timer(void) {
    struct itimerval off;
    memset(&off, 0, sizeof(off));
    setitimer(ITIMER_REAL, &off, NULL);
}

static void on_confirm_timeout(int unused) {
    int sig = s_pendingSignal;
    (void)unused;
    if (!sig)
        return;
    s_pendingSignal = 0;
    CrashPosix_RestorePrevious();
    raise(sig);
}

int CrashOs_Reentered(int sig, void *context) {
    (void)context;
    if (!s_pendingSignal || s_pendingSignal != sig)
        return 0;
    stop_confirm_timer();
    s_pendingSignal = 0;
    CrashPosix_RestorePrevious();
    return 1;
}

//----------------------------------------------------------------------------
// Registers and frames.

static uintptr_t context_pc(const ucontext_t *uc) {
#if defined(__aarch64__)
    return (uintptr_t)__darwin_arm_thread_state64_get_pc(uc->uc_mcontext->__ss);
#else
    return (uintptr_t)uc->uc_mcontext->__ss.__rip;
#endif
}

static uintptr_t context_fp(const ucontext_t *uc) {
#if defined(__aarch64__)
    return (uintptr_t)__darwin_arm_thread_state64_get_fp(uc->uc_mcontext->__ss);
#else
    return (uintptr_t)uc->uc_mcontext->__ss.__rbp;
#endif
}

static uintptr_t context_sp(const ucontext_t *uc) {
#if defined(__aarch64__)
    return (uintptr_t)__darwin_arm_thread_state64_get_sp(uc->uc_mcontext->__ss);
#else
    return (uintptr_t)uc->uc_mcontext->__ss.__rsp;
#endif
}

void CrashOs_WriteRegisters(void *context) {
    const ucontext_t *uc = (const ucontext_t *)context;
    CrashLine line;
    CrashLine_Begin(&line, "reg pc=");
    CrashLine_Hex(&line, context_pc(uc));
#if defined(__aarch64__)
    CrashLine_Str(&line, " lr=");
    CrashLine_Hex(&line, (uintptr_t)__darwin_arm_thread_state64_get_lr(uc->uc_mcontext->__ss));
#endif
    CrashLine_Str(&line, " fp=");
    CrashLine_Hex(&line, context_fp(uc));
    CrashLine_Str(&line, " sp=");
    CrashLine_Hex(&line, context_sp(uc));
    CrashLine_Write(&line);
}

/* The walk follows the frame records the ABI keeps in fp: each holds the caller's
   fp and the return address. Records are read only inside this thread's stack and
   must climb it, so a smashed record ends the walk rather than faulting. */
static void walk_frame_records(void *argument) {
    const ucontext_t *uc = (const ucontext_t *)argument;
    pthread_t self = pthread_self();
    uintptr_t high = (uintptr_t)pthread_get_stackaddr_np(self);
    uintptr_t low = high - (uintptr_t)pthread_get_stacksize_np(self);
    uintptr_t fp = context_fp(uc);
    int frames = 1;

    CrashBlock_Frame(context_pc(uc));
#if defined(__aarch64__)
    {
        /* A leaf that saves no frame record faults with its caller only in lr.
           When lr is not the first record's return address it is written as the
           next frame; it may also be a stale address in frame 0's own function,
           which a symbolizer can tell. */
        uintptr_t lr = (uintptr_t)__darwin_arm_thread_state64_get_lr(uc->uc_mcontext->__ss) & CODE_ADDRESS_MASK;
        uintptr_t firstReturn = 0;
        if (fp >= low && fp + 2 * sizeof(uintptr_t) <= high && !(fp & 7))
            firstReturn = ((uintptr_t *)fp)[1] & CODE_ADDRESS_MASK;
        if (lr && lr != firstReturn && CrashModule_Find(lr) >= 0) {
            CrashBlock_Frame(lr);
            frames++;
        }
    }
#endif
    while (frames < CRASH_MAX_WALK_FRAMES) {
        uintptr_t next;
        uintptr_t ret;
        if (fp < low || fp + 2 * sizeof(uintptr_t) > high || (fp & 7))
            return;
        next = ((uintptr_t *)fp)[0];
        ret = ((uintptr_t *)fp)[1] & CODE_ADDRESS_MASK;
        if (ret == 0)
            return;
        CrashBlock_Frame(ret);
        frames++;
        if (next <= fp)
            return;
        fp = next;
    }
}

void CrashOs_WriteFrames(void *context) {
    CrashPosix_Guarded("frames", walk_frame_records, context);
}

//----------------------------------------------------------------------------
// Chaining.

/* True when the instruction before pc is a system call: a signal delivered on its
   return was raised by the process itself or sent while the thread waited in the
   kernel, and the instruction would not fault again. */
static int after_system_call(uintptr_t pc) {
#if defined(__aarch64__)
    uintptr_t before = pc - 4;
    U32 instruction;
    int module = CrashModule_Find(before);
    if (module < 0 || CrashModule_Find(pc) != module)
        return 0;
    memcpy(&instruction, (const void *)before, sizeof(instruction));
    return (instruction & 0xffe0001f) == 0xd4000001; /* svc #imm */
#else
    uintptr_t before = pc - 2;
    const U8 *bytes;
    int module = CrashModule_Find(before);
    if (module < 0 || CrashModule_Find(pc) != module)
        return 0;
    bytes = (const U8 *)before;
    return bytes[0] == 0x0f && bytes[1] == 0x05; /* syscall */
#endif
}

static int raise_now(int sig, void *context) {
#if defined(__aarch64__)
    /* arm64 raises no SIGFPE from hardware: integer division by zero does not trap. */
    if (sig == SIGFPE)
        return 1;
#endif
    return sig == SIGABRT || after_system_call(context_pc((const ucontext_t *)context));
}

const char *CrashOs_ChainName(int sig, void *context) {
    return raise_now(sig, context) ? "raise" : "confirm";
}

void CrashOs_HandOn(int sig, siginfo_t *info, void *context) {
    struct itimerval timer;
    (void)info;
    if (raise_now(sig, context)) {
        /* Blocked until the handler returns, then delivered to the previous action. */
        CrashPosix_RestorePrevious();
        raise(sig);
        return;
    }
    s_pendingSignal = sig;
    memset(&timer, 0, sizeof(timer));
    timer.it_value.tv_usec = CONFIRM_MICROSECONDS;
    setitimer(ITIMER_REAL, &timer, NULL);
}

//----------------------------------------------------------------------------
int CrashOs_ActionFlags(void) {
    return 0;
}

void CrashOs_Prepare(void) {
    struct sigaction alarm;
    add_dyld();
    _dyld_register_func_for_add_image(on_add_image);

    memset(&alarm, 0, sizeof(alarm));
    alarm.sa_handler = on_confirm_timeout;
    alarm.sa_flags = SA_ONSTACK;
    sigemptyset(&alarm.sa_mask);
    sigaction(SIGALRM, &alarm, NULL);
}

U64 CrashOs_ThreadId(int *isMain) {
    U64 tid = 0;
    pthread_threadid_np(NULL, &tid);
    *isMain = pthread_main_np() != 0;
    return tid;
}

S64 CrashOs_Sender(const siginfo_t *info) {
    /* si_pid is 0 and si_code synthesized for a signal another process sends. */
    (void)info;
    return 0;
}

#endif
