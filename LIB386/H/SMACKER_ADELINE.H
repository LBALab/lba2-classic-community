//──────────────────────────────────────────────────────────────────────────
#ifndef	LIB_SMACKER_ADELINE
#define	LIB_SMACKER_ADELINE

// Wrapper for InitAcf SmackRegisterMemory stubs and libsmacker (LGPL) include.
// Named SMACKER_ADELINE.H so that #include <smacker.h> below resolves to
// libsmacker/smacker.h on case-insensitive filesystems (e.g. macOS).
//──────────────────────────────────────────────────────────────────────────
#ifdef USE_MVIDEO_SMACKER
#include	<smacker.h>
#ifdef __cplusplus
extern "C" {
#endif
extern void SmackRegisterMemory(void *ptr, U32 size);
extern void SmackResetMemory(void);
#ifdef __cplusplus
}
#endif
#elif defined(MVIDEO_USE_SMACKER)
#include	<SMACKER/SMACK.H>
#include	<SMACKER/SMACKER.H>
#else
/* No video backend: no-ops for InitAcf when LBA_EDITOR */
#ifdef __cplusplus
extern "C" {
#endif
static inline void SmackRegisterMemory(void *ptr, U32 size) { (void)ptr; (void)size; }
static inline void SmackResetMemory(void) {}
#ifdef __cplusplus
}
#endif
#endif

//──────────────────────────────────────────────────────────────────────────
#endif//LIB_SMACKER_ADELINE

//──────────────────────────────────────────────────────────────────────────
