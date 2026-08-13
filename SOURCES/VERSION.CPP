#include "VERSION.H"

#include "DEFINES.H"
#include "BUILD_INFO.h" /* LBA2_VERSION_STRING, LBA2_PRODUCT_NAME, LBA2_PRODUCT_NAME_DEMO */

#ifdef DEMO
char *Version = "\n" LBA2_PRODUCT_NAME_DEMO " " LBA2_VERSION_STRING
                " (" __DATE__ " / " __TIME__ ")\n\n";
#else
const char *Version = "\n" LBA2_PRODUCT_NAME " " LBA2_VERSION_STRING
                      " (" __DATE__ " / " __TIME__ ")\n\n";
#endif

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
