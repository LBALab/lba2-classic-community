#include <POLYGON/POLY.H>

#include <POLYGON/POLY_JMP.H>

#include <SVGA/SCREEN.H>

#include <limits.h>
#include <string.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

U32 Fill_Type = 0; // Polygon type number

U32 **Fill_Filler = NULL;
U32 Fill_Patch = 0;
U8 Fill_Color = 0;

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
	// TODO: Implement ASM
	printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
	return 0;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
