#include <SYSTEM/CRASH_CORE.H>

#if defined(CRASH_BACKEND_POSIX) && defined(__linux__)

#include <SYSTEM/CRASH_POSIX.H>

#include <elf.h>
#include <errno.h>
#include <link.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include <unwind.h>

/* Linux hands a signal on by queueing the original siginfo to the crashing thread
 * with rt_tgsigqueueinfo and returning. The signal stays blocked until the handler
 * returns, then reaches the previous action: a fault arrives before its instruction
 * runs again, and a sent or raised signal keeps its code and sender. The core and a
 * sanitizer's report then read as if the handler had not been there. raise()
 * instead would put the handler on the crashing stack and make the code SI_TKILL.
 *
 * Frames come from the unwinder, run from inside the handler: at -O3 on x86-64 the
 * frame pointer is a general register, so a frame-record walk stops at frame 0. It
 * reads through stack contents, so it runs under the guard. */

enum { MAX_WALK_FRAMES = 4096 };

//----------------------------------------------------------------------------
// Modules.

static char s_executablePath[256] = "";

/* Fills module from the program headers loaded at bias: the executable segments
   bound it, and the GNU build ID note identifies it. */
static void read_program_headers(CrashModule *module, uintptr_t bias, const ElfW(Phdr) * headers, int count) {
    uintptr_t start = UINTPTR_MAX;
    uintptr_t end = 0;
    int i;
    module->base = bias;
    for (i = 0; i < count; i++) {
        const ElfW(Phdr) *header = &headers[i];
        if (header->p_type == PT_LOAD && (header->p_flags & PF_X)) {
            uintptr_t segment = bias + header->p_vaddr;
            if (segment < start)
                start = segment;
            if (segment + header->p_memsz > end)
                end = segment + header->p_memsz;
        } else if (header->p_type == PT_NOTE && module->idLength == 0) {
            size_t align = header->p_align == 8 ? 8 : 4;
            const U8 *note = (const U8 *)(bias + header->p_vaddr);
            const U8 *notesEnd = note + header->p_memsz;
            while (note + sizeof(ElfW(Nhdr)) <= notesEnd) {
                const ElfW(Nhdr) *noteHeader = (const ElfW(Nhdr) *)note;
                const U8 *name = note + sizeof(ElfW(Nhdr));
                const U8 *desc = name + ((noteHeader->n_namesz + align - 1) & ~(align - 1));
                if (noteHeader->n_type == NT_GNU_BUILD_ID && noteHeader->n_namesz == 4 && !memcmp(name, "GNU", 4)) {
                    U32 length = noteHeader->n_descsz;
                    if (length > CRASH_MODULE_ID_SIZE)
                        length = CRASH_MODULE_ID_SIZE;
                    memcpy(module->id, desc, length);
                    module->idLength = (U8)length;
                    break;
                }
                note = desc + ((noteHeader->n_descsz + align - 1) & ~(align - 1));
            }
        }
    }
    module->start = start == UINTPTR_MAX ? 0 : start;
    module->end = end;
}

static int on_loaded_object(struct dl_phdr_info *info, size_t size, void *unused) {
    CrashModule *module = CrashModule_Reserve();
    (void)size;
    (void)unused;
    if (!module)
        return 1;
    read_program_headers(module, (uintptr_t)info->dlpi_addr, info->dlpi_phdr, info->dlpi_phnum);
    if (module->end == 0)
        return 0;
    CrashModule_SetName(module, info->dlpi_name && info->dlpi_name[0] ? info->dlpi_name : s_executablePath);
    CrashModule_Publish();
    return 0;
}

/* Libraries loaded after install, audio and video drivers among them, are found
   in the loader's own list, read in place: dl_iterate_phdr would take the loader's
   lock, which the crashing thread may hold. */
static void add_late_modules(void *unused) {
    struct link_map *map;
    (void)unused;
    for (map = _r_debug.r_map; map; map = map->l_next) {
        uintptr_t bias = (uintptr_t)map->l_addr;
        const ElfW(Ehdr) * elf;
        CrashModule *module;
        int known = 0;
        int i;
        if (bias == 0 || !map->l_name || !map->l_name[0])
            continue;
        for (i = 0; i < CrashModule_Count(); i++) {
            if (CrashModule_Get(i)->base == bias) {
                known = 1;
                break;
            }
        }
        if (known)
            continue;
        elf = (const ElfW(Ehdr) *)bias;
        if (memcmp(elf->e_ident, ELFMAG, SELFMAG) != 0)
            continue;
        module = CrashModule_Reserve();
        if (!module)
            return;
        read_program_headers(module, bias, (const ElfW(Phdr) *)(bias + elf->e_phoff), elf->e_phnum);
        if (module->end == 0)
            continue;
        CrashModule_SetName(module, map->l_name);
        CrashModule_Publish();
    }
}

//----------------------------------------------------------------------------
// Registers.

struct Registers {
    uintptr_t pc;
    uintptr_t sp;
    uintptr_t fp;
    uintptr_t lr; /* 0 where the architecture has none */
};

static Registers read_registers(const ucontext_t *uc) {
    Registers regs;
    memset(&regs, 0, sizeof(regs));
#if defined(__x86_64__)
    regs.pc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
    regs.sp = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
    regs.fp = (uintptr_t)uc->uc_mcontext.gregs[REG_RBP];
#elif defined(__i386__)
    regs.pc = (uintptr_t)uc->uc_mcontext.gregs[REG_EIP];
    regs.sp = (uintptr_t)uc->uc_mcontext.gregs[REG_ESP];
    regs.fp = (uintptr_t)uc->uc_mcontext.gregs[REG_EBP];
#elif defined(__aarch64__)
    regs.pc = (uintptr_t)uc->uc_mcontext.pc;
    regs.sp = (uintptr_t)uc->uc_mcontext.sp;
    regs.fp = (uintptr_t)uc->uc_mcontext.regs[29];
    regs.lr = (uintptr_t)uc->uc_mcontext.regs[30];
#elif defined(__arm__)
    regs.pc = (uintptr_t)uc->uc_mcontext.arm_pc;
    regs.sp = (uintptr_t)uc->uc_mcontext.arm_sp;
    regs.fp = (uintptr_t)uc->uc_mcontext.arm_fp;
    regs.lr = (uintptr_t)uc->uc_mcontext.arm_lr;
#endif
    return regs;
}

void CrashOs_WriteRegisters(void *context) {
    Registers regs = read_registers((const ucontext_t *)context);
    CrashLine line;
    CrashLine_Begin(&line, "reg pc=");
    CrashLine_Hex(&line, regs.pc);
    if (regs.lr) {
        CrashLine_Str(&line, " lr=");
        CrashLine_Hex(&line, regs.lr);
    }
    CrashLine_Str(&line, " fp=");
    CrashLine_Hex(&line, regs.fp);
    CrashLine_Str(&line, " sp=");
    CrashLine_Hex(&line, regs.sp);
    CrashLine_Write(&line);
}

//----------------------------------------------------------------------------
// Frames.

struct UnwindWalk {
    uintptr_t pc;
    int reachedFault; /* past the handler's own frames and the signal trampoline */
    int frames;
};

static _Unwind_Reason_Code on_unwind_frame(struct _Unwind_Context *context, void *argument) {
    UnwindWalk *walk = (UnwindWalk *)argument;
    int beforeInstruction = 0;
    uintptr_t ip = (uintptr_t)_Unwind_GetIPInfo(context, &beforeInstruction);
    if (!walk->reachedFault) {
        /* The frame the signal interrupted is marked as a signal frame, and its ip
           is the faulting pc itself rather than a return address. */
        if (ip == walk->pc && beforeInstruction)
            walk->reachedFault = 1;
        return _URC_NO_REASON;
    }
    if (ip == 0 || ++walk->frames >= MAX_WALK_FRAMES)
        return _URC_END_OF_STACK;
    CrashBlock_Frame(ip);
    return _URC_NO_REASON;
}

static void walk_unwind(void *argument) {
    _Unwind_Backtrace(on_unwind_frame, argument);
}

static _Unwind_Reason_Code on_prime_frame(struct _Unwind_Context *context, void *argument) {
    (void)context;
    (void)argument;
    return _URC_END_OF_STACK;
}

void CrashOs_WriteFrames(void *context) {
    UnwindWalk walk;
    walk.pc = read_registers((const ucontext_t *)context).pc;
    walk.reachedFault = 0;
    walk.frames = 0;
    CrashPosix_Guarded("modules", add_late_modules, NULL);
    CrashBlock_Frame(walk.pc);
    CrashPosix_Guarded("unwind", walk_unwind, &walk);
}

//----------------------------------------------------------------------------
// Chaining.

const char *CrashOs_ChainName(int sig, void *context) {
    (void)sig;
    (void)context;
    return "queue";
}

void CrashOs_HandOn(int sig, siginfo_t *info, void *context) {
    (void)context;
    CrashPosix_RestorePrevious();
    if (syscall(SYS_rt_tgsigqueueinfo, getpid(), (pid_t)syscall(SYS_gettid), sig, info) != 0)
        raise(sig);
}

int CrashOs_Reentered(int sig, void *context) {
    (void)sig;
    (void)context;
    return 0;
}

//----------------------------------------------------------------------------
int CrashOs_ActionFlags(void) {
    return 0;
}

void CrashOs_Prepare(void) {
    ssize_t length = readlink("/proc/self/exe", s_executablePath, sizeof(s_executablePath) - 1);
    s_executablePath[length > 0 ? length : 0] = '\0';
    dl_iterate_phdr(on_loaded_object, NULL);

    /* The unwinder finds its tables lazily; that first lookup must not happen
       inside the handler. */
    _Unwind_Backtrace(on_prime_frame, NULL);
}

U64 CrashOs_ThreadId(int *isMain) {
    pid_t tid = (pid_t)syscall(SYS_gettid);
    *isMain = tid == getpid();
    return (U64)tid;
}

S64 CrashOs_Sender(const siginfo_t *info) {
    /* SI_USER, SI_QUEUE and SI_TKILL are at or below zero; the kernel's own
       codes are positive and carry no sender. */
    return info->si_code <= 0 ? (S64)info->si_pid : 0;
}

#endif
