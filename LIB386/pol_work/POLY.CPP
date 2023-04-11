#include <POLYGON/POLY.H>

#include <POLYGON/POLY_JMP.H>

#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <FILLER.H>

#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

typedef S32 (*Jmp_XSlopeFunc)(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC);
typedef S32 (*Jmp_LeftSlopeFunc)(S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB);

extern Jmp_XSlopeFunc Jmp_XSlopeFPU[];
extern Jmp_LeftSlopeFunc Jmp_LeftSlopeFPU[];

U32 Fill_Type = 0; // Polygon type number

U32 **Fill_Restart = NULL;
Fill_Filler_Func Fill_Filler = NULL;
U32 Fill_Patch = 0;
U32 Fill_Color = 0;

Struc_Point *Fill_FirstPoint = NULL;
Struc_Point *Fill_LastPoint = NULL;
Struc_Point *Fill_LeftPoint = NULL;
Struc_Point *Fill_RightPoint = NULL;

U32 Fill_LeftSlope = 0;      ///< Left slope
U32 Fill_RightSlope = 0;     ///< Right slope

U32 Fill_CurY = 0;           ///< Current Y
U32 Fill_CurXMin = 0;        ///< Current XMin
U32 Fill_CurXMax = 0;        ///< Current XMax
PTR_U8 Fill_CurOffLine = NULL;     ///< Offset of the current video line

U32 Fill_CurGouraudMin = 0; // Left edge: gouraud value
U32 Fill_Gouraud_LeftSlope = 0; // Left slope for Gouraud shading
U32 Fill_Gouraud_XSlope = 0; // X Slope for Gouraud shading

U32 Fill_CurMapUMin = 0; // Left edge: MapU value
U32 Fill_MapU_LeftSlope = 0; // Left slope for MapU
U32 Fill_MapU_XSlope = 0;	// X Slope for MapU

U32 Fill_CurMapVMin = 0; // Left edge: MapV value
U32 Fill_MapV_LeftSlope = 0; // Left slope for MapV
U32 Fill_MapV_XSlope = 0;	// X Slope for MapV

U8 Fill_ReadFlag = FALSE;        ///< Flag telling which point to read
U8 Fill_ClipFlag = FALSE;        ///< Flag telling us what to clip
U8 Fill_Flag_Fog = FALSE;     // True if FOG active
U8 Fill_Flag_ZBuffer = FALSE;	// True if ZBUFFER is active
U8 Fill_Flag_NZW = FALSE;     // True if NZW active

Fill_Jump_Fn *Fill_Saut_Normal = Fill_N_Table_Jumps; // Filler table

S32 YB_YA = 0;
S32 YC_YA = 0;

// -----------------------------------------------------------------------------
S32 INV64(S32 a) { return LONG_MIN / a; }

S32 Read_Next_Right(Struc_Point *PtA, Struc_Point *PtB);

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
	return filler(Nb_Points, Ptr_Points, Color_Poly);
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

double Denom = 0;
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

void EnterClip(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA, S32 diff)
{
	/*
EnterClip::
			mov	eax,[Fill_ClipFlag]
			test	eax,CLIP_LIGHT
			je	@@No_ClipLight

			mov	eax,dword ptr [ebx].STRUC_POINT.Pt_Light
			mov	ecx,dword ptr [esi].STRUC_POINT.Pt_Light
			and	eax,0FFFFh
			and	ecx,0FFFFh
			sub	eax,ecx
			imul	[Clip_Num]
			idiv	ebp
			add	eax,ecx
			mov	[edi].STRUC_POINT.Pt_Light,ax
@@No_ClipLight:

			mov	eax,[Fill_ClipFlag]
			test	eax,CLIP_TEXTURE
			je	@@No_ClipTexture

					; ** MapU **
			mov	eax,dword ptr [ebx].STRUC_POINT.Pt_MapU
			mov	ecx,dword ptr [esi].STRUC_POINT.Pt_MapU
			and	eax,0FFFFh
			and	ecx,0FFFFh
			sub	eax,ecx
			imul	[Clip_Num]
			idiv	ebp
			add	eax,ecx
			mov	[edi].STRUC_POINT.Pt_MapU,ax

					; ** MapV **
			mov	eax,dword ptr [ebx].STRUC_POINT.Pt_MapU
			mov	ecx,dword ptr [esi].STRUC_POINT.Pt_MapU
			shr	eax,16
			shr	ecx,16
			sub	eax,ecx
			imul	[Clip_Num]
			idiv	ebp
			add	eax,ecx
			mov	[edi].STRUC_POINT.Pt_MapV,ax
@@No_ClipTexture:

			mov	eax,[Fill_ClipFlag]
			test	eax,CLIP_TEXTUREZ
			je	@@No_ClipTextureZ

					; ** W **
			mov	eax,dword ptr [ebx].STRUC_POINT.Pt_W
			mov	ecx,dword ptr [esi].STRUC_POINT.Pt_W
			sub	eax,ecx
			imul	[Clip_Num]
			idiv	ebp
			add	eax,ecx
			mov	[edi].STRUC_POINT.Pt_W,eax

					; ** MapU **
			mov	eax,dword ptr [esi].STRUC_POINT.Pt_MapU
			and	eax,0FFFFh
			imul	[esi].STRUC_POINT.Pt_W
			shr	eax,8
			shl	edx,24
			or	eax,edx
			mov	ecx,eax

			mov	eax,dword ptr [ebx].STRUC_POINT.Pt_MapU
			and	eax,0FFFFh
			imul	[ebx].STRUC_POINT.Pt_W
			shr	eax,8
			shl	edx,24
			or	eax,edx

			sub	eax,ecx
			imul	[Clip_Num]
			idiv	ebp
			add	eax,ecx

			mov	edx,eax
			shl	eax,8
			sar	edx,24
			idiv	[edi].STRUC_POINT.Pt_W
			mov	[edi].STRUC_POINT.Pt_MapU,ax

					; ** MapV **
			mov	eax,dword ptr [esi].STRUC_POINT.Pt_MapU
			shr	eax,16
			imul	[esi].STRUC_POINT.Pt_W
			shr	eax,8
			shl	edx,24
			or	eax,edx
			mov	ecx,eax

			mov	eax,dword ptr [ebx].STRUC_POINT.Pt_MapU
			shr	eax,16
			imul	[ebx].STRUC_POINT.Pt_W
			shr	eax,8
			shl	edx,24
			or	eax,edx

			sub	eax,ecx
			imul	[Clip_Num]
			idiv	ebp
			add	eax,ecx

			mov	edx,eax
			shl	eax,8
			sar	edx,24
			idiv	[edi].STRUC_POINT.Pt_W
			mov	[edi].STRUC_POINT.Pt_MapV,ax

			mov	eax,[Fill_ClipFlag]
			test	eax,CLIP_ZBUFFER
			je	@@No_ClipZBuffer

					; ** ZBuf **
							; If persp correct and Zbuf,
							; then we clip the zbuf with a
							; perspective correction
			mov	eax,dword ptr [esi].STRUC_POINT.Pt_Light
			shr	eax,16
			imul	[esi].STRUC_POINT.Pt_W
			shr	eax,8
			shl	edx,24
			or	eax,edx
			mov	ecx,eax

			mov	eax,dword ptr [ebx].STRUC_POINT.Pt_Light
			shr	eax,16
			imul	[ebx].STRUC_POINT.Pt_W
			shr	eax,8
			shl	edx,24
			or	eax,edx

			sub	eax,ecx
			imul	[Clip_Num]
			idiv	ebp
			add	eax,ecx

			mov	edx,eax
			shl	eax,8
			sar	edx,24
			idiv	[edi].STRUC_POINT.Pt_W
			mov	[edi].STRUC_POINT.Pt_ZO,ax

			jmp	@@No_ClipZBuffer

@@No_ClipTextureZ:
			mov	eax,[Fill_ClipFlag]
			test	eax,CLIP_ZBUFFER
			je	@@No_ClipZBuffer

			mov	eax,dword ptr [ebx].STRUC_POINT.Pt_Light
			mov	ecx,dword ptr [esi].STRUC_POINT.Pt_Light

			shr	eax,16

			shr	ecx,16

			sub	eax,ecx
			imul	[Clip_Num]
			idiv	ebp
			add	eax,ecx
			mov	[edi].STRUC_POINT.Pt_ZO,ax
@@No_ClipZBuffer:

			pop	ebp
			pop	ecx

			ret
	*/
  if (Fill_ClipFlag & CLIP_LIGHT)
	{
		dst->Pt_Light = pointA->Pt_Light + (pointB->Pt_Light - pointA->Pt_Light) * Clip_Num / diff;
	}

  if (Fill_ClipFlag & CLIP_TEXTURE)
  {
		dst->Pt_MapU = pointA->Pt_MapU + (pointB->Pt_MapU - pointA->Pt_MapU) * Clip_Num / diff;
		dst->Pt_MapV = pointA->Pt_MapV + (pointB->Pt_MapV - pointA->Pt_MapV) * Clip_Num / diff;
  }

  if (Fill_ClipFlag & CLIP_TEXTUREZ)
  {
		// W
		dst->Pt_W = pointA->Pt_W + (pointB->Pt_W - pointA->Pt_W) * Clip_Num / diff;

		// MapU
		S32 uwA = (pointA->Pt_MapU * pointA->Pt_W / 256);
		S32 uwB = (pointB->Pt_MapU * pointB->Pt_W / 256);
		S32 U_tmp = (uwB - uwA) * Clip_Num / diff + uwA;
		dst->Pt_MapU = U_tmp * 256 / dst->Pt_W;

		// MapV
		S32 vwA = (pointA->Pt_MapV * pointA->Pt_W / 256);
		S32 vwB = (pointB->Pt_MapV * pointB->Pt_W / 256);
		S32 V_tmp = (vwB - vwA) * Clip_Num / diff + vwA;
		dst->Pt_MapV = V_tmp * 256 / dst->Pt_W;

    if (Fill_ClipFlag & CLIP_ZBUFFER)
    {
			// ** ZBuf **
			// If persp correct and Zbuf,
			// then we clip the zbuf with a
			// perspective correction
			S32 zA = pointA->Pt_ZO * pointA->Pt_W / 256;
			S32 zB = pointB->Pt_ZO * pointB->Pt_W / 256;
			S32 ZO_tmp = (zB - zA) * Clip_Num / diff + zA;
			dst->Pt_ZO = ZO_tmp * 256 / dst->Pt_W;
    }
  }
  else
  {
    if ( Fill_ClipFlag & CLIP_ZBUFFER )
    {
			dst->Pt_ZO = pointA->Pt_ZO + (pointB->Pt_ZO - pointA->Pt_ZO) * Clip_Num / diff;
    }
  }
}

// ESI = PtA; EBX = Pt_B with PtA: point to clip; and PtB: valid point
// y=yA+[(XMin-xa)*(yB-yA)]/(xB-xA)
void Generic_ClipXMin(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA)
{
	/*
Generic_ClipXMin	PROC
			push	ecx
			push	ebp

			mov	edx,dword ptr [esi].STRUC_POINT.Pt_XE	; Current point
			mov	eax,dword ptr [ebx].STRUC_POINT.Pt_XE	; Last point

			mov	ecx,edx
			mov	ebp,eax

			shl	ecx,16
			and	edx,0FFFF0000h			; EDX = Ya shl 16

			shl	ebp,16
			and	eax,0FFFF0000h			; EAX = Yb shl 16

			sar	ecx,16				; ECX = Xa
			sub	eax,edx				; EAX = (Yb-Ya) shl 16

			sar	ebp,16				; EBP = Xb
			push	edx

			sub	ebp,ecx				; EBP = (Xb-Xa)
			mov	edx,[ClipXMin]

			sub	edx,ecx				; EDX = (XMin-Xa)

			mov	[Clip_Num],edx

			imul	edx
			idiv	ebp

			pop	edx
			mov	ecx,[ClipXMin]

			add	eax,edx
			and	ecx,0FFFFh

			and	eax,0FFFF0000h

			or	ecx,eax

			mov	dword ptr [edi].STRUC_POINT.Pt_XE,ecx

			jmp EnterClip
Generic_ClipXMin	ENDP
	*/
  S32 xDiff = pointB->Pt_XE - pointA->Pt_XE;

  Clip_Num = ClipXMin - pointA->Pt_XE;
	dst->Pt_XE = ClipXMin;
	dst->Pt_YE = pointA->Pt_YE + (pointB->Pt_YE - pointA->Pt_YE) * Clip_Num / xDiff;

	EnterClip(pointB, dst, pointA, xDiff);
}

// ESI = PtA; EBX = Pt_B
// y=yA+[(XMax-xa)*(yB-yA)]/(xB-xA)
void Generic_ClipXMax(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA)
{
	/*
Generic_ClipXMax	PROC
			push	ecx
			push	ebp

			mov	edx,dword ptr [esi].STRUC_POINT.Pt_XE	; Current point
			mov	eax,dword ptr [ebx].STRUC_POINT.Pt_XE	; Last point

			mov	ecx,edx
			mov	ebp,eax

			shl	ecx,16
			and	edx,0FFFF0000h			; EDX = Ya shl 16

			shl	ebp,16
			and	eax,0FFFF0000h			; EAX = Yb shl 16

			sar	ecx,16				; ECX = Xa
			sub	eax,edx				; EAX = (Yb-Ya) shl 16

			sar	ebp,16				; EBP = Xb
			push	edx

			sub	ebp,ecx				; EBP = (Xb-Xa)
			mov	edx,[ClipXMax]

			inc	edx				; XMax++ bcoz we don't draw the last pixel on a scanline

			sub	edx,ecx				; EDX = (XMin-Xa)

			mov	[Clip_Num],edx

			imul	edx
			idiv	ebp

			pop	edx
			mov	ecx,[ClipXMax]

			add	eax,edx
			and	ecx,0FFFFh

			and	eax,0FFFF0000h
			inc	ecx				; XMax++ bcoz we don't draw the last pixel on a scanline

			or	eax,ecx

			mov	dword ptr [edi].STRUC_POINT.Pt_XE,eax

			jmp	EnterClip
Generic_ClipXMax	ENDP
	*/

	S32 xDiff = pointB->Pt_XE - pointA->Pt_XE;

	// ClipXMax + 1 bcoz we don't draw the last pixel on a scanline
	Clip_Num = ClipXMax + 1 - pointA->Pt_XE;

	dst->Pt_XE = ClipXMax + 1;
	dst->Pt_YE = pointA->Pt_YE + (pointB->Pt_YE - pointA->Pt_YE) * Clip_Num / xDiff;

	EnterClip(pointB, dst, pointA, xDiff);
}

// ESI = PtA; EBX = Pt_B
// x=xa+[(YMin-ya)*(xB-xA)]/(yB-ya)
void Generic_ClipYMin(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA)
{
	/*
Generic_ClipYMin	PROC
			mov	edx,dword ptr [esi].STRUC_POINT.Pt_XE	; A
			mov	eax,dword ptr [ebx].STRUC_POINT.Pt_XE	; B

			push	ecx
			mov	ecx,edx

			shl	edx,16
			push	ebp

			sar	ecx,16				; ECX = Ya
			mov	ebp,eax

			shl	eax,16

			sar	ebp,16				; EBP = Yb
			sub	eax,edx				; EAX = (Xb-Xa) shl 16

			sar	edx,16				; EDX = Xa
			sub	ebp,ecx				; EBP = (Yb-Ya)

			push	edx
			mov	edx,[ClipYMin]

			sub	edx,ecx				; EDX = (YMin-Ya)

			mov	[Clip_Num],edx

			imul	edx				; EAX = (Ymin-Ya)(Xb-Xa)
			idiv	ebp

			sar	eax,16
			pop	edx

			add	eax,edx
			mov	ecx,[ClipYMin]

			shl	ecx,16
			and	eax,0FFFFh

			or	eax,ecx

			mov	dword ptr [edi].STRUC_POINT.Pt_XE,eax

			jmp	EnterClip
Generic_ClipYMin	ENDP
*/

	S32 yDiff = pointB->Pt_YE - pointA->Pt_YE;

	Clip_Num = ClipYMin - pointA->Pt_YE;
	dst->Pt_XE = pointA->Pt_XE + (pointB->Pt_XE - pointA->Pt_XE) * Clip_Num / yDiff;
	dst->Pt_YE = ClipYMin;

	EnterClip(pointB, dst, pointA, yDiff);
}

// ESI = PtA; EBX = Pt_B
// x=xa+[(YMax-ya)*(xB-xA)]/(yB-ya)
void Generic_ClipYMax(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA)
{
	/*
Generic_ClipYMax	PROC
			mov	edx,dword ptr [esi].STRUC_POINT.Pt_XE	; A
			mov	eax,dword ptr [ebx].STRUC_POINT.Pt_XE	; B

			push	ecx
			mov	ecx,edx

			shl	edx,16
			push	ebp

			sar	ecx,16				; ECX = Ya
			mov	ebp,eax

			shl	eax,16

			sar	ebp,16				; EBP = Yb
			sub	eax,edx				; EAX = (Xb-Xa) shl 16

			sar	edx,16				; EDX = Xa
			sub	ebp,ecx				; EBP = (Yb-Ya)

			push	edx
			mov	edx,[ClipYMax]

			sub	edx,ecx				; EDX = (YMin-Ya)

			mov	[Clip_Num],edx

			imul	edx				; EAX = (Ymin-Ya)(Xb-Xa)
			idiv	ebp

			sar	eax,16
			pop	edx

			add	eax,edx
			mov	ecx,[ClipYMax]

			shl	ecx,16
			and	eax,0FFFFh

			or	eax,ecx

			mov	dword ptr [edi].STRUC_POINT.Pt_XE,eax

			jmp	EnterClip
Generic_ClipYMax	ENDP
	*/

	S32 yDiff = pointB->Pt_YE - pointA->Pt_YE;

	Clip_Num = ClipYMax - pointA->Pt_YE;
	dst->Pt_XE = pointA->Pt_XE + (pointB->Pt_XE - pointA->Pt_XE) * Clip_Num / yDiff;
	dst->Pt_YE = ClipYMax;

	EnterClip(pointB, dst, pointA, yDiff);
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
		case CLIP_XMAX:
			Generic_ClipXMax(pointB, dst, pointA);
			break;
		case CLIP_YMIN:
			Generic_ClipYMin(pointB, dst, pointA);
			break;
		case CLIP_YMAX:
			Generic_ClipYMax(pointB, dst, pointA);
			break;
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
  Struc_Point *prevPoint = Fill_LastPoint; // Previous point
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
		|| (type == CLIP_XMAX && Fill_LastPoint->Pt_XE <= ClipXMax + 1)
		|| (type == CLIP_YMIN && Fill_LastPoint->Pt_YE >= ClipYMin)
		|| (type == CLIP_YMAX && Fill_LastPoint->Pt_YE <= ClipYMax))
	{
    isPointInvisible = 0;
	}

  while (currentPoint != Fill_LastPoint)
  {
    currentPoint++;
    if ((type == CLIP_XMIN && currentPoint->Pt_XE >= ClipXMin)
			|| (type == CLIP_XMAX && currentPoint->Pt_XE <= ClipXMax + 1)
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

S32 Draw_Triangle(Struc_Point *Ptr_Points)
{
/*
; ╔════════════════════════════════════════════════════════════════════════════════╗
; ║  Draw_Triangle : Remplissage d'un triangle                                     ║
; ╟────────────────────────────────────────────────────────────────────────────────╢
; ║  Appel : ESI = PTR sur une table du type Struc_Point (table des sommets)       ║
; ╟────────────────────────────────────────────────────────────────────────────────╢
; ║  Retour: Rien                                                                  ║
; ╟────────────────────────────────────────────────────────────────────────────────╢
; ║  Remarque: les sommets doivent etre ordonnes dans le sens trigo. (anti-        ║
; ║            horlogique).                                                        ║
; ║                                                                                ║
; ╟────────────────────────────────────────────────────────────────────────────────╢
; ║  Principe: (Exemple sur le Gouraud)                                            ║
; ║            (En fait le commentaire etait sur une routine dediee Gouraud et     ║
; ║            j'ai la flemme de le reecrire ;)                                    ║
; ║                                                                                ║
; ║                                                                                ║
; ║   Dans un triangle, l'increment du gouraud est constant.. Sisi, voici la       ║
; ║  preuve:                                                                       ║
; ║                                                                                ║
; ║                                                                                ║
; ║  Soit ABC notre triangle de coordonnees A(xa,ya,ga); B(xb,yb,gb); C(xc,yc,gc)  ║
; ║                                                                                ║
; ║                                                                                ║
; ║                   [(ga+y*Pente_Gouraud_Droite) - (ga+y*Pente_Gouraud_Gauche)]  ║
; ║  Inc_Gouraud(y) = ───────────────────────────────────────────────────────────  ║
; ║                           [(xa+y*Pente_Droite) - (xa+y*Pente_Gauche)]          ║
; ║                                                                                ║
; ║                                                                                ║
; ║                   Pente_Gouraud_Droite-Pente_Gouraud_Gauche                    ║
; ║                 = ─────────────────────────────────────────                    ║
; ║                           Pente_Droite-Pente_Gauche                            ║
; ║                                                                                ║
; ║                                                                                ║
; ║                 = Cste; d'ou gain.... :)                                       ║
; ║                                                                                ║
; ║                                                                                ║
; ║                                                                                ║
; ╚════════════════════════════════════════════════════════════════════════════════╝
			ALIGN	4
Draw_Triangle		PROC esi
			; *** Search the highest point in the list ***

			mov	eax,1
			mov	[Fill_FirstPoint],esi

			mov	[Fill_Patch],eax	;
			mov	eax,dword ptr [esi].STRUC_POINT.Pt_XE

			sar	eax,16			; EAX = Ya
			mov	edi,esi			; EDI = Highest point

			mov	ebp,eax			; EBP = Highest Y (least value)
			mov	ebx,dword ptr [esi+Size STRUC_POINT].STRUC_POINT.Pt_XE

			sar	ebx,16			; EBX = Yb
			add	esi,Size STRUC_POINT

			cmp	ebx,ebp
			jg	@@Not_Second

			mov	ebp,ebx
			mov	edi,esi
@@Not_Second:
			mov	ecx,dword ptr [esi+Size STRUC_POINT].STRUC_POINT.Pt_XE

			sar	ecx,16			; ECX = Yc
			add	esi,Size STRUC_POINT

			cmp	ecx,ebp
			jg	@@Not_Third

			mov	ebp,ecx
			mov	edi,esi
@@Not_Third:
			mov	[Fill_LeftPoint],edi
			mov	[Fill_RightPoint],edi

			cmp	eax,ebx			; Test if the poly is flat
			jne	@@Not_Flat

			cmp	ebx,ecx
			je	@@End
@@Not_Flat:
			mov	[Fill_LastPoint],esi	; Last point pointer
			mov	ebx,[PTR_TabOffLine]

			mov	eax,[Log]
			mov	[Fill_CurY],ebp		; EBP = Highest point_Y

			mov	ebx,[ebx+ebp*4]
			mov	ecx,READ_NEXT_L + READ_NEXT_R

			add	ebx,eax
			mov	esi,[Fill_LeftPoint]

			mov	[Fill_ReadFlag],ecx
			mov	[Fill_CurOffLine],ebx	; Current offset in log screen

					; *** Calculate the common denominator
					; for interpolation calculation ***

			mov	eax,[Fill_LastPoint]
			lea	edi,[esi+Size STRUC_POINT]

			cmp	edi,eax
			jbe	@@No_Wrap1
			mov	edi,[Fill_FirstPoint]
@@No_Wrap1:
			lea	ebp,[edi+Size STRUC_POINT]
			cmp	ebp,eax
			jbe	@@No_Wrap2
			mov	ebp,[Fill_FirstPoint]
@@No_Wrap2:
							; ESI = PtA
							; EDI = PtB
							; EBP = PtC

					; *** FPU math. ***
			fldcw	[Status_Int]	; 7 cycles lost (not pipelined...)



			; K1 = (Yb-Ya)
			; K2 = (Xc-Xa)
			; K3 = (Yc-Ya)
			; K4 = (Xb-Xa)
			; Denom = K1*K2 - K3*K4
						; 0   1   2   3   4   5   6   7
			fild	[esi].STRUC_POINT.Pt_XE	; Xa
			fild	[esi].STRUC_POINT.Pt_YE	; Ya  Xa
			fild	[edi].STRUC_POINT.Pt_XE	; Xb  Ya  Xa
			fild	[edi].STRUC_POINT.Pt_YE	; Yb  Xb  Ya  Xa
			fild	[ebp].STRUC_POINT.Pt_XE	; Xc  Yb  Xb  Ya  Xa
			fild	[ebp].STRUC_POINT.Pt_YE	; Yc  Xc  Yb  Xb  Ya  Xa

			fxch	st(2)		; Yb  Xc  Yc  Xb  Ya  Xa

			fsub	st,st(4)	; K1  Xc  Yc  Xb  Ya  Xa
			fxch	st(2)		; Yc  Xc  K1  Xb  Ya  Xa

			fsubrp	st(4),st	; Xc  K1  Xb  K3  Xa

			fsub	st,st(4)	; K2  K1  Xb  K3  Xa
			fxch	st(2)		; Xb  K1  K2  K3  Xa

			fsubrp	st(4),st	; K1  K2  K3  K4

			fld	st(0)		; K1  K1  K2  K3  K4

			fmulp	st(2),st	; K1  K12 K3  K4

			fld	st(2)		; K3  K1  K12 K3  K4

			fmulp	st(4),st	; K1  K12 K3  K34
			fxch	st(1)		; K12 K1  K3  K34

						; 2 cycles penalty

			fsubrp	st(3),st	; K1  K3  Denom

			mov	edx,[Fill_Type]

			fld	[F_256]		; 256 K1  K3  Denom
			fxch	st(3)		; Denom K1  K3  256

						; 1 cycle penalty

			fst	[Denom]		; Denom K1  K3  256

			mov	eax,[Denom]

			test	eax,eax
			je	@@EndFPU

			fdivp	st(3),st	; K1  K3  D

; Here we are always using ZBuffer, not needed but works for now...
;Patch_Jmp_XSlopeFPU::	jmp	[Jmp_XSlopeFPU+edx*4]
Patch_Jmp_XSlopeFPU::	jmp	[Jmp_XSlopeZBufFPU+edx*4]
*/
	// *** Search the highest point in the list ***
  Fill_FirstPoint = Ptr_Points;
  Fill_Patch = 1;

  S16 currentY = Ptr_Points[0].Pt_YE;
  Struc_Point *highestPoint = Ptr_Points;
  S16 highestY = Ptr_Points[0].Pt_YE; // lowest value
  S16 nextY = Ptr_Points[1].Pt_YE;
  Struc_Point *nextPoint = Ptr_Points + 1;

  if (nextY <= highestY)
  {
    highestY = nextY;
    highestPoint = nextPoint;
  }

  S16 nextNextY = nextPoint[1].Pt_YE;
  Struc_Point *nextNextPoint = nextPoint + 1;
  if (nextNextY <= highestY)
  {
    highestY = nextNextY;
    highestPoint = nextNextPoint;
  }

  Fill_LeftPoint = highestPoint;
  Fill_RightPoint = highestPoint;

	// Test if the poly is flat
  if (currentY == nextY && nextY == nextNextY)
	{
		return Fill_LeftSlope;
	}

  Fill_LastPoint = nextNextPoint;
  Fill_CurY = highestY;

  PTR_U8 screen = (PTR_U8)Log + PTR_TabOffLine[highestY];
  Fill_ReadFlag = READ_NEXT_L + READ_NEXT_R;
  Fill_CurOffLine = screen; // Current offset in log screen

	// *** Calculate the common denominator
	// for interpolation calculation ***
  Struc_Point *leftPoint = Fill_LeftPoint + 1;
  if (leftPoint > Fill_LastPoint)
	{
    leftPoint = Fill_FirstPoint;
	}

  Struc_Point *rightPoint = leftPoint + 1;
  if (rightPoint > Fill_LastPoint)
	{
    rightPoint = Fill_FirstPoint;
	}

	// esi = Fill_LeftPoint = PtA
	// edi = leftPoint = PtB
	// ebp = rightPoint = PtC
	Struc_Point *PtA = Fill_LeftPoint;
	Struc_Point *PtB = leftPoint;
	Struc_Point *PtC = rightPoint;

  Denom = (PtB->Pt_YE - PtA->Pt_YE) * (PtC->Pt_XE - PtA->Pt_XE)
				- (PtC->Pt_YE - PtA->Pt_YE) * (PtB->Pt_XE - PtA->Pt_XE);

	// FPU version divides Denom by 256.0, while integer versions
	// multiply by 256 the rest of values that will be divided by Denom.
	Denom /= 256.0;

	if (Denom == 0)
	{
		return Fill_LeftSlope;
	}

	return Jmp_XSlopeFPU[Fill_Type](PtA, PtB, PtC);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Fill_PolyClip: Display a n-angle (provided that the poly is convex)      ║
// ╟──────────────────────────────────────────────────────────────────────────╢
// ║ Appel : ECX = Nb points                                                  ║
// ║         ESI = PTR on the list of points                                  ║
// ║                                                                          ║
// ╟──────────────────────────────────────────────────────────────────────────╢
// ║ Retour: EAX = 0 if error (non-visible poly)                              ║
// ║                                                                          ║
// ╟──────────────────────────────────────────────────────────────────────────╢
// ║ ALL REGISTERS MODIFIED                                                   ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Fill_PolyClip(S32 Nb_Points, Struc_Point *Ptr_Points)
{
	// *** Get the poly-box so as to test the clipping ***
	Fill_FirstPoint = Ptr_Points;
	U32 counter = Nb_Points - 1;
	Struc_Point *points = Ptr_Points;

	S16 XMin = points[0].Pt_XE;
	S16 YMin = points[0].Pt_YE;
	S16 XMax = XMin;
	S16 YMax = YMin;
	points++;

	do
	{
		// @@Bounding_Box:

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
	points = Ptr_Points; // Restore List pointer

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
			return Draw_Triangle(points);
		}

		Draw_Triangle(points);

		// @@Ret_Triangle:

		// Copy the first point in the new location (the following one)
		Struc_Point *next = points + 1;
		memcpy(next, points, sizeof(Struc_Point));
		points = next;

		Nb_Points--;
	}

	return Fill_LeftSlope;
}

S32 Triangle_ReadNextEdge(Struc_Point *PtA, Struc_Point *PtB)
// edi, esi
{
	if ((Fill_ReadFlag & READ_NEXT_L) == 0)
	{
		return Read_Next_Right(PtA, PtB);
	}

  Struc_Point *newPoint = Fill_LeftPoint;
  Fill_ReadFlag &= ~READ_NEXT_L;

  while (1)
  {
		// @@Read_Next_L:
    Struc_Point *oldPoint = newPoint;
    newPoint++;
    if (newPoint > Fill_LastPoint)
		{
      newPoint = Fill_FirstPoint;
		}

    if (newPoint->Pt_YE != oldPoint->Pt_YE)
    {
			// If (NewY-OldY)<0 then we must end the rasterization
			if (newPoint->Pt_YE < oldPoint->Pt_YE)
			{
				return Fill_LeftSlope;
			}

      Fill_CurXMin = (oldPoint->Pt_XE << 16) + 0x8000;
      Fill_LeftPoint = newPoint;
      return Jmp_LeftSlopeFPU[Fill_Type](
				// For some reason diffX is << 16 but diffY is not
				(newPoint->Pt_XE - oldPoint->Pt_XE) << 16,
				(newPoint->Pt_YE - oldPoint->Pt_YE),
				oldPoint,
				newPoint);
    }
  }
}

S32 Triangle_ReadNextEdgeFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  return Triangle_ReadNextEdge(PtA, PtB);
}

S32 Gouraud_XSlopeFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC, S32 LB_LA, S32 LC_LA)
{
  YB_YA = PtB->Pt_YE - PtA->Pt_YE;
  YC_YA = PtC->Pt_YE - PtA->Pt_YE;

  Fill_Gouraud_XSlope = (YB_YA * LC_LA - YC_YA * LB_LA) / Denom;
  return Triangle_ReadNextEdge(PtA, PtB);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for Gouraud shading                                 ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_GouraudXSlopeFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  S32 LB_LA = PtB->Pt_Light - PtA->Pt_Light;
  S32 LC_LA = PtC->Pt_Light - PtA->Pt_Light;
  return Gouraud_XSlopeFPU(PtA, PtB, PtC, LB_LA, LC_LA);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for Dithered Gouraud shading (the intensity is      ║
// ║ saturated)                                                               ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_DitherXSlopeFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  if (PtA->Pt_Light >= 14 * 256)
  {
    // @@SatEDIUp:
    PtA->Pt_Light = 14 * 256 - 1;
  } else if (PtA->Pt_Light <= 255)
  {
    // @@SatEDILow:
    PtA->Pt_Light = 256;
  }

  if (PtB->Pt_Light >= 14 * 256)
  {
    // @@SatESIUp:
    PtB->Pt_Light = 14 * 256 - 1;
  }
  else if (PtB->Pt_Light <= 255)
  {
    // @@SatESILow:
    PtB->Pt_Light = 256;
  }

  if (PtC->Pt_Light >= 14 * 256)
  {
    // @@SatEBPUp:
    PtC->Pt_Light = 14 * 256 - 1;
  }
  else if (PtC->Pt_Light <= 255)
  {
    // @@SatEBPLow:
    PtC->Pt_Light = 256;
  }

  S32 LB_LA = PtB->Pt_Light - PtA->Pt_Light;
  S32 LC_LA = PtC->Pt_Light - PtA->Pt_Light;
  return Gouraud_XSlopeFPU(PtA, PtB, PtC, LB_LA, LC_LA);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for Texture Mapping                                 ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_TextureXSlopeFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
	YB_YA = PtB->Pt_YE - PtA->Pt_YE;
	YC_YA = PtC->Pt_YE - PtA->Pt_YE;

  Fill_MapU_XSlope = (YB_YA * (PtC->Pt_MapU - PtA->Pt_MapU)
      - YC_YA * (PtB->Pt_MapU - PtA->Pt_MapU))
     / Denom;
  Fill_MapV_XSlope = (YB_YA * (PtC->Pt_MapV - PtA->Pt_MapV)
      - YC_YA * (PtB->Pt_MapV - PtA->Pt_MapV))
     / Denom;
  return Triangle_ReadNextEdge(PtB, PtC);
}

S32 Calc_TextureGouraudXSlopeFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  S32 UA_UB = PtA->Pt_MapU - PtB->Pt_MapU;
  S32 UC_UB = PtC->Pt_MapU - PtB->Pt_MapU;
  S32 VA_VB = PtA->Pt_MapV - PtB->Pt_MapV;
  S32 VC_VB = PtC->Pt_MapV - PtB->Pt_MapV;
  S32 LC_LB = PtC->Pt_Light - PtB->Pt_Light;
  S32 LA_LB = PtA->Pt_Light - PtB->Pt_Light;
  Fill_MapU_XSlope = roundf((UC_UB * YB_YA - UA_UB * YC_YA) * Denom);
  Fill_MapV_XSlope = roundf((VC_VB * YB_YA - VA_VB * YC_YA) * Denom);
  Fill_Gouraud_XSlope = roundf(Denom * (YB_YA * LC_LB - YC_YA * LA_LB));
  return Triangle_ReadNextEdge(PtB, PtC);
}

S32 Calc_TextureDitherXSlopeFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return 0;
}

S32 Calc_TextureZXSlopeFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return 0;
}

S32 Calc_TextureZGouraudXSlopeFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return 0;
}

S32 Calc_TextureZXSlopeZBufFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return 0;
}

S32 Calc_XSlopeZBufferFlagFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return 0;
}

S32 Test_Scan()
{
	Struc_Point *right = Fill_RightPoint;
	Struc_Point *left = Fill_LeftPoint;
	S32 diffY = 0;

	// AGI
	if (left->Pt_YE < right->Pt_YE)
	{
		// Left
		Fill_ReadFlag = READ_NEXT_L;

		diffY = left->Pt_YE - Fill_CurY;
	}
	else if (left->Pt_YE > right->Pt_YE)
	{
		// Right
		Fill_ReadFlag = READ_NEXT_R;

		diffY = right->Pt_YE - Fill_CurY;
	}
	else
	{
		// Both
		Fill_ReadFlag = READ_NEXT_L | READ_NEXT_R;

		diffY = left->Pt_YE - Fill_CurY;
	}

	if (diffY == 0)
	{
		return Triangle_ReadNextEdge(right, left);
	}

	return Fill_Filler(diffY, Fill_CurXMin, Fill_CurXMax);
}

S32 Read_Next_Right(Struc_Point *PtA, Struc_Point *PtB)
{
	// edi, esi
/*
Read_Next_Right::
		mov	eax,[Fill_ReadFlag]

		test	eax,READ_NEXT_R
		je	@@Test_Scan

		mov	esi,[Fill_RightPoint]
		and	eax,not READ_NEXT_R

		mov	[Fill_ReadFlag],eax
		mov	eax,[Fill_FirstPoint]
@@Read_Next_R:
		mov	edi,esi
		sub	esi,Size STRUC_POINT

		cmp	esi,eax
		jae	@@No_LoopRight

		mov	esi,[Fill_LastPoint]
@@No_LoopRight:
						; EDI = Old point
						; ESI = New point
		mov	ebx,dword ptr [edi].STRUC_POINT.Pt_XE
		mov	ecx,dword ptr [esi].STRUC_POINT.Pt_XE

		mov	edx,ebx
		and	ecx,0FFFF0000h		; ECX = NewY shl 16

		shl	ebx,16			; EBX = OldXMin
		and	edx,0FFFF0000h		; EDX = OldY shl 16

		sub	ecx,edx			; If same Y then get next point
		je	@@Read_Next_R

		lea	ebx,[ebx+8000h]
		js	@@End			; If (NewY-OldY)<0 then we must end the rasterization

		shr	ecx,16

		push	ecx
		mov	eax,dword ptr [esi].STRUC_POINT.Pt_XE

		fild	dword ptr [esp]	; dY

		shl	eax,16
		mov	[Fill_CurXMax],ebx

		fdivr	[F_1]		; 1/dY

		sub	ebx,8000h

		sub	eax,ebx
		mov	[Fill_RightPoint],esi

		mov	[esp],eax

		fild	dword ptr [esp]	; dX  1/dY

		fmulp	st(1),st; dX/dY

		pop	eax

		fadd	[F_1]	; dX/dY+1

		fistp	[Fill_RightSlope]

		jmp	@@Test_Scan
	*/
	if ((Fill_ReadFlag & READ_NEXT_R) == 0)
	{
		return Test_Scan();
	}

	Struc_Point *point = Fill_RightPoint;
	Fill_ReadFlag &= ~READ_NEXT_R;
	Struc_Point *first = Fill_FirstPoint;
	S32 oldXMin = 0;
	S32 diffY = 0;
	Struc_Point *oldPoint = NULL;

	do
	{
		// @@Read_Next_R:
		oldPoint = point;
		point--;

		if (point < first)
		{
			point = Fill_LastPoint;
		}

		// @@No_LoopRight:
		S32 oldYMin = oldPoint->Pt_YE;
		S32 newYMin = point->Pt_YE;
		oldXMin = oldPoint->Pt_XE;
		diffY = newYMin - oldYMin;
	} while (diffY == 0); // If same Y then get next point

	if (diffY < 0)
	{
		return Fill_LeftSlope;
	}

	Fill_CurXMax = (oldXMin << 16) + 0x8000;
	Fill_RightPoint = point;
	S32 diffX = (point->Pt_XE - oldXMin) << 16;
	Fill_RightSlope = diffX / diffY + 1;

	return Test_Scan();
}

S32 Read_Next_RightFPU(S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
/*
;			******************
;			*** RIGHT EDGE ***
;			******************

Read_Next_RightFPU::
			fild	dword ptr [esp]	; dX  1/dY

			fmulp	st(1),st; dX/dY

			pop	eax
			pop	ebx

			fadd	[F_1]	; dX/dY+1

			fistp	[Fill_LeftSlope]
*/
	Fill_LeftSlope = diffX / diffY + 1;
	return Read_Next_Right(PtA, PtB);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the LeftSlope for Gouraud shading                              ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_GouraudLeftSlopeFPU(S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
  U32 lightA = (PtA->Pt_Light << 8) + 0x8000;
  U32 lightB = (PtB->Pt_Light + 0x80) << 8;
  S32 diffLight = lightB - lightA;
  Fill_CurGouraudMin = lightA + Fill_Color;
  Fill_Gouraud_LeftSlope = diffLight / diffY;
  Fill_LeftSlope = diffX / diffY + 1;

  return Read_Next_Right(PtA, PtB);
}

S32 Calc_GouraudTableLeftSlopeFPU(S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return 0;
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the LeftSlope for Texture Mapping                              ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_TextureLeftSlopeFPU(S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
//int __usercall Calc_TextureLeftSlope@<eax>(signed int a1@<ecx>, int a2@<edi>, int a3@<esi>)
{
  Fill_CurMapUMin = PtA->Pt_MapU << 8;
  Fill_MapU_LeftSlope = ((PtB->Pt_MapU - PtA->Pt_MapU) << 8) / diffY;
  Fill_CurMapVMin = PtA->Pt_MapV << 8;
  Fill_MapV_LeftSlope = ((PtB->Pt_MapV - PtA->Pt_MapV) << 8) / diffY;
	Fill_LeftSlope = diffX / diffY + 1;
  return Read_Next_Right(PtA, PtB);
}

S32 Calc_TextureGouraudLeftSlopeFPU(S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
  int v3; // eax@1
  int v4; // ebx@1
  signed __int64 v5; // rtt@1
  int v6; // eax@1
  int v7; // ebx@1
  int v8; // eax@1
  int v9; // ebx@1

  v3 = PtB->Pt_MapU + 128;
  v4 = PtA->Pt_MapU + 128;
  LODWORD(v5) = (v3 - v4) << 8;
  HIDWORD(v5) = (v3 - v4) >> 24;
  Fill_CurGouraudMin = v4 << 8;
  Fill_Gouraud_LeftSlope = roundf(v5 / a1) + (roundf(v5 / a1) >> 31);
  v6 = PtB->Pt_Light;
  v7 = PtA->Pt_Light;
  LODWORD(v5) = (v6 - v7) << 8;
  HIDWORD(v5) = (v6 - v7) >> 24;
  Fill_CurMapUMin = v7 << 8;
  Fill_MapU_LeftSlope = roundf(v5 / a1) + (roundf(v5 / a1) >> 31);
  v8 = PtB->Pt_MapV;
  v9 = PtA->Pt_MapV;
  LODWORD(v5) = (v8 - v9) << 8;
  HIDWORD(v5) = (v8 - v9) >> 24;
  Fill_CurMapVMin = v9 << 8;
  Fill_MapV_LeftSlope = (unsigned __int64)(v5 / a1) + ((unsigned int)(v5 / a1) >> 31);
  return Read_Next_Right(PtA, PtB);
}

S32 Calc_TextureZLeftSlopeFPU(S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return 0;
}

S32 Calc_TextureZGouraudLeftSlopeFPU(S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return 0;
}

S32 Calc_TextureZLeftSlopeZBufFPU(S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return 0;
}

S32 Calc_LeftSlopeZBufferFlagFPU(S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return 0;
}

Jmp_XSlopeFunc Jmp_XSlopeFPU[] = {
	Triangle_ReadNextEdgeFPU, // Solid
	Triangle_ReadNextEdgeFPU, // Flat
	Triangle_ReadNextEdgeFPU, // Transp.
	Triangle_ReadNextEdgeFPU, // Trame
	Calc_GouraudXSlopeFPU, // Gouraud
	Calc_DitherXSlopeFPU, // Dither
	Calc_GouraudXSlopeFPU, // Gouraud table
	Calc_DitherXSlopeFPU, // Dither table
	Calc_TextureXSlopeFPU, // Texture Solid
	Calc_TextureXSlopeFPU, // Texture Flat
	Calc_TextureGouraudXSlopeFPU, // Texture Gouraud
	Calc_TextureDitherXSlopeFPU, // Texture Gouraud Dither
	Calc_TextureXSlopeFPU, // Texture Solid Chroma Key
	Calc_TextureXSlopeFPU, // Texture Flat Chroma Key
	Calc_TextureGouraudXSlopeFPU, // Texture Gouraud Chroma Key
	Calc_TextureDitherXSlopeFPU, // Texture Gouraud Dither Chroma Key
	Calc_TextureZXSlopeFPU, // Texture Z Solid
	Calc_TextureZXSlopeFPU, // Texture Z Flat
	Calc_TextureZGouraudXSlopeFPU, // Texture Z Gouraud
	Calc_TextureZGouraudXSlopeFPU, // Texture Z Dither
	Calc_TextureZXSlopeFPU, // Texture Z Solid Chroma Key
	Calc_TextureZXSlopeFPU, // Texture Z Flat Chroma Key
	Calc_TextureZGouraudXSlopeFPU, // Texture Z Gouraud Chroma Key
	Calc_TextureZGouraudXSlopeFPU, // Texture Z Dither Chroma Key
	Calc_TextureZXSlopeZBufFPU, // Texture Z Solid
	Calc_XSlopeZBufferFlagFPU, // ZBuffer only
};

Jmp_LeftSlopeFunc Jmp_LeftSlopeFPU[] = {
	Read_Next_RightFPU, // Solid
	Read_Next_RightFPU, // Flat
	Read_Next_RightFPU, // Transp.
	Read_Next_RightFPU, // Trame
	Calc_GouraudLeftSlopeFPU, // Gouraud
	Calc_GouraudLeftSlopeFPU, // Dither
	Calc_GouraudTableLeftSlopeFPU, // Gouraud table
	Calc_GouraudTableLeftSlopeFPU, // Dither table
	Calc_TextureLeftSlopeFPU, // Texture Solid
	Calc_TextureLeftSlopeFPU, // Texture Flat
	Calc_TextureGouraudLeftSlopeFPU, // Texture Gouraud
	Calc_TextureGouraudLeftSlopeFPU, // Texture Gouraud Dither
	Calc_TextureLeftSlopeFPU, // Texture Solid Chroma Key
	Calc_TextureLeftSlopeFPU, // Texture Flat Chroma Key
	Calc_TextureGouraudLeftSlopeFPU, // Texture Gouraud Chroma Key
	Calc_TextureGouraudLeftSlopeFPU, // Texture Gouraud Dither Chroma Key
	Calc_TextureZLeftSlopeFPU, // Texture Z Solid
	Calc_TextureZLeftSlopeFPU, // Texture Z Flat
	Calc_TextureZGouraudLeftSlopeFPU, // Texture Z Gouraud
	Calc_TextureZGouraudLeftSlopeFPU, // Texture Z Dither
	Calc_TextureZLeftSlopeFPU, // Texture Z Solid Chroma Key
	Calc_TextureZLeftSlopeFPU, // Texture Z Flat Chroma Key
	Calc_TextureZGouraudLeftSlopeFPU, // Texture Z Gouraud Chroma Key
	Calc_TextureZGouraudLeftSlopeFPU, // Texture Z Dither Chroma Key
	Calc_TextureZLeftSlopeZBufFPU, // Texture Z Solid
	Calc_LeftSlopeZBufferFlagFPU, // ZBuffer only
};

// =============================================================================
#ifdef __cplusplus
}
#endif
