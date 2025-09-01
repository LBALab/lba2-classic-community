/*

		.386p

		.MODEL	SMALL, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.CODE

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	C	ObjectInitTexture

;*══════════════════════════════════════════════════════════════════════════*
;void	ObjectInitTexture(T_OBJ *obj, void *texture)

;#pragma aux ObjectInitTexture		\
;	parm		[ebx] [eax]	\
;	modify exact 	[]

ObjectInitTexture proc

		mov	[ebx].OBJ_3D.NextTexture, eax
		cmp	[ebx].OBJ_3D.Texture, -1
		jne	oktexture

		mov	[ebx].OBJ_3D.Texture, eax
oktexture:
		ret

ObjectInitTexture endp

;*══════════════════════════════════════════════════════════════════════════*
; 		The
		END
*/

#include <ANIM/TEXTURE.H>
#include <stdio.h>

void ObjectInitTexture(T_OBJ_3D *obj, void *texture)
{
  // Always set NextTexture to the provided texture
  obj->NextTexture = texture;
  
  // If Texture is uninitialized (-1), set it to the provided texture
  if (obj->Texture == (void*)-1)
  {
    obj->Texture = texture;
  }
}
