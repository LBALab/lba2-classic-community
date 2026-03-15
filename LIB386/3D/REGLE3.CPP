#include <3D/REGLE3.H>
#include <SYSTEM/DEBUG_TRACES.H>

/*
 * Linear interpolation: Val1 + (Val2-Val1) * Step / NbSteps
 *
 * The ASM uses imul (32×32→64 signed) then idiv (64/32→32).
 * We match this with S64 intermediate to avoid 32-bit overflow.
 */
S32 RegleTrois(S32 Val1, S32 Val2, S32 NbSteps, S32 CurrentStep)
{
	LIB386_TRACE_CPP("RegleTrois", "1", "V1=%d V2=%d NbStep=%d Step=%d", Val1, Val2, NbSteps, CurrentStep);
	S32 result;
	if (NbSteps <= 0)
		result = Val2;
	else
		result = (S32)((S64)(Val2 - Val1) * CurrentStep / NbSteps) + Val1;
	LIB386_TRACE_CPP("RegleTrois", "2", "return=%d", result);
	return result;
}

S32 BoundRegleTrois(S32 Val1, S32 Val2, S32 NbSteps, S32 CurrentStep)
{
	LIB386_TRACE_CPP("BoundRegleTrois", "1", "V1=%d V2=%d NbStep=%d Step=%d", Val1, Val2, NbSteps, CurrentStep);
	S32 result;
	if (CurrentStep <= 0)
		result = Val1;
	else if (CurrentStep >= NbSteps)
		result = Val2;
	else
		result = (S32)((S64)(Val2 - Val1) * CurrentStep / NbSteps) + Val1;
	LIB386_TRACE_CPP("BoundRegleTrois", "2", "return=%d", result);
	return result;
}
