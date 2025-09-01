#include <ANIM/INTANIM.H>

#include <ANIM.H>
#include <ANIM/INTFRAME.H>
#include <ANIM/INTERDEP.H>

S32	ObjectSetInterAnim(T_OBJ_3D *obj)
{
  S32 status = ObjectSetInterDep(obj);
  if ((status & FLAG_CHANGE) // any change?
      && !(status & FLAG_FRAME)) // frame ?
  {
    ObjectSetInterFrame(obj);
  }

  return status;
}
