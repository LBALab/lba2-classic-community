/*
;----------------------------------------------------------------------------
;				S_LINE.ASM 386
;			      (c) Adeline 1993-96
;----------------------------------------------------------------------------
            OPTION SCOPED

			.386
			.MODEL FLAT, C

			INCLUDE FILLER.INC

;----------------------------------------------------------------------------
			.DATA

;----------------------------------------------------------------------------
			extrn	C ClipXMin		:DWORD
			extrn	C ClipYMin		:DWORD
			extrn	C ClipXMax		:DWORD
			extrn	C ClipYMax		:DWORD
			extrn	C Log			:DWORD
			extrn	C PtrZBuffer		:DWORD
			extrn	C PTR_TabOffLine		:DWORD
			extrn	C ScreenPitch		:DWORD
			Extrn	C	Fill_Flag_Fog		:BYTE
			Extrn	C	Fill_Flag_ZBuffer	:BYTE
			Extrn	C	Fill_Flag_NZW		:BYTE
			Extrn	C	Fill_Logical_Palette	:BYTE

Z1			dd	0			; For the ZBuffer
Z2			dd	0
DZ			dd	0
Color			db	0

s_eax			dd	0
s_ebx			dd	0
s_ecx			dd	0
s_edx			dd	0
s_esi			dd	0
s_edi			dd	0
s_ebp			dd	0

; Runtime code patch refactor, uses memory instead of instruction substitution
LinePatch   DWORD 12345678h
PtrLog      DWORD 12345678h
PtrZBuf     DWORD 12345678h
PtrZBuf1    DWORD 12345678h
YPtrZBuf    DWORD 12345678h
YPtrZBuf1   DWORD 12345678h
YPtrLog     DWORD 12345678h
Row         DWORD 12345678h
YDX         DWORD 12345678h
YDY         DWORD 12345678h
YDY1        DWORD 12345678h
YDZ         DWORD 12345678h
DXPatch     DWORD 12345678h
DY          DWORD 12345678h
DZPatch     DWORD 12345678h
DX1         DWORD 12345678h
IZ          DWORD 12345678h
YIZ         DWORD 12345678h
ColorX      BYTE 12h
ColorY      BYTE 12h

;----------------------------------------------------------------------------
			.CODE

;----------------------------------------------------------------------------
			public	C	Line
			public	C	LineZBufNZW
			public	C	LineZBuf
			public	C	Line_A

;----------------------------------------------------------------------------
;		LineZBufNZW( x0, y0, z0, x1, y1, z1, coul )
;
LineZBufNZW		PROC	USES esi edi ebp ebx,\
			x0:DWORD, y0:DWORD, zb0:DWORD,\
			x1:DWORD, y1:DWORD, zb1:DWORD,\
			coul:DWORD

; 			X0:ax  Y0:bx
;			X1:cx  Y1:DX
;			couleur: bp

			mov	eax, x0
			mov	ebx, y0
			mov	edi, zb0
			mov	ecx, x1
			mov	edx, y1
			mov	esi, zb1
			mov	ebp, coul

			call	Line_ZBuffer_NZW

			ret
LineZBufNZW		ENDP


;----------------------------------------------------------------------------
;		LineZBufNZW( x0, y0, z0, x1, y1, z1, coul )
;
LineZBuf		PROC	USES esi edi ebp ebx,\
			x0:DWORD, y0:DWORD, zb0:DWORD,\
			x1:DWORD, y1:DWORD, zb1:DWORD,\
			coul:DWORD

; 			X0:ax  Y0:bx
;			X1:cx  Y1:DX
;			couleur: bp

			mov	eax, x0
			mov	ebx, y0
			mov	edi, zb0
			mov	ecx, x1
			mov	edx, y1
			mov	esi, zb1
			mov	ebp, coul

			call	Line_ZBuffer

			ret
LineZBuf		ENDP
*/

#include <SVGA/FIL_LINE.H>
#include <POLYGON/POLY.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <stdio.h>

S32 Z1 = 0;			// For the ZBuffer
S32 Z2 = 0;
S32 DZ = 0;
U8 Color = 0;

// Runtime code patch refactor, uses memory instead of instruction substitution
S32 LinePatch = 0x12345678;
U8 *PtrLog = NULL;
U16 *PtrZBuf = NULL;
U16 *PtrZBuf1 = NULL;
U16 *YPtrZBuf = NULL;
U16 *YPtrZBuf1 = NULL;
U8 *YPtrLog = NULL;
U32 Row = 0x12345678;
U32 YDX = 0x12345678;
U32 YDY = 0x12345678;
U32 YDY1 = 0x12345678;
U32 YDZ = 0x12345678;
S32 DXPatch = 0x12345678;
S32 DY = 0x12345678;
S32 DZPatch = 0x12345678;
S32 DX1 = 0x12345678;
S32 IZ = 0x12345678;
U32 YIZ = 0x12345678;
U8 ColorX = 0x12;
U8 ColorY = 0x12;

// TODO: Verify
void Line_ZBuffer(S32 x0, S32 y0, S32 x1, S32 y1, S32 col, S32 z1, S32 z2)
{
  Z1 = z1 << 16;
  Z2 = z2 << 16;

  S32 color = col;
  col &= 0xFF;
  if (Fill_Flag_Fog)
  {
    color = Fill_Logical_Palette[col];
  }
  Color = color;

  while (true)
  {
    // @@Trace_Still:
    DZ = Z2 - Z1;

    S32 diffX = x1 - x0;
    S32 diffY = y1 - y0;

    if (diffX == 0)
    {
      // *** DeltaX nul ***
      // @@DX_Nul:
      if (x0 < ClipXMin || x0 > ClipXMax)
      {
        return;
      }

      if (diffY == 0)
      {
        // *** Plot ***
        // @@Plot:
        if (y0 < ClipYMin || y0 > ClipYMax)
        {
          return;
        }

        U8 *dst = (U8 *)Log + PTR_TabOffLine[y0] + x0;
        *dst = Color;
        return;
      }

      if (y0 < ClipYMin)
      {
        if (y1 < ClipYMin)
        {
          return;
        }

        Z1 += (ClipYMin - y0) * DZ / diffY;
        y0 = ClipYMin;
        continue;
      }

      // @@DX_Y1:
      if (y0 > ClipYMax)
      {
        if (y1 > ClipYMax)
        {
          return;
        }

        Z1 += (ClipYMax - y0) * DZ / diffY;
        y0 = ClipYMax;
        continue;
      }

      // @@DX_Y1b:
      if (y1 < ClipYMin)
      {
        Z2 += (ClipYMin - y1) * DZ / diffY;
        y1 = ClipYMin;
        continue;
      }

      // @@DX_Y2:
      if (y1 > ClipYMax)
      {
        Z2 += (ClipYMax - y1) * DZ / diffY;
        y1 = ClipYMax;
        continue;
      }
    }
    else if (diffY == 0)
    {
      // *** DeltaY nul ***
      // @@DY_Nul:
      if (y0 < ClipYMin || y0 > ClipYMax)
      {
        return;
      }

      if (x0 < ClipXMin)
      {
        if (x1 < ClipXMin)
        {
          return;
        }

        Z1 -= (x0 - ClipXMin) * DZ / diffX;
        x0 = ClipXMin;
        continue;
      }

      // @@DY_X1:
      if (x0 > ClipXMax)
      {
        if (x1 > ClipXMax)
        {
          return;
        }

        Z1 -= (x0 - ClipXMax) * DZ / diffX;
        x0 = ClipXMax;
        continue;
      }

      // @@DY_X1b:
      if (x1 < ClipXMin)
      {
        Z2 -= (x1 - ClipXMin) * DZ / diffX;
        x1 = ClipXMin;
        continue;
      }

      // @@DY_X2:
      if (x1 > ClipXMax)
      {
        Z2 -= (x1 - ClipXMax) * DZ / diffX;
        x1 = ClipXMax;
        continue;
      }
    }
    else if (x0 < ClipXMin)
    {
      if (x1 < ClipXMin)
      {
        return;
      }

      Z1 -= (x0 - ClipXMin) * DZ / diffX;
      y0 -= (x0 - ClipXMin) * diffY / diffX;
      x0 = ClipXMin;
      continue;
    }
    // @@Clip_X0:
    else if (x0 > ClipXMax)
    {
      if (x1 > ClipXMax)
      {
        return;
      }

      Z1 -= (x0 - ClipXMax) * DZ / diffX;
      y0 -= (x0 - ClipXMax) * diffY / diffX;
      x0 = ClipXMax;
      continue;
    }
    // @@Clip_X1:
    else if (x1 < ClipXMin)
    {
      Z2 -= (x1 - ClipXMin) * DZ / diffX;
      y1 -= (x1 - ClipXMin) * diffY / diffX;
      x1 = ClipXMin;
      continue;
    }
    // @@Clip_X2:
    else if (x1 > ClipXMax)
    {
      Z2 -= (x1 - ClipXMax) * DZ / diffX;
      y1 -= (x1 - ClipXMax) * diffY / diffX;
      x1 = ClipXMax;
      continue;
    }
    // @@Clip_X3:
    else if (y0 < ClipYMin)
    {
      if (y1 < ClipYMin)
      {
        return;
      }
      Z1 += (ClipYMin - y0) * DZ / diffY;
      x0 += (ClipYMin - y0) * diffX / diffY;
      y0 = ClipYMin;
      continue;
    }
    // @@Clip_Y0:
    else if (y0 > ClipYMax)
    {
      if (y1 > ClipYMax)
      {
        return;
      }

      Z1 += (ClipYMax - y0) * DZ / diffY;
      x0 += (ClipYMax - y0) * diffX / diffY;
      y0 = ClipYMax;
      continue;
    }
    // @@Clip_Y1:
    else if (y1 < ClipYMin)
    {
      Z2 += (ClipYMin - y1) * DZ / diffY;
      x1 += (ClipYMin - y1) * diffX / diffY;
      y1 = ClipYMin;
      continue;
    }
    // @@Clip_Y2:
    else if (y1 > ClipYMax)
    {
      Z2 += (ClipYMax - y1) * DZ / diffY;
      x1 += (ClipYMax - y1) * diffX / diffY;
      y1 = ClipYMax;
      continue;
    }

    // @@Clip_Y3:
    // @@Draw_Line:
    S32 incrY = ScreenPitch;
    S32 incrX = 1;
    S32 deltaX = x1 - x0;
    if (deltaX < 0)
    {
      deltaX = -deltaX;
      incrX = -incrX;
    }

    S32 deltaY = y1 - y0;
    if (deltaY < 0)
    {
      deltaY = -deltaY;
      incrY = -incrY;
    }

    U32 offset = PTR_TabOffLine[y0] + x0; // ebx

    if (deltaX >= deltaY)
    {
      DY = deltaY;
      DXPatch = deltaX;
      DX1 = deltaX;

      S32 dz = DZ;
      if (incrX < 0)
      {
        dz = -dz;
      }

      S64 bigDZ = (((S64)(dz >> 31)) << 32) | dz;
      S32 dz_by_deltaX = bigDZ / deltaX;
      DZPatch = dz_by_deltaX << 16;
      IZ = dz_by_deltaX >> 16;

      LinePatch = incrY;

      PtrLog = (U8 *)Log;
      PtrZBuf = PtrZBuffer;
      PtrZBuf1 = PtrZBuffer;

      S32 halfX = deltaX / 2;
      S32 x = deltaX;

      U32 decZ1 = Z1 << 16; // esi
      U32 intZ1 = Z1 >> 16; // edi

      do
      {
        // @@Do_X:
        if (PtrZBuf[offset] >= (intZ1 & 0xFFFF))
        {
          PtrZBuf1[offset] = intZ1 & 0xFFFF;
          PtrLog[offset] = Color;
        }

        halfX += DY;
        if (halfX >= DXPatch)
        {
          halfX -= DX1;
          offset += LinePatch;
        }

        offset += incrX;

        U8 carry = ((U64)(U32)decZ1 + (U64)(U32)DZPatch) > 0xFFFFFFFF;
        decZ1 += DZPatch;
        intZ1 += IZ + carry;

        x--;
      } while (x >= 0);

      return;
    }
    else
    {
      // @@Draw_Y:
      YDX = deltaX;
      YDY = deltaY;
      YDY1 = deltaY;

      S32 dz = DZ;
      if (incrY < 0)
      {
        dz = -dz;
      }

      S64 bigDZ = (((S64)(dz >> 31)) << 32) | dz;
      S32 dz_by_deltaY = bigDZ / deltaY;
      YDZ = dz_by_deltaY << 16;
      YIZ = dz_by_deltaY >> 16;

      Row = incrX;

      YPtrLog = (U8 *)Log;
      YPtrZBuf = PtrZBuffer;
      YPtrZBuf1 = PtrZBuffer;

      S32 halfY = deltaY / 2;
      S32 y = deltaY;

      U32 decZ1 = Z1 << 16; // esi
      U32 intZ1 = Z1 >> 16; // edi

      do
      {
        // @@Do_Y:
        if (YPtrZBuf[offset] >= (intZ1 & 0xFFFF))
        {
          YPtrZBuf1[offset] = intZ1 & 0xFFFF;
          YPtrLog[offset] = Color;
        }

        halfY += YDX;
        if (halfY >= YDY)
        {
          halfY -= YDY1;
          offset += Row;
        }

        offset += incrY;

        U8 carry = ((U64)(U32)decZ1 + (U64)(U32)YDZ) > 0xFFFFFFFF;
        decZ1 += YDZ;
        intZ1 += YIZ + carry;

        y--;
      } while (y >= 0);

      return;
    }
  }
}

// TODO: Verify
void Line_ZBuffer_NZW(S32 x0, S32 y0, S32 x1, S32 y1, S32 col, S32 z1, S32 z2)
{
  Z1 = z1 << 16;
  Z2 = z2 << 16;

  S32 color = col;
  col &= 0xFF;
  if (Fill_Flag_Fog)
  {
    color = Fill_Logical_Palette[col];
  }
  Color = color;

  while (true)
  {
    // @@Trace_Still:
    DZ = Z2 - Z1;

    S32 diffX = x1 - x0;
    S32 diffY = y1 - y0;

    if (diffX == 0)
    {
      // *** DeltaX nul ***
      // @@DX_Nul:
      if (x0 < ClipXMin || x0 > ClipXMax)
      {
        return;
      }

      if (diffY == 0)
      {
        // *** Plot ***
        // @@Plot:
        if (y0 < ClipYMin || y0 > ClipYMax)
        {
          return;
        }

        U8 *dst = (U8 *)Log + PTR_TabOffLine[y0] + x0;
        *dst = Color;
        return;
      }

      if (y0 < ClipYMin)
      {
        if (y1 < ClipYMin)
        {
          return;
        }

        Z1 += (ClipYMin - y0) * DZ / diffY;
        y0 = ClipYMin;
        continue;
      }

      // @@DX_Y1:
      if (y0 > ClipYMax)
      {
        if (y1 > ClipYMax)
        {
          return;
        }

        Z1 += (ClipYMax - y0) * DZ / diffY;
        y0 = ClipYMax;
        continue;
      }

      // @@DX_Y1b:
      if (y1 < ClipYMin)
      {
        Z2 += (ClipYMin - y1) * DZ / diffY;
        y1 = ClipYMin;
        continue;
      }

      // @@DX_Y2:
      if (y1 > ClipYMax)
      {
        Z2 += (ClipYMax - y1) * DZ / diffY;
        y1 = ClipYMax;
        continue;
      }
    }
    else if (diffY == 0)
    {
      // *** DeltaY nul ***
      // @@DY_Nul:
      if (y0 < ClipYMin || y0 > ClipYMax)
      {
        return;
      }

      if (x0 < ClipXMin)
      {
        if (x1 < ClipXMin)
        {
          return;
        }

        Z1 -= (x0 - ClipXMin) * DZ / diffX;
        x0 = ClipXMin;
        continue;
      }

      // @@DY_X1:
      if (x0 > ClipXMax)
      {
        if (x1 > ClipXMax)
        {
          return;
        }

        Z1 -= (x0 - ClipXMax) * DZ / diffX;
        x0 = ClipXMax;
        continue;
      }

      // @@DY_X1b:
      if (x1 < ClipXMin)
      {
        Z2 -= (x1 - ClipXMin) * DZ / diffX;
        x1 = ClipXMin;
        continue;
      }

      // @@DY_X2:
      if (x1 > ClipXMax)
      {
        Z2 -= (x1 - ClipXMax) * DZ / diffX;
        x1 = ClipXMax;
        continue;
      }
    }
    else if (x0 < ClipXMin)
    {
      if (x1 < ClipXMin)
      {
        return;
      }

      Z1 -= (x0 - ClipXMin) * DZ / diffX;
      y0 -= (x0 - ClipXMin) * diffY / diffX;
      x0 = ClipXMin;
      continue;
    }
    // @@Clip_X0:
    else if (x0 > ClipXMax)
    {
      if (x1 > ClipXMax)
      {
        return;
      }

      Z1 -= (x0 - ClipXMax) * DZ / diffX;
      y0 -= (x0 - ClipXMax) * diffY / diffX;
      x0 = ClipXMax;
      continue;
    }
    // @@Clip_X1:
    else if (x1 < ClipXMin)
    {
      Z2 -= (x1 - ClipXMin) * DZ / diffX;
      y1 -= (x1 - ClipXMin) * diffY / diffX;
      x1 = ClipXMin;
      continue;
    }
    // @@Clip_X2:
    else if (x1 > ClipXMax)
    {
      Z2 -= (x1 - ClipXMax) * DZ / diffX;
      y1 -= (x1 - ClipXMax) * diffY / diffX;
      x1 = ClipXMax;
      continue;
    }
    // @@Clip_X3:
    else if (y0 < ClipYMin)
    {
      if (y1 < ClipYMin)
      {
        return;
      }
      Z1 += (ClipYMin - y0) * DZ / diffY;
      x0 += (ClipYMin - y0) * diffX / diffY;
      y0 = ClipYMin;
      continue;
    }
    // @@Clip_Y0:
    else if (y0 > ClipYMax)
    {
      if (y1 > ClipYMax)
      {
        return;
      }

      Z1 += (ClipYMax - y0) * DZ / diffY;
      x0 += (ClipYMax - y0) * diffX / diffY;
      y0 = ClipYMax;
      continue;
    }
    // @@Clip_Y1:
    else if (y1 < ClipYMin)
    {
      Z2 += (ClipYMin - y1) * DZ / diffY;
      x1 += (ClipYMin - y1) * diffX / diffY;
      y1 = ClipYMin;
      continue;
    }
    // @@Clip_Y2:
    else if (y1 > ClipYMax)
    {
      Z2 += (ClipYMax - y1) * DZ / diffY;
      x1 += (ClipYMax - y1) * diffX / diffY;
      y1 = ClipYMax;
      continue;
    }

    // @@Clip_Y3:
    // @@Draw_Line:
    S32 incrY = ScreenPitch;
    S32 incrX = 1;
    S32 deltaX = x1 - x0;
    if (deltaX < 0)
    {
      deltaX = -deltaX;
      incrX = -incrX;
    }

    S32 deltaY = y1 - y0;
    if (deltaY < 0)
    {
      deltaY = -deltaY;
      incrY = -incrY;
    }

    U32 offset = PTR_TabOffLine[y0] + x0; // ebx

    if (deltaX >= deltaY)
    {
      DY = deltaY;
      DXPatch = deltaX;
      DX1 = deltaX;

      S32 dz = DZ;
      if (incrX < 0)
      {
        dz = -dz;
      }

      S64 bigDZ = (((S64)(dz >> 31)) << 32) | dz;
      S32 dz_by_deltaX = bigDZ / deltaX;
      DZPatch = dz_by_deltaX << 16;
      IZ = dz_by_deltaX >> 16;

      LinePatch = incrY;

      PtrLog = (U8 *)Log;
      PtrZBuf = PtrZBuffer;

      S32 halfX = deltaX / 2;
      S32 x = deltaX;

      U32 decZ1 = Z1 << 16; // esi
      U32 intZ1 = Z1 >> 16; // edi

      do
      {
        // @@Do_X:
        if (PtrZBuf[offset] >= (intZ1 & 0xFFFF))
        {
          PtrLog[offset] = Color;
        }

        halfX += DY;
        if (halfX >= DXPatch)
        {
          halfX -= DX1;
          offset += LinePatch;
        }

        offset += incrX;

        U8 carry = ((U64)(U32)decZ1 + (U64)(U32)DZPatch) > 0xFFFFFFFF;
        decZ1 += DZPatch;
        intZ1 += IZ + carry;

        x--;
      } while (x >= 0);

      return;
    }
    else
    {
      // @@Draw_Y:
      YDX = deltaX;
      YDY = deltaY;
      YDY1 = deltaY;

      S32 dz = DZ;
      if (incrY < 0)
      {
        dz = -dz;
      }

      S64 bigDZ = (((S64)(dz >> 31)) << 32) | dz;
      S32 dz_by_deltaY = bigDZ / deltaY;
      YDZ = dz_by_deltaY << 16;
      YIZ = dz_by_deltaY >> 16;

      Row = incrX;

      YPtrLog = (U8 *)Log;
      YPtrZBuf = PtrZBuffer;

      S32 halfY = deltaY / 2;
      S32 y = deltaY;

      U32 decZ1 = Z1 << 16; // esi
      U32 intZ1 = Z1 >> 16; // edi

      do
      {
        // @@Do_Y:
        if (YPtrZBuf[offset] >= (intZ1 & 0xFFFF))
        {
          YPtrLog[offset] = Color;
        }

        halfY += YDX;
        if (halfY >= YDY)
        {
          halfY -= YDY1;
          offset += Row;
        }

        offset += incrY;

        U8 carry = ((U64)(U32)decZ1 + (U64)(U32)YDZ) > 0xFFFFFFFF;
        decZ1 += YDZ;
        intZ1 += YIZ + carry;

        y--;
      } while (y >= 0);

      return;
    }
  }
}

void Line_Entry(S32 x0, S32 y0, S32 x1, S32 y1, S32 col)
{
  S32 color = col;
  col &= 0xFF;
  if (Fill_Flag_Fog)
  {
    color = Fill_Logical_Palette[col];
  }
  ColorX = color;
  ColorY = color;
  Color = color;

  while (true)
  {
    // @@Trace_Still:
    S32 diffX = x1 - x0;
    if (diffX == 0)
    {
      // *** DeltaX nul ***
      // @@DX_Nul:
      if (x0 < ClipXMin || x0 > ClipXMax)
      {
        return;
      }

      S32 diffY = y1 - y0;
      if (diffY == 0)
      {
        // *** Plot ***
        // @@Plot:
        if (y0 < ClipYMin || y0 > ClipYMax)
        {
          return;
        }

        U8 *dst = (U8 *)Log + PTR_TabOffLine[y0] + x0;
        *dst = Color;
        return;
      }

      if (y0 < ClipYMin)
      {
        if (y1 < ClipYMin)
        {
          return;
        }

        y0 = ClipYMin;
      }

      // @@DX_Y1:
      if (y0 > ClipYMax)
      {
        if (y1 > ClipYMax)
        {
          return;
        }

        y0 = ClipYMax;
      }

      // @@DX_Y1b:
      if (y1 < ClipYMin)
      {
        y1 = ClipYMin;
      }

      // @@DX_Y2:
      if (y1 > ClipYMax)
      {
        y1 = ClipYMax;
      }

      // @@DX_Y2b:
      S32 newDiffY = y1 - y0;
      if (newDiffY < 0)
      {
        newDiffY = -newDiffY;
        y0 = y1;
      }

      // @@DX_Ok:
      U8 *dst = (U8 *)Log + PTR_TabOffLine[y0] + x1;
      newDiffY++;
      do
      {
        *dst = Color;
        dst += ScreenPitch;
        newDiffY--;
      } while (newDiffY > 0);

      return;
    }

    S32 diffY = y1 - y0;
    if (diffY == 0)
    {
      // *** DeltaY nul ***
      // @@DY_Nul:
      if (y0 < ClipYMin || y0 > ClipYMax)
      {
        return;
      }

      if (x0 < ClipXMin)
      {
        if (x1 < ClipXMin)
        {
          return;
        }

        x0 = ClipXMin;
      }

      // @@DY_X1:
      if (x0 > ClipXMax)
      {
        if (x1 > ClipXMax)
        {
          return;
        }

        x0 = ClipXMax;
      }

      // @@DY_X1b:
      if (x1 < ClipXMin)
      {
        x1 = ClipXMin;
      }

      // @@DY_X2:
      if (x1 > ClipXMax)
      {
        x1 = ClipXMax;
      }

      // @@DY_X2b:
      S32 newDiffX = x1 - x0;
      if (newDiffX < 0)
      {
        newDiffX = -newDiffX;
        x0 = x1;
      }

      // @@DY_Ok:
      U8 *dst = (U8 *)Log + PTR_TabOffLine[y0] + x0;
      newDiffX++;
      do
      {
        *dst = Color;
        dst++;
        newDiffX--;
      } while (newDiffX > 0);
    }

    if (x0 < ClipXMin)
    {
      if (x1 < ClipXMin)
      {
        return;
      }

      y0 = y0 - (x0 - ClipXMin) * diffY / diffX;
      x0 = ClipXMin;
    }
    // @@Clip_X0:
    else if (x0 > ClipXMax)
    {
      if (x1 > ClipXMax)
      {
        return;
      }

      y0 = y0 - (x0 - ClipXMax) * diffY / diffX;
      x0 = ClipXMax;
    }
    // @@Clip_X1:
    else if (x1 < ClipXMin)
    {
      y1 = y1 - (x1 - ClipXMin) * diffY / diffX;
      x1 = ClipXMin;
    }
    // @@Clip_X2:
    else if (x1 > ClipXMax)
    {
      y1 = y1 - (x1 - ClipXMax) * diffY / diffX;
      x1 = ClipXMax;
    }
    // @@Clip_X3:
    else if (y0 < ClipYMin)
    {
      if (y1 < ClipYMin)
      {
        return;
      }
      x0 = (ClipYMin - y0) * diffX / diffY + x0;
      y0 = ClipYMin;
    }
    // @@Clip_Y0:
    else if (y0 > ClipYMax)
    {
      if (y1 > ClipYMax)
      {
        return;
      }

      x0 = (ClipYMax - y0) * diffX / diffY + x0;
      y0 = ClipYMax;
    }
    // @@Clip_Y1:
    else if (y1 < ClipYMin)
    {
      x1 = x1 + (ClipYMin - y1) * diffX / diffY;
      y1 = ClipYMin;
    }
    // @@Clip_Y2:
    else if (y1 > ClipYMax)
    {
      x1 = x1 + (ClipYMax - y1) * diffX / diffY;
      y1 = ClipYMax;
    }
    // @@Clip_Y3:
    else
    {
      S32 incrY = ScreenPitch;
      S32 incrX = 1;
      S32 deltaX = x1 - x0;
      if (deltaX < 0)
      {
        deltaX = -deltaX;
        incrX = -incrX;
      }

      S32 deltaY = y1 - y0;
      if (deltaY < 0)
      {
        deltaY = -deltaY;
        incrY = -incrY;
      }

      U8 *dst = (U8 *)Log + PTR_TabOffLine[y0] + x0;

      if (deltaX > deltaY)
      {
        S32 halfX = deltaX / 2;
        S32 x = deltaX;

        do
        {
          // @@Do_X:
          *dst = ColorX;

          halfX += deltaY;
          if (halfX >= deltaX)
          {
            halfX -= deltaX;
            dst += incrY;
          }

          dst += incrX;
          x--;
        } while (x >= 0);

        return;
      }
      else
      {
        // @@Draw_Y:
        S32 halfY = deltaY / 2;
        S32 y = deltaY;

        do
        {
          // @@Do_Y:
          *dst = ColorY;

          halfY += deltaX;
          if (halfY >= deltaY)
          {
            halfY -= deltaY;
            dst += incrX;
          }

          dst += incrY;
          y--;
        } while (y >= 0);

        return;
      }
    }
  }
}

void Line(S32 x0, S32 y0, S32 x1, S32 y1, S32 col)
{
	Line_Entry(x0, y0, x1, y1, col);
}

void Line_A(S32 x0, S32 y0, S32 x1, S32 y1, S32 col, S32 z1, S32 z2)
{
	if (Fill_Flag_NZW)
	{
		Line_ZBuffer_NZW(x0, y0, x1, y1, col, z1, z2);
		return;
	}

	if (Fill_Flag_ZBuffer)
	{
		Line_ZBuffer(x0, y0, x1, y1, col, z1, z2);
		return;
	}

	Line_Entry(x0, y0, x1, y1, col);
}