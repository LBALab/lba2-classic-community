#include <3D/REGLE3.H>
#include <stdio.h>

S32 RegleTrois(S32 Val1, S32 Val2, S32 NbSteps, S32 CurrentStep)
{
	/*
RegleTrois	proc	\
		Valeur1:DWORD, Valeur2:DWORD, \
		NbStep:DWORD, Step:DWORD

		mov	ecx, NbStep
		mov	eax, Valeur2
		test	ecx, ecx
		jle	short erreur

		sub	eax, Valeur1
		imul	Step
		idiv	ecx
		add	eax, Valeur1

erreur: 	ret

RegleTrois	endp
	*/
	if (NbSteps <= 0) {
			return 0;
	}

	return ((Val2 - Val1) * CurrentStep) / NbSteps + Val1;
}

S32 BoundRegleTrois(S32 Val1, S32 Val2, S32 NbSteps, S32 CurrentStep)
{
	/*
BoundRegleTrois proc	\
		Valeur1:DWORD, Valeur2:DWORD, \
		NbStep:DWORD, Step:DWORD

		mov	edx, Step
		mov	eax, Valeur2
		mov	ecx, Valeur1

		test	edx, edx
		jle	troppetit

		cmp	edx, NbStep
		jge	tropgrand

		sub	eax, ecx
		imul	edx
		idiv	NbStep
		add	eax, ecx

tropgrand:	ret

troppetit:	mov	eax, ecx
		ret

BoundRegleTrois endp
	*/
	if (CurrentStep <= 0)
	{
		return Val1;
	}
	else if (CurrentStep >= NbSteps)
	{
		return Val2;
	}
	else
	{
		return ((Val2 - Val1) * CurrentStep / NbSteps) + Val1;
	}
}
