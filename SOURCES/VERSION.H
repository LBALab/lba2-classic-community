#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/// The publisher DistribVersion declares, as a name. Six values, documented in
/// docs/VERSIONS.md, and the mapping lives here so the console command that
/// parses these names and the boot banner that prints one cannot disagree.
typedef struct {
    const char *name;
    S32 version;
} T_DISTRIB_NAME;

extern const T_DISTRIB_NAME DistribNames[];
extern const int DistribNameCount;

/// Name for \p version, or "unknown" for a value no constant claims.
const char *Distrib_Name(S32 version);

#ifdef __cplusplus
}
#endif
