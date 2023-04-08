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
  S16 *v8; // esi@5
  int v12; // eax@7
  int v13; // ecx@8
  signed int v14; // eax@9
  int v15; // edi@9
  int v16; // ecx@9
  int v17; // edx@9
  int v18; // ecx@9
  int v19; // edx@9
  int v20; // ecx@9
  int result; // eax@9
  S16 v22; // dx@12
  int v23; // edx@12
  int v24; // edi@12
  int v25; // ecx@12
  int v26; // edx@12
  int v27; // eax@12
  int v28; // ecx@12
  int v29; // edx@12
  int v30; // ecx@12
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
	int a2; // edi

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
  v8 = (S16 *)((U64)obj->NextOfsFrame + (U64)objAnim.Ptr);

  unsigned int v11 = 0; // ebp@6

  if (obj->Time >= obj->NextTimer)
  {
    v11 = 0x10000;
		obj->Status = FLAG_FRAME + FLAG_CHANGE;
  }
  else
  {
		obj->Status = FLAG_CHANGE;
    U32 nbSteps = obj->NextTimer - obj->LastTimer;
    U32 deltaSteps = obj->Time - obj->NextTimer;
    v11 = ((deltaSteps << 16) + ((nbSteps + 1) >> 1)) / nbSteps;
  }
  v25 = obj->Alpha;
	v29 = obj->Beta;
  if (obj->Master & 1 )
  {
    v22 = v8[5]; // Alpha
    a2 = v8[6]; // Beta
    v12 = v8[7]; // Gamma
    v23 = (signed int)(v11 * (obj->Beta << 20 >> 20)) >> 16;
    v24 = (signed int)(v11 * (a2 << 20 >> 20)) >> 16;
    v25 = v23 + obj->Alpha - obj->LastAnimStepAlpha;
    obj->LastAnimStepAlpha = v23;
    v26 = obj->Beta;
    obj->Alpha = v25 & 0xFFF;
    v27 = (signed int)(v11 * (v12 << 20 >> 20)) >> 16;
    v28 = v27 + obj->Gamma;
    v29 = (v24 + v26 - obj->LastAnimStepBeta) & 0xFFF;
    obj->LastAnimStepBeta = v24;
    v30 = v28 - obj->LastAnimStepGamma;
    obj->LastAnimStepGamma = v27;
    v13 = v30 & 0xFFF;
    obj->Beta = v29;
    obj->Gamma = v13;
	}
	else
	{
		v13 = obj->Gamma;
  }
  InitMatrixStd(&MatrixLib2, v25, v29, v13);
  v14 = v11 * v8[3];
  v15 = (signed int)(v11 * v8[2]) >> 16;
  v16 = ((signed int)(v11 * v8[1]) >> 16) - obj->LastAnimStepX;
  obj->LastAnimStepX = (signed int)(v11 * v8[1]) >> 16;
  v14 >>= 16;
  v17 = v15 - obj->LastAnimStepY;
  v18 = v14 - obj->LastAnimStepZ;
  obj->LastAnimStepY = v15;
  obj->LastAnimStepZ = v14;
  obj->Interpolator = v11;
  RotatePoint(&MatrixLib2, v16, v17, v18);
  v19 = obj->Y + Y0;
  v20 = obj->Z + Z0;
  obj->X += X0;
  result = obj->Status;
  obj->Y = v19;
  obj->Z = v20;
  if ( result != 4 )
  {
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
    result = obj->Status;
    memcpy(obj->CurrentFrame, (const void *)((U64)anim + (U64)v32 + 16), 4 * v35);
  }
  return result;
}