#include "BOXZBUF.H"

#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>

#include <stdio.h>

S32 IncZ = 0;

// ZBufBoxOverWrite2: Copies terrain pixels from Screen onto Log wherever the
// terrain Z-buffer is closer than the object's interpolated Z depth.
// Called by DrawRecover() after an object is rendered to Log, to re-apply
// terrain occlusion (fences, buildings, etc.) over the object.
//
// Parameters:
//   z0 = camera-space depth at the object's feet (world Y = yw)
//   z1 = camera-space depth at the object's head (world Y = yw + ymax)
//   xmin..ymax = screen bounding box of the object
//
// On screen, ymin is the TOP (head) and ymax is the BOTTOM (feet).
// The ASM starts the Z interpolation from z1 (head, at screen top) and
// increments toward z0 (feet, at screen bottom), so:
//   deltaZBuf = scaledZ0 - scaledZ1  (feet - head)
//   IncZ      = deltaZBuf / deltaY
//   starting scaledZ = scaledZ1 (head)
S32	ZBufBoxOverWrite2(S32 z0, S32 z1,
		S32 xmin, S32 ymin,
		S32 xmax, S32 ymax
) {
  S32 deltaY = ymax - ymin + 1; // eax
  S32 deltaX = xmax - xmin + 1; // ebx
  // ASM: ESI = z1 * Factor (head), EDI = z0 * Factor (feet)
  S32 scaledZ0 = Fill_ZBuffer_Factor * z0; // feet_scaled (EDI in ASM)
  S32 scaledZ1 = Fill_ZBuffer_Factor * z1; // head_scaled (ESI in ASM)
  // ASM: sub edi, esi → deltaZBuf = feet - head
  S32 deltaZBuf = scaledZ0 - scaledZ1;
  IncZ = (S32)((S64)deltaZBuf / deltaY);
  // ASM loop starts with ESI (head_scaled), interpolating toward feet
  S32 scaledZ = scaledZ1;
  U32 startOffset = TabOffLine[ymin] + xmin + deltaX; // edx
  U8 *dstLog = (U8 *)Log + startOffset; // ebp
  U16 *dstZBuf = PtrZBuffer + startOffset; // edi
  U8 *dstScreen = (U8 *)Screen + startOffset; // edx
  S32 i = -deltaX;
  U8 flag = 1;

  do
  {
    // ASM uses shr (logical/unsigned shift), not sar (arithmetic/signed shift)
    U32 z0Int = (U32)scaledZ >> 16; // esi

    while (1)
    {
      // Read ZBuffer (no AGI)
      U16 z = dstZBuf[i];

      // Is our pixel hidden?
      if (z >= z0Int)
      {
        flag = 0;

        while (1)
        {
          i++;
          if (i > 0)
          {
            break;
          }

          // Read ZBuffer (no AGI)
          U16 z = dstZBuf[i];

          // Is our pixel hidden?
          if (z < z0Int)
          {
            dstLog[i] = dstScreen[i]; // No: Copy it
          }
        }

        break;
      }

      dstLog[i] = dstScreen[i]; // No: Copy it
      i++;
      if (i > 0)
      {
        break;
      }
    }

    i = -deltaX;
    dstScreen += ScreenPitch;
    dstLog += ScreenPitch;
    dstZBuf += ScreenPitch;
    scaledZ += IncZ;
    deltaY--;
  }
  while (deltaY != 0);

  return flag;
}
