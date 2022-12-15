//---------------------------------------------------------------------------
#include	<SYSTEM/ADELINE.H>
#include	<SYSTEM/INITIMER.H>
#include	<3D/MOVE.H>

//---------------------------------------------------------------------------
void	ChangeSpeedMove(MOVE *pmove, S32 speed)
{
	U32	timer, delta	;

	timer = TimerRefHR	;

	delta = timer-pmove->LastTimer ;

	if(delta|pmove->Speed)
	{
		pmove->Acc += (timer-pmove->LastTimer)*pmove->Speed	;
	 	pmove->LastTimer = timer				;
	}

	pmove->Speed = speed	;
}

//---------------------------------------------------------------------------
void	InitMove(MOVE *pmove, S32 speed)
{
	RestartMove(pmove)	;
	pmove->Speed = speed	;
}

//---------------------------------------------------------------------------
void	ChangeSpeedAngleMove(MOVE *pmove, S32 speed, S32 start, S32 end)
{
	ChangeSpeedMove(pmove, AdjustSpeedAngle(speed, start, end)) ;
}

//---------------------------------------------------------------------------
void	InitAngleMove(MOVE *pmove, S32 speed, S32 start, S32 end)
{
	InitMove(pmove, AdjustSpeedAngle(speed, start, end))	;
}

//---------------------------------------------------------------------------
void	ChangeSpeedBoundAngleMove(BOUND_MOVE *pbmove, S32 speed, S32 end)
{
	ChangeSpeedMove(&(pbmove->Move), AdjustSpeedAngle(speed, pbmove->Cur, end))	;
	pbmove->End = end&4095	;
}

//---------------------------------------------------------------------------
void	InitBoundAngleMove(BOUND_MOVE *pbmove, S32 speed, S32 start, S32 end)
{
	InitAngleMove(&(pbmove->Move), speed, start, end)	;
	pbmove->Cur = start&4095;
	pbmove->End = end&4095	;
}

//---------------------------------------------------------------------------
void	InitBoundMove(BOUND_MOVE *pbmove, S32 speed, S32 start, S32 end)
{
	if(start==end)
	{
		speed = 0	;
	}
	else if(start>end)
	{
		speed = -speed	;
	}

	InitMove(&(pbmove->Move), speed);

	pbmove->Cur = start	;
	pbmove->End = end	;
}

//---------------------------------------------------------------------------
S32	GetDeltaMove(MOVE *pmove)
{
	U32	timer, delta	;

	timer = TimerRefHR	;

	delta = timer-pmove->LastTimer ;

	if(delta|pmove->Speed)
	{
		pmove->Acc += (timer-pmove->LastTimer)*pmove->Speed	;
	 	pmove->LastTimer = timer				;
		return GetDeltaAccMove(&(pmove->Acc))			;
	}
	else
	{
		return 0 ;
	}
}

//---------------------------------------------------------------------------
S32	GetBoundMove(BOUND_MOVE *pbmove)
{
	S32	temp, cur;

	temp = GetDeltaMove(&(pbmove->Move))	;
	cur  = pbmove->Cur	;

	if(temp)
	{
		S32	end	;

		end = pbmove->End	;

		cur += temp		;

		if(pbmove->Move.Speed>0)
		{
			if(cur >= end)
			{
				goto over	;
			}
		}
		else
		{
			if(cur <= end)
			{
over:				cur = end		;
				pbmove->Move.Speed = 0	;
			}
		}

		pbmove->Cur = cur	;
	}

	return cur	;
}

//---------------------------------------------------------------------------
S32	GetBoundAngleMove(BOUND_MOVE *pbmove)
{
	S32	temp, cur;

	temp = GetDeltaMove(&(pbmove->Move))	;
	cur  = pbmove->Cur	;

	if(temp)
	{
		S32	end	;

		end = pbmove->End	;

		if(pbmove->Move.Speed>0)
		{
			if(cur>end)
			{
				end += 4096	;
			}

			cur += temp	;

			if(cur >= end)
			{
				goto over ;
			}
		}
		else
		{
			if(cur<end)
			{
				end -= 4096	;
			}

			cur += temp	;

			if(cur <= end)
			{
over:				cur = end		;
				pbmove->Move.Speed = 0	;
			}
		}

		cur &= 4095		;
		pbmove->Cur = cur	;
	}

	return cur	;
}

//---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

S32 AdjustSpeedAngle(S32 speed, S32 start, S32 end) {
  S32 returnValue;

#ifdef __MSC_VER
  __asm {
		pusha
		mov	ebx, speed
		mov	ecx, start
		mov	edx, end

		xor	eax, eax
		and	ecx, 4095
		and	edx, 4095
		sub	edx, ecx
		jz	ok
		mov	eax, ebx
		jns	skip
		neg	edx
		neg	eax
	skip:	cmp	edx, 2048
		jl	ok
	chg:	neg	eax
	ok:
		mov returnValue, eax
		popa
  }
#elif __GNUC__
  __asm__ __volatile__("pusha\n\t"
                       "movl %1, %%ebx\n\t"
                       "movl %2, %%ecx\n\t"
                       "movl %3, %%edx\n\t"
                       "xorl %%eax, %%eax\n\t"
                       "andl $4095, %%ecx\n\t"
                       "andl $4095, %%edx\n\t"
                       "subl %%ecx, %%edx\n\t"
                       "jz ok%=\n\t"
                       "movl %%ebx, %%eax\n\t"
                       "jns skip%=\n\t"
                       "negl %%edx\n\t"
                       "negl %%eax\n\t"
             "skip%=:\n\t"
                       "cmpl $2048, %%edx\n\t"
                       "jl ok%=\n\t"
             "chg%=:\n\t"
                       "negl %%eax\n\t"
             "ok%=:\n\t"
                       "movl %%eax, %0\n\t"
                       "popa\n\t"
                       : "=m"(returnValue)
                       : "m"(speed), "m"(start), "m"(end)
                       : "%eax", "%ebx", "%ecx", "%edx");
#else
#error "Unsupported compiler!"
#endif

  return returnValue;
}

S32 GetDeltaAccMove(S32 *acc) {
  S32 returnValue;

#ifdef __MSC_VER
  __asm {
		pusha
		mov ebx, acc
		mov     ecx, 1000
		mov     eax, [ebx]
		cmp	eax, ecx
		jge	ok
		cmp	eax, -1000
		jg	skip
	ok:	mov     edx, eax
		sar     edx, 31
		idiv    ecx
		mov     [ebx], edx
		jmp	end
	skip:	xor	eax, eax
	end:
		mov returnValue, eax
		popa
  }
#elif __GNUC__
  __asm__ __volatile__("pusha\n\t"
                       "movl %1, %%ebx\n\t"
                       "movl $1000, %%ecx\n\t"
                       "movl (%%ebx), %%eax\n\t"
                       "cmpl %%ecx, %%eax\n\t"
                       "jge ok%=\n\t"
                       "cmpl $-1000, %%eax\n\t"
                       "jg skip%=\n\t"
             "ok%=:\n\t"
                       "movl %%eax, %%edx\n\t"
                       "sarl $31, %%edx\n\t"
                       "idivl %%ecx\n\t"
                       "movl %%edx, (%%ebx)\n\t"
                       "jmp end%=\n\t"
             "skip%=:\n\t"
                       "xorl %%eax, %%eax\n\t"
             "end%=:\n\t"
                       "movl %%eax, %0\n\t"
                       "popa\n\t"
                       : "=m"(returnValue)
                       : "m"(acc)
                       : "%eax", "%ebx", "%ecx", "%edx");
#else
#error "Unsupported compiler!"
#endif

  return returnValue;
}

#ifdef __cplusplus
}
#endif


//---------------------------------------------------------------------------
