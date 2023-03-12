#include <POLYGON/POLY.H>

#include <POLYGON/POLY_JMP.H>

#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <FILLER.H>

#include <limits.h>
#include <string.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

U32 Fill_Type = 0; // Polygon type number

U32 **Fill_Restart = NULL;
U32 **Fill_Filler = NULL;
U32 Fill_Patch = 0;
U8 Fill_Color = 0;

Struc_Point *Fill_FirstPoint = NULL;
Struc_Point *Fill_LastPoint = NULL;
Struc_Point *Fill_LeftPoint = NULL;
Struc_Point *Fill_RightPoint = NULL;

U32 Fill_LeftSlope = 0;      ///< Left slope
U32 Fill_RightSlope = 0;     ///< Right slope

U32 Fill_CurY = 0;           ///< Current Y
U32 Fill_CurXMin = 0;        ///< Current XMin
U32 Fill_CurXMax = 0;        ///< Current XMax
U32 Fill_CurOffLine = 0;     ///< Offset of the current video line

U8 Fill_ReadFlag = FALSE;        ///< Flag telling which point to read
U8 Fill_ClipFlag = FALSE;        ///< Flag telling us what to clip
U8 Fill_Flag_Fog = FALSE;     // True if FOG active
U8 Fill_Flag_ZBuffer = FALSE;	// True if ZBUFFER is active
U8 Fill_Flag_NZW = FALSE;     // True if NZW active

Fill_Jump_Fn *Fill_Saut_Normal = Fill_N_Table_Jumps; // Filler table

// -----------------------------------------------------------------------------
S32 INV64(S32 a) { return LONG_MIN / a; }

S32 Fill_Poly(S32 Type_Poly, S32 Color_Poly, S32 Nb_Points,
							Struc_Point *Ptr_Points) {
	/*
; ╔══════════════════════════════════════════════════════════════════════════╗
; ║ Fill_Poly : Polygon filler                                               ║
; ╟──────────────────────────────────────────────────────────────────────────╢
; ║ Call  : EAX = Polygon type (ie flat, gouraud, textured...)               ║
; ║         EBX = Polygon color (if needed by the type)                      ║
; ║         ECX = Nb of points in that polygon                               ║
; ║         ESI = Ptr on the point list of this polygon                      ║
; ║               (list of STRUC_POLY_POINT)                                 ║
; ║                                                                          ║
; ╟──────────────────────────────────────────────────────────────────────────╢
; ║ Return: EAX = 0 if error (polygon not filled)                            ║
; ║                                                                          ║
; ╟──────────────────────────────────────────────────────────────────────────╢
; ║ ALL REGISTERS ARE MODIFIED (EXCEPT EBP)                                  ║
; ║                                                                          ║
; ║ The value given to EAX is not check for validity, so be sure to fill it  ║
; ║ with a correct poly type number                                          ║
; ║                                                                          ║
; ╚══════════════════════════════════════════════════════════════════════════╝
			ALIGN	4
Fill_PolyFast		PROC
			push	ebp
			xor	ebp,ebp
			mov	[Fill_LeftSlope],ebp

			pushad
			mov	ebx,Offset TabOffLine
			call	SetScreenPitch
			popad

			mov	[Fill_Type],eax
			and	ebx,0FFh

	push ebx
	mov ebx,[Fill_Saut_Normal]
	lea eax,[ebx+eax*4]
	pop ebx
	jmp	[eax]

Fill_PolyFast		ENDP
	*/
	Fill_LeftSlope = 0;
	SetScreenPitch(TabOffLine);

	Fill_Type = Type_Poly;
	Color_Poly = Color_Poly & 0xFF;

	Fill_Jump_Fn filler = Fill_Saut_Normal[Type_Poly];
	return filler(Nb_Points, Ptr_Points);
}

void Switch_Fillers_C(U32 Bank) {
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  /*
#ifdef __MSC_VER
  __asm {
		pusha
		mov eax, Bank
		call Switch_Fillers_ASM
		popa
  }
#elif __GNUC__
  __asm__ __volatile__("pusha\n\t"
                       "movl %0, %%eax\n\t"
                       "call %1\n\t"
                       "popa\n\t"
                       :
                       : "m"(Bank), "m"(Switch_Fillers_ASM)
                       : "%eax");
#else
#error "Unsupported compiler!"
#endif
*/
}

#include <stdio.h>

PTR_U8 PtrCLUTGouraud = NULL;
PTR_U8 PtrCLUTFog = NULL;
PTR_U8 PtrTruePal = NULL;

PTR_U8 PtrMap = NULL;
PTR_U16 PtrZBuffer = NULL;
S32 RepMask = 0;

S32 Fill_Z_Fog_Near = 0;     ///< Fog Range
S32 Fill_Z_Fog_Far = 0;      ///< Fog Range
U32 Fill_ZBuffer_Factor = 0; ///< normalizing factor for Z Buffer
U32 Fill_Fog_Factor = 0;     ///< Fog Factor
U32 Fill_ScaledFogNear = 0;  ///< Scaled Near Fog value
U32 IsPolygonHidden = 0;     ///< normalizing factor for Z Buffer
U8 Fill_Logical_Palette[256]; ///< Logical Palette
U32 ScreenPitch = 0;         ///< Screen Pitch
PTR_U32 PTR_TabOffLine = NULL;  ///< Screen Pitch

Struc_Point List_Clip_XMin[NB_MAX_EDGES];
Struc_Point List_Clip_XMax[NB_MAX_EDGES];
Struc_Point List_Clip_YMin[NB_MAX_EDGES];
Struc_Point List_Clip_YMax[NB_MAX_EDGES];

S32 Clip_Num = 0;

void SetScreenPitch(U32 *New_TabOffLine)
{
  ScreenPitch = New_TabOffLine[1];
  PTR_TabOffLine = New_TabOffLine;
}

void SetCLUT(U32 defaultline)
{
  /*
; ╔══════════════════════════════════════════════════════════════════════════════════╗
; ║ SetCLUT: Initialize some CLUT pointers                                           ║
; ╟──────────────────────────────────────────────────────────────────────────────────╢
; ║ Appel: EAX = Line number in PtrCLUTFog that represents the palette CLUT.         ║
; ║                                                                                  ║
; ╚══════════════════════════════════════════════════════════════════════════════════╝

SetCLUT			PROC clut_ptr:	DWORD
			push	eax
			push	ebx
			push	ecx
			push	esi
			push	edi

			mov eax, clut_ptr

			shl	eax,8
			mov	ecx,[PtrCLUTFog]

			mov	ebx,eax
			add	eax,ecx

			mov	ecx,[PtrTruePal]
			and	ebx,0F000h

			cmp	eax,ecx
			je	@@End

			mov	[PtrTruePal],eax
			mov	ecx,[PtrCLUTFog]

			add	ebx,ecx
			mov	esi,eax

			mov	[PtrCLUTGouraud],ebx
			mov	edi,Offset Fill_Logical_Palette

			mov	ecx,256/8
@@Copy:
			mov	eax,[esi]
			mov	ebx,[esi+4]
			mov	[edi],eax
			mov	[edi+4],ebx
			add	esi,8
			add	edi,8
			dec	ecx
			jne	@@Copy
@@End:
			pop	edi
			pop	esi
			pop	ecx
			pop	ebx
			pop	eax
			ret
SetCLUT			ENDP
  */
  U8 *nextPtrTruePal = PtrCLUTFog + (defaultline << 8);

  // compare and jump to end if equal
  if (PtrTruePal == nextPtrTruePal)
  {
    return;
  }

  // update PtrTruePal and calculate new value for PtrCLUTGouraud
  PtrTruePal = nextPtrTruePal;
  PtrCLUTGouraud = PtrCLUTFog + ((defaultline << 8) & 0xF000);
  U8 *dst = (U8 *)Fill_Logical_Palette;
  U8 *src = (U8 *)PtrTruePal;

  for (int i = 0; i < 256 / 8; i++)
  {
    memcpy(dst, src, 8);
    dst += 8;
    src += 8;
  }
}

void SetFog(S32 z_near, S32 z_far)
{
  /*
; ╔══════════════════════════════════════════════════════════════════════════════════╗
; ║ Set_Fog: Initialize the fog parameters                                           ║
; ╟──────────────────────────────────────────────────────────────────────────────────╢
; ║ Appel: EAX = Z near (at which the fog starts)                                    ║
; ║        EBX = Z far (at which everything is under fog)                            ║
; ║                                                                                  ║
; ╚══════════════════════════════════════════════════════════════════════════════════╝

SetFog			PROC \
			uses eax ebx ecx edx edi esi ebp\
			znear: DWORD, zfar:DWORD
			mov eax, znear
			mov ebx, zfar

			pushad

			test	ebx,ebx
			jne	@@Ok1
			mov	ebx,1
@@Ok1:

			mov	[Fill_Z_Fog_Near],eax
			mov	[Fill_Z_Fog_Far],ebx

			xor	eax,eax
			mov	edx,1
			div	ebx
			mov	[Fill_ZBuffer_Factor],eax	; Pt_ZO = (Z*Fill_ZBuffer_Factor) SHR 16


			imul	eax,[Fill_Z_Fog_Near]
			shr	eax,16
			mov	[Fill_ScaledFogNear],eax


			mov	ebx,65535
			sub	ebx,eax
			test	ebx,ebx
			jne	@@Ok2
			mov	ebx,1
@@Ok2:
			mov	eax,256*65536
			xor	edx,edx
			div	ebx
			mov	[Fill_Fog_Factor],eax		; Fog value= 0 if Zbuf<FogNear
								;	   = (Zbuf-Near)*FogFactor SHR 20 if ZBuf>FogNear
			popad
			ret
SetFog			ENDP
  */
    if (z_far == 0)
        z_far = 1;

    Fill_Z_Fog_Near = z_near;
    Fill_Z_Fog_Far = z_far;

    Fill_ZBuffer_Factor = 0x100000000 / z_far;
    Fill_ScaledFogNear = (Fill_Z_Fog_Near * Fill_ZBuffer_Factor) >> 16;

    U32 temp = 65535 - Fill_ScaledFogNear;
    if (temp == 0)
        temp = 1;

    Fill_Fog_Factor = 256 * 65536 / temp;
}

void Triangle_ReadNextEdge(void)
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
}

// ESI = PtA; EBX = Pt_B with PtA: point to clip; and PtB: valid point
// y=yA+[(XMin-xa)*(yB-yA)]/(xB-xA)
void Generic_ClipXMin(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA)
{
  S32 xDiff = pointB->Pt_XE - pointA->Pt_XE;

  Clip_Num = ClipXMin - pointA->Pt_XE;
	dst->Pt_XE = ClipXMin;
	dst->Pt_YE = pointA->Pt_YE + (pointB->Pt_YE - pointA->Pt_YE * Clip_Num) / xDiff;
  if (Fill_ClipFlag & CLIP_LIGHT)
	{
		dst->Pt_Light = pointA->Pt_Light + (pointB->Pt_Light - pointA->Pt_Light) * Clip_Num / xDiff;
	}

  if (Fill_ClipFlag & CLIP_TEXTURE)
  {
		dst->Pt_MapU = pointA->Pt_MapU + (pointB->Pt_MapU - pointA->Pt_MapU) * Clip_Num / xDiff;
		dst->Pt_MapV = pointA->Pt_MapV + (pointB->Pt_MapV - pointA->Pt_MapV) * Clip_Num / xDiff;
  }

  if (Fill_ClipFlag & CLIP_TEXTUREZ)
  {
		// W
		dst->Pt_W = pointA->Pt_W + (pointB->Pt_W - pointA->Pt_W) * Clip_Num / xDiff;

		// MapU
		S32 uwA = (pointA->Pt_MapU * pointA->Pt_W / 256);
		S32 uwB = (pointB->Pt_MapU * pointB->Pt_W / 256);
		S32 U_tmp = (uwB - uwA) * Clip_Num / xDiff + uwA;
		dst->Pt_MapU = U_tmp * 256 / dst->Pt_W;

		// MapV
		S32 vwA = (pointA->Pt_MapV * pointA->Pt_W / 256);
		S32 vwB = (pointB->Pt_MapV * pointB->Pt_W / 256);
		S32 V_tmp = (vwB - vwA) * Clip_Num / xDiff + vwA;
		dst->Pt_MapV = V_tmp * 256 / dst->Pt_W;

    if (Fill_ClipFlag & CLIP_ZBUFFER)
    {
			// ** ZBuf **
			// If persp correct and Zbuf,
			// then we clip the zbuf with a
			// perspective correction
			S32 zA = pointA->Pt_ZO * pointA->Pt_W / 256;
			S32 zB = pointB->Pt_ZO * pointB->Pt_W / 256;
			S32 ZO_tmp = (zB - zA) * Clip_Num / xDiff + zA;
			dst->Pt_ZO = ZO_tmp * 256 / dst->Pt_W;
    }
  }
  else
  {
    if ( Fill_ClipFlag & CLIP_ZBUFFER )
    {
			dst->Pt_ZO = pointA->Pt_ZO + (pointB->Pt_ZO - pointA->Pt_ZO) * Clip_Num / xDiff;
    }
  }
}

// Clip types
#define CLIP_XMIN	0
#define CLIP_XMAX	1
#define CLIP_YMIN	2
#define CLIP_YMAX	3

void Clipper_Jmp(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA, U8 type)
{
	switch (type)
	{
		case CLIP_XMIN:
			Generic_ClipXMin(pointB, dst, pointA);
			break;
		// case CLIP_XMAX:
		// 	Generic_ClipXMax(pointB, dst, pointA);
		// 	break;
		// case CLIP_YMIN:
		// 	Generic_ClipYMin(pointB, dst, pointA);
		// 	break;
		// case CLIP_YMAX:
		// 	Generic_ClipYMax(pointB, dst, pointA);
		// 	break;
	}
}

/*
 ╔═══════════════════════════════════════════════════════════════════════════╗
 ║ Clipping : Generic macro                                                  ║
 ╟───────────────────────────────────────────────────────────────────────────╢
 ║ Call : [Fill_FirstPoint] = Ptr on the beginning of the list               ║
 ║        [Fill_LastPoint]  = Ptr on the end of the list                     ║
 ╟───────────────────────────────────────────────────────────────────────────╢
 ║ Return: ESI = Ptr on the new points-list                                  ║
 ║         ECX = Nb of points in that new list                               ║
 ║                                                                           ║
 ╚═══════════════════════════════════════════════════════════════════════════╝
*/
S32 Fill_Clip(S32 Nb_Points, Struc_Point *Ptr_Points, U8 type)
{
	Struc_Point *prevPoint = NULL; // Previous point
  Struc_Point *currentPoint = Fill_FirstPoint; // Current point

  Struc_Point *clipPoints = NULL;

  S32 newNbPoints = 0;
  currentPoint--;

	if (type == CLIP_XMIN)
	{
		clipPoints = List_Clip_XMin;
	}
	else if (type == CLIP_XMAX)
	{
		clipPoints = List_Clip_XMax;
	}
	else if (type == CLIP_YMIN)
	{
		clipPoints = List_Clip_YMin;
	}
	else if (type == CLIP_YMAX)
	{
		clipPoints = List_Clip_YMax;
	}

  U8 isPointInvisible = 1;

  if ((type == CLIP_XMIN && Fill_LastPoint->Pt_XE >= ClipXMin)
		|| (type == CLIP_XMAX && Fill_LastPoint->Pt_XE <= ClipXMax)
		|| (type == CLIP_YMIN && Fill_LastPoint->Pt_YE >= ClipYMin)
		|| (type == CLIP_YMAX && Fill_LastPoint->Pt_YE <= ClipYMax))
	{
    isPointInvisible = 0;
	}

  while (currentPoint != Fill_LastPoint)
  {
    currentPoint++;
    if ((type == CLIP_XMIN && currentPoint->Pt_XE >= ClipXMin)
			|| (type == CLIP_XMAX && currentPoint->Pt_XE <= ClipXMax)
			|| (type == CLIP_YMIN && currentPoint->Pt_YE >= ClipYMin)
			|| (type == CLIP_YMAX && currentPoint->Pt_YE <= ClipYMax))
    {
      if (isPointInvisible)
      {
        Clipper_Jmp(prevPoint, clipPoints, currentPoint, type);
        clipPoints++;
        newNbPoints++;
      }

			memcpy(clipPoints, currentPoint, sizeof(Struc_Point));

      ++newNbPoints;
      clipPoints++;
      isPointInvisible = 0;
    }
    else if (!isPointInvisible)
    {
			// Current point is off-screen
			Clipper_Jmp(prevPoint, clipPoints, currentPoint, type);
      clipPoints++; // A point was created: registers it!
      newNbPoints++;
      isPointInvisible = 1; // Last point not visible
    }

		prevPoint = currentPoint;
  }

	Struc_Point *nextPtrPoints = NULL;
	if (type == CLIP_XMIN)
	{
		nextPtrPoints = List_Clip_XMin;
	}
	else if (type == CLIP_XMAX)
	{
		nextPtrPoints = List_Clip_XMax;
	}
	else if (type == CLIP_YMIN)
	{
		nextPtrPoints = List_Clip_YMin;
	}
	else if (type == CLIP_YMAX)
	{
		nextPtrPoints = List_Clip_YMax;
	}

  return Fill_PolyClip(newNbPoints, nextPtrPoints);
}

S32 Fill_ClipXMin(S32 Nb_Points, Struc_Point *Ptr_Points)
{
	return Fill_Clip(Nb_Points, Ptr_Points, CLIP_XMIN);
}

S32 Fill_ClipXMax(S32 Nb_Points, Struc_Point *Ptr_Points)
{
	return Fill_Clip(Nb_Points, Ptr_Points, CLIP_XMAX);
}

S32 Fill_ClipYMin(S32 Nb_Points, Struc_Point *Ptr_Points)
{
	return Fill_Clip(Nb_Points, Ptr_Points, CLIP_YMIN);
}

S32 Fill_ClipYMax(S32 Nb_Points, Struc_Point *Ptr_Points)
{
	return Fill_Clip(Nb_Points, Ptr_Points, CLIP_YMAX);
}

S32 Fill_PolyClip(S32 Nb_Points, Struc_Point *Ptr_Points)
{
	/*
; ╔══════════════════════════════════════════════════════════════════════════╗
; ║ Fill_PolyClip: Display a n-angle (provided that the poly is convex)      ║
; ╟──────────────────────────────────────────────────────────────────────────╢
; ║ Appel : ECX = Nb points                                                  ║
; ║         ESI = PTR on the list of points                                  ║
; ║                                                                          ║
; ╟──────────────────────────────────────────────────────────────────────────╢
; ║ Retour: EAX = 0 if error (non-visible poly)                              ║
; ║                                                                          ║
; ╟──────────────────────────────────────────────────────────────────────────╢
; ║ ALL REGISTERS MODIFIED                                                   ║
; ║                                                                          ║
; ╚══════════════════════════════════════════════════════════════════════════╝
			ALIGN	4
Fill_PolyClip		PROC
			mov	ebx,Offset @@Fill_Restart
			mov	[Fill_Restart],ebx

@@Fill_Restart:
			; *** Get the poly-box so as to test the clipping ***

			lea	eax,[ecx-1]		; Loop counter in EAX
			push	ecx

			mov	ecx,dword ptr [esi].STRUC_POINT.Pt_XE
			push	esi

			mov	edx,ecx
			mov	[Fill_FirstPoint],esi

			sar	edx,16
			add	esi,Size STRUC_POINT

			shl	ecx,16
			mov	ebp,edx

			sar	ecx,16

			mov	edi,ecx
							; (ECX, EDX, EDI, EBP)
							; (XMin, YMin, XMax, YMax)
@@Bounding_Box:
			push	eax			; What follows has a bad pairing... How can we pair when we have NOTHING to do ?
			mov	eax,dword ptr [esi].STRUC_POINT.Pt_XE
			mov	ebx,eax

			shl	eax,16
			sar	ebx,16
			sar	eax,16

			cmp	edx,ebx
			jle	@@Not_YMin

			mov	edx,ebx
@@Not_YMin:
			cmp	ebp,ebx
			jge	@@Not_YMax
			mov	ebp,ebx
@@Not_YMax:
			cmp	ecx,eax
			jle	@@Not_XMin
			mov	ecx,eax
@@Not_XMin:
			cmp	edi,eax
			jge	@@Not_XMax
			mov	edi,eax
@@Not_XMax:
			pop	eax
			add	esi,Size STRUC_POINT

			dec	eax
			jne	@@Bounding_Box

			cmp	ebp,edx
			jle	@@End			; Skip poly if DeltaY<=0

				; *** Is the polygon at least visible ? ***

			mov	eax,[ClipXMax]
			mov	ebx,[ClipXMin]

			inc	eax			; Bcoz we don't draw the last pixel on a scanline

			cmp	ecx,eax
			jg	@@End

			cmp	edi,ebx
			jl	@@End

			mov	eax,[ClipYMax]
			mov	ebx,[ClipYMin]

			cmp	edx,eax
			jg	@@End

			cmp	ebp,ebx
			jl	@@End


				; *** Should we clip the polygon ? ***

			mov	eax,[ClipXMin]
			sub	esi,Size STRUC_POINT

			mov	ebx,[ClipXMax]
			mov	[Fill_LastPoint],esi

			cmp	ecx,eax
			pop	esi			; Restore List pointer

			pop	ecx			; Restore Nb Pts
			jl	Fill_ClipXMin

			inc	ebx			; Bcoz we don't draw the last pixel on a scanline

			cmp	edi,ebx
			jg	Fill_ClipXMax

			mov	eax,[ClipYMin]
			mov	ebx,[ClipYMax]

			cmp	edx,eax
			jl	Fill_ClipYMin

			cmp	ebp,ebx
			jg	Fill_ClipYMax


Fill_PolyNoClip::
			cmp	ecx,3			; Is it a triangle ?
			je	Draw_Triangle

			push	ecx
			push	Offset @@Ret_Triangle

			push	esi			; Will be poped by Draw_Triangle in EBP
			mov	ecx,3			; (BCOZ EBP is saved in Fill_Poly and restored in Draw_Triangle)

			jmp	Draw_Triangle
@@Ret_Triangle:
			pop	ecx
			lea	esi,[ebp+Size STRUC_POINT]; Next ListPoint pointer

			mov	eax,[ebp]		; Copy the first point in the new location (the following one)
			mov	ebx,[ebp+4]

			mov	[esi],eax
			mov	[esi+4],ebx

			mov	eax,[ebp+8]
			mov	ebx,[ebp+12]

			mov	[esi+8],eax
			mov	[esi+12],ebx

			dec	ecx
			jmp	Fill_PolyNoClip
@@End:
			pop	esi
			pop	ecx
			pop	ebp
			mov	eax,[Fill_LeftSlope]
			ret
Fill_PolyClip		ENDP
	*/
	// *** Get the poly-box so as to test the clipping ***
	Fill_FirstPoint = Ptr_Points;
	U32 counter = Nb_Points - 1;
	Struc_Point *points = Ptr_Points;

	S16 XMin = 0;
	S16 XMax = 0;
	S16 YMin = 0;
	S16 YMax = 0;

	do
	{
		// @@Bounding_Box:
		XMin = points->Pt_XE;
		XMax = XMin;
		YMin = points->Pt_YE;
		YMax = YMin;

		points++;

		// What follows has a bad pairing... How can we pair when we have NOTHING to do ?
		if (YMin > points->Pt_YE)
		{
			YMin = points->Pt_YE;
		}

		if (YMax < points->Pt_YE)
		{
			YMax = points->Pt_YE;
		}

		if (XMin > points->Pt_XE)
		{
			XMin = points->Pt_XE;
		}

		if (XMax < points->Pt_XE)
		{
			XMax = points->Pt_XE;
		}

		points++;
		counter--;
	} while (counter > 0);

	if (YMax <= YMin)
	{
		return Fill_LeftSlope; // Skip poly if DeltaY<=0
	}

	// *** Is the polygon at least visible ? ***

	// +1 because we don't draw the last pixel on a scanline
	if (XMin > ClipXMax + 1 || XMax < ClipXMin
			|| YMin > ClipYMax || YMax < ClipYMin)
	{
		return Fill_LeftSlope;
	}

	// *** Should we clip the polygon ? ***
	points--;
	Fill_LastPoint = points;

	if (XMin < ClipXMin)
	{
		return Fill_ClipXMin(Nb_Points, Ptr_Points);
	}

	// +1 because we don't draw the last pixel on a scanline
	if (XMax > ClipXMax + 1)
	{
		return Fill_ClipXMax(Nb_Points, Ptr_Points);
	}

	if (YMin < ClipYMin)
	{
		return Fill_ClipYMin(Nb_Points, Ptr_Points);
	}

	if (YMax > ClipYMax)
	{
		return Fill_ClipYMax(Nb_Points, Ptr_Points);
	}

	while (true)
	{
		// Fill_PolyNoClip::
		if (Nb_Points == 3)
		{
			// TODO: return Draw_Triangle(Nb_Points, Ptr_Points);
		}

		// TODO: No idea why this is here...
		/*
			push	ecx
			push	Offset @@Ret_Triangle

			push	esi			; Will be poped by Draw_Triangle in EBP
			mov	ecx,3			; (BCOZ EBP is saved in Fill_Poly and restored in Draw_Triangle)

			jmp	Draw_Triangle
		*/

		// @@Ret_Triangle:
		// TODO: copy points
		Struc_Point *next = points + 1;

		// Copy the first point in the new location (the following one)
		memcpy(next, points, sizeof(Struc_Point));
	}

	// TODO: return Fill_PolyNoCLip(); ??

	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return Fill_LeftSlope;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
