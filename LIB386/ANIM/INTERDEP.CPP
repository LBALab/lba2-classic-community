#include <ANIM/INTERDEP.H>

#include <stdlib.h>
#include <string.h>

#include <SYSTEM/TIMER.H>
#include <ANIM.H>
#include <3D/IMATSTD.H>
#include <3D/ROT3D.H>
#include <3D/CAMERA.H>

S32 ObjectSetInterDep(T_OBJ_3D *obj)
{
  obj->Texture = obj->NextTexture;
  obj->Body = obj->NextBody;

  // Same time as before?
  if (TimerRefHR == obj->Time)
  {
    if (!(obj->Status & FLAG_BODY))
    {
      obj->Status = 0; // didn't do anything
      return obj->Status;
    }
  }
  else if (TimerRefHR < obj->Time)
  {
    U32 delta = obj->Time - TimerRefHR;
    obj->NextTimer -= delta;
    obj->LastTimer -= delta;
    obj->Time = TimerRefHR;
    obj->Status = 0;
    return obj->Status; // didn't do anything
  }

  obj->Time = TimerRefHR; // Upgrade time
  T_PTR_NUM objAnim = obj->Anim;

  if (TransFctAnim != NULL)
  {
    objAnim.Ptr = TransFctAnim(objAnim.Num);
  }

  void *anim = objAnim.Ptr;
  S16 *frame = (S16 *)((U8 *)objAnim.Ptr + (U64)obj->NextOfsFrame); // esi

  U32 interpolator = 0; // ebp@6

  if (obj->Time >= obj->NextTimer)
  {
    interpolator = 0x10000;
    obj->Status = FLAG_FRAME + FLAG_CHANGE;
  }
  else
  {
    obj->Status = FLAG_CHANGE;
    U32 nbSteps = obj->NextTimer - obj->LastTimer;
    U32 deltaSteps = obj->Time - obj->LastTimer;
    interpolator = ((deltaSteps << 16) + ((nbSteps + 1) >> 1)) / nbSteps;
  }

  S32 newAlpha = obj->Alpha;
  S32 newBeta = obj->Beta;
  S32 newGamma = obj->Gamma;

  if (obj->Master & 1)
  {
    S32 alpha = frame[5]; // Alpha
    S32 alphaDelta = (S32)(interpolator * (alpha << 20 >> 20)) >> 16;
    newAlpha = (obj->Alpha + alphaDelta - obj->LastAnimStepAlpha) & 0xFFF;
    obj->LastAnimStepAlpha = alphaDelta;
    obj->Alpha = newAlpha;

    S32 beta = frame[6]; // Beta
    S32 betaDelta = (S32)(interpolator * (beta << 20 >> 20)) >> 16;
    newBeta = (obj->Beta + betaDelta - obj->LastAnimStepBeta) & 0xFFF;
    obj->LastAnimStepBeta = betaDelta;
    obj->Beta = newBeta;

    S32 gamma = frame[7]; // Gamma
    S32 gammaDelta = (S32)(interpolator * (gamma << 20 >> 20)) >> 16;
    newGamma = (obj->Gamma + gammaDelta - obj->LastAnimStepGamma) & 0xFFF;
    obj->LastAnimStepGamma = gammaDelta;
    obj->Gamma = newGamma;
  }

  InitMatrixStd(&MatrixLib2, newAlpha, newBeta, newGamma);

  S32 frameX = frame[1]; // X
  S32 frameY = frame[2]; // Y
  S32 frameZ = frame[3]; // Z

  S32 xDelta = (S32)(interpolator * frameX) >> 16;
  S32 yDelta = (S32)(interpolator * frameY) >> 16;
  S32 zDelta = (S32)(interpolator * frameZ) >> 16;

  S32 rotX = xDelta - obj->LastAnimStepX;
  S32 rotY = yDelta - obj->LastAnimStepY;
  S32 rotZ = zDelta - obj->LastAnimStepZ;

  obj->LastAnimStepX = xDelta;
  obj->LastAnimStepY = yDelta;
  obj->LastAnimStepZ = zDelta;
  obj->Interpolator = interpolator;

  RotatePoint(&MatrixLib2, rotX, rotY, rotZ);

  obj->X += X0;
  obj->Y += Y0;
  obj->Z += Z0;

  if (obj->Status == FLAG_CHANGE)
  {
    return obj->Status;
  }

  obj->Interpolator = 0;

  obj->LastAnimStepX = 0;
  obj->LastAnimStepY = 0;
  obj->LastAnimStepZ = 0;

  obj->LastAnimStepAlpha = 0;
  obj->LastAnimStepBeta = 0;
  obj->LastAnimStepGamma = 0;

  PTR_U32 prevNextOfsFrame = obj->NextOfsFrame;
  obj->LastFrame = obj->NextFrame;
  obj->LastOfsFrame = prevNextOfsFrame;
  obj->LastOfsIsPtr = 0;

  S32 nextFrame = obj->NextFrame + 1;

  S32 nextNbGroups = obj->NextNbGroups;
  obj->LastNbGroups = nextNbGroups;
  obj->NbGroups = nextNbGroups;

  S32 size = 2 * nextNbGroups - 2;

  PTR_U32 nextOfsFrame = NULL; // eax@14

  if (nextFrame >= obj->NbFrames)
  {
    nextFrame = obj->LoopFrame;
    nextOfsFrame = (PTR_U32)obj->LoopOfsFrame;
    obj->Status = FLAG_FRAME + FLAG_LAST_FRAME + FLAG_CHANGE;
  }
  else
  {
    nextOfsFrame = (U32 *)((U8 *)prevNextOfsFrame + 4 * size + 16);
  }

  obj->NextOfsFrame = nextOfsFrame;
  S16 *nextFrameData = (S16 *)((U8 *)anim + (U64)nextOfsFrame);
  obj->Master = nextFrameData[4];
  obj->NextFrame = nextFrame;
  obj->LastTimer = obj->Time;
  obj->NextTimer = obj->Time + nextFrameData[0];
  memcpy(obj->CurrentFrame,
         ((U8 *)anim + (U64)prevNextOfsFrame + 16),
         4 * size);

  return obj->Status;
}