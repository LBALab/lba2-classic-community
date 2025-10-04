#include <POLYGON/CLIPPERZ.H>

void Clipping_ZFPU(STRUC_CLIPVERTEX *current, STRUC_CLIPVERTEX *last, STRUC_CLIPVERTEX *dst, int zclip)
{
  S32 currentZ = current->V_Z0;
  S32 lastZ = last->V_Z0;
  long double factor = (long double)(zclip - lastZ) / (long double)(currentZ - lastZ);
  
  U32 currentMapU = *(U32*)&current->V_MapU;
  U32 lastMapU = *(U32*)&last->V_MapU;
  
  S32 currentU = currentMapU & 0xFFFF;
  S32 currentV = (currentMapU >> 16) & 0xFFFF;
  S32 lastU = lastMapU & 0xFFFF;
  S32 lastV = (lastMapU >> 16) & 0xFFFF;
  
  S32 diffU = currentU - lastU;
  S32 diffV = currentV - lastV;
  
  S32 currentLight = current->V_Light & 0xFFFF;
  S32 lastLight = last->V_Light & 0xFFFF;
  S32 diffLight = currentLight - lastLight;
  
  long double interpX = (long double)last->V_X0 + (long double)(current->V_X0 - last->V_X0) * factor;
  long double interpY = (long double)last->V_Y0 + (long double)(current->V_Y0 - last->V_Y0) * factor;
  long double interpU = (long double)lastU + (long double)diffU * factor;
  long double interpV = (long double)lastV + (long double)diffV * factor;
  long double interpLight = (long double)lastLight + (long double)diffLight * factor;
  
  dst->V_X0 = (S32)interpX;
  dst->V_Y0 = (S32)interpY;
  dst->V_Z0 = zclip;
  
  U32 finalU = ((S32)interpU) & 0xFFFF;
  U32 finalV = ((S32)interpV) & 0xFFFF;
  *(U32*)&dst->V_MapU = finalU | (finalV << 16);
  
  dst->V_Light = (U16)((S32)interpLight);
  dst->V_Dummy = current->V_Dummy;
}

U32 ClipperZ(STRUC_CLIPVERTEX dst[], STRUC_CLIPVERTEX src[], U32 nbvertex, S32 zclip, S32 flag)
{
  U32 newNbVertex = 0;
  STRUC_CLIPVERTEX *outVertex = dst;
  
  if (flag == 0)
  {
    STRUC_CLIPVERTEX *currentVertex = src - 1;
    STRUC_CLIPVERTEX *lastVertex = &src[nbvertex - 1];
    U8 lastVertexVisible = (lastVertex->V_Z0 < zclip) ? 1 : 0;

    while (1)
    {
      currentVertex++;
      
      if (currentVertex->V_Z0 >= zclip)
      {
        // Current vertex is in the good half-space
        if (lastVertexVisible)
        {
          Clipping_ZFPU(currentVertex, lastVertex, outVertex, zclip);
          outVertex++;
          newNbVertex++;
        }
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
      }
      else
      {
        // Current vertex is in the wrong half-space
        if (!lastVertexVisible)
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
      
      lastVertex = currentVertex;
      if (currentVertex == &src[nbvertex - 1])
      {
        return newNbVertex;
      }
    }
  }
  else // Negative half-space case
  {
    STRUC_CLIPVERTEX *currentVertex = src - 1;
    STRUC_CLIPVERTEX *lastVertex = &src[nbvertex - 1];
    U8 lastVertexVisible = (lastVertex->V_Z0 > zclip) ? 1 : 0;

    while (1)
    {
      currentVertex++;
      
      if (currentVertex->V_Z0 <= zclip)
      {
        // Current vertex is in the good half-space
        if (lastVertexVisible)
        {
          Clipping_ZFPU(currentVertex, lastVertex, outVertex, zclip);
          outVertex++;
          newNbVertex++;
        }
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
      }
      else
      {
        // Current vertex is in the wrong half-space
        if (!lastVertexVisible)
        {
          Clipping_ZFPU(currentVertex, lastVertex, outVertex, zclip);
          outVertex++;
          newNbVertex++;
          lastVertexVisible = 1;
          if (currentVertex == &src[nbvertex - 1])
          {
            return newNbVertex;
          }
        }
      }
      
      lastVertex = currentVertex;
      if (currentVertex == &src[nbvertex - 1])
      {
        return newNbVertex;
      }
    }
  }
}
