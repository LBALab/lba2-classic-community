#include <POLYGON/POLY.H>

#include <stdio.h>

S32 Z_Value = 0; // Z Value for the ZBuffer
S32 Sph_YMin = 0; // YMin et YMax du disque a l'ecran
S32 Sph_YMax = 0;
S32 Sph_XMin = 0; // XMin et XMax pour le trace d'une ligne de balayage du disque
S32 Sph_XMax = 0;
U8 Sph_Coul = 0;

// ╔═════════════════════════════════════════════════════════════════════════════════╗
// ║ Sph_Line_Flat : Trace un segment horizontal en Flat                             ║
// ╟─────────────────────────────────────────────────────────────────────────────────╢
// ║ Appel : [Sph_XMin], [Sph_XMax] : Extremites du segment                          ║
// ║         EBX = Y a l'ecran                                                       ║
// ║         [Sph_XCoul] : Couleur                                                   ║
// ╟─────────────────────────────────────────────────────────────────────────────────╢
// ║ REGISTRES NON MODIFIES                                                          ║
// ║                                                                                 ║
// ╚═════════════════════════════════════════════════════════════════════════════════╝
__int64 Sph_Line_Flat(__int64 a1, int a2)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
  /*
  int v2; // edi@1
  unsigned int v3; // ecx@1
  __int16 v4; // ax@2
  char v5; // cf@4
  unsigned int i; // ecx@4
  __int64 v8; // [sp-20h] [bp-20h]@1

  v8 = a1;
  v2 = Sph_XMin + Log + TabOffLine[a2];
  v3 = Sph_XMax - Sph_XMin;
  if ( Sph_XMax > Sph_XMin )
  {
    LOBYTE(v4) = Sph_Coul;
    HIBYTE(v4) = Sph_Coul;
    if ( v2 & 1 )
    {
      *(_BYTE *)v2++ = Sph_Coul;
      --v3;
    }
    v5 = v3 & 1;
    for ( i = v3 >> 1; i; --i )
    {
      *(_WORD *)v2 = v4;
      v2 += 2;
    }
    if ( v5 )
      *(_BYTE *)v2 = v4;
  }
  return v8;
  */
}

__int64 Sph_Line_FlatZBuf(__int64 a1, int a2)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
  /*

  int v2; // ebx@1
  char *v3; // edi@1
  unsigned __int16 *v4; // ebx@1
  int v5; // ecx@1
  char v6; // al@2
  unsigned __int16 v7; // dx@2
  __int64 v9; // [sp-20h] [bp-20h]@1

  v9 = a1;
  v2 = TabOffLine[a2];
  v3 = (char *)(Sph_XMin + Log + v2);
  v4 = (unsigned __int16 *)(Sph_XMin + Sph_XMin + PtrZBuffer + 2 * v2);
  v5 = Sph_XMax - Sph_XMin;
  if ( Sph_XMax > Sph_XMin )
  {
    v6 = Sph_Coul;
    v7 = Z_Value;
    do
    {
      if ( *v4 >= v7 )
      {
        *v4 = v7;
        *v3 = v6;
      }
      ++v4;
      ++v3;
      --v5;
    }
    while ( v5 );
  }
  return v9;
  */
}

__int64 Sph_Line_FlatNZW(__int64 a1, int a2)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
  /*
  int v2; // ebx@1
  char *v3; // edi@1
  unsigned __int16 *v4; // ebx@1
  int v5; // ecx@1
  char v6; // al@2
  unsigned __int16 v7; // dx@2
  __int64 v9; // [sp-20h] [bp-20h]@1

  v9 = a1;
  v2 = TabOffLine[a2];
  v3 = (char *)(Sph_XMin + Log + v2);
  v4 = (unsigned __int16 *)(Sph_XMin + Sph_XMin + PtrZBuffer + 2 * v2);
  v5 = Sph_XMax - Sph_XMin;
  if ( Sph_XMax > Sph_XMin )
  {
    v6 = Sph_Coul;
    v7 = Z_Value;
    do
    {
      if ( *v4 >= v7 )
        *v3 = v6;
      ++v4;
      ++v3;
      --v5;
    }
    while ( v5 );
  }
  return v9;
  */
}

// ╔═════════════════════════════════════════════════════════════════════════════════╗
// ║ Sph_Line_Transp : Trace un segment horizontal en Transparence                   ║
// ╟─────────────────────────────────────────────────────────────────────────────────╢
// ║ Appel : [Sph_XMin], [Sph_XMax] : Extremites du segment                          ║
// ║         EBX = Y a l'ecran                                                       ║
// ║         [Sph_XCoul] : Couleur                                                   ║
// ╟─────────────────────────────────────────────────────────────────────────────────╢
// ║ REGISTRES NON MODIFIES                                                          ║
// ║                                                                                 ║
// ╚═════════════════════════════════════════════════════════════════════════════════╝
void Sph_Line_Transp(int a1)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return;
  /*
  int v1; // edi@1
  unsigned int v2; // ecx@1
  __int16 v3; // bx@2
  char v4; // dl@4
  unsigned int v5; // ecx@4
  int v6; // edx@4

  v1 = Sph_XMin + Log + TabOffLine[a1];
  v2 = Sph_XMax - Sph_XMin;
  if ( Sph_XMax > Sph_XMin )
  {
    LOBYTE(v3) = Sph_Coul & 0xF0;
    HIBYTE(v3) = Sph_Coul & 0xF0;
    if ( v1 & 1 )
    {
      *(_BYTE *)v1 = v3 + (*(_BYTE *)v1 & 0xF);
      ++v1;
      --v2;
    }
    v4 = v2;
    v5 = v2 >> 1;
    v6 = v4 & 1;
    while ( v5 )
    {
      *(_WORD *)v1 = v3 + (*(_WORD *)v1 & 0xF0F);
      v1 += 2;
      --v5;
    }
    if ( v6 )
      *(_BYTE *)v1 = v3 + (*(_BYTE *)v1 & 0xF);
  }
  */
}

// ╔════════════════════════════════════════════════════════════════════════════════════════════════════════════╗
// ║  Fill_Sphere : Trace un disque a l'ecran                                                                   ║
// ╟────────────────────────────────────────────────────────────────────────────────────────────────────────────╢
// ║  Appel : EAX = C_x                                                                                         ║
// ║          EBI = C_y                                                                                         ║
// ║          ECX = Rayon                                                                                       ║
// ║          EDX = Couleur                                                                                     ║
// ║          ESI = Type du disque (soit Flat, soit Transparent=2)                                              ║
// ║          EDI = Zbuffer value for the sphere                                                                ║
// ║                                                                                                            ║
// ╚════════════════════════════════════════════════════════════════════════════════════════════════════════════╝
void Fill_Sphere(S32 Type_Sphere, S32 Color_Sphere, S32 Centre_X,
                 S32 Centre_Y, S32 Rayon, S32 zBufferValue)
{
  int (*v6)(void); // edi@2
  int v7; // ebp@10
  int v8; // edi@10
  int v9; // ecx@10
  int v10; // esi@10
  int v11; // edx@10
  int v12; // ebp@10
  int v13; // eax@10
  signed int v14; // ebx@10
  int v15; // eax@10
  int v16; // eax@13
  int v17; // ecx@24
  int v18; // edx@24
  unsigned __int8 v19; // cf@24
  int v20; // edx@25
  int v21; // ecx@25
  int v23; // ebx@29
  int v24; // eax@31
  int v25; // eax@35
  int v26; // ebx@40
  int v27; // eax@42
  int v28; // eax@46
  int v29; // ebx@52
  int v30; // eax@54
  int v31; // eax@58
  int v32; // ebx@63
  int v33; // eax@65
  int v34; // eax@69

  if (Fill_Flag_ZBuffer == 1)
  {
    Z_Value = zBufferValue;
    v6 = (int (*)(void))Sph_Line_FlatZBuf;
    if (Fill_Flag_NZW == 1)
      v6 = (int (*)(void))Sph_Line_FlatNZW;
    if (Type_Sphere == 2)
      v6 = (int (*)(void))Sph_Line_Transp;
  }
  else
  {
    v6 = (int (*)(void))Sph_Line_Flat;
    if (Type_Sphere == 2)
      v6 = (int (*)(void))Sph_Line_Transp;
  }
  Color_Sphere = (unsigned __int8)Color_Sphere;
  if (Fill_Flag_Fog == 1)
    LOBYTE(Color_Sphere) = Fill_Logical_Palette[Color_Sphere];
  Sph_Coul = Color_Sphere;
  Fill_Filler = v6;
  v7 = Rayon;
  v8 = Rayon;
  v9 = Centre_Y;
  v10 = Centre_Y;
  v11 = 0;
  v12 = -v7;
  v13 = Centre_Y;
  v14 = 0;
  v15 = v13 - v8;
  if (v15 > ClipYMax)
    return; // 0;
  if (v15 < ClipYMin)
  {
    v14 = 1;
    v15 = ClipYMin;
  }
  Sph_YMin = v15;
  v16 = v10 + v8;
  if (v10 + v8 < ClipYMin)
    return; // 0;
  if (v16 > ClipYMax)
  {
    ++v14;
    v16 = ClipYMax;
  }
  Sph_YMax = v16;
  if (v9 + v8 < ClipXMin)
    return; // 0;
  if (v9 + v8 > ClipXMax)
    ++v14;
  if (v9 - v8 > ClipXMax)
    return; // 0;
  if (v9 - v8 < ClipXMin)
    ++v14;
  if (!v14)
  {
    while (v11 <= v8)
    {
      Sph_XMax = v9 + v8;
      Sph_XMin = v9 - v8;
      Sph_XMin = Fill_Filler();
      Sph_XMax = v17 + v8;
      Fill_Filler();
      v19 = __CFADD__(v18, v12);
      v12 += v18;
      if (v19)
      {
        Sph_XMax = v9 + v18;
        Sph_XMin = v9 - v18;
        Sph_XMin = Fill_Filler();
        Sph_XMax = v21 + v20;
        Fill_Filler();
        v12 -= --v8;
      }
      v11 = v18 + 1;
    }
    return; // 1;
  }
  while (v11 <= v8)
  {
    v23 = v10 + v11;
    if (v10 + v11 <= Sph_YMax && v23 >= Sph_YMin)
    {
      v24 = v9 - v8;
      if (v9 - v8 < ClipXMin)
      {
        v24 = ClipXMin;
      }
      else if (v24 > ClipXMax)
      {
        Sph_YMax = v23 - 1;
        goto LABEL_40;
      }
      Sph_XMin = v24;
      v25 = v9 + v8;
      if (v9 + v8 > ClipXMax)
      {
        v25 = ClipXMax;
LABEL_39:
        Sph_XMax = v25;
        Fill_Filler();
        goto LABEL_40;
      }
      if (v25 >= ClipXMin)
        goto LABEL_39;
      Sph_YMax = v23 - 1;
    }
LABEL_40:
    v26 = v10 - v11;
    if (v10 - v11 <= Sph_YMax && v26 >= Sph_YMin)
    {
      v27 = v9 - v8;
      if (v9 - v8 < ClipXMin)
      {
        v27 = ClipXMin;
      }
      else if (v27 > ClipXMax)
      {
        Sph_YMin = v26 + 1;
        goto LABEL_51;
      }
      Sph_XMin = v27;
      v28 = v9 + v8;
      if (v9 + v8 > ClipXMax)
      {
        v28 = ClipXMax;
      }
      else if (v28 < ClipXMin)
      {
        Sph_YMin = v26 + 1;
        goto LABEL_51;
      }
      Sph_XMax = v28;
      Fill_Filler();
    }
LABEL_51:
    v19 = __CFADD__(v11, v12);
    v12 += v11;
    if (!v19)
      goto LABEL_75;
    v29 = v10 + v8;
    if (v10 + v8 <= Sph_YMax && v29 >= Sph_YMin)
    {
      v30 = v9 - v11;
      if (v9 - v11 < ClipXMin)
      {
        v30 = ClipXMin;
      }
      else if (v30 > ClipXMax)
      {
        Sph_YMax = v29 - 1;
        goto LABEL_63;
      }
      Sph_XMin = v30;
      v31 = v9 + v11;
      if (v9 + v11 > ClipXMax)
      {
        v31 = ClipXMax;
LABEL_62:
        Sph_XMax = v31;
        Fill_Filler();
        goto LABEL_63;
      }
      if (v31 >= ClipXMin)
        goto LABEL_62;
      Sph_YMax = v29 - 1;
    }
LABEL_63:
    v32 = v10 - v8;
    if (v10 - v8 > Sph_YMax || v32 < Sph_YMin)
      goto LABEL_74;
    v33 = v9 - v11;
    if (v9 - v11 < ClipXMin)
    {
      v33 = ClipXMin;
    }
    else if (v33 > ClipXMax)
    {
      Sph_YMin = v32 + 1;
      goto LABEL_74;
    }
    Sph_XMin = v33;
    v34 = v9 + v11;
    if (v9 + v11 > ClipXMax)
    {
      v34 = ClipXMax;
LABEL_73:
      Sph_XMax = v34;
      Fill_Filler();
      goto LABEL_74;
    }
    if (v34 >= ClipXMin)
      goto LABEL_73;
    Sph_YMin = v32 + 1;
LABEL_74:
    v12 -= --v8;
LABEL_75:
    ++v11;
  }
  if (Sph_YMin < Sph_YMax)
  {
    return; // 1;
  }

  return; // 0;
}
