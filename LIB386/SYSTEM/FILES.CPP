#include <SYSTEM/FILES.H>

#include <SYSTEM/DISCIMG.H>

#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>

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

// =============================================================================
#ifdef __cplusplus
}
#endif
