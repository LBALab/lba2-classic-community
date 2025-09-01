#include <3D/LPROJ.H>

#include <3D/CAMERA.H>
#include <SVGA/SCREENXY.H>
#include <SYSTEM/UTILS.H>

void ProjectList3DF(TYPE_PT *Dst, TYPE_VT16 *Src, S32 NbPt, S32 OrgX,
										S32 OrgY, S32 OrgZ)
{
  TYPE_VT16 *end; // ebp@2
  TYPE_VT16 *current; // ecx@2
  TYPE_VT16 *next; // esi@2
  S32 v11; // eax@2
  TYPE_PT *v12; // ebx@3
  long double v13; // fst6@3
  long double v14; // fst5@3
  long double v15; // fst3@3
  long double v16; // fst4@3
  long double v17; // fst2@3
  TYPE_VT16 *v18; // ecx@4
  int v20; // eax@4
  int v21; // ecx@5
  int v22; // eax@5
  TYPE_PT v23; // ecx@17
  long double v24; // fst3@19
  long double v25; // fst4@19
  int v26; // ecx@19
  int v27; // eax@19
  TYPE_PT v28; // ecx@31
  int v30; // edx@37
  int v31; // eax@39

  if (NbPt == 0)
  {
    return;
  }

  end = Src + NbPt;
  current = Src;
  next = Src + 1;
  v11 = OrgZ - Src->Z;

  while (v11 < NearClip)
  {
    Dst->X = -32768; // 0x8000
    Dst->Y = -32768;
    ++Dst;
    if (next == end)
    {
      return;
    }
    v11 = OrgZ - next->Z;
    next++;
  }

  current = next - 1;

  Z0 = v11;
  Y0 = current->Y - OrgY;
  X0 = current->X - OrgX;
  v12 = Dst;
  v13 = FRatioX;
  v14 = FRatioY;
  v15 = (long double)X0;
  v16 = (long double)Y0;
  v17 = FRatioX / (long double)v11;

  if (next != end)
  {
    while (1)
    {
      v18 = next;
      ++Dst;
      v20 = OrgZ - next->Z;
      next++;
      if (v20 < NearClip)
      {
        break;
      }

LABEL_5:
      Z0 = v20;
      Y0 = v18->Y - OrgY;
      X0 = v18->X - OrgX;
      Xp = (S32)ROUND(v15 * v17);
      Yp = (S32)ROUND(v16 * v17 * v14);
      v15 = (long double)X0;
      v16 = (long double)Y0;
      v17 = v13 / (long double)v20;
      v21 = XCentre + Xp;
      v22 = YCentre + Yp;
      if (XCentre + Xp > 0x7FFF || v21 < -32767 || v22 > 0x7FFF || v22 < -32767)
      {
        v23.X = -32768;
        v23.Y = -32768;
      }
      else
      {
        if (v21 < ScreenXMin)
        {
          ScreenXMin = XCentre + Xp;
        }
        if (v21 > ScreenXMax)
        {
          ScreenXMax = XCentre + Xp;
        }
        if (v22 < ScreenYMin)
        {
          ScreenYMin = YCentre + Yp;
        }
        if (v22 > ScreenYMax)
        {
          ScreenYMax = YCentre + Yp;
        }
        v23.X = v21;
        v23.Y = v22;
      }
      *v12 = v23;
      v12 = Dst;
      if (next == end)
      {
        goto LABEL_19;
      }
    }
    v30 = NearClip;
    while (1)
    {
      Dst->X = -32768;
      Dst->Y = -32768;
      ++Dst;
      if (next == end)
      {
        break;
      }
      v31 = next->Z;
      next++;
      v20 = -(v31 - OrgZ);
      if (v20 >= v30)
      {
        v18 = next - 1;
        goto LABEL_5;
      }
    }
  }
LABEL_19:
  v24 = v15 * v17;
  v25 = v16 * v17 * v14;
  Xp = (S32)v24;
  Yp = (S32)v25;
  v26 = XCentre + (S32)v24;
  v27 = YCentre + (S32)v25;
  if (v26 > 0x7FFF || v26 < -32767 || v27 > 0x7FFF || v27 < -32767)
  {
    v28.X = -32768;
    v28.Y = -32768;
  }
  else
  {
    if (v26 < ScreenXMin)
    {
      ScreenXMin = XCentre + (S32)v24;
    }
    if (v26 > ScreenXMax)
    {
      ScreenXMax = XCentre + (S32)v24;
    }
    if (v27 < ScreenYMin)
    {
      ScreenYMin = YCentre + (S32)v25;
    }
    if (v27 > ScreenYMax)
    {
      ScreenYMax = YCentre + (S32)v25;
    }
    v28.X = v26;
    v28.Y = v27;
  }
  *v12 = v28;
}

Func_ProjectList *ProjectList3D = ProjectList3DF;
Func_ProjectList *ProjectList = ProjectList3DF;
