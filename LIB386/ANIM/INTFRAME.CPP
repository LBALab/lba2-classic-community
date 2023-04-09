#include <ANIM/INTFRAME.H>
#include <ANIM.H>
#include <stddef.h>

void ObjectSetInterFrame(T_OBJ_3D *obj)
{
  if (obj->Status == FLAG_FRAME)
    return;

  if (obj->Status != FLAG_CHANGE)
    return;

  obj->Status = 0;

  void *anim = obj->Anim.Ptr;
  if (TransFctAnim != NULL)
  {
    anim = TransFctAnim(obj->Anim.Num);
  }

  U16 *nextPtr = (U16 *)((U64)anim + (U64)obj->NextOfsFrame + 16);
  U16 *lastPtr = (U16 *)((U64)anim + 16);

  if (obj->LastOfsIsPtr != 1)
  {
    lastPtr = (U16 *)((U64)lastPtr + (U64)obj->LastOfsFrame);
  }

  // only one group? exit
  if (obj->NbGroups == 1)
    return;

  T_GROUP_INFO *group = &obj->CurrentFrame[0];

  for (U32 i = 0; i < obj->NbGroups; i++)
  {
    U16 type = nextPtr[0];
    group->Type = type;

    // Rotations
    if (type == TYPE_ROTATE)
    {
      U16 next = nextPtr[1];
      U16 last = lastPtr[1];
      U32 diffAlpha = (next - last) & 0xFFF; // & 4095
      if (diffAlpha != 0)
      {
        diffAlpha = (diffAlpha << 20) >> 20;
        diffAlpha = (diffAlpha * obj->Interpolator) >> 16;
        last = (last + diffAlpha) & 0xFFF;
      }

      next = nextPtr[2];
      group->Alpha = last;
      last = lastPtr[2];

      U32 diffBeta = (next - last) & 0xFFF;
      if (diffBeta != 0)
      {
        diffBeta = (diffBeta << 20) >> 20;
        diffBeta = (diffBeta * obj->Interpolator) >> 16;
        last = (last + diffBeta) & 0xFFF;
      }

      next = nextPtr[3];
      group->Beta = last;
      last = lastPtr[3];

      U32 diffGamma = (next - last) & 0xFFF;
      if (diffGamma != 0)
      {
        diffGamma = (diffGamma << 20) >> 20;
        diffGamma = (diffGamma * obj->Interpolator) >> 16;
        last = (last + diffGamma) & 0xFFF;
      }

      next = nextPtr[4];
      group->Gamma = last;
      last = lastPtr[4];

      nextPtr = (U16 *)((U64)nextPtr + 8);
      lastPtr = (U16 *)((U64)lastPtr + 8);
      group++;
    }
    else // Translations
    {
      U16 next = nextPtr[1];
      U16 last = lastPtr[1];
      U32 diffX = (next - last);
      if (diffX != 0)
      {
        diffX = (diffX * obj->Interpolator) >> 16;
        last = last + diffX;
      }

      next = nextPtr[2];
      group->Alpha = last; // X
      last = lastPtr[2];

      U32 diffY = (next - last);
      if (diffY != 0)
      {
        diffY = (diffY * obj->Interpolator) >> 16;
        last = last + diffY;
      }

      next = nextPtr[3];
      group->Beta = last; // Y
      last = lastPtr[3];

      U32 diffZ = (next - last);
      if (diffZ != 0)
      {
        diffZ = (diffZ * obj->Interpolator) >> 16;
        last = last + diffZ;
      }

      next = nextPtr[4];
      group->Gamma = last; // Z
      last = lastPtr[4];

      nextPtr = (U16 *)((U64)nextPtr + 8);
      lastPtr = (U16 *)((U64)lastPtr + 8);
      group++;
    }
  }
}
