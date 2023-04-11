#include <SYSTEM/FILES.H>

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

#define MODE_READ  (O_RDONLY | O_BINARY)
#define MODE_WRITE (O_WRONLY | O_BINARY | O_CREAT | O_TRUNC)
#define MODE_RW    (O_RDWR   | O_BINARY | O_CREAT)

// --- Private functions -------------------------------------------------------
S32 OpenMode(const char *filename, int mode) {
  assert(filename != NULL);
  S32 handle = open(filename, mode, (S_IRUSR | S_IWUSR));
  return (handle == -1) ? 0 : handle;
}

// --- Public functions --------------------------------------------------------
S32 OpenRead(const char *filename) { return OpenMode(filename, MODE_READ); }
S32 OpenWrite(const char *filename) { return OpenMode(filename, MODE_WRITE); }
S32 OpenReadWrite(const char *filename) { return OpenMode(filename, MODE_RW); }

S32 Close(S32 handle) {
  assert(handle > 0);
  return close(handle);
}

S32 Read(S32 handle, void *buffer, U32 maxReadBytes) {
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

  S32 fsize = lseek(handle, 0, SEEK_END);
  Close(handle);

  return (U32)((fsize == -1) ? 0 : fsize);
}

bool ExistsFileOrDir(const char* path) {
  assert(path != NULL);

  struct stat status = {};
  return (stat(path, &status) == 0);
}

bool IsDirectory(const char* path) {
  assert(path != NULL);

  struct stat status = {};
  return ((stat(path, &status) == 0) && ((status.st_mode & S_IFMT) == S_IFDIR));
}

// =============================================================================
#ifdef __cplusplus
}
#endif
