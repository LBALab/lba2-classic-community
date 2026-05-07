#include "BUILD_INFO.h" /* LBA2_VERSION_STRING */

#ifdef DEMO
char *Version = "\nLBA2 \"Twinsen's Odyssey\" Demo Version " LBA2_VERSION_STRING
                " (" __DATE__ " / " __TIME__ ")\n\n";
#else
const char *Version = "\nLBA2 \"Twinsen's Odyssey\" " LBA2_VERSION_STRING
                      " (" __DATE__ " / " __TIME__ ")\n\n";
#endif
