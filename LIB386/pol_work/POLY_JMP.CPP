/*
; **************************************************************************
; **************************************************************************
; **************************************************************************
; ***                                                                    ***
; ***                         POLYGON LIBRARY                            ***
; ***                                                                    ***
; **************************************************************************
; ***                                                                    ***
; ***                     (Adeline Software Intl.)                       ***
; ***                           1995-96-97                               ***
; ***                                                                    ***
; **************************************************************************
; **************************************************************************
; **************************************************************************
*/

#include <POLYGON/POLY_JMP.H>

#include <FILLER.H>
#include <POLYGON/POLY.H>
#include <POLYGON/POLYFLAT.H>
#include <POLYGON/POLYGOUR.H>
#include <POLYGON/POLYGTEX.H>
#include <POLYGON/POLYTEXT.H>
#include <POLYGON/POLYTEXZ.H>
#include <POLYGON/POLYTZF.H>

#include <stdio.h>

S32 Jmp_Solid(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_Flat;
	Fill_ClipFlag = CLIP_FLAT;
	Fill_Color.Num = color | (color << 8);
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_Transparent(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_Transparent		PROC
			mov	eax,Offset Filler_Transparent
			and	ebx,0F0h

			mov	[Fill_Filler],eax
			mov	edx,CLIP_FLAT

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_Transparent		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_Trame(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_Trame		PROC
			mov	eax,Offset Filler_Trame
			mov	[Fill_Color],ebx

			mov	[Fill_Filler],eax
			mov	edx,CLIP_FLAT

			mov	[Fill_ClipFlag],edx
			xor	eax,eax

			mov	[Fill_Trame_Parity],eax
			jmp	Fill_PolyClip
Jmp_Trame		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_SolidFog(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_Flat;
  Fill_ClipFlag = CLIP_FLAT;
  color = (color & 0xFF00) | Fill_Logical_Palette[color];
  color |= color << 8;
  Fill_Color.Num = color;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TrameFog(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TrameFog		PROC
			mov	bl,[Fill_Logical_Palette+ebx]
			mov	eax,Offset Filler_Trame
			mov	[Fill_Color],ebx

			mov	[Fill_Filler],eax
			mov	edx,CLIP_FLAT

			mov	[Fill_ClipFlag],edx
			xor	eax,eax

			mov	[Fill_Trame_Parity],eax
			jmp	Fill_PolyClip
Jmp_TrameFog		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_SolidZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_FlatZBuf;
  Fill_ClipFlag = CLIP_FLAT + CLIP_ZBUFFER;
  Fill_Color.Num = color | (color << 8);

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TransparentZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TransparentZBuf;
  Fill_ClipFlag = CLIP_FLAT + CLIP_ZBUFFER;
  Fill_Color.Num = color & 0xF0;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TrameZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TrameZBuf		PROC
			mov	eax,Offset Filler_TrameZBuf
			mov	[Fill_Color],ebx

			mov	[Fill_Filler],eax
			xor	eax,eax

			mov	[Fill_Trame_Parity],eax
			mov	eax,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_ClipFlag],eax
			jmp	Fill_PolyClip
Jmp_TrameZBuf		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_SolidFogZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  color = Fill_Logical_Palette[color] & 0xFF;
  color = (color << 8) | color;

  Fill_Filler = Filler_FlatZBuf;
  Fill_ClipFlag = CLIP_FLAT + CLIP_ZBUFFER;
  Fill_Color.Num = color;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TrameFogZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TrameFogZBuf	PROC
			mov	bl,[Fill_Logical_Palette]
			mov	eax,Offset Filler_TrameZBuf

			mov	[Fill_Filler],eax
			xor	eax,eax

			mov	[Fill_Trame_Parity],eax
			mov	eax,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_Color],ebx
			mov	[Fill_ClipFlag],eax

			jmp	Fill_PolyClip
Jmp_TrameFogZBuf	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_SolidNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_SolidNZW		PROC
			mov	eax,Offset Filler_FlatNZW
			mov	edx,ebx

			shl	edx,8
			mov	[Fill_Filler],eax

			or	ebx,edx
			mov	edx,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_SolidNZW		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_TransparentNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TransparentNZW	PROC
			mov	eax,Offset Filler_TransparentNZW
			and	ebx,0F0h

			mov	[Fill_Filler],eax
			mov	[Fill_Color],ebx
			mov	eax,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_ClipFlag],eax
			jmp	Fill_PolyClip
Jmp_TransparentNZW	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_TrameNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TrameNZW		PROC
			mov	eax,Offset Filler_TrameNZW
			mov	[Fill_Color],ebx

			mov	[Fill_Filler],eax
			xor	eax,eax

			mov	[Fill_Trame_Parity],eax
			mov	eax,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_ClipFlag],eax
			jmp	Fill_PolyClip
Jmp_TrameNZW		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_SolidFogNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  color = Fill_Logical_Palette[color] & 0xFF;
  color = (color << 8) | color;
  Fill_Filler = Filler_FlatNZW;
  Fill_ClipFlag = CLIP_FLAT+CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TrameFogNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TrameFogNZW		PROC
			mov	bl,[Fill_Logical_Palette]
			mov	eax,Offset Filler_TrameNZW

			mov	[Fill_Filler],eax
			xor	eax,eax

			mov	[Fill_Trame_Parity],eax
			mov	eax,CLIP_FLAT+CLIP_ZBUFFER

			mov	[Fill_Color],ebx
			mov	[Fill_ClipFlag],eax

			jmp	Fill_PolyClip
Jmp_TrameFogNZW		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

// *****************
// ***  GOURAUD  ***
// *****************

S32 Jmp_Gouraud(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_Gouraud;
  Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT;
  Fill_Color.Num = color << 16;
  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_Dither(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_Dither;
  Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT;
  Fill_Color.Num = color << 16;
  return Fill_PolyClip(Nb_Points, Ptr_Points);
}



S32 Jmp_GouraudTable(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_GouraudTable	PROC
			mov	eax,Offset Filler_GouraudTable
			mov	edx,CLIP_FLAT+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	[Fill_ClipFlag],edx

			mov	[Fill_Color],ebx
			jmp	Fill_PolyClip
Jmp_GouraudTable	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}




S32 Jmp_DitherTable(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_DitherTable		PROC
			mov	eax,Offset Filler_DitherTable
			mov	edx,CLIP_FLAT+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	[Fill_ClipFlag],edx

			mov	[Fill_Color],ebx
			jmp	Fill_PolyClip
Jmp_DitherTable		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_GouraudFog(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_GouraudFog;
  Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT;
  Fill_Color.Num = color << 16;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_DitherFog(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_DitherFog;
  Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT;
  Fill_Color.Num = color << 16;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_GouraudZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_GouraudZBuf;
  Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT + CLIP_ZBUFFER;
  Fill_Color.Num = color << 16;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_DitherZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_DitherZBuf		PROC
			shl	ebx,16
			mov	eax,Offset Filler_DitherZBuf

			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_DitherZBuf		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}




S32 Jmp_GouraudTableZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_GouraudTableZBuf	PROC
			mov	eax,Offset Filler_GouraudTableZBuf
			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	[Fill_ClipFlag],edx

			mov	[Fill_Color],ebx
			jmp	Fill_PolyClip
Jmp_GouraudTableZBuf	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_DitherTableZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_DitherTableZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER;
  Fill_Color.Ptr = PtrCLUTGouraud + color;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_GouraudFogZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_GouraudFogZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER;
  Fill_Color.Num = color << 16;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_DitherFogZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_DitherFogZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER;
  Fill_Color.Num = color << 16;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_GouraudNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_GouraudNZW		PROC
			shl	ebx,16
			mov	eax,Offset Filler_GouraudNZW

			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_GouraudNZW		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}


S32 Jmp_DitherNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_DitherNZW		PROC
			shl	ebx,16
			mov	eax,Offset Filler_DitherNZW

			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER
			mov	[Fill_Filler],eax

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],ebx

			jmp	Fill_PolyClip
Jmp_DitherNZW		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_GouraudTableNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_GouraudTableNZW	PROC
			mov	eax,Offset Filler_GouraudTableNZW
			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	[Fill_ClipFlag],edx

			mov	[Fill_Color],ebx
			jmp	Fill_PolyClip
Jmp_GouraudTableNZW	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_DitherTableNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_DitherTableNZW	PROC
			mov	eax,Offset Filler_DitherTableNZW
			mov	edx,CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			add	ebx,eax
			mov	[Fill_ClipFlag],edx

			mov	[Fill_Color],ebx
			jmp	Fill_PolyClip
Jmp_DitherTableNZW	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}



S32 Jmp_GouraudFogNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_GouraudFogNZW;
  Fill_ClipFlag = CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER;
  Fill_Color.Num = color << 16;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}


S32 Jmp_DitherFogNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_DitherFogNZW;
  Fill_ClipFlag = CLIP_FLAT+CLIP_LIGHT+CLIP_ZBUFFER;
  Fill_Color.Num = color << 16;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

// *****************
// ***  TEXTURE  ***
// *****************

S32 Jmp_Texture(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_Texture;
	Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTURE;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}


S32 Jmp_TextureFlat(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureFlat		PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureFlat

			mov	edx,CLIP_FLAT+CLIP_TEXTURE
			mov	[Fill_Color],ebx

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFlat		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}


S32 Jmp_TextureChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureChromaKey	PROC
			mov	eax,Offset Filler_TextureChromaKey
			mov	edx,CLIP_FLAT+CLIP_TEXTURE

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureChromaKey	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}



S32 Jmp_TextureFlatChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureFlatChromaKey PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureFlatChromaKey

			mov	edx,CLIP_FLAT+CLIP_TEXTURE
			mov	[Fill_Color],ebx

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFlatChromaKey ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}




S32 Jmp_TextureFog(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureFog		PROC
			mov	eax,Offset Filler_TextureFog
			mov	edx,CLIP_FLAT+CLIP_TEXTURE

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFog		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}



S32 Jmp_TextureChromaKeyFog(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureChromaKeyFog	PROC
			mov	eax,Offset Filler_TextureChromaKeyFog
			mov	edx,CLIP_FLAT+CLIP_TEXTURE

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureChromaKeyFog	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_TextureZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureChromaKeyZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureChromaKeyZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureFlatZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureFlatZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER;
  Fill_Color.Ptr = PtrCLUTGouraud + (color << 8);

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureFlatChromaKeyZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureFlatChromaKeyZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER;
  Fill_Color.Ptr = PtrCLUTGouraud + (color << 8);

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureFogZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureFogZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureChromaKeyFogZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureChromaKeyFogZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureNZW		PROC
			mov	eax,Offset Filler_TextureNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureNZW		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}



S32 Jmp_TextureChromaKeyNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureChromaKeyNZW PROC
			mov	eax,Offset Filler_TextureChromaKeyNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureChromaKeyNZW ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_TextureFlatNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureFlatNZW;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER;
  Fill_Color.Ptr = PtrCLUTGouraud + (color << 8);

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureFlatChromaKeyNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureFlatChromaKeyNZW;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER;
  Fill_Color.Ptr = PtrCLUTGouraud + (color << 8);

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureFogNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureFogNZW	PROC
			mov	eax,Offset Filler_TextureFogNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureFogNZW	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_TextureChromaKeyFogNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureChromaKeyFogNZW PROC
			mov	eax,Offset Filler_TextureChromaKeyFogNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureChromaKeyFogNZW ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

// *************************
// ***  GOURAUD TEXTURE  ***
// *************************

S32 Jmp_GouraudTexture(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureGouraud;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE + CLIP_LIGHT;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}


S32 Jmp_DitherTexture(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureDither;
  Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE + CLIP_LIGHT;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}


S32 Jmp_GouraudTextureChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_GouraudTextureChromaKey PROC
			mov	eax,Offset Filler_TextureGouraudChromaKey
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_GouraudTextureChromaKey ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}


S32 Jmp_DitherTextureChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_DitherTextureChromaKey PROC
			mov	eax,Offset Filler_TextureDitherChromaKey
			mov	edx,CLIP_FLAT+CLIP_TEXTURE+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_DitherTextureChromaKey ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_GouraudTextureZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureGouraudZBuf;
  Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE + CLIP_LIGHT + CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_GouraudTextureChromaKeyZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureGouraudChromaKeyZBuf;
  Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE + CLIP_LIGHT + CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_GouraudTextureNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureGouraudNZW;
  Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE + CLIP_LIGHT + CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_GouraudTextureChromaKeyNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureGouraudChromaKeyNZW;
  Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE + CLIP_LIGHT + CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

//			********************************************
//			***  TEXTURE with PERSPECTIVE CORRECTION ***
//			********************************************

S32 Jmp_TextureZ(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZ		PROC
			mov	eax,Offset Filler_TextureZ
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZ		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}


S32 Jmp_TextureZFlat(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZFlat	PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureZFlat

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ
			add	eax,ebx

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],eax

			jmp	Fill_PolyClip
Jmp_TextureZFlat	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}




S32 Jmp_TextureZChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZChromaKey	PROC
			mov	eax,Offset Filler_TextureZChromaKey
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZChromaKey	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}



S32 Jmp_TextureZFlatChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZFlatChromaKey PROC
			shl	ebx,8
			mov	eax,Offset Filler_TextureZFlatChromaKey

			mov	[Fill_Filler],eax
			mov	eax,[PtrCLUTGouraud]

			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ
			add	eax,ebx

			mov	[Fill_ClipFlag],edx
			mov	[Fill_Color],eax

			jmp	Fill_PolyClip
Jmp_TextureZFlatChromaKey ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}



S32 Jmp_TextureZFog(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZFog		PROC
			mov	eax,Offset Filler_TextureZFog
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZFog		ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}




S32 Jmp_TextureZChromaKeyFog(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZChromaKeyFog PROC
			mov	eax,Offset Filler_TextureZChromaKeyFog
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZChromaKeyFog ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}




S32 Jmp_TextureZZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZZBuf	PROC
			mov	eax,Offset Filler_TextureZZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZZBuf	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_TextureZFlatZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureZFlatZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER;
  Fill_Color.Ptr = PtrCLUTGouraud + (color << 8);

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureZChromaKeyZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZChromaKeyZBuf PROC
			mov	eax,Offset Filler_TextureZChromaKeyZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZChromaKeyZBuf ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_TextureZFlatChromaKeyZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureZFlatChromaKeyZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER;
  Fill_Color.Ptr = PtrCLUTGouraud + (color << 8);

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureZFogZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureZFogZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureZChromaKeyFogZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZChromaKeyFogZBuf PROC
			mov	eax,Offset Filler_TextureZChromaKeyFogZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZChromaKeyFogZBuf ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_TextureZNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZNZW	PROC
			mov	eax,Offset Filler_TextureZNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZNZW	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_TextureZFlatNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureZFlatNZW;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER;
  Fill_Color.Ptr = PtrCLUTGouraud + (color << 8);

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureZChromaKeyNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZChromaKeyNZW PROC
			mov	eax,Offset Filler_TextureZChromaKeyNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZChromaKeyNZW ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_TextureZFlatChromaKeyNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureZFlatChromaKeyNZW;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER;
  Fill_Color.Ptr = PtrCLUTGouraud + (color << 8);

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureZFogNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureZFogNZW;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureZChromaKeyFogNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZChromaKeyFogNZW PROC
			mov	eax,Offset Filler_TextureZChromaKeyFogNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZChromaKeyFogNZW ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

// ****************************************************************
// ***  TEXTURE with PERSPECTIVE CORRECTION and GOURAUD SHADING ***
// ****************************************************************

S32 Jmp_TextureZGouraud(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZGouraud	PROC
			mov	eax,Offset Filler_TextureZGouraud
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZGouraud	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}


S32 Jmp_TextureZGouraudChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZGouraudChromaKey PROC
			mov	eax,Offset Filler_TextureZGouraudChromaKey
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_LIGHT

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZGouraudChromaKey ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}




S32 Jmp_TextureZGouraudZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZGouraudZBuf	PROC
			mov	eax,Offset Filler_TextureZGouraudZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZGouraudZBuf	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}



S32 Jmp_TextureZGouraudChromaKeyZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZGouraudChromaKeyZBuf PROC
			mov	eax,Offset Filler_TextureZGouraudChromaKeyZBuf
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZGouraudChromaKeyZBuf ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}



S32 Jmp_TextureZGouraudNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZGouraudNZW	PROC
			mov	eax,Offset Filler_TextureZGouraudNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZGouraudNZW	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}



S32 Jmp_TextureZGouraudChromaKeyNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZGouraudChromaKeyNZW PROC
			mov	eax,Offset Filler_TextureZGouraudChromaKeyNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_LIGHT+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZGouraudChromaKeyNZW ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

// ************************************************
// ***  TEXTURE FOG with PERSPECTIVE CORRECTION ***
// ************************************************

S32 Jmp_TextureZFogSmooth(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZFogSmooth	PROC
			mov	eax,Offset Filler_TextureZFogSmooth
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZFogSmooth	ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_TextureZFogSmoothZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureZFogSmoothZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureZFogSmoothNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	/*
Jmp_TextureZFogSmoothNZW PROC
			mov	eax,Offset Filler_TextureZFogSmoothNZW
			mov	edx,CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER

			mov	[Fill_Filler],eax
			mov	[Fill_ClipFlag],edx

			jmp	Fill_PolyClip
Jmp_TextureZFogSmoothNZW ENDP
	*/
    // TODO: Implement ASM
    printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
		return 0;
}

S32 Jmp_FlagZBuffer(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_FlagZBuffer;
  Fill_ClipFlag = CLIP_FLAT+CLIP_ZBUFFER;
  IsPolygonHidden = 1;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

// ************************************************************************
// ************************************************************************
// ***                                                                  ***
// ***           JUMP TABLE (DEFAULT POLYGONS)                          ***
// ***                                                                  ***
// ************************************************************************
// ************************************************************************
Fill_Jump_Fn Fill_N_Table_Jumps[] = {
	// **** FLAT + SPECIAUX ****
	Jmp_Solid,		// 0 Solid
	Jmp_Solid,		// 1 Flat
	Jmp_Transparent,		// 2 Transparent
	Jmp_Trame,		// 3 Trame

	// **** GOURAUD ****
	Jmp_Gouraud,		// 4 Gouraud
	Jmp_Dither,		// 5 Gouraud Dither
	Jmp_GouraudTable,	// 6 Gouraud table
	Jmp_DitherTable,		// 7 Gouraud Dither table

	// **** MAPPING ****
	// ** Without Chromakey **
	Jmp_Texture,		// 8 Texture Solid
	Jmp_TextureFlat,		// 9 Texture Flat

	Jmp_GouraudTexture,	// 10 Texture Gouraud
	Jmp_DitherTexture,	// 11 Texture Gouraud Dither

	// ** With Chromakey **
	Jmp_TextureChromaKey,	// 12 Texture Solid Chromakey
	Jmp_TextureFlatChromaKey,// 13 Texture Flat Chromakey

	Jmp_GouraudTextureChromaKey,// 14 Texture Gouraud Chromakey
	Jmp_DitherTextureChromaKey,// 15 Texture Gouraud Dither Chromakey


	// **** MAPPING + PERSPECTIVE CORRECTED ****
	// ** Without Chromakey **
	Jmp_TextureZ,		// 16 Texture Z Solid
	Jmp_TextureZFlat,	// 17 Texture Z Flat

	Jmp_TextureZGouraud,	// 18 Texture Z Gouraud
	Jmp_TextureZGouraud,	// 19 Texture Z Gouraud Dither

	// ** With Chromakey **
	Jmp_TextureZChromaKey,	// 20  Texture Z Solid Chromakey
	Jmp_TextureZFlatChromaKey,//21  Texture Z Flat Chromakey

	Jmp_TextureZGouraudChromaKey,// 22  Texture Z Gouraud Chromakey
	Jmp_TextureZGouraudChromaKey,// 23  Texture Z Gouraud Dither Chromakey

	Jmp_TextureZFogSmooth,	// 24 Texture Z Fog Smooth
	Jmp_FlagZBuffer,		// 25 Flag if object totally hidden
};

// ************************************************************************
// ************************************************************************
// ***                                                                  ***
// ***           JUMP TABLE (FOGGED  POLYGONS)                          ***
// ***                                                                  ***
// ************************************************************************
// ************************************************************************
Fill_Jump_Fn Fill_Fog_Table_Jumps[] = {
	// **** FLAT + SPECIAUX ****
	Jmp_SolidFog,		// 0 Solid
	Jmp_SolidFog,		// 1 Flat
	Jmp_Transparent,		// 2 Transparent
	Jmp_TrameFog,		// 3 Trame

	// **** GOURAUD ****
	Jmp_GouraudFog,		// 4 Gouraud
	Jmp_DitherFog,		// 5 Gouraud Dither
	Jmp_GouraudTable,	// 6 Gouraud table
	Jmp_DitherTable,		// 7 Gouraud Dither table

	// **** MAPPING ****
	// ** Without Chromakey **
	Jmp_TextureFog,		// 8 Texture Solid
	Jmp_TextureFlat,		// 9 Texture Flat

	Jmp_GouraudTexture,	// 10 Texture Gouraud
	Jmp_DitherTexture,	// 11 Texture Gouraud Dither

	// ** With Chromakey **
	Jmp_TextureChromaKeyFog,	// 12 Texture Solid Chromakey
	Jmp_TextureFlatChromaKey,// 13 Texture Flat Chromakey

	Jmp_GouraudTextureChromaKey,// 14 Texture Gouraud Chromakey
	Jmp_DitherTextureChromaKey,// 15 Texture Gouraud Dither Chromakey


	// **** MAPPING + PERSPECTIVE CORRECTED ****
	// ** Without Chromakey **
	Jmp_TextureZFog,		// 16 Texture Z Solid
	Jmp_TextureZFlat,	// 17 Texture Z Flat

	Jmp_TextureZGouraud,	// 18 Texture Z Gouraud
	Jmp_TextureZGouraud,	// 19 Texture Z Gouraud Dither

	// ** With Chromakey **
	Jmp_TextureZChromaKeyFog,// 20  Texture Z Solid Chromakey
	Jmp_TextureZFlatChromaKey,//21  Texture Z Flat Chromakey

	Jmp_TextureZGouraudChromaKey,// 22  Texture Z Gouraud Chromakey
	Jmp_TextureZGouraudChromaKey,// 23  Texture Z Gouraud Dither Chromakey

	Jmp_TextureZFogSmooth,	// 24 Texture Z Fog Smooth
	Jmp_FlagZBuffer,		// 25 Flag if object totally hidden
};

// ************************************************************************
// ************************************************************************
// ***                                                                  ***
// ***           JUMP TABLE (ZBUFFERED POLYGONS)                        ***
// ***                                                                  ***
// ************************************************************************
// ************************************************************************
Fill_Jump_Fn Fill_ZBuf_Table_Jumps[] = {
	// **** FLAT + SPECIAUX ****
	Jmp_SolidZBuf,		// 0 Solid
	Jmp_SolidZBuf,		// 1 Flat
	Jmp_TransparentZBuf,	// 2 Transparent
	Jmp_TrameZBuf,		// 3 Trame

	// **** GOURAUD ****
	Jmp_GouraudZBuf,		// 4 Gouraud
	Jmp_DitherZBuf,		// 5 Gouraud Dither
	Jmp_GouraudTableZBuf,	// 6 Gouraud table
	Jmp_DitherTableZBuf,	// 7 Gouraud Dither table

	// **** MAPPING ****
	// ** Without Chromakey **
	Jmp_TextureZBuf,		// 8 Texture Solid
	Jmp_TextureFlatZBuf,	// 9 Texture Flat

	Jmp_GouraudTextureZBuf,	// 10 Texture Gouraud
	Jmp_GouraudTextureZBuf,	// 11 Texture Gouraud Dither

	// ** With Chromakey **
	Jmp_TextureChromaKeyZBuf,// 12 Texture Solid Chromakey
	Jmp_TextureFlatChromaKeyZBuf,// 13 Texture Flat Chromakey

	Jmp_GouraudTextureChromaKeyZBuf,// 14 Texture Gouraud Chromakey
	Jmp_GouraudTextureChromaKeyZBuf,// 15 Texture Gouraud Dither Chromakey


	// **** MAPPING + PERSPECTIVE CORRECTED ****
	// ** Without Chromakey **
	Jmp_TextureZZBuf,	// 16 Texture Z Solid
	Jmp_TextureZFlatZBuf,	// 17 Texture Z Flat

	Jmp_TextureZGouraudZBuf,	// 18 Texture Z Gouraud
	Jmp_TextureZGouraudZBuf,	// 19 Texture Z Gouraud Dither

	// ** With Chromakey **
	Jmp_TextureZChromaKeyZBuf,// 20  Texture Z Solid Chromakey
	Jmp_TextureZFlatChromaKeyZBuf,//21  Texture Z Flat Chromakey

	Jmp_TextureZGouraudChromaKeyZBuf,// 22  Texture Z Gouraud Chromakey
	Jmp_TextureZGouraudChromaKeyZBuf,// 23  Texture Z Gouraud Dither Chromakey

	Jmp_TextureZFogSmoothZBuf,// 24 Texture Z Fog Smooth
	Jmp_FlagZBuffer,		// 25 Flag if object totally hidden
};

// ************************************************************************
// ************************************************************************
// ***                                                                  ***
// ***           JUMP TABLE (ZBUFFERED POLYGONS)                        ***
// ***                                                                  ***
// ************************************************************************
// ************************************************************************
Fill_Jump_Fn Fill_FogZBuf_Table_Jumps[] = {
	// **** FLAT + SPECIAUX ****
	Jmp_SolidFogZBuf,	// 0 Solid
	Jmp_SolidFogZBuf,	// 1 Flat
	Jmp_TransparentZBuf,	// 2 Transparent
	Jmp_TrameFogZBuf,	// 3 Trame

	// **** GOURAUD ****
	Jmp_GouraudFogZBuf,	// 4 Gouraud
	Jmp_DitherFogZBuf,	// 5 Gouraud Dither
	Jmp_GouraudTableZBuf,	// 6 Gouraud table
	Jmp_DitherTableZBuf,	// 7 Gouraud Dither table

	// **** MAPPING ****
	// ** Without Chromakey **
	Jmp_TextureFogZBuf,	// 8 Texture Solid
	Jmp_TextureFlatZBuf,	// 9 Texture Flat

	Jmp_GouraudTextureZBuf,	// 10 Texture Gouraud
	Jmp_GouraudTextureZBuf,	// 11 Texture Gouraud Dither

	// ** With Chromakey **
	Jmp_TextureChromaKeyFogZBuf,// 12 Texture Solid Chromakey
	Jmp_TextureFlatChromaKeyZBuf,// 13 Texture Flat Chromakey

	Jmp_GouraudTextureChromaKeyZBuf,// 14 Texture Gouraud Chromakey
	Jmp_GouraudTextureChromaKeyZBuf,// 15 Texture Gouraud Dither Chromakey


	// **** MAPPING + PERSPECTIVE CORRECTED ****
	// ** Without Chromakey **
	Jmp_TextureZFogZBuf,	// 16 Texture Z Solid
	Jmp_TextureZFlatZBuf,	// 17 Texture Z Flat

	Jmp_TextureZGouraudZBuf,	// 18 Texture Z Gouraud
	Jmp_TextureZGouraudZBuf,	// 19 Texture Z Gouraud Dither

	// ** With Chromakey **
	Jmp_TextureZChromaKeyFogZBuf,// 20  Texture Z Solid Chromakey
	Jmp_TextureZFlatChromaKeyZBuf,//21  Texture Z Flat Chromakey

	Jmp_TextureZGouraudChromaKeyZBuf,// 22  Texture Z Gouraud Chromakey
	Jmp_TextureZGouraudChromaKeyZBuf,// 23  Texture Z Gouraud Dither Chromakey

	Jmp_TextureZFogSmoothZBuf,// 24 Texture Z Fog Smooth
	Jmp_FlagZBuffer,		// 25 Flag if object totally hidden
};


// ************************************************************************
// ************************************************************************
// ***                                                                  ***
// ***           JUMP TABLE (ZBUFFERED without ZBuf update POLYGONS)    ***
// ***                                                                  ***
// ************************************************************************
// ************************************************************************
Fill_Jump_Fn Fill_NZW_Table_Jumps[] = {
	// **** FLAT + SPECIAUX ****
	Jmp_SolidNZW,		// 0 Solid
	Jmp_SolidNZW,		// 1 Flat
	Jmp_TransparentNZW,	// 2 Transparent
	Jmp_TrameNZW,		// 3 Trame

	// **** GOURAUD ****
	Jmp_GouraudNZW,		// 4 Gouraud
	Jmp_DitherNZW,		// 5 Gouraud Dither
	Jmp_GouraudTableNZW,	// 6 Gouraud table
	Jmp_DitherTableNZW,	// 7 Gouraud Dither table

	// **** MAPPING ****
	// ** Without Chromakey **
	Jmp_TextureNZW,		// 8 Texture Solid
	Jmp_TextureFlatNZW,	// 9 Texture Flat

	Jmp_GouraudTextureNZW,	// 10 Texture Gouraud
	Jmp_GouraudTextureNZW,	// 11 Texture Gouraud Dither

	// ** With Chromakey **
	Jmp_TextureChromaKeyNZW,// 12 Texture Solid Chromakey
	Jmp_TextureFlatChromaKeyNZW,// 13 Texture Flat Chromakey

	Jmp_GouraudTextureChromaKeyNZW,// 14 Texture Gouraud Chromakey
	Jmp_GouraudTextureChromaKeyNZW,// 15 Texture Gouraud Dither Chromakey


	// **** MAPPING + PERSPECTIVE CORRECTED ****
	// ** Without Chromakey **
	Jmp_TextureZNZW,	// 16 Texture Z Solid
	Jmp_TextureZFlatNZW,	// 17 Texture Z Flat

	Jmp_TextureZGouraudNZW,	// 18 Texture Z Gouraud
	Jmp_TextureZGouraudNZW,	// 19 Texture Z Gouraud Dither

	// ** With Chromakey **
	Jmp_TextureZChromaKeyNZW,// 20  Texture Z Solid Chromakey
	Jmp_TextureZFlatChromaKeyNZW,//21  Texture Z Flat Chromakey

	Jmp_TextureZGouraudChromaKeyNZW,// 22  Texture Z Gouraud Chromakey
	Jmp_TextureZGouraudChromaKeyNZW,// 23  Texture Z Gouraud Dither Chromakey

	Jmp_TextureZFogSmoothNZW,// 24 Texture Z Fog Smooth
	Jmp_FlagZBuffer,		// 25 Flag if object totally hidden
};

// ************************************************************************
// ************************************************************************
// ***                                                                  ***
// ***           JUMP TABLE (ZBUFFERED without ZBuf update POLYGONS)    ***
// ***                                                                  ***
// ************************************************************************
// ************************************************************************
Fill_Jump_Fn Fill_FogNZW_Table_Jumps[] = {
	// **** FLAT + SPECIAUX ****
	Jmp_SolidFogNZW,		// 0 Solid
	Jmp_SolidFogNZW,		// 1 Flat
	Jmp_TransparentNZW,	// 2 Transparent
	Jmp_TrameFogNZW,		// 3 Trame

	// **** GOURAUD ****
	Jmp_GouraudFogNZW,	// 4 Gouraud
	Jmp_DitherFogNZW,	// 5 Gouraud Dither
	Jmp_GouraudTableNZW,	// 6 Gouraud table
	Jmp_DitherTableNZW,	// 7 Gouraud Dither table

	// **** MAPPING ****
	// ** Without Chromakey **
	Jmp_TextureFogNZW,	// 8 Texture Solid
	Jmp_TextureFlatNZW,	// 9 Texture Flat

	Jmp_GouraudTextureNZW,	// 10 Texture Gouraud
	Jmp_GouraudTextureNZW,	// 11 Texture Gouraud Dither

	// ** With Chromakey **
	Jmp_TextureChromaKeyFogNZW,// 12 Texture Solid Chromakey
	Jmp_TextureFlatChromaKeyNZW,// 13 Texture Flat Chromakey

	Jmp_GouraudTextureChromaKeyNZW,// 14 Texture Gouraud Chromakey
	Jmp_GouraudTextureChromaKeyNZW,// 15 Texture Gouraud Dither Chromakey


	// **** MAPPING + PERSPECTIVE CORRECTED ****
	// ** Without Chromakey **
	Jmp_TextureZFogNZW,	// 16 Texture Z Solid
	Jmp_TextureZFlatNZW,	// 17 Texture Z Flat

	Jmp_TextureZGouraudNZW,	// 18 Texture Z Gouraud
	Jmp_TextureZGouraudNZW,	// 19 Texture Z Gouraud Dither

	// ** With Chromakey **
	Jmp_TextureZChromaKeyFogNZW,// 20  Texture Z Solid Chromakey
	Jmp_TextureZFlatChromaKeyNZW,//21  Texture Z Flat Chromakey

	Jmp_TextureZGouraudChromaKeyNZW,// 22  Texture Z Gouraud Chromakey
	Jmp_TextureZGouraudChromaKeyNZW,// 23  Texture Z Gouraud Dither Chromakey

	Jmp_TextureZFogSmoothNZW,	// 24 Texture Z Fog Smooth
	Jmp_FlagZBuffer,		// 25 Flag if object totally hidden
};
