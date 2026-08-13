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

/// Which data master a copy of the game was built from.
///
/// Five of the six sites that read `DistribVersion` ask the same two-way
/// question, European master or American, and only the sixth needs to know a
/// publisher. So this is the part of release identity the assets can answer.
/// Which logo a release was licensed to show is not in them.
typedef enum {
    DATA_MASTER_UNRECOGNISED = 0, ///< A size no sampled release has.
    DATA_MASTER_LBA2,             ///< European master. Little Big Adventure 2.
    DATA_MASTER_TWINSEN,          ///< American master. Twinsen's Odyssey.
    DATA_MASTER_DEMO,             ///< The 1997 playable demo, which is neither.
} T_DATA_MASTER;

/// Master for a `RESS.HQR` of \p size bytes.
///
/// `RESS` and not one of the larger banks because it is invariant within a
/// publisher: the European and Brazilian Activision pressings ship it byte for
/// byte while differing in `SCENE` and `TEXT`, so an unsampled locale still
/// answers this question correctly. Three of its fifty entries carry the
/// difference between the two masters and none of them is branding, which is
/// what makes it a record of the build rather than of the seller.
///
/// A size is a weak hash and the sample is small, so
/// `DATA_MASTER_UNRECOGNISED` is an ordinary answer, not a failure.
T_DATA_MASTER Distrib_MasterFromRessSize(U32 size);

/// Name for the boot banner: "LBA2", "Twinsen's Odyssey", "the 1997 demo".
const char *Distrib_MasterName(T_DATA_MASTER master);

/// The master a declared \p version implies, which is the same two-way split
/// every consumer makes: `{UNKNOWN, EA}` is European, everything else American.
/// Lets a declaration and a measurement be compared on the one axis they share.
T_DATA_MASTER Distrib_MasterForVersion(S32 version);

/// What the data says this run is, `DATA_MASTER_UNRECOGNISED` until boot looks.
void Distrib_SetMaster(T_DATA_MASTER master);
T_DATA_MASTER Distrib_GetMaster(void);

/// Settle release identity from what the config declared and what the data says.
///
/// A declaration always wins. The installer stating the release is the original
/// contract, `DistribVersion` is latched at boot into derived state that a later
/// change would reach only half of, and a table built from a handful of samples
/// should not outrank something a human wrote down.
///
/// With nothing declared the data answers, and `\p outSplash` is what keeps that
/// safe: a release that ships no config ships no publisher branding either, so a
/// detected value drives behaviour without putting a logo in front of anyone.
void Distrib_Resolve(bool declared, S32 declaredVersion, T_DATA_MASTER master,
                     S32 *outVersion, bool *outSplash);

/// Whether the publisher splash is drawn this run. Not written back to the
/// config: the default is derived from whether a release was declared, and
/// storing it would freeze one install's answer into a profile.
void Distrib_SetShowSplash(bool show);
bool Distrib_ShowSplash(void);

#ifdef __cplusplus
}
#endif
