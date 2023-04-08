/*

		.686p
		.XMM

		.MODEL	SMALL, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.DATA

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	X0			:DWORD
		EXTRN	C	Y0			:DWORD
		EXTRN	C	Z0			:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	MatrixLib2		:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	InitMatrixStd		:DWORD
		EXTRN	C	RotatePoint		:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	TimerRefHR		:DWORD

		ASSUME  DS:SEG TimerRefHR

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	TransFctAnim		:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		.CODE

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	C	ObjectSetInterDep

;*══════════════════════════════════════════════════════════════════════════*
*/
#include <ANIM/INTERDEP.H>

#include <stdlib.h>
#include <string.h>

#include <SYSTEM/TIMER.H>
#include <ANIM.H>
#include <3D/IMATSTD.H>
#include <3D/ROT3D.H>
#include <3D/CAMERA.H>

S32	ObjectSetInterDep(T_OBJ_3D *obj)
{
  int v31; // edx@13
  PTR_U32 v32; // esi@13
  int v33; // edx@13
  int v34; // eax@13
  int v35; // ecx@13
  PTR_U32 v36; // eax@14
  S16 *v37; // eax@15
  int v38; // edi@15
  int v39; // ebp@15
  int v40; // eax@15

	obj->Texture = obj->NextTexture;
	obj->Body = obj->NextBody;

	// Same time as before?
  if (TimerRefHR == obj->Time)
  {
    if (!(obj->Status & FLAG_BODY))
		{
      obj->Status = 0; // didn't do anything
      return obj->Status;
		}
  }
  else if ( TimerRefHR < obj->Time )
  {
    U32 delta = obj->Time - TimerRefHR;
    obj->NextTimer -= delta;
    obj->LastTimer -= delta;
		obj->Time = TimerRefHR;
		obj->Status = 0;
    return obj->Status; // didn't do anything
  }

	obj->Time = TimerRefHR; // Upgrade time
	T_PTR_NUM objAnim = obj->Anim;

	if (TransFctAnim != NULL)
	{
		objAnim.Ptr = TransFctAnim(objAnim.Num);
	}

  void *anim = objAnim.Ptr;
  S16 *frame = (S16 *)((U8 *)objAnim.Ptr + (U64)obj->NextOfsFrame); // esi

  U32 interpolator = 0; // ebp@6

  if (obj->Time >= obj->NextTimer)
  {
    interpolator = 0x10000;
		obj->Status = FLAG_FRAME + FLAG_CHANGE;
  }
  else
  {
		obj->Status = FLAG_CHANGE;
    U32 nbSteps = obj->NextTimer - obj->LastTimer;
    U32 deltaSteps = obj->Time - obj->NextTimer;
    interpolator = ((deltaSteps << 16) + ((nbSteps + 1) >> 1)) / nbSteps;
  }

  S32 newAlpha = obj->Alpha;
  S32 newBeta = obj->Beta;
  S32 newGamma = obj->Gamma;

  if (obj->Master & 1)
  {
    S32 alpha = frame[5]; // Alpha
    S32 alphaDelta = (S32)(interpolator * (alpha << 20 >> 20)) >> 16;
    obj->LastAnimStepAlpha = alphaDelta;
    newAlpha = (obj->Alpha + alphaDelta - obj->LastAnimStepAlpha) & 0xFFF;
    obj->Alpha = newAlpha;

    S32 beta = frame[6]; // Beta
    S32 betaDelta = (S32)(interpolator * (beta << 20 >> 20)) >> 16;
    obj->LastAnimStepBeta = betaDelta;
    newBeta = (obj->Beta + betaDelta - obj->LastAnimStepBeta) & 0xFFF;
    obj->Beta = newBeta;

    S32 gamma = frame[7]; // Gamma
    S32 gammaDelta = (S32)(interpolator * (gamma << 20 >> 20)) >> 16;
    obj->LastAnimStepGamma = gammaDelta;
    newGamma = (obj->Gamma + gammaDelta - obj->LastAnimStepGamma) & 0xFFF;
    obj->Gamma = newGamma;
  }

  InitMatrixStd(&MatrixLib2, newAlpha, newBeta, newGamma);

  S32 frameX = frame[1]; // X
  S32 frameY = frame[2]; // Y
  S32 frameZ = frame[3]; // Z

  S32 xDelta = (S32)(interpolator * frameX) >> 16;
  S32 yDelta = (S32)(interpolator * frameY) >> 16;
  S32 zDelta = (S32)(interpolator * frameZ) >> 16;

  S32 rotX = xDelta - obj->LastAnimStepX;
  S32 rotY = yDelta - obj->LastAnimStepY;
  S32 rotZ = zDelta - obj->LastAnimStepZ;

  obj->LastAnimStepX = xDelta;
  obj->LastAnimStepY = yDelta;
  obj->LastAnimStepZ = zDelta;
  obj->Interpolator = interpolator;

  RotatePoint(&MatrixLib2, rotX, rotY, rotZ);

  obj->X += X0;
  obj->Y += Y0;
  obj->Z += Z0;

  if (obj->Status == FLAG_CHANGE)
  {
    return obj->Status;
  }

  obj->Interpolator = 0;
  obj->LastAnimStepX = 0;
  obj->LastAnimStepY = 0;
  obj->LastAnimStepZ = 0;
  obj->LastAnimStepAlpha = 0;
  obj->LastAnimStepBeta = 0;
  obj->LastAnimStepGamma = 0;
  v31 = obj->NextFrame;
  v32 = obj->NextOfsFrame;
  obj->LastFrame = v31;
  obj->LastOfsFrame = v32;
  obj->LastOfsIsPtr = 0;
  v33 = v31 + 1;
  v34 = obj->NextNbGroups;
  obj->LastNbGroups = v34;
  obj->NbGroups = v34;
  v35 = 2 * v34 - 2;

  if ( v33 >= obj->NbFrames )
  {
    v33 = obj->LoopFrame;
    v36 = (PTR_U32)obj->LoopOfsFrame;
    obj->Status = FLAG_FRAME + FLAG_LAST_FRAME + FLAG_CHANGE;
  }
  else
  {
    v36 = v32 + 4 * v35 + 16;
  }

  obj->NextOfsFrame = v36;
  v37 = (S16 *)((U64)anim + (U64)v36);
  v38 = obj->Time;
  v39 = v37[4];
  v40 = v37[0];
  obj->Master = v39;
  obj->NextFrame = v33;
  obj->LastTimer = v38;
  obj->NextTimer = v38 + v40;
  memcpy(obj->CurrentFrame, (const void *)((U64)anim + (U64)v32 + 16), 4 * v35);

  return obj->Status;
}