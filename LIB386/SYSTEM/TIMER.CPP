#include <SYSTEM/TIMER.H>

#include <SYSTEM/WINDOW.H> // For AppActive

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_timer.h>
#include <assert.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Public state ------------------------------------------------------------
volatile U32 TimerSystemHR;
volatile U32 TimerRefHR;
volatile S32 CmptFrame;
volatile S32 NbFramePerSecond;

// --- Private state -----------------------------------------------------------
volatile S32 TimerLock;
volatile S32 CmptMemoTimerRef;
static U32 MemoTimerRefHR;
U32 LastTime;
U32 LastEvaluate;

// --- Initialization ----------------------------------------------------------
void InitTimer() {
  LastTime = LastEvaluate = SDL_GetTicks();
  ManageTime();
}

void EndTimer() {
  // Empty
}

// -----------------------------------------------------------------------------
void LockTimer() {
  ManageTime();
  ++TimerLock;
}

void UnlockTimer() {
  if (TimerLock > 0) {
    ManageTime();
    --TimerLock;
  }
}

void SaveTimer() {
  if (!CmptMemoTimerRef++) {
    ManageTime();
    MemoTimerRefHR = TimerRefHR;
  }
}

void RestoreTimer() {
  if (CmptMemoTimerRef) {
    if (!--CmptMemoTimerRef) {
      ManageTime();
      TimerRefHR = MemoTimerRefHR;
    }
  }
}

void SetTimerHR(U32 time) {
  LockTimer();
  TimerRefHR = time;
  UnlockTimer();
}

void ManageTime() {
  TimerSystemHR = SDL_GetTicks();

  if (!TimerLock) {
    TimerRefHR += TimerSystemHR - LastTime;
  }

  LastTime = TimerSystemHR;

  // FPS Calculation
  S32 time = TimerSystemHR - LastEvaluate;
  if (time >= 1000) {
    NbFramePerSecond = (CmptFrame * 1000 + time / 2) / time;
    CmptFrame = 0;
    LastEvaluate += (time / 1000) * 1000;
  }
}

void HandleEventsTimer(const void *event) {
  assert(event != NULL);

  const SDL_Event *sdlEvent = (SDL_Event *)(event);

  if (sdlEvent->type == SDL_WINDOWEVENT) {
    switch (sdlEvent->window.event) {
    case SDL_WINDOWEVENT_FOCUS_GAINED:
      if (!AppActive) {
        UnlockTimer();
      }
      break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
      if (AppActive) {
        LockTimer();
      }
      break;
    }
  }
}

// =============================================================================
#ifdef __cplusplus
}
#endif
