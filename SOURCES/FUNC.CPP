#include "FUNC.H"

U8 SearchBoundColRGB( U8 Rouge, U8 Vert, U8 Bleu, U8 *Palette, U8 coulmin, U8 coulmax )
{
  unsigned int v6; // ecx@1
  unsigned int v7; // edi@1
  U8 *v8; // esi@1
  int v9; // edx@3
  int v10; // eax@3
  unsigned int v11; // edx@3
  U32 v13; // [sp-4h] [bp-10h]@1

  v6 = coulmin;
  v7 = -1;
  v13 = coulmin;
  v8 = Palette + 3 * coulmin;
  do
  {
    while ( 1 )
    {
      v9 = (*(v8 + 1) - Rouge) * (*(v8 + 1) - Rouge) + (*v8 - Vert) * (*v8 - Vert);
      v10 = *(v8 + 2);
      v8 += 3;
      ++v6;
      v11 = (v10 - Bleu) * (v10 - Bleu) + v9;
      if ( v11 < v7 )
        break;
      if ( v6 > coulmax )
        return (v13 - 1) & 0xFF;
    }
    v7 = v11;
    v13 = v6;
  }
  while ( v6 <= coulmax );
  return (v13 - 1) & 0xFF;
}
