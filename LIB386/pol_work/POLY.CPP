#include <POLYGON/POLY.H>

#include <SVGA/SCREEN.H>

#include <limits.h>
#include <string.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

U32 Fill_Type = 0; // Polygon type number

U32 Fill_LeftSlope = 0;      ///< Left slope
U32 Fill_RightSlope = 0;     ///< Right slope

U8 Fill_ReadFlag = FALSE;        ///< Flag telling which point to read
U8 Fill_ClipFlag = FALSE;        ///< Flag telling us what to clip
U8 Fill_Flag_Fog = FALSE;     // True if FOG active
U8 Fill_Flag_ZBuffer = FALSE;	// True if ZBUFFER is active
U8 Fill_Flag_NZW = FALSE;     // True if NZW active

void *Fill_Saut_Normal = NULL; // Filler table

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

	// TODO: This filler is a pointer to the filler function to call. I still
	// need to figure out the signature of those fillers which are in POLY_JMP.
	// U32 *filler = ((U32 **)Fill_Saut_Normal)[Type_Poly];
	// return filler();
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);

  return 0;
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

// =============================================================================
#ifdef __cplusplus
}
#endif
