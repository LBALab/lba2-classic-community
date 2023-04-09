#include <ANIM/BODY.H>
#include <ANIM.H>

void	ObjectInitBody(T_OBJ_3D *obj, void *body)
{
	obj->Status |= FLAG_BODY;

	// TODO: this mix of Ptr and Num is confusing and might not work on 64bit systems
	obj->NextBody.Ptr = body;
	if (obj->Body.Num == -1)
	{
		obj->Body.Ptr = body;
	}
}
