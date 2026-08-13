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
