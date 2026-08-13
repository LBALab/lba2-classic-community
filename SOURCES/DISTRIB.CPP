#include "DISTRIB.H"

#include "DEFINES.H"

const T_DISTRIB_NAME DistribNames[] = {
    {"unknown", UNKNOWN_VERSION},
    {"activision", ACTIVISION_VERSION},
    {"activision_sud", ACTIVISION_SUD_VERSION},
    {"ea", EA_VERSION},
    {"virgin", VIRGIN_VERSION},
    {"virgin_asia", VIRGIN_ASIA_VERSION},
};

const int DistribNameCount = (int)(sizeof(DistribNames) / sizeof(DistribNames[0]));

const char *Distrib_Name(S32 version) {
    for (int i = 0; i < DistribNameCount; i++) {
        if (DistribNames[i].version == version) {
            return DistribNames[i].name;
        }
    }
    return "unknown";
}

static bool distribDeclared = false;

void Distrib_SetDeclared(bool declared) { distribDeclared = declared; }

bool Distrib_WasDeclared(void) { return distribDeclared; }

/* Measured with scripts/dev/fingerprint_distro.py across the releases on hand.
   Both Activision pressings agree here despite being different locales, and the
   EA disc agrees with every 2point21 re-release, whose Common/ trees are byte
   for byte that master. */
static const struct {
    U32 size;
    T_DATA_MASTER master;
} RessSizes[] = {
    {582473, DATA_MASTER_LBA2},
    {582445, DATA_MASTER_TWINSEN},
    {289888, DATA_MASTER_DEMO},
};

T_DATA_MASTER Distrib_MasterFromRessSize(U32 size) {
    /* Zero is what FileSize returns for a file it could not open, so it has to
       miss rather than match an entry that happened to be zero. */
    if (size == 0) {
        return DATA_MASTER_UNRECOGNISED;
    }
    const int count = (int)(sizeof(RessSizes) / sizeof(RessSizes[0]));
    for (int i = 0; i < count; i++) {
        if (RessSizes[i].size == size) {
            return RessSizes[i].master;
        }
    }
    return DATA_MASTER_UNRECOGNISED;
}

const char *Distrib_MasterName(T_DATA_MASTER master) {
    switch (master) {
    case DATA_MASTER_LBA2:
        return "LBA2";
    case DATA_MASTER_TWINSEN:
        return "Twinsen's Odyssey";
    case DATA_MASTER_DEMO:
        return "the 1997 demo";
    default:
        return "unrecognised";
    }
}

T_DATA_MASTER Distrib_MasterForVersion(S32 version) {
    switch (version) {
    case UNKNOWN_VERSION:
    case EA_VERSION:
        return DATA_MASTER_LBA2;
    default:
        return DATA_MASTER_TWINSEN;
    }
}

void Distrib_Resolve(bool declared, S32 declaredVersion, T_DATA_MASTER master,
                     S32 *outVersion, bool *outSplash) {
    if (declared) {
        *outVersion = declaredVersion;
        *outSplash = true;
        return;
    }

    /* ACTIVISION_VERSION is the American arm's entry point rather than a claim
       about who sold the copy: every consumer treats 1, 2, 4 and 5 alike, and
       the data cannot tell them apart. The European arm stays UNKNOWN_VERSION,
       which already behaves as EA everywhere except the splash, so recognising
       a re-release changes nothing about how it runs. */
    *outVersion = (master == DATA_MASTER_TWINSEN) ? ACTIVISION_VERSION : UNKNOWN_VERSION;
    *outSplash = false;
}

static T_DATA_MASTER distribMaster = DATA_MASTER_UNRECOGNISED;

void Distrib_SetMaster(T_DATA_MASTER master) { distribMaster = master; }

T_DATA_MASTER Distrib_GetMaster(void) { return distribMaster; }

/* True until the config is read, so a failure to get that far leaves the splash
   behaving as it always has rather than silently disappearing. */
static bool distribShowSplash = true;

void Distrib_SetShowSplash(bool show) { distribShowSplash = show; }

bool Distrib_ShowSplash(void) { return distribShowSplash; }
