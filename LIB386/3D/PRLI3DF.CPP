#include <3D/LPROJ.H>

#include <SVGA/SCREENXY.H>
#include <3D/CAMERA.H>

void ProjectList3DF(TYPE_PT *Dst, TYPE_VT16 *Src, S32 NbPt, S32 OrgX,
										S32 OrgY, S32 OrgZ)
{
  int v6; // ebp@2
  S32 v7; // ecx@2
  int v8; // edx@2
  int v9; // eax@2
  int v10; // esi@2
  int v11; // eax@2
  int *v12; // ebx@3
  long double v13; // fst6@3
  long double v14; // fst5@3
  long double v15; // fst3@3
  long double v16; // fst4@3
  long double v17; // fst2@3
  S32 v18; // ecx@4
  int v19; // eax@4
  int v20; // eax@4
  int v21; // ecx@5
  int v22; // eax@5
  int v23; // ecx@17
  long double v24; // fst3@19
  long double v25; // fst4@19
  int v26; // ecx@19
  int v27; // eax@19
  int v28; // ecx@31
  int v29; // eax@35
  int v30; // edx@37
  int v31; // eax@39

  if ( NbPt )
  {
    v6 = Src + 8 * NbPt;
    v7 = *(_DWORD *)Src;
    v8 = NearClip;
    v9 = *(_WORD *)(Src + 4);
    v10 = Src + 8;
    v11 = -(v9 - OrgZ);
    if ( v11 < NearClip )
    {
      while ( 1 )
      {
        *Dst = -2147450880;
        ++Dst;
        if ( v10 == v6 )
          break;
        v29 = *(_WORD *)(v10 + 4);
        v10 += 8;
        v11 = -(v29 - OrgZ);
        if ( v11 >= v8 )
        {
          v7 = *(_DWORD *)(v10 - 8);
          goto LABEL_3;
        }
      }
    }
    else
    {
LABEL_3:
      Z0 = v11;
      Y0 = (v7 >> 16) - OrgY;
      X0 = (S16)v7 - OrgX;
      v12 = Dst;
      v13 = FRatioX;
      v14 = FRatioY;
      v15 = (long double)X0;
      v16 = (long double)Y0;
      v17 = FRatioX / (long double)v11;
      if ( v10 != v6 )
      {
        while ( 1 )
        {
          v18 = *(_DWORD *)v10;
          ++Dst;
          v19 = *(_WORD *)(v10 + 4) - OrgZ;
          v10 += 8;
          v20 = -v19;
          if ( v20 < NearClip )
            break;
LABEL_5:
          Z0 = v20;
          Y0 = (v18 >> 16) - OrgY;
          X0 = (S16)v18 - OrgX;
          Xp = (S32)(v15 * v17);
          Yp = (S32)(v16 * v17 * v14);
          v15 = (long double)X0;
          v16 = (long double)Y0;
          v17 = v13 / (long double)v20;
          v21 = XCentre + Xp;
          v22 = YCentre + Yp;
          if ( XCentre + Xp > 0x7FFF || v21 < -32767 || v22 > 0x7FFF || v22 < -32767 )
          {
            v23 = -2147450880;
          }
          else
          {
            if ( v21 < ScreenXMin )
              ScreenXMin = XCentre + Xp;
            if ( v21 > ScreenXMax )
              ScreenXMax = XCentre + Xp;
            if ( v22 < ScreenYMin )
              ScreenYMin = YCentre + Yp;
            if ( v22 > ScreenYMax )
              ScreenYMax = YCentre + Yp;
            v23 = (v22 << 16) | (U16)v21;
          }
          *v12 = v23;
          v12 = Dst;
          if ( v10 == v6 )
            goto LABEL_19;
        }
        v30 = NearClip;
        while ( 1 )
        {
          *Dst = -2147450880;
          ++Dst;
          if ( v10 == v6 )
            break;
          v31 = *(_WORD *)(v10 + 4);
          v10 += 8;
          v20 = -(v31 - OrgZ);
          if ( v20 >= v30 )
          {
            v18 = *(_DWORD *)(v10 - 8);
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
      if ( v26 > 0x7FFF || v26 < -32767 || v27 > 0x7FFF || v27 < -32767 )
      {
        v28 = -2147450880;
      }
      else
      {
        if ( v26 < ScreenXMin )
          ScreenXMin = XCentre + (S32)v24;
        if ( v26 > ScreenXMax )
          ScreenXMax = XCentre + (S32)v24;
        if ( v27 < ScreenYMin )
          ScreenYMin = YCentre + (S32)v25;
        if ( v27 > ScreenYMax )
          ScreenYMax = YCentre + (S32)v25;
        v28 = (v27 << 16) | (U16)v26;
      }
      *v12 = v28;
    }
  }
}

Func_ProjectList *ProjectList3D = ProjectList3DF;
Func_ProjectList *ProjectList = ProjectList3DF;
