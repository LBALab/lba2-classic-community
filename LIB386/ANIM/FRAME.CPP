#include <ANIM/FRAME.H>
#include <ANIM.H>
#include <SYSTEM/TIMER.H>

#include <stddef.h>

void ObjectSetFrame(T_OBJ_3D *obj, U32 frame)
{
  T_PTR_NUM nextBody = obj->NextBody;
  void *nextTexture = obj->NextTexture;
  if (frame >= obj->NbFrames)
  {
    return;
  }

  obj->Body = nextBody;
  obj->Texture = nextTexture;

  void *anim = obj->Anim.Ptr;
  if (TransFctAnim != NULL)
  {
    anim = TransFctAnim(obj->Anim.Num);
  }

  obj->Interpolator = 0;
  obj->LastAnimStepX = 0;
  obj->LastAnimStepY = 0;
  obj->LastAnimStepZ = 0;

  U16 *animPtr = (U16 *)anim;
  U16 nbGroups = animPtr[1];

  obj->LastAnimStepAlpha = 0;
  obj->LastAnimStepBeta = 0;
  obj->LastAnimStepGamma = 0;
  obj->LastOfsIsPtr = 0;

  obj->LastNbGroups = nbGroups;
  obj->NextNbGroups = nbGroups;
  obj->NbGroups = nbGroups;

  obj->LastTimer = TimerRefHR;
  obj->Time = TimerRefHR;
  obj->Status = FLAG_FRAME;

  U32 ofsFrame = (nbGroups * 8 + 8) * frame + 8;
  obj->LastOfsFrame = (PTR_U32)ofsFrame;
  obj->LastFrame = frame;

  U32 *animFrame = (U32 *)((U8 *)anim + ofsFrame + 16);
  U32 *currentFrame = (U32 *)obj->CurrentFrame;
  for (U32 i = 0; i < nbGroups * 2 - 2; i++)
  {
    *(currentFrame++) = *(animFrame++);
  }
  frame++;

  if (frame == obj->NbFrames)
  {
    U16 animTime = animPtr[0];
    obj->NextFrame = 0;
    obj->NextOfsFrame = (PTR_U32)8;
    obj->NextTimer = obj->LastTimer + animTime;
    obj->Master = ((U16 *)animFrame)[8];
  }
  else
  {
    U16 animTime = ((U16 *)animFrame)[0];
    obj->NextFrame = frame;
    obj->NextOfsFrame = (U32 *)((U64)animFrame - (U64)anim);
    obj->NextTimer = obj->LastTimer + animTime;
    obj->Master = ((U16 *)animFrame)[4];
  }
}
