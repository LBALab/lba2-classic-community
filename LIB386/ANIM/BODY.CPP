#include <ANIM/BODY.H>
#include <ANIM.H>
#include <SYSTEM/DEBUG_TRACES.H>

void ObjectInitBody(T_OBJ_3D *obj, void *body) {
    LIB386_TRACE_CPP("ObjectInitBody", "1", "obj=%p body=%p", (void *)obj, body);
    obj->Status |= FLAG_BODY;

    // TODO: this mix of Ptr and Num is confusing and might not work on 64bit systems
    obj->NextBody.Ptr = body;
    if (obj->Body.Num == -1) {
        obj->Body.Ptr = body;
    }
}
