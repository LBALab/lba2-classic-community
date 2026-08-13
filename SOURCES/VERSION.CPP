#include "BUILD_INFO.h" /* LBA2_VERSION_STRING, LBA2_PRODUCT_NAME, LBA2_PRODUCT_NAME_DEMO */

#ifdef DEMO
char *Version = "\n" LBA2_PRODUCT_NAME_DEMO " " LBA2_VERSION_STRING
                " (" __DATE__ " / " __TIME__ ")\n\n";
#else
const char *Version = "\n" LBA2_PRODUCT_NAME " " LBA2_VERSION_STRING
                      " (" __DATE__ " / " __TIME__ ")\n\n";
#endif
