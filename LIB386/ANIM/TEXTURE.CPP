#include <ANIM/TEXTURE.H>
#include <stdio.h>
#include <SYSTEM/DEBUG_TRACES.H>

void ObjectInitTexture(T_OBJ_3D *obj, void *texture)
{
    LIB386_TRACE_CPP("ObjectInitTexture", "1", "obj=%p tex=%p", (void*)obj, texture);
    obj->NextTexture = texture;
    if (obj->Texture == (void *)-1)
    {
        obj->Texture = texture;
    }
}
