#include "EXTERN.H"

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/SCREENXY.H>

/*
; ╔════════════════════════════════════════════════════════════════════════╗
; ║   Trace de droite Zbuffer (methode toute con (Lucas)) No ZBuf write    ║
; ╟────────────────────────────────────────────────────────────────────────╢
; ║ Appel : (EAX,EBX), (ECX,EDX) : points extremes                         ║
; ║         STACK= Color                                                   ║
; ║         EDI  = CurrentZ1                                               ║
; ║         ESI  = CurrentZ2                                               ║
; ║			; Return value: bit field                                          ║
; ║			;		0 : line not displayed                                         ║
; ║			;		1 : line at least partially visible                            ║
; ║			;		2 : line intersects the background                             ║
; ╚════════════════════════════════════════════════════════════════════════╝
*/
S32	LineRain(S32 x0, S32 y0, S32 z0, S32 x1, S32 y1, S32 z1, S32 coul )
{
  int v1; // eax@1
  int v2; // ebx@1
  int v3; // ecx@1
  int v4; // edx@1
  int v5; // esi@1
  int v6; // edi@1
  int v7; // ebp@1
  U8 v8; // al@1
  int v9; // eax@3
  signed int v10; // edi@4
  signed int v11; // esi@5
  int v12; // edi@14
  signed int v13; // ecx@14
  int v14; // edi@14
  int v15; // ebp@16
  int v16; // esi@16
  int v17; // ebx@18
  signed int v18; // eax@19
  signed int v19; // eax@21
  int v20; // ebp@21
  unsigned int v21; // edx@21
  int v22; // esi@21
  unsigned int v23; // eax@21
  unsigned int v24; // edi@21
  int v25; // ecx@22
  U8 v26; // of@28
  unsigned int result; // eax@29
  signed int v28; // eax@30
  signed int v29; // eax@32
  int v30; // ecx@32
  unsigned int v31; // edx@32
  unsigned int v32; // eax@32
  int v33; // esi@32
  unsigned int v34; // edi@32
  int v35; // ebp@33
  int v36; // eax@43
  int v37; // eax@45
  int v38; // ST08_4@46
  int v39; // eax@46
  int v40; // ST08_4@47
  int v41; // eax@47
  signed int v42; // esi@56
  int v43; // ebx@84
  unsigned int v44; // eax@84
  int v45; // ebp@84
  int v46; // [sp-1Ch] [bp-20h]@1

  v1 = x0;
  v2 = y0;
  v3 = x1;
  v4 = y1;
  v5 = z1 << 16;
  v6 = z0 << 16;
  v7 = coul;
  U16 *YPtrZBuf = NULL;
  U8 *YPtrLog = NULL;
  U8 *PtrLog = NULL;
  U16 *PtrZBuf = NULL;
  S32 DELTA_Y = 0x12345678;
  S32 DELTA_X = 0x12345678;
  S32 DELTA_X1 = 0x12345678;
  S32 Line = 0x12345678;
  S32 Col = 0x12345678;
  S32 DELTA_Z = 0x12345678;
  S32 IZ = 0x12345678;
  S32 YDELTA_X = 0x12345678;
  S32 YDELTA_Y = 0x12345678;
  S32 YDELTA_Y1 = 0x12345678;
  S32 Col2 = 0x12345678;
  S32 Line2 = 0x12345678;
  S32 YDELTA_Z = 0x12345678;
  S32 YIZ = 0x12345678;
  S32 CurrentZ2 = v5;
  S32 CurrentZ1 = v6;
  S32 DZ = 0;
  v46 = v1;
  v8 = v7 & 0xFF;
  if ( Fill_Flag_Fog == 1 )
  {
    v8 = Fill_Logical_Palette[v8];
  }
  U8 Color = v8;
  v9 = v46;
  while ( 1 )
  {
    DZ = CurrentZ2 - CurrentZ1;
    v10 = v3 - v9;
    if ( v3 == v9 )
    {
      if ( v9 < ClipXMin || v9 > ClipXMax )
      {
        ScreenXMin = 0x7FFFFFFF;
        ScreenYMin = 0x7FFFFFFF;
        ScreenXMax = 2147483648;
        ScreenYMax = 2147483648;
        return 0;
      }
      v42 = v4 - v2;
      if ( v4 == v2 )
      {
        goto LABEL_80;
      }
      if ( v2 < ClipYMin )
      {
        if ( v4 < ClipYMin )
        {
          ScreenXMin = 0x7FFFFFFF;
          ScreenYMin = 0x7FFFFFFF;
          ScreenXMax = 2147483648;
          ScreenYMax = 2147483648;
          return 0;
        }
        CurrentZ1 -= DZ * (S64)(v2 - ClipYMin) / v42;
        v2 = ClipYMin;
        v42 = v4 - ClipYMin;
        if ( v4 == ClipYMin )
        {
          goto LABEL_80;
        }
      }
      if ( v2 > ClipYMax )
      {
        if ( v4 > ClipYMax )
        {
          ScreenXMin = 0x7FFFFFFF;
          ScreenYMin = 0x7FFFFFFF;
          ScreenXMax = 2147483648;
          ScreenYMax = 2147483648;
          return 0;
        }
        CurrentZ1 -= DZ * (S64)(v2 - ClipYMax) / v42;
        v2 = ClipYMax;
        v42 = v4 - ClipYMax;
        if ( v4 == ClipYMax )
        {
          goto LABEL_80;
        }
      }
      if ( v4 < ClipYMin )
      {
        CurrentZ2 -= DZ * (S64)(v4 - ClipYMin) / v42;
        v4 = ClipYMin;
        v42 = ClipYMin - v2;
        if ( ClipYMin == v2 )
        {
          goto LABEL_80;
        }
      }
      if ( v4 > ClipYMax )
      {
        CurrentZ2 -= DZ * (S64)(v4 - ClipYMax) / v42;
        v4 = ClipYMax;
        if ( ClipYMax == v2 )
        {
          goto LABEL_80;
        }
      }
      goto LABEL_14;
    }
    v11 = v4 - v2;
    if ( v4 == v2 )
    {
      break;
    }
    if ( v9 < ClipXMin )
    {
      if ( v3 < ClipXMin )
      {
        ScreenXMin = 0x7FFFFFFF;
        ScreenYMin = 0x7FFFFFFF;
        ScreenXMax = 2147483648;
        ScreenYMax = 2147483648;
        return 0;
      }
      v36 = v9 - ClipXMin;
      CurrentZ1 -= DZ * (S64)v36 / v10;
      v2 -= v11 * (S64)v36 / v10;
      v9 = ClipXMin;
    }
    else if ( v9 > ClipXMax )
    {
      if ( v3 > ClipXMax )
      {
        ScreenXMin = 0x7FFFFFFF;
        ScreenYMin = 0x7FFFFFFF;
        ScreenXMax = 2147483648;
        ScreenYMax = 2147483648;
        return 0;
      }
      v37 = v9 - ClipXMax;
      CurrentZ1 -= DZ * (S64)v37 / v10;
      v2 -= v11 * (S64)v37 / v10;
      v9 = ClipXMax;
    }
    else if ( v3 < ClipXMin )
    {
      v38 = v9;
      CurrentZ2 -= DZ * (S64)(v3 - ClipXMin) / v10;
      v39 = v11 * (S64)(v3 - ClipXMin) / v10;
      v3 = ClipXMin;
      v4 -= v39;
      v9 = v38;
    }
    else if ( v3 > ClipXMax )
    {
      v40 = v9;
      CurrentZ2 -= DZ * (S64)(v3 - ClipXMax) / v10;
      v41 = v11 * (S64)(v3 - ClipXMax) / v10;
      v3 = ClipXMax;
      v4 -= v41;
      v9 = v40;
    }
    else if ( v2 < ClipYMin )
    {
      if ( v4 < ClipYMin )
      {
        ScreenXMin = 0x7FFFFFFF;
        ScreenYMin = 0x7FFFFFFF;
        ScreenXMax = 2147483648;
        ScreenYMax = 2147483648;
        return 0;
      }
      CurrentZ1 -= DZ * (S64)(v2 - ClipYMin) / v11;
      v9 -= v10 * (S64)(v2 - ClipYMin) / v11;
      v2 = ClipYMin;
    }
    else if ( v2 > ClipYMax )
    {
      if ( v4 > ClipYMax )
      {
        ScreenXMin = 0x7FFFFFFF;
        ScreenYMin = 0x7FFFFFFF;
        ScreenXMax = 2147483648;
        ScreenYMax = 2147483648;
        return 0;
      }
      CurrentZ1 -= DZ * (S64)(v2 - ClipYMax) / v11;
      v9 -= v10 * (S64)(v2 - ClipYMax) / v11;
      v2 = ClipYMax;
    }
    else if ( v4 < ClipYMin )
    {
      CurrentZ2 -= DZ * (S64)(v4 - ClipYMin) / v11;
      v3 -= v10 * (S64)(v4 - ClipYMin) / v11;
      v4 = ClipYMin;
    }
    else
    {
      if ( v4 <= ClipYMax )
      {
        goto LABEL_14;
      }
      CurrentZ2 -= DZ * (S64)(v4 - ClipYMax) / v11;
      v3 -= v10 * (S64)(v4 - ClipYMax) / v11;
      v4 = ClipYMax;
    }
  }
  if ( v2 < ClipYMin || v2 > ClipYMax )
  {
    ScreenXMin = 0x7FFFFFFF;
    ScreenYMin = 0x7FFFFFFF;
    ScreenXMax = 2147483648;
    ScreenYMax = 2147483648;
    return 0;
  }
  if ( v9 < ClipXMin )
  {
    if ( v3 < ClipXMin )
    {
      ScreenXMin = 0x7FFFFFFF;
      ScreenYMin = 0x7FFFFFFF;
      ScreenXMax = 2147483648;
      ScreenYMax = 2147483648;
      return 0;
    }
    CurrentZ1 -= DZ * (S64)(v9 - ClipXMin) / v10;
    v9 = ClipXMin;
    v10 = v3 - ClipXMin;
    if ( v3 == ClipXMin )
    {
      goto LABEL_80;
    }
  }
  if ( v9 > ClipXMax )
  {
    if ( v3 > ClipXMin )
    {
      ScreenXMin = 0x7FFFFFFF;
      ScreenYMin = 0x7FFFFFFF;
      ScreenXMax = 2147483648;
      ScreenYMax = 2147483648;
      return 0;
    }
    CurrentZ1 -= DZ * (S64)(v9 - ClipXMax) / v10;
    v9 = ClipXMax;
    v10 = v3 - ClipXMax;
    if ( v3 == ClipXMax )
    {
      goto LABEL_80;
    }
  }
  if ( v3 >= ClipXMin
    || (CurrentZ2 -= DZ * (S64)(v3 - ClipXMin) / v10, v3 = ClipXMin, v10 = ClipXMin - v9, ClipXMin != v9) )
  {
    if ( v3 <= ClipXMax || (CurrentZ2 -= DZ * (S64)(v3 - ClipXMax) / v10, v3 = ClipXMax, ClipXMax != v9) )
    {
LABEL_14:
      ScreenXMin = v9;
      ScreenXMax = v3;
      v12 = v3;
      v13 = 1;
      v14 = v12 - v9;
      if ( v14 < 0 )
      {
        v14 = -v14;
        v13 = -1;
        ScreenXMin = -1;
        ScreenXMax = v9;
      }
      ScreenYMin = v2;
      ScreenYMax = v4;
      v15 = ScreenPitch;
      v16 = v4 - v2;
      if ( v4 - v2 < 0 )
      {
        v16 = -v16;
        v15 = -ScreenPitch;
        ScreenYMin = v4;
        ScreenYMax = v2;
      }
      v17 = v9 + PTR_TabOffLine[v2];
      if ( v14 < (unsigned int)v16 )
      {
        S32 YDELTA_X = v14;
        S32 YDELTA_Y = v16;
        S32 YDELTA_Y1 = v16;
        v28 = DZ;
        if ( v15 < 0 )
        {
          v28 = -DZ;
        }
        v29 = ((unsigned int)(v28 / v16) >> 31) + v28 / v16;
        S32 YDELTA_Z = v29 << 16;
        YIZ = v29 >> 16;
        S32 Col2 = v13;
        S32 Line2 = v15;
        U8 *YPtrLog = (U8 *)Log;
        U16 *YPtrZBuf = PtrZBuffer;
        v30 = v16;
        v31 = (unsigned int)v16 >> 1;
        v32 = (U8)Color;
        v33 = CurrentZ1 << 16;
        v34 = ((unsigned int)CurrentZ1 >> 16) - 100;
        do
        {
          v35 = YPtrZBuf[v17] - v34;
          if ( v35 >= 0 )
          {
            if ( v35 < 200 )
            {
              v32 |= 0x02 << 8;
            }
            v32 |= 0x01 << 8;
            YPtrLog[v17] = v32;
          }
          v31 += YDELTA_X;
          if ( v31 >= YDELTA_Y )
          {
            v31 -= YDELTA_Y1;
            v17 += Col2;
          }
          v17 += Line2;
          U8 carry = (U64)YDELTA_Z + (U64)v33 > 0xFFFFFFFF;
          v33 += YDELTA_Z;
          v34 += YIZ + carry;
          v33 += YDELTA_Z;
          //v26 = __OFSUB__(v30--, 1);
          v30--;
        }
        //while ( !((v30 < 0) ^ v26) );
        while (v30 >= 0);
        result = v32 >> 8;
      }
      else
      {
        DELTA_Y = v16;
        DELTA_X = v14;
        DELTA_X1 = v14;
        v18 = DZ;
        if ( v13 < 0 )
        {
          v18 = -DZ;
        }
        v19 = ((unsigned int)(v18 / v14) >> 31) + v18 / v14;
        DELTA_Z = v19 << 16;
        IZ = v19 >> 16;
        Line = v15;
        Col = v13;
        PtrLog = (U8 *)Log;
        PtrZBuf = PtrZBuffer;
        v20 = v14;
        v21 = (unsigned int)v14 >> 1;
        v22 = CurrentZ1 << 16;
        v23 = (U8)Color;
        v24 = ((unsigned int)CurrentZ1 >> 16) - 100;
        do
        {
          v25 = PtrZBuf[v17] - v24;
          if ( v25 >= 0 )
          {
            if ( v25 < 200 )
            {
              v23 |= 0x02u << 8;
            }
            v23 |= 0x01u << 8;
            PtrLog[v17] = v23;
          }
          v21 += DELTA_Y;
          if ( v21 >= DELTA_X )
          {
            v21 -= DELTA_X1;
            v17 += Line;
          }
          v17 += Col;
          U8 carry = (U64)DELTA_Z + (U64)v22 > 0xFFFFFFFF;
          v22 += DELTA_Z;
          v24 += IZ + carry;
          v22 += DELTA_Z;
          // v26 = __OFSUB__(v20--, 1);
          v20--;
        }
        // while ( !((v20 < 0) ^ v26) );
        while (v20 >= 0);
        result = v23 >> 8;
      }
      return result;
    }
  }
LABEL_80:
  if ( v2 < ClipYMin )
  {
    ScreenXMin = 0x7FFFFFFF;
    ScreenYMin = 0x7FFFFFFF;
    ScreenXMax = 2147483648;
    ScreenYMax = 2147483648;
    return 0;
  }
  if ( v2 > ClipYMax )
  {
    ScreenXMin = 0x7FFFFFFF;
    ScreenYMin = 0x7FFFFFFF;
    ScreenXMax = 2147483648;
    ScreenYMax = 2147483648;
    return 0;
  }
  if ( v9 < ClipXMin )
  {
    ScreenXMin = 0x7FFFFFFF;
    ScreenYMin = 0x7FFFFFFF;
    ScreenXMax = 2147483648;
    ScreenYMax = 2147483648;
    return 0;
  }
  if ( v9 > ClipXMax )
  {
    ScreenXMin = 0x7FFFFFFF;
    ScreenYMin = 0x7FFFFFFF;
    ScreenXMax = 2147483648;
    ScreenYMax = 2147483648;
    return 0;
  }
  ScreenXMin = v9;
  ScreenYMin = v2;
  ScreenXMax = v9;
  ScreenYMax = v2;
  v43 = v9 + PTR_TabOffLine[v2];
  v44 = (U8)Color;
  v45 = PtrZBuffer[v43] - ((CurrentZ1 >> 16) - 100);
  if ( v45 < 0 )
  {
    ScreenXMin = 0x7FFFFFFF;
    ScreenYMin = 0x7FFFFFFF;
    ScreenXMax = 2147483648;
    ScreenYMax = 2147483648;
    return 0;
  }
  if ( v45 < 200 )
  {
    v44 |= 0x0200u;
  }
  ((U8 *)Log)[v43] = v44;
  v44 |= 0x0100u;
  return v44 >> 8;
}
