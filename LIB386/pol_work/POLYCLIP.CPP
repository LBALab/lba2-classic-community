#include <POLYGON/CLIPPERZ.H>

void Clipping_ZFPU(STRUC_CLIPVERTEX *current, STRUC_CLIPVERTEX *last, STRUC_CLIPVERTEX *dst, int zclip) //, int zclip, int dst, int src)
{
  // NOTE: in ASM this function starts with fldcw	[Status_Int]
  //       which basically means conversions from float to int chop the
  //       fractional part.
  S32 currentZ = current->V_Z0;
  S32 lastZ = last->V_Z0;
  long double factor = (long double)(zclip - lastZ) / (long double)(currentZ - lastZ);
  S32 lastU = last->V_MapU;
  S32 diffV = current->V_MapV - last->V_MapV;
  S32 lastLight = last->V_Light;
  S32 diffLight = current->V_Light - lastLight;
  long double clippedU = (long double)(current->V_MapU - lastU) * factor;
  long double clippedY = (long double)last->V_Y0 + (long double)(current->V_Y0 - last->V_Y0) * factor;
  long double lastV = (long double)last->V_MapV;

  dst->V_X0 = (S32)((long double)last->V_X0 + (long double)(current->V_X0 - last->V_X0) * factor);
  dst->V_Y0 = (S32)clippedY;
  dst->V_Z0 = zclip;
  dst->V_MapU = (U16)((long double)lastU + clippedU);
  dst->V_MapV = (U16)(lastV + (long double)diffV * factor);
  dst->V_Light = (U16)(factor * (long double)diffLight + (long double)lastLight);
  dst->V_Dummy = current->V_Dummy;
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
  U32 newNbVertex; // ecx@2
  STRUC_CLIPVERTEX *v7; // esi@2
  signed int v8; // edx@2
  signed int v9; // eax@3
  STRUC_CLIPVERTEX *v15; // esi@12
  S32 v16; // eax@12
  signed int v17; // edx@12
  signed int v18; // eax@13

  v5 = dst;
  if (!flag) // Which half-space?
  {
    newNbVertex = 0;
    v7 = src - 1;
    STRUC_CLIPVERTEX *lastVertex = &src[nbvertex - 1]; // Last vertex
    v8 = lastVertex->V_Z0 > zclip;
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
        if (v7 == lastVertex)
        {
          return newNbVertex;
        }
      }
      else
      {
        Clipping_ZFPU(v7, lastVertex, v5, zclip);
        v5++;
        newNbVertex++;
        v8 = 1;
        if (v7 == lastVertex)
        {
          return newNbVertex;
        }
      }
    }
    // ************************************************
    // *** Current vertex is in the good half-space ***
    // ************************************************
    if (v8)
    {
      Clipping_ZFPU(v7, lastVertex, v5, zclip);
      v5++;
      newNbVertex++;
    }
    v7->V_X0 = v5->V_X0;
    v7->V_Y0 = v5->V_Y0;
    v7->V_Z0 = v5->V_Z0;
    v7->V_MapU = v5->V_MapU;
    v7->V_MapV = v5->V_MapV;
    v7->V_Light = v5->V_Light;
    v7->V_Dummy = v5->V_Dummy;
    ++newNbVertex;
    v5++;
    v8 = 0;
    goto LABEL_10;
  }
  newNbVertex = 0;
  v15 = src - 1;
  STRUC_CLIPVERTEX *lastVertex = &src[nbvertex - 1]; // last vertex
  v16 = lastVertex->V_Z0;
  v17 = v16 < zclip; // 1 if vertex in wrong half-space

  // *****************
  // *** Main loop ***
  // *****************
  do
  {
    while (1)
    {
      v18 = v15[1].V_Z0;
      v15++;
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
      Clipping_ZFPU(v15, lastVertex, v5, zclip);
      v5++; // A point was created: registers it!
      newNbVertex++;
      v17 = 1; // Last point not visible
      if (v15 == lastVertex)
      {
        return newNbVertex;
      }
    }
    // @@Visible:
    if (v17)
    {
      Clipping_ZFPU(v15, lastVertex, v5, zclip);
      v5++;
      newNbVertex++;
    }
    // @@Pas_Inters:
    v5->V_X0 = v15->V_X0;
    v5->V_Y0 = v15->V_Y0;
    v5->V_Z0 = v15->V_Z0;
    v5->V_MapU = v15->V_MapU;
    v5->V_MapV = v15->V_MapV;
    v5->V_Light = v15->V_Light;
    v5->V_Dummy = v15->V_Dummy;
    newNbVertex++;
    v5++;
    v17 = 0;
    // @@TestEnd:
LABEL_20:
    ;
  }
  while (v15 != lastVertex);
  return newNbVertex;
}
