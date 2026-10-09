#pragma once

#include <PLATFORM/PLATFORM.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Initialization ----------------------------------------------------------
bool InitEvents();
void EndEvents();

// --- Interface ---------------------------------------------------------------
void ManageEvents();

/** Called for each platform event. Return true to mark the event consumed. */
typedef int (*EventFilterFn)(const PlatformEvent *event);
void SetEventFilter(EventFilterFn fn);

// =============================================================================
#ifdef __cplusplus
}
#endif
