#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#pragma pack(push, 4)
typedef struct {
  S32 Speed;
  S32 Acc;
  U32 LastTimer;
} MOVE;
#pragma pack(pop)

#pragma pack(push, 4)
typedef struct {
  MOVE Move;
  S32 Cur;
  S32 End;
} BOUND_MOVE;
#pragma pack(pop)

// -----------------------------------------------------------------------------
S32 GetDeltaAccMove(S32 *acc);
S32 AdjustSpeedAngle(S32 speed, S32 start, S32 end);
void InitMove(MOVE *pmove, S32 speed);
void ChangeSpeedMove(MOVE *pmove, S32 speed);
void ChangeSpeedAngleMove(MOVE *pmove, S32 speed, S32 start, S32 end);
void InitAngleMove(MOVE *pmove, S32 speed, S32 start, S32 end);
void ChangeSpeedBoundAngleMove(BOUND_MOVE *pbmove, S32 speed, S32 end);
void InitBoundMove(BOUND_MOVE *pbmove, S32 speed, S32 start, S32 end);
void InitBoundAngleMove(BOUND_MOVE *pbmove, S32 speed, S32 start, S32 end);
S32 GetDeltaMove(MOVE *pmove);
S32 GetBoundMove(BOUND_MOVE *pbmove);
S32 GetBoundAngleMove(BOUND_MOVE *pbmove);

// -----------------------------------------------------------------------------
#define GetSpeedMove(pmove) (pmove)->Speed

#define RestartMove(pmove)                                                     \
  {                                                                            \
    (pmove)->Acc = 500;                                                        \
    (pmove)->LastTimer = TimerRefHR;                                           \
  }

// =============================================================================
#ifdef __cplusplus
}
#endif
