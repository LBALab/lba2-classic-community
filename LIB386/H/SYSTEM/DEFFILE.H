//****************************************************************************
#ifndef LIB_SYSTEM_DEFFILE
#define LIB_SYSTEM_DEFFILE

//****************************************************************************
#include <SYSTEM/ITOA.H>

//****************************************************************************
#ifdef __cplusplus
extern "C" {
#endif

//****************************************************************************
extern char *OrgPtrDef;
extern char *EndPtrDef;

//****************************************************************************
extern S32 DefFileReadSkipComments;

//****************************************************************************
S32 DefFileBufferInit(char *file, void *buffer, S32 maxsize);

//****************************************************************************
char *DefFileBufferReadString(const char *ident);

//****************************************************************************
const char *DefFileBufferReadStringDefault(const char *ident, const char *def);

//****************************************************************************
S32 DefFileBufferReadValue(const char *ident);

//****************************************************************************
S32 DefFileBufferReadValueDefault(const char *ident, S32 def);

//****************************************************************************
S32 DefFileBufferReadValue2(const char *ident, S32 *result);

//****************************************************************************
S32 DefFileBufferWriteString(const char *ident, const char *string);

//****************************************************************************
#define DefFileBufferWriteValue(ident, value) \
    DefFileBufferWriteString(ident, Itoa(value))

//****************************************************************************
// Batched writes. Between Begin and End, DefFileBufferWriteString splices into a
// working temp with ordinary buffered writes (no per-key flush); the whole batch
// is committed with a single fsync + atomic rename at End. Outside a batch each
// DefFileBufferWriteString is independently atomic (one fsync + rename per call)
// -- correct but slow when a save rewrites dozens of keys. Begin returns FALSE
// if a batch is already active or the buffer is not initialised, in which case
// writes simply fall back to the per-call atomic path. End returns FALSE if no
// batch is active or the commit fails (destination then left untouched).
S32 DefFileBufferBeginBatch(void);
S32 DefFileBufferEndBatch(void);

//****************************************************************************
#ifdef __cplusplus
}
#endif

//****************************************************************************
#endif //LIB_SYSTEM_DEFFILE

//****************************************************************************
