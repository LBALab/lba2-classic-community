#include <SYSTEM/FILENAME.H>

#include <SYSTEM/ADELINE.H>
#include <SYSTEM/LIMITS.H>

#include <string.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
void StripExt(char *fname) {
  // Copied from:
  // https://stackoverflow.com/questions/43163677/how-do-i-strip-a-file-extension-from-a-string-in-c
  char *end = fname + strlen(fname);

  while (end > fname && *end != '.' && *end != '\\' && *end != '/') {
    --end;
  }

  if ((end > fname && *end == '.') &&
      (*(end - 1) != '\\' && *(end - 1) != '/')) {
    *end = '\0';
  }
}

// TODO: Review uses, probably not conformant to file path from DIRECTORIES.H
char *AddExt(char *path, const char *ext) {
  if (ext[0] == '.') {
    ++ext;
  }

  StripExt(path);
  strcat(path, ".");
  strcat(path, ext);

  return (path);
}

char *GetFileName(char *pathname) {
  static char fileName[ADELINE_MAX_FILENAME];
  fileName[0] = '\0';

  // If file name comes as a string from a resource and uses a different path
  //  separator than the platform one, this may fail. Should implement for any
  //  possibility?
  char *lastSlash = strrchr(pathname, ADELINE_PATH_SEP_CHAR);
  if (lastSlash != NULL) {
    char *filenameStart = lastSlash + 1;

    size_t fileNameLen = ADELINE_MAX_FILENAME - 1;
    char *lastDot = strrchr(filenameStart, '.');
    if (lastDot != NULL) {
      fileNameLen = lastDot - filenameStart;
      fileNameLen = ade_min(fileNameLen, ADELINE_MAX_FILENAME - 1);
    }

    strncpy(fileName, filenameStart, fileNameLen);
    fileName[fileNameLen] = '\0';
  }

  return fileName;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
