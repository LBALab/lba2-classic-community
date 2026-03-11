#include <3D/REGLE3.H>

/*
 * Linear interpolation: Val1 + (Val2-Val1) * Step / NbSteps
 *
 * The ASM uses imul (32×32→64 signed) then idiv (64/32→32).
 * We match this with S64 intermediate to avoid 32-bit overflow.
 */
S32 RegleTrois(S32 Val1, S32 Val2, S32 NbSteps, S32 CurrentStep)
{
	if (NbSteps <= 0)
		return Val2;

	return (S32)((S64)(Val2 - Val1) * CurrentStep / NbSteps) + Val1;
}

S32 BoundRegleTrois(S32 Val1, S32 Val2, S32 NbSteps, S32 CurrentStep)
{
	if (CurrentStep <= 0)
		return Val1;

	if (CurrentStep >= NbSteps)
		return Val2;

	return (S32)((S64)(Val2 - Val1) * CurrentStep / NbSteps) + Val1;
}
