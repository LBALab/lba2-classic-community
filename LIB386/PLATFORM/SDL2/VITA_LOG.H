#ifndef LBA2_SDL2_VITA_LOG_H
#define LBA2_SDL2_VITA_LOG_H

/* Optional network setup errors stay in the local trace and do not stop boot. */
bool SDL2_VitaInitDiagnostics(const char *tracePath, const char *networkConfigPath);
void SDL2_VitaEndDiagnostics(void);

#endif
