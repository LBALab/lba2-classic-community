#include <ANIM/STOFRAME.H>

#include <SYSTEM/TIMER.H>
#include <SYSTEM/DEBUG_TRACES.H>

void ObjectStoreFrame(T_OBJ_3D *Obj)
{
    LIB386_TRACE_CPP("ObjectStoreFrame", "1", "obj=%p", (void*)Obj);
    Obj->LastTimer = TimerRefHR;
    Obj->Time = TimerRefHR;

    Obj->LastNbGroups = Obj->NbGroups;
    Obj->LastOfsFrame = (PTR_U32)PtrLib3DBufferAnim;
    Obj->LastOfsIsPtr = 1;

    Obj->Status = 0;
    U32 *currentFrame = (U32 *)(Obj->CurrentFrame);
    U32 *dst = (U32 *)(PtrLib3DBufferAnim);
    dst[0] = dst[1] = dst[2] = dst[3] = 0; // initialize memory at edi to 0
    dst += 4;

    U32 count = Obj->NbGroups * 2 - 2;

    for (U32 i = 0; i < count; i++)
    {
        dst[i] = currentFrame[i];
    }
    dst += count;
    currentFrame += count;

    if (dst > (PTR_U32)EndLib3DBufferAnim)
    {
        dst = (PTR_U32)Lib3DBufferAnim;
    }

    PtrLib3DBufferAnim = (PTR_U8)dst;
    Obj->LastFrame = -1;
}
