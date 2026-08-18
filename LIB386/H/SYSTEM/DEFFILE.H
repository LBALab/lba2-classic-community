//****************************************************************************
#ifndef LIB_SYSTEM_DEFFILE
#define LIB_SYSTEM_DEFFILE

//****************************************************************************
#include <SYSTEM/ITOA.H>
#include <SYSTEM/LIMITS.H> /* ADELINE_MAX_PATH, for the saved-state struct below */

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
// Layered reads. After DefFileBufferInit has loaded the file a run owns, this
// appends a lower-priority source to the same buffer: the config shipped with
// the game data. Reads scan forward and stop at the first match, so a key the
// owned file defines shadows the same key underneath, while a key only the
// lower layer defines is still found. A key neither defines falls through to
// the compiled default the read site names.
//
// Returns FALSE when the layer is absent, empty, corrupt, or will not fit,
// leaving the buffer as it was. An absent layer is ordinary, not an error.
//
// Read-only: DefFileBufferWriteString and DefFileBufferBeginBatch both refuse
// while a layer is attached, since rewriting the buffer would persist it into
// the owned file. Any writer must DefFileBufferInit on its own file first,
// which clears the layer.
S32 DefFileBufferAppendFile(const char *file);

// Which source answered a key, for a caller that has to tell a value the run
// owns from one it merely inherited.
//
// The buffer holds the owned file with any appended layers below it, and a read
// stops at the first match, so the value alone cannot say which one supplied it.
// A setting read from a layer is real for this run and is not the player's
// preference: writing it back into the owned file would make the install's
// choice outlive the install. Telling them apart is what this is for.
//
// Presence alone is already answerable: DefFileBufferReadValue2 returns FALSE for
// a key the buffer does not hold. This adds the part that is not, which source
// held it, and folds presence in so a caller needs one call rather than two.
//
// With no layer attached every key present is OWNED.
typedef enum {
    DEFFILE_ORIGIN_ABSENT = 0, // no such key; the caller's default answers
    DEFFILE_ORIGIN_OWNED,      // the file this run owns
    DEFFILE_ORIGIN_LAYER       // a lower-priority layer appended beneath it
} T_DEFFILE_ORIGIN;

T_DEFFILE_ORIGIN DefFileBufferKeyOrigin(const char *ident);

// Save and restore the whole read window, for a caller that needs a transient
// read against a buffer of its own without disturbing the one the boot
// assembled. Without it, any such read silently drops the layers for everything
// that reads after it.
typedef struct {
    char *org;
    char *end;
    S32 bufferSize;
    S32 layersAttached;
    char *layerStart;
    char fileName[ADELINE_MAX_PATH];
} T_DEFFILE_STATE;

void DefFileBufferSaveState(T_DEFFILE_STATE *out);
void DefFileBufferRestoreState(const T_DEFFILE_STATE *in);

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
