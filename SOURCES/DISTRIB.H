#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/// The publisher `DistribVersion` declares, as a name.
///
/// Nothing here is about the engine's own version. `Version` the build string
/// lives in VERSION.CPP and is a different thing that happens to share the
/// word; docs/VERSIONS.md separates the six that do.
///
/// Six values, and the mapping lives in one place so the console command that
/// parses these names and the boot banner that prints one cannot disagree.
typedef struct {
    const char *name;
    S32 version;
} T_DISTRIB_NAME;

extern const T_DISTRIB_NAME DistribNames[];
extern const int DistribNameCount;

/// Name for \p version, or "unknown" for a value no constant claims.
const char *Distrib_Name(S32 version);

/// Whether the config named a Version at all, as against the engine falling
/// back to UNKNOWN_VERSION.
///
/// Worth separating, because absent is the ordinary state: the modern store
/// builds ship no key, so their data is European and their config says nothing,
/// and a banner calling that "unknown" reports the majority case as a fault.
void Distrib_SetDeclared(bool declared);
bool Distrib_WasDeclared(void);

#ifdef __cplusplus
}
#endif
