#include <POLYGON/CLIPPERZ.H>

U32 Precalc_Offset[16] = {
  0 * sizeof(STRUC_CLIPVERTEX),
  1 * sizeof(STRUC_CLIPVERTEX),
  2 * sizeof(STRUC_CLIPVERTEX),
  3 * sizeof(STRUC_CLIPVERTEX),
  4 * sizeof(STRUC_CLIPVERTEX),
  5 * sizeof(STRUC_CLIPVERTEX),
  6 * sizeof(STRUC_CLIPVERTEX),
  7 * sizeof(STRUC_CLIPVERTEX),
  8 * sizeof(STRUC_CLIPVERTEX),
  9 * sizeof(STRUC_CLIPVERTEX),
  10 * sizeof(STRUC_CLIPVERTEX),
  11 * sizeof(STRUC_CLIPVERTEX),
  12 * sizeof(STRUC_CLIPVERTEX),
  13 * sizeof(STRUC_CLIPVERTEX),
  14 * sizeof(STRUC_CLIPVERTEX),
  15 * sizeof(STRUC_CLIPVERTEX)
};

S16 Clipping_ZFPU(int a1) //, int a2, int a3, int a4)
{
  return 0;
  /*
  int v4; // ecx@1
  int v5; // edx@1
  long double v6; // fst7@1
  int v7; // ST14_4@1
  int v8; // eax@1
  int v9; // ST04_4@1
  int v10; // ST00_4@1
  long double v11; // fst3@1
  long double v12; // fst4@1
  long double v13; // fst6@1
  __int16 result; // ax@1

  v4 = *(_DWORD *)(a4 + 8);
  v5 = *(_DWORD *)(a1 + 8);
  *(_DWORD *)(a3 + 8) = a2;
  v6 = (long double)(a2 - v5) / (long double)(v4 - v5);
  v7 = *(_DWORD *)(a1 + 12) & 0xFFFF;
  v8 = (*(_DWORD *)(a4 + 12) >> 16) - (*(_DWORD *)(a1 + 12) >> 16);
  v9 = *(_DWORD *)(a1 + 16) & 0xFFFF;
  v10 = (*(_DWORD *)(a4 + 16) & 0xFFFF) - v9;
  v11 = (long double)((*(_DWORD *)(a4 + 12) & 0xFFFF) - v7) * v6;
  v12 = (long double)*(signed int *)(a1 + 4) + (long double)(*(_DWORD *)(a4 + 4) - *(_DWORD *)(a1 + 4)) * v6;
  v13 = (long double)(signed int)(*(_DWORD *)(a1 + 12) >> 16);
  *(_DWORD *)a3 = (signed int)((long double)*(signed int *)a1 + (long double)(*(_DWORD *)a4 - *(_DWORD *)a1) * v6);
  *(_DWORD *)(a3 + 4) = (signed int)v12;
  *(_WORD *)(a3 + 16) = (signed __int16)(v6 * (long double)v10 + (long double)v9);
  *(_DWORD *)(a3 + 12) = ((signed int)(v13 + (long double)v8 * v6) << 16) | (unsigned __int16)(signed int)((long double)v7 + v11);
  result = *(_WORD *)(a4 + 18);
  *(_WORD *)(a3 + 18) = result;
  return result;
  */
}

// Call:
// ESI = Src vertex list (list of STRUC_CLIPVERTEX)
// EDI = Dst vertex list (list of STRUC_CLIPVERTEX)
// EAX = Clipping plane
// EBX = Sign of comparison (select the visible half space)
// 	0  means positive
// 	-1 means negative
// ECX = Nb of vertex in src list
// Return:
// EAX = Nb of vertex in new list

// STRUC_CLIPVERTEX dst[],
// STRUC_CLIPVERTEX src[],
// U32 nbvertex,
// S32 zclip,
// S32 flag
U32 ClipperZ(STRUC_CLIPVERTEX dst[], STRUC_CLIPVERTEX src[],
									U32 nbvertex, S32 zclip, S32 flag)
{
  STRUC_CLIPVERTEX *v5; // edi@1
  int v6; // ecx@2
  STRUC_CLIPVERTEX *v7; // esi@2
  signed int v8; // edx@2
  signed int v9; // eax@3
  int v10; // ecx@5
  int v11; // ecx@8
  int v12; // edx@9
  int v13; // edx@9
  int v14; // eax@9
  STRUC_CLIPVERTEX *v15; // esi@12
  S32 v16; // eax@12
  signed int v17; // edx@12
  signed int v18; // eax@13
  int v19; // ecx@15
  int v20; // ecx@18
  int v21; // edx@19
  int v22; // edx@19
  int v23; // eax@19
  STRUC_CLIPVERTEX *v25; // [sp-1Ch] [bp-20h]@2
  STRUC_CLIPVERTEX *v26; // [sp-1Ch] [bp-20h]@12
  int savedregs; // [sp+4h] [bp+0h]@2

  v5 = dst;
  if (!flag) // Which half-space?
  {
    v6 = 0;
    v7 = src - 1;
    v25 = &src[nbvertex - 1]; // Last vertex
    v8 = v25->V_Z0 > zclip;
    // *****************
    // *** Main loop ***
    // *****************
    while (1)
    {
      // @@NegTest_Next_Pt:
      // Check next point
      v9 = v7[1].V_Z0;
      v7++; // Current point
      if (v9 <= zclip)
      {
        break;
      }
      // *************************************************
      // *** Current vertex is in the wrong half-space ***
      // *************************************************
      if (v8) // Was last point visible ?
      {
LABEL_10:
        if (v7 == v25)
        {
          return v6;
        }
      }
      else
      {
        Clipping_ZFPU(v6);
        v5++;
        v6 = v10 + 1;
        v8 = 1;
        if (v7 == v25)
        {
          return v6;
        }
      }
    }
    // ************************************************
    // *** Current vertex is in the good half-space ***
    // ************************************************
    if (v8)
    {
      Clipping_ZFPU(v6);
      v5++;
      v6 = v11 + 1;
    }
    v7->V_X0 = v5->V_X0;
    v7->V_Y0 = v5->V_Y0;
    v7->V_Z0 = v5->V_Z0;
    v7->V_MapU = v5->V_MapU;
    v7->V_MapV = v5->V_MapV;
    v7->V_Light = v5->V_Light;
    v7->V_Dummy = v5->V_Dummy;
    ++v6;
    v5++;
    v8 = 0;
    goto LABEL_10;
  }
  v6 = 0;
  v15 = src - 1;
  v26 = &src[nbvertex - 1]; // last vertex
  v16 = v26->V_Z0;
  v17 = v16 < zclip; // 1 if vertex in wrong half-space

  // *****************
  // *** Main loop ***
  // *****************
  do
  {
    while (1)
    {
      v18 = v15[1].V_Z0;
      v15 ++;
      if (v18 >= zclip)
      {
        break;
      }
      // *************************************************
      // *** Current vertex is in the wrong half-space ***
      // *************************************************
      if (v17) // Was last point visible ?
      {
        goto LABEL_20;
      }
      Clipping_ZFPU(v6);
      v5++; // A point was created: registers it!
      v6 = v19 + 1;
      v17 = 1; // Last point not visible
      if (v15 == v26)
      {
        return v6;
      }
    }
    // @@Visible:
    if (v17)
    {
      Clipping_ZFPU(v6);
      v5++;
      v6 = v20 + 1;
    }
    // @@Pas_Inters:
    v21 = v15->V_Y0;
    v5->V_X0 = v15->V_X0;
    v5->V_Y0 = v15->V_Y0;
    v5->V_Z0 = v15->V_Z0;
    v5->V_MapU = v15->V_MapU;
    v5->V_MapV = v15->V_MapV;
    v5->V_Light = v15->V_Light;
    v5->V_Dummy = v15->V_Dummy;
    ++v6;
    v5++;
    v17 = 0;
    // @@TestEnd:
LABEL_20:
    ;
  }
  while (v15 != v26);
  return v6;
}
