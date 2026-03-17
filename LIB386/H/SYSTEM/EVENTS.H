#pragma once

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Initialization ----------------------------------------------------------
bool InitEvents();
void EndEvents();

// --- Interface ---------------------------------------------------------------
void ManageEvents();

/** Optional: called for each SDL event (e.g. for console). Return true to mark event consumed. */
typedef int (*EventFilterFn)(const void *sdlEvent);
void SetEventFilter(EventFilterFn fn);

// =============================================================================
#ifdef __cplusplus
}
#endif
