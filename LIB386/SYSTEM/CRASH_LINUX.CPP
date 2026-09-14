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
#include <sys/uio.h>
#include <unistd.h>
#if !defined(__ANDROID__)
#include <unwind.h>
#endif

#if !defined(SA_EXPOSE_TAGBITS)
#define SA_EXPOSE_TAGBITS 0x00000800
#endif

/* Linux and Android hand a signal on by queueing the original siginfo to the
 * crashing thread with rt_tgsigqueueinfo and returning. The signal stays blocked
 * until the handler returns, then reaches the previous action: a fault arrives
 * before its instruction runs again, and a sent or raised signal keeps its code and
 * sender. The core or tombstone and a sanitizer's report then read as if the
 * handler had not been there. raise() instead would put the handler on the crashing
 * stack and make the code SI_TKILL.
 *
 * On Linux, frames come from the unwinder, run from inside the handler: at -O3 on
 * x86-64 the frame pointer is a general register, so a frame-record walk stops at
 * frame 0. It reads through stack contents, so it runs under the guard.
 *
 * On Android an unwinder that faults on a smashed stack leaves its own frames in
 * the tombstone, so frames come from the frame-record walk instead, reading
 * through process_vm_readv, which fails on a bad address rather than faulting.
 * Modules are the ones loaded at install. */

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

#if !defined(__ANDROID__)
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
#endif

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

#if !defined(__ANDROID__)

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
    if (ip == 0 || ++walk->frames >= CRASH_MAX_WALK_FRAMES)
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

#if defined(__x86_64__) || defined(__i386__) || defined(__aarch64__)
struct ReturnStep {
    Registers regs;
    uintptr_t pc;
    uintptr_t sp;
};

/* The pc and sp a call leaves its caller with when it returns at once. */
static void read_return_step(void *argument) {
    ReturnStep *step = (ReturnStep *)argument;
#if defined(__aarch64__)
    step->pc = step->regs.lr;
    step->sp = step->regs.sp;
#else
    step->pc = *(const uintptr_t *)step->regs.sp;
    step->sp = step->regs.sp + sizeof(uintptr_t);
#endif
}

static void write_pc_sp(ucontext_t *uc, uintptr_t pc, uintptr_t sp) {
#if defined(__x86_64__)
    uc->uc_mcontext.gregs[REG_RIP] = (greg_t)pc;
    uc->uc_mcontext.gregs[REG_RSP] = (greg_t)sp;
#elif defined(__i386__)
    uc->uc_mcontext.gregs[REG_EIP] = (greg_t)pc;
    uc->uc_mcontext.gregs[REG_ESP] = (greg_t)sp;
#else
    uc->uc_mcontext.pc = pc;
    uc->uc_mcontext.sp = sp;
#endif
}
#endif

void CrashOs_WriteFrames(void *context) {
    Registers regs = read_registers((const ucontext_t *)context);
    UnwindWalk walk;
    walk.pc = regs.pc;
    walk.reachedFault = 0;
    walk.frames = 0;
    CrashPosix_Guarded("modules", add_late_modules, NULL);
    CrashBlock_Frame(walk.pc);
    CrashPosix_Guarded("unwind", walk_unwind, &walk);

#if defined(__x86_64__) || defined(__i386__) || defined(__aarch64__)
    /* A call through a null or wild pointer faults at a pc in no module, which has
       no unwind information, so the walk ends there. The call's return address
       leads on: the walk runs again on the context rewritten as if the call had
       returned, which the unwinder reads through the signal frame, and the context
       is put back before the signal is handed on. */
    if (walk.frames == 0 && CrashModule_Find(regs.pc) < 0) {
        ReturnStep step;
        step.regs = regs;
        step.pc = 0;
        if (CrashPosix_Guarded("return", read_return_step, &step) == 0 && step.pc) {
            CrashBlock_Frame(step.pc);
            walk.pc = step.pc;
            walk.reachedFault = 0;
            write_pc_sp((ucontext_t *)context, step.pc, step.sp);
            CrashPosix_Guarded("unwind", walk_unwind, &walk);
            write_pc_sp((ucontext_t *)context, regs.pc, regs.sp);
        }
    }
#endif
}

#else

static int s_processReadable = 0;

static int read_words(uintptr_t address, uintptr_t words[2]) {
    struct iovec local;
    struct iovec remote;
    local.iov_base = words;
    local.iov_len = 2 * sizeof(uintptr_t);
    remote.iov_base = (void *)address;
    remote.iov_len = 2 * sizeof(uintptr_t);
    return syscall(SYS_process_vm_readv, getpid(), &local, 1, &remote, 1, 0) == (long)(2 * sizeof(uintptr_t));
}

#if defined(__aarch64__)
/* The top byte of a pointer may carry a memory tag, and a return address a pointer
   authentication code below it. */
#define CODE_ADDRESS_MASK 0x0000ffffffffffffULL

static void walk_frame_records(void *argument) {
    Registers regs = read_registers((const ucontext_t *)argument);
    uintptr_t lr = regs.lr & CODE_ADDRESS_MASK;
    uintptr_t fp = regs.fp;
    uintptr_t record[2];
    uintptr_t firstReturn = 0;
    int frames = 1;

    CrashBlock_Frame(regs.pc);
    if (!s_processReadable) {
        /* Without a reader that fails safely, no frame record is read. */
        if (lr && CrashModule_Find(lr) >= 0)
            CrashBlock_Frame(lr);
        return;
    }
    /* A leaf that saves no frame record faults with its caller only in lr. When lr
       is not the first record's return address it is written as the next frame; it
       may also be a stale address in frame 0's own function, which a symbolizer can
       tell. */
    if (fp && !(fp & 7) && read_words(fp, record))
        firstReturn = record[1] & CODE_ADDRESS_MASK;
    if (lr && lr != firstReturn && CrashModule_Find(lr) >= 0) {
        CrashBlock_Frame(lr);
        frames++;
    }
    while (frames < CRASH_MAX_WALK_FRAMES) {
        uintptr_t ret;
        if (fp == 0 || (fp & 7) || fp < regs.sp || !read_words(fp, record))
            return;
        ret = record[1] & CODE_ADDRESS_MASK;
        if (ret == 0)
            return;
        CrashBlock_Frame(ret);
        frames++;
        if (record[0] <= fp)
            return;
        fp = record[0];
    }
}
#else
/* armv7 mixes ARM and Thumb frame layouts, so only frame 0 and lr are written. */
static void walk_frame_records(void *argument) {
    Registers regs = read_registers((const ucontext_t *)argument);
    CrashBlock_Frame(regs.pc);
    if (regs.lr && CrashModule_Find(regs.lr) >= 0)
        CrashBlock_Frame(regs.lr);
}
#endif

/* No guard: the walk reads stack memory only through process_vm_readv, so it
   cannot fault, and the guard's mask changes would pass through libsigchain,
   which leaves out the signals ART claims when a mask is set. SIGSEGV would come
   out of the handler unblocked, and the signal handed on would arrive inside it
   rather than after it returns. */
void CrashOs_WriteFrames(void *context) {
    walk_frame_records(context);
}

#endif

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
#if defined(__ANDROID__)
    /* Keeps the memory tag in si_addr, as debuggerd's own handler does. */
    return SA_EXPOSE_TAGBITS;
#else
    return 0;
#endif
}

void CrashOs_Prepare(void) {
    ssize_t length = readlink("/proc/self/exe", s_executablePath, sizeof(s_executablePath) - 1);
    s_executablePath[length > 0 ? length : 0] = '\0';
    dl_iterate_phdr(on_loaded_object, NULL);

#if !defined(__ANDROID__)
    /* The unwinder finds its tables lazily; that first lookup must not happen
       inside the handler. */
    _Unwind_Backtrace(on_prime_frame, NULL);
#else
    {
        /* process_vm_readv on itself may be refused by a seccomp policy; the walk
           reads nothing then. */
        uintptr_t probe[2];
        uintptr_t copy[2];
        probe[0] = (uintptr_t)&probe;
        probe[1] = (uintptr_t)&copy;
        copy[0] = copy[1] = 0;
        s_processReadable = read_words((uintptr_t)probe, copy) && copy[0] == probe[0] && copy[1] == probe[1];
    }
#endif
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
