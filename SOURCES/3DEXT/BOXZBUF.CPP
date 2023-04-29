#include "BOXZBUF.H"

#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>

S32 IncZ = 0;

S32	ZBufBoxOverWrite2(S32 z0, S32 z1,
		S32 xmin, S32 ymin,
		S32 xmax, S32 ymax
) {
  U32 deltaY; // eax@1
  U32 v4; // esi@1
  int deltaX; // ebx@1
  int v7; // edx@1
  U8 *v8; // ebp@1
  U16 *v9; // edi@1
  U8 *v10; // edx@1
  int v11; // ebx@1
  char v12; // ch@1
  U32 v13; // esi@2
  U8 v14; // of@4
  int v16; // [sp-24h] [bp-28h]@2
  U32 v17; // [sp-20h] [bp-24h]@2
  U32 v18; // [sp-1Ch] [bp-20h]@2

  deltaY = ymax - ymin + 1;
  v4 = Fill_ZBuffer_Factor * z0;
  deltaX = xmax - xmin + 1;
  IncZ = (S32)(Fill_ZBuffer_Factor * z1 - v4) / (S32)deltaY;
  v7 = TabOffLine[ymin] + xmin + deltaX;
  v8 = (U8 *)Log + v7;
  v9 = PtrZBuffer + v7;
  v10 = (U8 *)Screen + v7;
  v11 = -deltaX;
  v12 = 1;
  do
  {
    v18 = deltaY;
    v17 = v4;
    v13 = v4 >> 16;
    v16 = v11;
    while ( 1 )
    {
      // Read ZBuffer (no AGI)
      U16 z = *(v9 + v11);
      // Is our pixel hidden?
      if ( z >= v13 )
        break;
      v8[v11] = v10[v11];
      v14 = __OFADD__(1, v11++);
      if ( !((U8)((v11 < 0) ^ v14) | (v11 == 0)) )
      {
        goto LABEL_9;
      }
    }
    v12 = 0;
    while ( 1 )
    {
      v14 = __OFADD__(1, v11++);
      if ( !((U8)((v11 < 0) ^ v14) | (v11 == 0)) )
      {
        break;
      }
      U16 z = *(v9 + v11);
      if (z < v13)
      {
        v8[v11] = v10[v11];
      }
    }
LABEL_9:
    v11 = v16;
    v10 += ScreenPitch;
    v8 += ScreenPitch;
    v9 += 2 * ScreenPitch;
    v4 = IncZ + v17;
    deltaY = v18 - 1;
  }
  while ( v18 != 1 );

  return (U8)v12;
}
