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
#include <POLYGON/POLYTZG.H>

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
	Fill_Filler = Filler_Transparent;
	Fill_ClipFlag = CLIP_FLAT;
	Fill_Color.Num = color & 0xF0;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_Trame(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_Trame;
	Fill_ClipFlag = CLIP_FLAT;
	Fill_Color.Num = color;
	Fill_Trame_Parity = 0;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
  Fill_Filler = Filler_Trame;
  Fill_ClipFlag = CLIP_FLAT;
  Fill_Color.Num = Fill_Logical_Palette[color & 0xFF];
  Fill_Trame_Parity = 0;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_TrameZBuf;
	Fill_ClipFlag = CLIP_FLAT + CLIP_ZBUFFER;
	Fill_Color.Num = color;
	Fill_Trame_Parity = 0;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_TrameZBuf;
	Fill_Trame_Parity = 0;
	Fill_ClipFlag = CLIP_FLAT + CLIP_ZBUFFER;
	Fill_Color.Num = Fill_Logical_Palette[color];
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_SolidNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_FlatNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_ZBUFFER;
	Fill_Color.Num = color | (color << 8);
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TransparentNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TransparentNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_ZBUFFER;
	Fill_Color.Num = color & 0xF0;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TrameNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TrameNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_ZBUFFER;
	Fill_Color.Num = color;
	Fill_Trame_Parity = 0;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_TrameNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_ZBUFFER;
	Fill_Color.Num = Fill_Logical_Palette[color];
	Fill_Trame_Parity = 0;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_GouraudTable;
	Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT;
	Fill_Color.Ptr = PtrCLUTGouraud + color;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}




S32 Jmp_DitherTable(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_DitherTable;
	Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT;
	Fill_Color.Ptr = PtrCLUTGouraud + color;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_DitherZBuf;
	Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT + CLIP_ZBUFFER;
	Fill_Color.Num = color << 16;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}




S32 Jmp_GouraudTableZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_GouraudTableZBuf;
	Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT + CLIP_ZBUFFER;
	Fill_Color.Ptr = PtrCLUTGouraud + color;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_GouraudNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT + CLIP_ZBUFFER;
	Fill_Color.Num = color << 16;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}


S32 Jmp_DitherNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_DitherNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT + CLIP_ZBUFFER;
	Fill_Color.Num = color << 16;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_GouraudTableNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_GouraudTableNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT + CLIP_ZBUFFER;
	Fill_Color.Ptr = PtrCLUTGouraud + color;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_DitherTableNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_DitherTableNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_LIGHT + CLIP_ZBUFFER;
	Fill_Color.Ptr = PtrCLUTGouraud + color;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
  Fill_Filler = Filler_TextureFlat;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTURE;
  Fill_Color.Num = color << 8;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}


S32 Jmp_TextureChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureChromaKey;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}



S32 Jmp_TextureFlatChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureFlatChromaKey;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE;
	Fill_Color.Num = color << 8;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}




S32 Jmp_TextureFog(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureFog;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}



S32 Jmp_TextureChromaKeyFog(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureChromaKeyFog;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_TextureNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE + CLIP_ZBUFFER;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
}



S32 Jmp_TextureChromaKeyNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureChromaKeyNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE + CLIP_ZBUFFER;
	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_TextureFogNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureChromaKeyFogNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureChromaKeyFogNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_TextureGouraudChromaKey;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE + CLIP_LIGHT;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}


S32 Jmp_DitherTextureChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureDitherChromaKey;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTURE + CLIP_LIGHT;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_TextureZ;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}


S32 Jmp_TextureZFlat(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZFlat;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ;
	Fill_Color.Ptr = PtrCLUTGouraud + (color << 8);

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}




S32 Jmp_TextureZChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZChromaKey;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}



S32 Jmp_TextureZFlatChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZFlatChromaKey;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ;
	Fill_Color.Ptr = PtrCLUTGouraud + (color << 8);

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}



S32 Jmp_TextureZFog(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZFog;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}




S32 Jmp_TextureZChromaKeyFog(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZChromaKeyFog;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}




S32 Jmp_TextureZZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZZBuf;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_TextureZChromaKeyZBuf;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_TextureZChromaKeyFogZBuf;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureZNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_TextureZChromaKeyNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
	Fill_Filler = Filler_TextureZChromaKeyFogNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}

// ****************************************************************
// ***  TEXTURE with PERSPECTIVE CORRECTION and GOURAUD SHADING ***
// ****************************************************************

S32 Jmp_TextureZGouraud(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZGouraud;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_LIGHT;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}


S32 Jmp_TextureZGouraudChromaKey(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZGouraudChromaKey;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_LIGHT;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}




S32 Jmp_TextureZGouraudZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZGouraudZBuf;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_LIGHT + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}



S32 Jmp_TextureZGouraudChromaKeyZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZGouraudChromaKeyZBuf;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_LIGHT + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}



S32 Jmp_TextureZGouraudNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZGouraudNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_LIGHT + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}



S32 Jmp_TextureZGouraudChromaKeyNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZGouraudChromaKeyNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_LIGHT + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}

// ************************************************
// ***  TEXTURE FOG with PERSPECTIVE CORRECTION ***
// ************************************************

S32 Jmp_TextureZFogSmooth(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZFogSmooth;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureZFogSmoothZBuf(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
  Fill_Filler = Filler_TextureZFogSmoothZBuf;
  Fill_ClipFlag = CLIP_FLAT+CLIP_TEXTUREZ+CLIP_ZBUFFER;

  return Fill_PolyClip(Nb_Points, Ptr_Points);
}

S32 Jmp_TextureZFogSmoothNZW(S32 Nb_Points, Struc_Point *Ptr_Points, U16 color)
{
	Fill_Filler = Filler_TextureZFogSmoothNZW;
	Fill_ClipFlag = CLIP_FLAT + CLIP_TEXTUREZ + CLIP_ZBUFFER;

	return Fill_PolyClip(Nb_Points, Ptr_Points);
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
