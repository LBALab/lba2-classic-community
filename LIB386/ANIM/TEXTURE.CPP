#include <ANIM/TEXTURE.H>
#include <stdio.h>

void ObjectInitTexture(T_OBJ_3D *obj, void *texture)
{
    obj->NextTexture = texture;
    if (obj->Texture == (void *)-1)
    {
        obj->Texture = texture;
    }
}
