#pragma once

/* Plain OS calls shared by the backends that have no SDK to ask: null and UWP.
   Each backend still builds its own table and names itself in its own errors;
   these only do the work. Failures go to one error slot per thread, which the
   backends hand out as system.get_error. */

#include <PLATFORM/PLATFORM.H>

/* The error slot. Fail logs the message and returns false. */
bool HostOs_Fail(const char *format, ...);
const char *HostOs_Error(void);
void HostOs_ClearError(void);

uint64_t HostOs_Counter(void);
uint64_t HostOs_Frequency(void);
uint64_t HostOs_Ticks(void);
void HostOs_Delay(uint32_t ms);
uint64_t HostOs_ThreadID(void);
int32_t HostOs_CPUCount(void);
int32_t HostOs_RAM(void);

/* malloc'd; NULL (with the error set) when out of memory. */
char *HostOs_Copy(const char *text);
char *HostOs_CurrentDirectory(void);
/* The executable's folder with a trailing separator, kept for the process. */
const char *HostOs_BasePath(void);

bool HostOs_IsDirectory(const char *path);
/* Creates the folder and any missing parents. Starts from the deepest folder
   that exists, so it works where the folders above are hidden (a UWP app
   cannot look at Q:\Users). An existing file of that name is an error. */
bool HostOs_MakeDirectory(const char *path);
/* A file, or an empty folder; an absent path is already removed. */
bool HostOs_Remove(const char *path);
