#include <ANIM/CLEAR.H>
#include <string.h>

void	ObjectClear(T_OBJ_3D *obj)
{
	memset(obj, 0, sizeof(T_OBJ_3D));

	obj->Body.Num = -1;
	obj->NextBody.Num = -1;
	obj->Texture = (void *)-1;
	obj->NextTexture = (void *)-1;
	obj->Anim.Num = -1;
}
