#include <SYSTEM/FILES.H>

#include <SYSTEM/DISCIMG.H>
#include <SYSTEM/LIMITS.H>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // MoveFileExA (atomic replace)
#include <io.h>      // _commit (fsync equivalent)
#include <process.h> // _getpid
#endif

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Private definitions -----------------------------------------------------
#ifndef _WIN32 // Portability - O_BINARY exists only on Windows platform
#define O_BINARY 0
#endif // _WIN32

#define MODE_READ (O_RDONLY | O_BINARY)
#define MODE_WRITE (O_WRONLY | O_BINARY | O_CREAT | O_TRUNC)
#define MODE_RW (O_RDWR | O_BINARY | O_CREAT)

// --- Private functions -------------------------------------------------------
S32 OpenMode(const char *filename, int mode) {
    assert(filename != NULL);
    S32 handle = open(filename, mode, (S_IRUSR | S_IWUSR));
    return (handle == -1) ? 0 : handle;
}

// --- Public functions --------------------------------------------------------
// OpenRead resolves the filesystem first, then falls back to a mounted disc
// image (DiscImage_OpenRead returns 0 when nothing is mounted or the file is
// not in the image, so installs without an image behave exactly as before).
// The disc handle it returns is tagged; Read/Seek/Close dispatch on it. Writes
// never touch the image.
S32 OpenRead(const char *filename) {
    S32 handle = OpenMode(filename, MODE_READ);
    if (handle) {
        return handle;
    }
    return DiscImage_OpenRead(filename);
}
S32 OpenWrite(const char *filename) { return OpenMode(filename, MODE_WRITE); }
S32 OpenReadWrite(const char *filename) { return OpenMode(filename, MODE_RW); }

S32 Close(S32 handle) {
    if (DiscImage_IsHandle(handle)) {
        return DiscImage_Close(handle);
    }
    assert(handle > 0);
    return close(handle);
}

S32 Read(S32 handle, void *buffer, U32 maxReadBytes) {
    if (DiscImage_IsHandle(handle)) {
        return DiscImage_Read(handle, buffer, maxReadBytes);
    }
    assert(handle > 0);
    assert(buffer != NULL);
    return read(handle, buffer, maxReadBytes);
}

S32 Write(S32 handle, const void *buffer, U32 maxWriteBytes) {
    assert(handle > 0);
    assert(buffer != NULL);
    return write(handle, buffer, maxWriteBytes);
}

S32 Seek(S32 handle, S32 offset, ESeekOrigin origin) {
    if (DiscImage_IsHandle(handle)) {
        return DiscImage_Seek(handle, offset, (S32)origin);
    }
    assert(handle > 0);

    int stdOrigin;
    switch (origin) {
    case SEEK_FROM_START:
        stdOrigin = SEEK_SET;
        break;
    case SEEK_FROM_CURRENT:
        stdOrigin = SEEK_CUR;
        break;
    case SEEK_FROM_END:
        stdOrigin = SEEK_END;
        break;
    default:
        assert(false);
        break;
    }

    return lseek(handle, offset, stdOrigin);
}

U32 Delete(const char *filename) {
    assert(filename != NULL);
    S32 result = unlink(filename);
    return (result != -1);
}

S32 Touch(const char *filename) {
    assert(filename != NULL);
    return utime(filename, NULL);
}

U32 FileSize(const char *filename) {
    assert(filename != NULL);

    S32 handle = OpenRead(filename);
    if (!handle) {
        return 0;
    }

    // Seek/Close dispatch on the handle, so a disc-image file is sized from its
    // directory record (no data read) just as a real file is sized via lseek.
    S32 fsize = Seek(handle, 0, SEEK_FROM_END);
    Close(handle);

    return (U32)((fsize == -1) ? 0 : fsize);
}

bool ExistsFileOrDir(const char *path) {
    assert(path != NULL);

    struct stat status = {};
    if (stat(path, &status) == 0) {
        return true;
    }
    return DiscImage_Exists(path);
}

bool IsDirectory(const char *path) {
    assert(path != NULL);

    struct stat status = {};
    return ((stat(path, &status) == 0) && ((status.st_mode & S_IFMT) == S_IFDIR));
}

// --- Atomic (crash-safe) writes ----------------------------------------------
// A small fixed table maps an in-flight atomic-write handle to its temp path and
// final destination. The config writer is the only caller and never has more
// than one write in flight, so a handful of slots is ample. A handle of 0 marks
// a free slot -- 0 is never a valid write handle in this layer (OpenMode returns
// 0 on failure, and the engine treats fd 0 as invalid throughout).
#define ATOMIC_WRITE_SLOTS 4

typedef struct {
    S32 handle;
    char finalPath[ADELINE_MAX_PATH];
    char tempPath[ADELINE_MAX_PATH];
} AtomicWriteSlot;

static AtomicWriteSlot s_atomicWriteSlots[ATOMIC_WRITE_SLOTS];

// Returns the slot index holding `handle`, or -1 if none. Pass 0 to find a free
// slot.
static int AtomicSlotFind(S32 handle) {
    for (int i = 0; i < ATOMIC_WRITE_SLOTS; i++) {
        if (s_atomicWriteSlots[i].handle == handle) {
            return i;
        }
    }
    return -1;
}

// Flush a write handle's data to stable storage before the rename. Without it, a
// crash just after the rename could expose a file the directory says is N bytes
// but whose contents never reached the disk. Returns 0 on success.
static int AtomicFlush(S32 handle) {
#ifdef _WIN32
    return _commit(handle);
#else
    return fsync(handle);
#endif
}

// Atomically replace dst with tmp. POSIX rename() replaces in a single step;
// Windows rename() fails if dst already exists, so use MoveFileEx with
// REPLACE_EXISTING (the documented same-volume atomic replace). Returns 0 on
// success.
static int AtomicRename(const char *tmp, const char *dst) {
#ifdef _WIN32
    return MoveFileExA(tmp, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
#else
    return rename(tmp, dst);
#endif
}

static int AtomicGetPid(void) {
#ifdef _WIN32
    return (int)_getpid();
#else
    return (int)getpid();
#endif
}

S32 OpenWriteAtomic(const char *finalPath) {
    assert(finalPath != NULL);

    const int slot = AtomicSlotFind(0); // first free slot
    if (slot < 0) {
        return 0; // too many concurrent atomic writes
    }

    // Build a unique temp name alongside the target so the later rename stays on
    // the same filesystem (a cross-filesystem rename is a copy, not atomic). The
    // pid + a process-local counter keeps names distinct across processes;
    // O_EXCL guards against any residual collision.
    static int counter = 0;
    char tempPath[ADELINE_MAX_PATH];
    S32 handle = 0;
    for (int attempt = 0; attempt < 64; attempt++) {
        const int n = snprintf(tempPath, sizeof(tempPath), "%s.tmp.%d.%d",
                               finalPath, AtomicGetPid(), ++counter);
        if (n < 0 || (size_t)n >= sizeof(tempPath)) {
            return 0; // path + temp suffix does not fit; fail rather than corrupt
        }
        const int fd = open(tempPath, O_WRONLY | O_CREAT | O_EXCL | O_BINARY,
                            (S_IRUSR | S_IWUSR));
        if (fd != -1) {
            handle = fd;
            break;
        }
        if (errno != EEXIST) {
            return 0; // real error (permissions, missing directory, ...)
        }
        // EEXIST: a stale temp from this pid+counter; bump and retry.
    }
    if (!handle) {
        return 0;
    }

    s_atomicWriteSlots[slot].handle = handle;
    strncpy(s_atomicWriteSlots[slot].finalPath, finalPath, ADELINE_MAX_PATH - 1);
    s_atomicWriteSlots[slot].finalPath[ADELINE_MAX_PATH - 1] = '\0';
    strncpy(s_atomicWriteSlots[slot].tempPath, tempPath, ADELINE_MAX_PATH - 1);
    s_atomicWriteSlots[slot].tempPath[ADELINE_MAX_PATH - 1] = '\0';
    return handle;
}

S32 CommitWriteAtomic(S32 handle) {
    const int slot = (handle != 0) ? AtomicSlotFind(handle) : -1;
    if (slot < 0) {
        return 0;
    }

    // Always close the handle (even if the flush fails) so the fd never leaks.
    const int flushed = (AtomicFlush(handle) == 0);
    const int closed = (close(handle) == 0);

    if (!flushed || !closed) {
        unlink(s_atomicWriteSlots[slot].tempPath);
        s_atomicWriteSlots[slot].handle = 0;
        return 0;
    }

    if (AtomicRename(s_atomicWriteSlots[slot].tempPath,
                     s_atomicWriteSlots[slot].finalPath) != 0) {
        unlink(s_atomicWriteSlots[slot].tempPath);
        s_atomicWriteSlots[slot].handle = 0;
        return 0;
    }

    s_atomicWriteSlots[slot].handle = 0;
    return 1;
}

void AbortWriteAtomic(S32 handle) {
    const int slot = (handle != 0) ? AtomicSlotFind(handle) : -1;
    if (slot < 0) {
        return;
    }
    close(handle);
    unlink(s_atomicWriteSlots[slot].tempPath);
    s_atomicWriteSlots[slot].handle = 0;
}

S32 WriteFileAtomic(const char *finalPath, const void *buffer, U32 size) {
    assert(finalPath != NULL);
    assert(buffer != NULL || size == 0);

    const S32 handle = OpenWriteAtomic(finalPath);
    if (!handle) {
        return 0;
    }

    const U8 *p = (const U8 *)buffer;
    U32 offset = 0;
    while (offset < size) {
        const U32 chunk = (size - offset > 65536U) ? 65536U : (size - offset);
        const S32 w = Write(handle, p + offset, chunk);
        if (w < 0 || (U32)w != chunk) {
            AbortWriteAtomic(handle);
            return 0;
        }
        offset += chunk;
    }

    return CommitWriteAtomic(handle);
}

// =============================================================================
#ifdef __cplusplus
}
#endif
