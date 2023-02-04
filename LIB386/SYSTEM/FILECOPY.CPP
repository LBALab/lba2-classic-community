#include <SYSTEM/FILECOPY.H>

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/FILES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
bool Copy(const char *sname, const char *dname) {
  S32 shandle, dhandle;
  U32 size, n, buf;

  size = FileSize(sname);
  if (!size) {
    return false;
  }

  shandle = OpenRead(sname);
  if (!shandle) {
    return false;
  }

  dhandle = OpenWrite(dname);
  if (!dhandle) {
    Close(shandle);
    return false;
  }

  for (n = size; n > 0; n--) { // faster and smaller to count down 8-)
    U32 s;

    s = Read(shandle, &buf, 4);
    if (s != Write(dhandle, &buf, s)) {
      break;
    }
  }

  Close(shandle);
  Close(dhandle);

  return (n == 0); // if n=0 then copy is ok so return TRUE
}

// =============================================================================
#ifdef __cplusplus
}
#endif
