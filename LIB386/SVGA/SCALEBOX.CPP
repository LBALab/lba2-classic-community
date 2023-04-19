#include <SVGA/SCALEBOX.H>

#include <SVGA/SCREEN.H>

U32 PoidFort = 0;
U32 AccuVirguleH = 0;
U32 VirguleH = 0;
U32 PoidFortH = 0;
U32 OffsetLine = 0;
U32 Largeur = 0;
U32 Hauteur = 0;

void ScaleBox(S32 xs0, S32 ys0, S32 xs1, S32 ys1, void *ptrs, S32 xd0, S32 yd0,
              S32 xd1, S32 yd1, void *ptrd)
{
  unsigned int v10; // ecx@1
  unsigned int v11; // eax@1
  unsigned int v12; // ebx@1
  U8 *v13; // edi@1
  U8 *v14; // esi@1
  unsigned int v15; // eax@1
  int v16; // ecx@2
  unsigned int v17; // edx@2
  int v18; // eax@4
  int v19; // eax@4
  int v21; // [sp+4h] [bp-4h]@0
  U8 *savedregs; // [sp+8h] [bp+0h]@4

  v10 = xd1 - xd0 + 1;
  v11 = ((xs1 - xs0 + 1) << 16) / v10;
  Largeur = xd1 - xd0 + 1;
  OffsetLine = ModeDesiredX - v10;
  v12 = v11 << 16;
  PoidFort = v11 >> 16;
  v13 = (U8 *)ptrd + TabOffLine[yd0] + xd0;
  v14 = (U8 *)ptrs + TabOffLine[ys0] + xs0;
  v15 = ((ys1 - ys0 + 1) << 16) / (unsigned int)(yd1 - yd0 + 1);
  Hauteur = yd1 - yd0 + 1;
  VirguleH = v15 << 16;
  PoidFortH = v15 >> 16;
  AccuVirguleH = 0;
  savedregs = v14;
  do
  {
    v16 = Largeur;
    v17 = 0;
    do
    {
      *v13 = *v14;
      U8 carry = (U64)v17 + (U64)v12 > 0xFFFFFFFF;
      v17 += v12;
      v14 += PoidFort + carry;
      ++v13;
      --v16;
    }
    while ( v16 );
    U8 carry = (U64)AccuVirguleH + (U64)VirguleH > 0xFFFFFFFF;
    AccuVirguleH += VirguleH;
    v18 = PoidFortH + carry;
    v13 += OffsetLine;
    v14 = savedregs;
    v19 = TabOffLine[v18];
    --Hauteur;
  }
  while ( Hauteur );
}
