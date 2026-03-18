#include <ANIM/INTANIM.H>

#include <ANIM.H>
#include <ANIM/INTFRAME.H>
#include <ANIM/INTERDEP.H>
#include <SYSTEM/DEBUG_TRACES.H>

S32 ObjectSetInterAnim(T_OBJ_3D *obj)
{
    LIB386_TRACE_CPP("ObjectSetInterAnim", "1", "obj=%p", (void*)obj);
    S32 status = ObjectSetInterDep(obj);
    if ((status & FLAG_CHANGE)     // any change?
        && !(status & FLAG_FRAME)) // frame ?
    {
        ObjectSetInterFrame(obj);
    }

    return status;
}

