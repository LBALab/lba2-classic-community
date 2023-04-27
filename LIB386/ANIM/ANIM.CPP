#include <ANIM.H>

#include <stdlib.h>

#include <SYSTEM/TIMER.H>

void ObjectInitAnim(T_OBJ_3D *obj, void *anim)
{
  if (obj->Anim.Ptr == anim)
  {
    return;
  }

  obj->Anim.Ptr = anim;

  if (TransFctAnim != NULL)
  {
    anim = TransFctAnim(obj->Anim.Num);
  }

  U16 *animPtr = (U16 *)anim;

  U16 animNbFrames = animPtr[0];
  U16 animNbGroups = animPtr[1];
  U16 animLoopFrame = animPtr[2];

  obj->Interpolator = 0;
  obj->Master = animPtr[8];
  obj->NbFrames = animNbFrames;
  obj->NextNbGroups = animNbGroups;
  obj->LoopFrame = animLoopFrame;

  obj->LastAnimStepX = 0;
  obj->LastAnimStepY = 0;
  obj->LastAnimStepZ = 0;

  obj->LoopOfsFrame = animLoopFrame * (animNbGroups + 1) * 8 + 8;

  obj->LastAnimStepAlpha = 0;
  obj->LastAnimStepBeta = 0;
  obj->LastAnimStepGamma = 0;

  obj->LastTimer = TimerRefHR;
  obj->Time = TimerRefHR;

  if (obj->LastOfsFrame == 0)
  {
    obj->Status = FLAG_FRAME;
    obj->LastOfsFrame = (PTR_U32)8;
    obj->LastOfsIsPtr = 0;
    obj->LastFrame = 0;

    obj->LastNbGroups = animNbGroups;
    obj->NbGroups = animNbGroups;

    U32 *currentFrame = (U32 *)obj->CurrentFrame;
    U32 *srcAnim = (U32 *)animPtr + 4 + 2; // group 1?
    U32 count = animNbGroups * 2 - 2;

    for (int i = 0; i < count; i++)
    {
      currentFrame[i] = srcAnim[i];
    }
    currentFrame += count;
    srcAnim += count;

    if (obj->NbFrames != 1)
    {
      obj->NextOfsFrame = (PTR_U32)((U64)srcAnim - (U64)animPtr);
      obj->NextFrame = 1;
      obj->NextTimer = TimerRefHR + ((U16 *)srcAnim)[0];
    }
    else
    {
      obj->NextOfsFrame = (PTR_U32)8;
      obj->NextFrame = 0;
      obj->NextTimer = TimerRefHR + ((U16 *)animPtr)[4];
    }

    return;
  }

  obj->Status = 0;
  obj->NextOfsFrame = (PTR_U32)8;
  obj->NextFrame = 0;
  obj->LastNbGroups = animNbGroups;

  U32 targetNbGroups = obj->NbGroups;
  U32 increasedToNbGroups = 0;

  if (animNbGroups != targetNbGroups)
  {
    obj->NbGroups = animNbGroups;

    if (animNbGroups > targetNbGroups)
    {
      increasedToNbGroups = targetNbGroups;
      animNbGroups = targetNbGroups;
    }
    else
    {
      targetNbGroups = animNbGroups;
    }
  }

  U16 animFrameDeltaTime = animPtr[4];
  obj->NextTimer = TimerRefHR + animFrameDeltaTime;
  obj->LastOfsFrame = (U32 *)PtrLib3DBufferAnim;
  obj->LastOfsIsPtr = 1;

  U32 *bufferAnim = (U32 *)PtrLib3DBufferAnim;
  bufferAnim[0] = 0;
  bufferAnim[1] = 0;
  bufferAnim[2] = 0;
  bufferAnim[3] = 0;

  obj->LastFrame = -1;

  bufferAnim += 4;
  U32 *currentFrame = (U32 *)obj->CurrentFrame;
  U32 count = animNbGroups * 2 - 2;
  for (int i = 0; i < count; i++)
  {
    bufferAnim[i] = currentFrame[i];
  }
  bufferAnim += count;

  if (increasedToNbGroups != 0)
  {
    if (TransFctAnim != NULL)
    {
      anim = TransFctAnim(obj->Anim.Num);
    }

    U32 diff = obj->NbGroups - increasedToNbGroups;
    U32 *nextOfsFrame = (U32 *)((U8 *)anim + (U64)obj->NextOfsFrame) + (increasedToNbGroups + 1) * 8;
    U32 count = diff << 1;
    for (int i = 0; i < count; i++)
    {
      bufferAnim[i] = nextOfsFrame[i];
    }

    bufferAnim += count;
  }

  if ((U8 *)bufferAnim > EndLib3DBufferAnim)
  {
    PtrLib3DBufferAnim = Lib3DBufferAnim;
    return;
  }

  PtrLib3DBufferAnim = (U8 *)bufferAnim;
}
