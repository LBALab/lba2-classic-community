#include <ANIM/ANIM.H>
#include <ANIM.H>
#include <SYSTEM/TIMER.H>
#include <stdio.h>

void ObjectInitAnim(T_OBJ_3D *obj, void *anim)
{
	// TODO: Revisit this whole function, it's a mess
	if (obj->Anim.Ptr == anim)
	{
		return;
	}

	obj->Anim.Ptr = anim;

	if (TransFctAnim != NULL)
	{
		anim = TransFctAnim(obj->Anim.Num);
	}

	U16 *animPtr = (U16 *)anim;

	U16 animNbFrames = animPtr[0];
	U16 animNbGroups = animPtr[1];
	U16 animLoopFrame = animPtr[2];

	obj->Interpolator = 0;
	obj->Master = animPtr[8];
	obj->NbFrames = animNbFrames;
	obj->NextNbGroups = animNbGroups;
	obj->LoopFrame = animLoopFrame;

	obj->LastAnimStepX = 0;
	obj->LastAnimStepY = 0;
	obj->LastAnimStepZ = 0;

	obj->LoopOfsFrame = animLoopFrame * (obj->NbFrames + 1) * 8 + 8;

	obj->LastAnimStepAlpha = 0;
	obj->LastAnimStepBeta = 0;
	obj->LastAnimStepGamma = 0;

	obj->LastTimer = TimerRefHR;
	obj->Time = TimerRefHR;

	U32 somethingEBP = 0;

	if (obj->LastOfsFrame != 0)
	{
		obj->Status = 0;
		obj->NextOfsFrame = (PTR_U32)8;
		obj->NextFrame = 0;
		obj->LastNbGroups = animNbGroups;

		U32 targetNbGroups = 8;

		if (obj->LastNbGroups != targetNbGroups)
		{
			obj->NbGroups = animNbGroups;
			if (obj->LastNbGroups > targetNbGroups)
			{
				somethingEBP = targetNbGroups;
				animNbGroups = targetNbGroups;
			}
			else
			{
				targetNbGroups = animNbGroups;
			}
		}
	}
	else
	{
		obj->Status = FLAG_FRAME;
		obj->LastOfsFrame = (PTR_U32)8;
		obj->LastOfsIsPtr = 0;
		obj->LastFrame = 0;

		obj->LastNbGroups = animNbGroups;
		obj->NbGroups = animNbGroups;

		U32 *currentFrame = (U32 *)obj->CurrentFrame;
		U32 *dstAnim = (U32 *)animPtr + 4 + 2; // group 1?

		for (int i = 0; i < animNbGroups; i++)
		{
			currentFrame[i] = dstAnim[i];
		}
		currentFrame += animNbGroups;
		dstAnim += animNbGroups;

		if (obj->NbFrames != 1)
		{
			obj->NextOfsFrame = (PTR_U32)((U64)dstAnim - (U64)animPtr);
			obj->NextFrame = 1;
			obj->NextTimer = TimerRefHR + ((U16 *)dstAnim)[0];
			return;
		}
		else
		{
			obj->NextOfsFrame = dstAnim;
			obj->NextFrame = 0;
			obj->NextTimer = TimerRefHR + ((U16 *)dstAnim)[0];
			return;
		}
	}

	U16 animFrameDeltaTime = animPtr[4];
	obj->NextTimer = TimerRefHR + animFrameDeltaTime;
	obj->LastOfsFrame = (U32 *)PtrLib3DBufferAnim;
	obj->LastOfsIsPtr = 1;

	U32 *bufferAnim = (U32 *)PtrLib3DBufferAnim;
	bufferAnim[0] = 0;
	bufferAnim[1] = 0;
	bufferAnim[2] = 0;
	bufferAnim[3] = 0;

	obj->LastFrame = -1;

	bufferAnim += 4;
	U32 *currentFrame = (U32 *)obj->CurrentFrame;

	for (int i = 0; i < animNbGroups; i++)
	{
		bufferAnim[i] = currentFrame[i];
	}
	bufferAnim += animNbGroups;

	if (somethingEBP != 0)
	{
		if (TransFctAnim != NULL)
		{
			anim = TransFctAnim(obj->Anim.Num);
		}

		animNbGroups = obj->NbGroups - somethingEBP;
		U32 *nextOfsFrame = (U32 *)((U64)anim + (U64)obj->NextOfsFrame) + somethingEBP + 1;
		for (int i = 0; i < animNbGroups; i++)
		{
			bufferAnim[i] = nextOfsFrame[i];
		}

		bufferAnim += animNbGroups;
	}

	if ((U8 *)bufferAnim <= EndLib3DBufferAnim)
	{
		PtrLib3DBufferAnim = (U8 *)bufferAnim;
		return;
	}

	PtrLib3DBufferAnim = Lib3DBufferAnim;
}
