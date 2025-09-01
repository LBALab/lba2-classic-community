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
  U32 newNbVertex = 0;

  STRUC_CLIPVERTEX *outVertex = dst;
  if (flag == 0) // Which half-space?
  {
    STRUC_CLIPVERTEX *currentVertex = src - 1;
    STRUC_CLIPVERTEX *lastVertex = &src[nbvertex - 1];
    U8 lastVertexVisible = lastVertex->V_Z0 < zclip;

    // *****************
    // *** Main loop ***
    // *****************
    while (1)
    {
      // @@NegTest_Next_Pt:
      currentVertex++; // Current point
      if (currentVertex->V_Z0 >= zclip)
      {
        break;
      }
      // *************************************************
      // *** Current vertex is in the wrong half-space ***
      // *************************************************
      if (lastVertexVisible) // Was last point visible ?
      {
LABEL_10:
        if (currentVertex == lastVertex)
        {
          return newNbVertex;
        }
      }
      else
      {
        Clipping_ZFPU(currentVertex, lastVertex, outVertex, zclip);
        outVertex++;
        newNbVertex++;
        lastVertexVisible = 1;
        if (currentVertex == lastVertex)
        {
          return newNbVertex;
        }
      }
    }
    // ************************************************
    // *** Current vertex is in the good half-space ***
    // ************************************************
    if (lastVertexVisible)
    {
      Clipping_ZFPU(currentVertex, lastVertex, outVertex, zclip);
      outVertex++;
      newNbVertex++;
    }
    currentVertex->V_X0 = outVertex->V_X0;
    currentVertex->V_Y0 = outVertex->V_Y0;
    currentVertex->V_Z0 = outVertex->V_Z0;
    currentVertex->V_MapU = outVertex->V_MapU;
    currentVertex->V_MapV = outVertex->V_MapV;
    currentVertex->V_Light = outVertex->V_Light;
    currentVertex->V_Dummy = outVertex->V_Dummy;
    ++newNbVertex;
    outVertex++;
    lastVertexVisible = 0;
    goto LABEL_10;
  }

  STRUC_CLIPVERTEX *currentVertex = src - 1;
  STRUC_CLIPVERTEX *lastVertex = &src[nbvertex - 1]; // last vertex
  U8 lastVertexVisible = lastVertex->V_Z0 > zclip; // 1 if vertex in wrong half-space

  // *****************
  // *** Main loop ***
  // *****************
  do
  {
    while (1)
    {
      currentVertex++;
      if (currentVertex->V_Z0 <= zclip)
      {
        break;
      }
      // *************************************************
      // *** Current vertex is in the wrong half-space ***
      // *************************************************
      if (lastVertexVisible) // Was last point visible ?
      {
        goto LABEL_20;
      }
      Clipping_ZFPU(currentVertex, lastVertex, outVertex, zclip);
      outVertex++; // A point was created: registers it!
      newNbVertex++;
      lastVertexVisible = 1; // Last point not visible
      if (currentVertex == lastVertex)
      {
        return newNbVertex;
      }
    }
    // @@Visible:
    if (lastVertexVisible)
    {
      Clipping_ZFPU(currentVertex, lastVertex, outVertex, zclip);
      outVertex++;
      newNbVertex++;
    }
    // @@Pas_Inters:
    outVertex->V_X0 = currentVertex->V_X0;
    outVertex->V_Y0 = currentVertex->V_Y0;
    outVertex->V_Z0 = currentVertex->V_Z0;
    outVertex->V_MapU = currentVertex->V_MapU;
    outVertex->V_MapV = currentVertex->V_MapV;
    outVertex->V_Light = currentVertex->V_Light;
    outVertex->V_Dummy = currentVertex->V_Dummy;
    newNbVertex++;
    outVertex++;
    lastVertexVisible = 0;
    // @@TestEnd:
LABEL_20:
    ;
  }
  while (currentVertex != lastVertex);

  return newNbVertex;
}
