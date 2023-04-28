#include <POLYGON/POLY.H>

#include <stdio.h>

#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>

S32 Z_Value = 0; // Z Value for the ZBuffer
S32 Sph_YMin = 0; // YMin et YMax du disque a l'ecran
S32 Sph_YMax = 0;
S32 Sph_XMin = 0; // XMin et XMax pour le trace d'une ligne de balayage du disque
S32 Sph_XMax = 0;
U8 Sph_Coul = 0;

typedef void (*Sphere_Filler_Func)(U32 screenY);

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
void Sph_Line_Flat(U32 screenY)
{
  U8* dst = (U8 *)Log + TabOffLine[screenY] + Sph_XMin;
  U32 nbCols = Sph_XMax - Sph_XMin;
  for (int i = 0; i < nbCols; ++i)
  {
    dst[i] = Sph_Coul;
  }
}

void Sph_Line_FlatZBuf(U32 screenY)
{
  U32 lineOffset = TabOffLine[screenY];
  U8 *screen = (U8 *)Log + lineOffset + Sph_XMin;
  U16 *zBuffer = PtrZBuffer + lineOffset + Sph_XMin;
  U32 diffX = Sph_XMax - Sph_XMin;
  if (Sph_XMax <= Sph_XMin)
  {
    return;
  }

  U8 color = Sph_Coul;
  U16 z = Z_Value & 0xFF;
  do
  {
    if (*zBuffer >= Z_Value)
    {
      *zBuffer = Z_Value;
      *screen = color;
    }
    ++zBuffer;
    ++screen;
    --diffX;
  }
  while (diffX);
}

void Sph_Line_FlatNZW(U32 screenY)
{
  U32 lineOffset = TabOffLine[screenY];
  U8 * screen = (U8 *)Log + lineOffset + Sph_XMin;
  U16 * zBuffer = PtrZBuffer + Sph_XMin + lineOffset;
  U32 diffX = Sph_XMax - Sph_XMin;

  if (Sph_XMax <= Sph_XMin)
  {
    return;
  }

  U8 color = Sph_Coul;
  U16 z = Z_Value & 0xFF;
  do
  {
    if (*zBuffer >= z)
    {
      *screen = color;
    }
    ++zBuffer;
    ++screen;
    --diffX;
  }
  while (diffX);
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
void Sph_Line_Transp(U32 screenY)
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
  int lineFunct; // edx@4

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
    lineFunct = v4 & 1;
    while ( v5 )
    {
      *(_WORD *)v1 = v3 + (*(_WORD *)v1 & 0xF0F);
      v1 += 2;
      --v5;
    }
    if ( lineFunct )
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
  Sphere_Filler_Func lineFunct; // edi@2
  int v7; // ebp@10
  int v8; // edi@10
  int v11; // edx@10
  signed int v14; // ebx@10
  int yMin; // eax@10
  int yMax; // eax@13
  U8 v19; // cf@24
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
    lineFunct = Sph_Line_FlatZBuf;
    if (Fill_Flag_NZW == 1)
    {
      lineFunct = Sph_Line_FlatNZW;
    }
    else if (Type_Sphere == 2)
    {
      lineFunct = Sph_Line_Transp;
    }
  }
  else
  {
    lineFunct = Sph_Line_Flat;
    if (Type_Sphere == 2)
    {
      lineFunct = Sph_Line_Transp;
    }
  }
  // @@Type_Ok:
  Color_Sphere = Color_Sphere & 0xFF;
  if (Fill_Flag_Fog == 1)
  {
    Color_Sphere = Fill_Logical_Palette[Color_Sphere];
  }
  Sph_Coul = Color_Sphere;
  v7 = Rayon;
  v8 = Rayon;
  v11 = 0;
  v7 = -v7;
  v14 = 0;
  yMin = Centre_Y - Rayon;
  if (yMin > ClipYMax)
  {
    return; // 0;
  }
  if (yMin < ClipYMin)
  {
    ++v14;
    yMin = ClipYMin;
  }
  Sph_YMin = yMin;
  yMax = Centre_Y + Rayon;
  if (yMax < ClipYMin)
  {
    return; // 0;
  }
  if (yMax > ClipYMax)
  {
    ++v14;
    yMax = ClipYMax;
  }
  Sph_YMax = yMax;
  if (Centre_X + Rayon < ClipXMin)
  {
    return; // 0;
  }
  if (Centre_X + Rayon > ClipXMax)
  {
    ++v14;
  }
  if (Centre_X - Rayon > ClipXMax)
  {
    return; // 0;
  }
  if (Centre_X - Rayon < ClipXMin)
  {
    ++v14;
  }
  if (!v14)
  {
    while (v11 <= v8)
    {
      Sph_XMax = Centre_X + v8;
      Sph_XMin = Centre_X - v8;
      lineFunct(Centre_Y + v11);
      Sph_XMin = Centre_X - v8;
      Sph_XMax = Centre_X + v8;
      lineFunct(Centre_Y - v11);
      v19 = (U64)v11 + (U64)v7 > 0xFFFFFFFF;
      v7 += v11;
      if (v19)
      {
        Sph_XMax = Centre_X + v11;
        Sph_XMin = Centre_X - v11;
        lineFunct(Centre_Y + v11);
        Sph_XMin = Centre_X - v11;
        Sph_XMax = Centre_X + v11;
        lineFunct(Centre_Y - v11);
        --v8;
        v7 -= v8;
      }
      v11++;
    }
    return; // 1;
  }

  while (v11 <= v8)
  {
    v23 = Centre_Y + v11;
    if (v23 <= Sph_YMax && v23 >= Sph_YMin)
    {
      v24 = Centre_X - v8;
      if (v24 > ClipXMax)
      {
        Sph_YMax = v23 - 1;
      }
      else
      {
        if (v24 < ClipXMin)
        {
          v24 = ClipXMin;
        }

        Sph_XMin = v24;
        v25 = Centre_X + v8;
        if (Centre_X + v8 > ClipXMax)
        {
          v25 = ClipXMax;
          Sph_XMax = v25;
          lineFunct(v23);
        }
        else
        {
          if (v25 >= ClipXMin)
          {
            Sph_XMax = v25;
            lineFunct(v23);
          }
          else
          {
            Sph_YMax = v23 - 1;
          }
        }
      }
    }

    v26 = Centre_Y - v11;
    if (v26 <= Sph_YMax && v26 >= Sph_YMin)
    {
      v27 = Centre_X - v8;
      if (v27 > ClipXMax)
      {
        Sph_YMin = v26 + 1;
      }
      else
      {
        if (v27 < ClipXMin)
        {
          v27 = ClipXMin;
        }

        Sph_XMin = v27;
        v28 = Centre_X + v8;
        if (v28 < ClipXMin)
        {
          Sph_YMin = v26 + 1;
        }
        else
        {
          if (v28 > ClipXMax)
          {
            v28 = ClipXMax;
          }

          Sph_XMax = v28;
          lineFunct(v26);
        }
      }
    }

    v19 = (U64)v11 + (U64)v7 > 0xFFFFFFFF;
    v7 += v11;
    if (v19)
    {
      v29 = Centre_Y + v8;
      if (v29 <= Sph_YMax && v29 >= Sph_YMin)
      {
        v30 = Centre_X - v11;
        if (v30 > ClipXMax)
        {
          Sph_YMax = v29 - 1;
        }
        else
        {
          if (v30 < ClipXMin)
          {
            v30 = ClipXMin;
          }

          Sph_XMin = v30;
          v31 = Centre_X + v11;
          if (Centre_X + v11 > ClipXMax)
          {
            v31 = ClipXMax;
            Sph_XMax = v31;
            lineFunct(v29);
          }
          else
          {
            if (v31 >= ClipXMin)
            {
              Sph_XMax = v31;
              lineFunct(v29);
            }
            else
            {
              Sph_YMax = v29 - 1;
            }
          }
        }
      }

      v32 = Centre_Y - v8;
      if (v32 <= Sph_YMax && v32 >= Sph_YMin)
      {
        v33 = Centre_X - v11;
        if (v33 > ClipXMax)
        {
          Sph_YMin = v32 + 1;
        }
        else
        {
          if (v33 < ClipXMin)
          {
            v33 = ClipXMin;
          }

          Sph_XMin = v33;
          v34 = Centre_X + v11;
          if (Centre_X + v11 > ClipXMax)
          {
            v34 = ClipXMax;
            Sph_XMax = v34;
            lineFunct(v32);
          }
          else
          {
            if (v34 >= ClipXMin)
            {
              Sph_XMax = v34;
              lineFunct(v32);
            }
            else
            {
              Sph_YMin = v32 + 1;
            }
          }
        }
      }

      v7 -= --v8;
    }

    ++v11;
  }

  if (Sph_YMin < Sph_YMax)
  {
    return; // 1;
  }

  return; // 0;
}
