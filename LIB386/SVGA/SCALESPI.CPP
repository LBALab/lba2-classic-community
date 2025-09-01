
#include <SVGA/SCALESPT.H>

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/SCREENXY.H>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  U8 Delta_X;
  U8 Delta_Y;
  U8 Hot_X;
  U8 Hot_Y;
} Struc_Sprite;

void *Start = 0;

U32 Start_X = 0;
U32 Start_Y = 0;
U32 Skip_Next_Line = 0;

U32 X_Dec = 0x12345678;
U32 X_Int = 0x12345678;
U32 Y_Dec = 0x12345678;
U32 Y_Int = 0x12345678;

void ScaleSprite(S32 num, S32 x, S32 y, S32 factorx, S32 factory,
										void *ptrbank)
{
  S32 v1; // eax@1
  S32 v2; // ebx@1
  S32 v3; // ecx@1
  U32 *v4; // esi@1
  Struc_Sprite *v5; // esi@4
  unsigned int v6; // ecx@4
  int v7; // ebx@4
  int v8; // eax@4
  unsigned int v9; // edx@4
  int v11; // edi@6
  int v12; // ebx@6
  int v13; // edi@9
  unsigned int v14; // ecx@12
  int v15; // ebx@12
  int v16; // eax@12
  unsigned int v17; // edx@12
  int v19; // edi@14
  int v20; // edi@17
  U8 *v21; // edi@20
  U8 *v22; // esi@20
  int v25; // ebp@20
  int v26; // ecx@21
  U8 *v27; // esi@22
  int v28; // ebx@22
  unsigned int v29; // edx@22
  int v30; // ebx@23
  Struc_Sprite *v31; // esi@37
  int v34; // edi@37
  int v35; // edx@37
  int v36; // edi@40
  int v37; // ebx@43
  int v38; // edi@43
  int v39; // edi@46
  int v40; // ebp@49
  int v41; // ecx@49
  int v42; // edx@50
  U8 *v43; // esi@51
  U8 *v44; // edi@51
  U8 *v45; // esi@53
  int v46; // ebp@53
  int v47; // ebx@53
  int v49; // [sp-10h] [bp-14h]@23
  U8 *v50; // [sp-Ch] [bp-10h]@23
  U8 *v51; // [sp-8h] [bp-Ch]@23
  int v52; // [sp-4h] [bp-8h]@23
  int v53; // [sp-4h] [bp-8h]@54

  // mov	eax,[factorx]
  // mov	ebx,[factory]

  // mov	ecx,[numbrick]
  // mov	esi,[bankbrick]
  v1 = factorx;
  v2 = factory;
  v3 = num;
  v4 = (U32 *)ptrbank;
  if (v1 <= 0)
  {
    if (!v1)
    {
      ScreenXMin = 32000;
      ScreenXMax = -32000;
      ScreenYMin = 32000;
      ScreenYMax = -32000;
      return;
    }
    factorx = 0x7FFFFFFF;
  }
  if (v1 == 65536 && v2 == 65536)
  {
    v31 = (Struc_Sprite *)((U8 *)v4 + v4[v3]);
    v1 = ClipXMax + 1;
    v34 = v31->Hot_X + x;
    v35 = v31->Delta_X;
    if (v34 <= v1)
    {
      ScreenXMin = v34;
      Start_X = 0;
      if (v34 < ClipXMin)
      {
        ScreenXMin = ClipXMin;
        Start_X = ClipXMin - v34;
      }
      v34 += v35;
      ScreenXMax = v34;
      if (v34 >= ClipXMin)
      {
        if (v34 >= v1)
        {
          ScreenXMax = v1;
        }
        v37 = ClipYMin;
        v38 = v31->Hot_Y + y;
        v1 = ClipYMax + 1;
        ScreenYMin = v38;
        if (v38 <= v1)
        {
          Start_Y = 0;
          if (v38 <= ClipYMin)
          {
            ScreenYMin = ClipYMin;
            Start_Y = ClipYMin - v38;
            v37 = ClipYMin;
          }
          v38 += v31->Delta_Y;
          if (v38 >= v37)
          {
            ScreenYMax = v38;
            if (v38 >= v1)
            {
              ScreenYMax = ClipYMax + 1;
            }
            v1 = (v1 & 0xFFFFFF00) | (ScreenYMin & 0xFF);
            v40 = v31->Delta_X;
            v41 = ScreenXMax - ScreenXMin;
            if (ScreenXMax > ScreenXMin)
            {
              v42 = ScreenYMax - ScreenYMin;
              if (ScreenYMax > ScreenYMin)
              {
                v43 = (U8 *)(v31 + 1);
                v44 = (U8 *)((U8 *)Log + ScreenXMin + TabOffLine[ScreenYMin]);
                if (Start_Y)
                {
                  v43 += v40 * Start_Y;
                }
                v45 = (U8 *)(Start_X + v43);
                v46 = v40 - v41;
                v47 = ModeDesiredX - v41;
                do
                {
                  v53 = v41;
                  do
                  {
                    v1 = (v1 & 0xFFFFFF00) | *v45;
                    if (*v45)
                    {
                      *v44 = v1;
                    }
                    ++v44;
                    ++v45;
                    --v41;
                  }
                  while (v41);
                  v41 = v53;
                  v45 += v46;
                  v44 += v47;
                  --v42;
                }
                while (v42);
                return;
              }
            }
          }
        }
      }
    }

    ScreenXMin = 32000;
    ScreenXMax = -32000;
    ScreenYMin = 32000;
    ScreenYMax = -32000;
    return;
  }
  if (v2 <= 0)
  {
    if (!v2)
    {
      ScreenXMin = 32000;
      ScreenXMax = -32000;
      ScreenYMin = 32000;
      ScreenYMax = -32000;
      return;
    }
    factory = 0x7FFFFFFF;
  }
  v5 = (Struc_Sprite *)((U8 *)v4 + v4[v3]);
  Start = v5;
  v6 = (S64)(factorx * v5->Delta_X) >> 16;
  v6 |= v4[v3] << 16; // New Delta_X
  v7 = (S64)(factorx * v5->Hot_X) >> 16;
  v7 |= (v4[v3] << 16) << 16; // New Hot_X
  v8 = 0x100000000LL / factorx;
  factorx = v8;
  v9 = (v5->Delta_X >> 1) << 16;
  if (v5->Delta_X & 1)
  {
    v9 += 0x8000;
  }
  v9 -= (v6 >> 1) * v8;
  Start_X = v9;
  v11 = v7 + x; // Left_X
  v12 = ClipXMin;
  v1 = ClipXMax + 1;
  ScreenXMin = v11; // Offset in Log screen (X coord)
  if (v11 > ClipXMax + 1) // Out of the screen ?
  {
    ScreenXMin = 32000;
    ScreenXMax = -32000;
    ScreenYMin = 32000;
    ScreenYMax = -32000;
    return;
  }
  if (v11 < ClipXMin)
  {
    ScreenXMin = ClipXMin;
    v12 = ClipXMin;
    v9 = factorx * (ClipXMin - 1 - v11) + Start_X;
    Start_X += v9; // 16:16
  }
  v13 = v6 + v11; // Right_X
  if (v13 < v12)
  {
    ScreenXMin = 32000;
    ScreenXMax = -32000;
    ScreenYMin = 32000;
    ScreenYMax = -32000;
    return;
  }
  ScreenXMax = v13;
  if (v13 >= v1)
  {
    ScreenXMax = v1;
  }
  v14 = factory * (S64)(v5->Delta_Y) >> 16;
  v9 <<= 16;
  v14 |= v9; // New Delta_Y
  v15 = factory * (S64)(v5->Hot_Y) >> 16;
  v9 <<= 16;
  v15 |= v9; // New Hot_Y
  v16 = 0x100000000LL / factory;
  factory = v16; // Inverse factory
  v17 = (v5->Delta_Y >> 1) << 16;
  if (v5->Delta_Y & 1)
  {
    v17 += 0x8000;
  }
  // @@NoY:
  Start_Y = v17 - (v14 >> 1) * v16;
  v19 = v15 + y; // Top Y
  v1 = ClipYMax + 1;
  ScreenYMin = v19; // Offset in Log screen (Y coord)
  if (v19 > v1) // Out of the screen ?
  {
    ScreenXMin = 32000;
    ScreenXMax = -32000;
    ScreenYMin = 32000;
    ScreenYMax = -32000;
    return;
  }
  if (v19 < ClipYMin)
  {
    ScreenYMin = ClipYMin;
    Start_Y += factory * (ClipYMin - 1 - v19) + Start_Y; // 16:16
  }
  // No_ClipYMin:
  v20 = v14 + v19; // Low_Y
  ScreenYMax = v20;
  if (v20 <= ClipYMin)
  {
    ScreenXMin = 32000;
    ScreenXMax = -32000;
    ScreenYMin = 32000;
    ScreenYMax = -32000;
    return;
  }
  if (v20 > v1)
  {
    ScreenYMax = v1;
  }
  // No_ClipYMax:
  v21 = (U8 *)((U8 *)Log + TabOffLine[ScreenYMin] + ScreenXMin);
  // Get pointer on sprite's raw buffer
  // according to the first scanline to be drawn
  Skip_Next_Line = v5->Delta_X;
  v22 = (U8 *)v5 + sizeof(Struc_Sprite) + Skip_Next_Line * ((U32)Start_Y >> 16);
  X_Dec = factorx << 16;
  X_Int = factorx >> 16;
  Y_Dec = factory << 16;
  Y_Int = factory >> 16;
  //v1 = (v1 & 0xFFFFFF00) | (ScreenYMin & 0xFF);
  v25 = ScreenYMax - ScreenYMin; // deltaY
  if (ScreenYMax <= ScreenYMin)
  {
    ScreenXMin = 32000;
    ScreenXMax = -32000;
    ScreenYMin = 32000;
    ScreenYMax = -32000;
    return;
  }
  //v1 = (v1 & 0xFFFFFF00) | (Start_X & 0xFF);
  v26 = ScreenXMax - ScreenXMin; // deltaX
  if (ScreenXMax <= ScreenXMin)
  {
    ScreenXMin = 32000;
    ScreenXMax = -32000;
    ScreenYMin = 32000;
    ScreenYMax = -32000;
    return;
  }
  v27 = v22 + (Start_X >> 16);
  v28 = Start_Y << 16;
  Start_X <<= 16;
  v29 = 0;
  do
  {
    v52 = v26; // deltaX
    v51 = v27; // Pointer on sprite's raw buffer
    v50 = v21; // Pointer on Log screen
    v49 = v28; // Fract. part of StartY
    v30 = Start_X; // Fract. part of StartX
    do
    {
      if (*v27)
      {
        *v21 = *v27;
      }
      U8 carry = ((U64)v30 + (U64)X_Dec) > 0xFFFFFFFF;
      v30 += X_Dec;
      v27 += X_Int + carry;
      ++v21;
      --v26;
    }
    while (v26);
    v27 = v51;
    U8 carry = ((U64)v49 + (U64)Y_Dec) > 0xFFFFFFFF;
    v28 = Y_Dec + v49;
    v29 += Y_Int + carry;
    v26 = v52;
    for (; v29 != 0; --v29)
    {
      v27 += Skip_Next_Line;
    }
    v21 = &v50[ModeDesiredX];
    --v25;
  }
  while (v25);
}

#ifdef __cplusplus
}
#endif
