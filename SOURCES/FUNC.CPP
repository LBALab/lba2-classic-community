#include "FUNC.H"

U8 SearchBoundColRGB(U8 Rouge, U8 Vert, U8 Bleu, U8 *Palette, U8 coulmin, U8 coulmax)
{
  U32 coul; // ecx@1
  U32 prevCol; // edi@1
  U32 newCol; // edx@3
  U32 boundCol; // [sp-4h] [bp-10h]@1

  coul = coulmin;
  prevCol = -1;
  boundCol = coulmin;
  U8 *colors = Palette + 3 * coulmin;
  do
  {
    while ( 1 )
    {
      U8 r = colors[0];
      U8 g = colors[1];
      U8 b = colors[2];

      newCol = (r - Rouge) * (r - Rouge) + (g - Vert) * (g - Vert) + (b - Bleu) * (b - Bleu);
      colors += 3;
      ++coul;
      if (newCol < prevCol)
      {
        break;
      }

      if (coul > coulmax)
      {
        return (boundCol - 1) & 0xFF;
      }
    }
    prevCol = newCol;
    boundCol = coul;
  }
  while ( coul <= coulmax );

  return (boundCol - 1) & 0xFF;
}
