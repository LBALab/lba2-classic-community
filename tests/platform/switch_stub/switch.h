#pragma once

#include <stdint.h>
#include <netinet/in.h>

typedef uint32_t Result;
#define R_SUCCEEDED(result) ((result) == 0)
#define R_FAILED(result) ((result) != 0)

typedef enum {
    AppletOperationMode_Handheld,
    AppletOperationMode_Console
} AppletOperationMode;

typedef struct {
    int initialized;
} ErrorApplicationConfig;

#ifdef __cplusplus
extern "C" {
#endif

AppletOperationMode appletGetOperationMode(void);
Result errorApplicationCreate(ErrorApplicationConfig *config, const char *dialog, const char *fullscreen);
Result errorApplicationShow(ErrorApplicationConfig *config);
extern struct in_addr __nxlink_host;
Result socketInitializeDefault(void);
int nxlinkStdio(void);
void socketExit(void);

#ifdef __cplusplus
}
#endif
