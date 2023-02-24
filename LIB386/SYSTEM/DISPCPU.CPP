//----------------------------------------------------------------------------
#include <SYSTEM/ADELINE.H>
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/CMDLINE.H>
#include <SYSTEM/DISPOS.H>
#include <SYSTEM/CPU.H>
#include <SYSTEM/DISPCPU.H>

#include <stdlib.h>
#include <string.h>

//----------------------------------------------------------------------------
static inline void DisplayCache(CACHE cache)
{
        LogPrintf(	"%d KB, "
	                "%d-way(s) set associative, "
	                "%d bytes lines.\n",
	                cache.Size,
	                cache.Ways,
			cache.LineSize)	;
}

//----------------------------------------------------------------------------
void	DisplayCPU()
{
	LogPrintf("CPU Manufacturer Identification String: %s ", ProcessorManufacturerIDString);
	switch(ProcessorSignature.Manufacturer)
	{
	case 0:
		LogPuts("(Unknown)\n")	;
		break	;
	case 1:
		LogPuts("(Intel)\n")	;
		break	;
	case 2:
		LogPuts("(Cyrix)\n")	;
		break	;
	case 3:
		LogPuts("(AMD)\n")	;
		break	;
	}

	LogPrintf("Family  : %d (", ProcessorSignature.Family);
	switch(ProcessorSignature.Family)
	{
	case 6:
		LogPrintf("Pentium Pro/Pentium II");
		break	;
	case 5:
		LogPrintf("Pentium")	;
		break	;
	case 4:
		LogPrintf("486")	;
		break	;
	case 3:
		LogPrintf("386")	;
		break	;
	default:
		LogPrintf("Unknown")	;
		break	;
	}
	LogPuts(" Class Processor)")	;

	LogPrintf("Model   : %d\n", 	ProcessorSignature.Model)	;
	LogPrintf("Stepping: %d\n\n", 	ProcessorSignature.Stepping)	;

	LogPrintf("FPU Instructions%s supported\n",	ProcessorSignature.FPU	  ? "" : " Not");
	LogPrintf("CPUID Instruction%s supported\n",	ProcessorSignature.CPUID  ? "" : " Not");
	LogPrintf("MMX Instructions%s supported\n\n", 	ProcessorFeatureFlags.MMX ? "" : " Not");

	LogPrintf("Level 1 ")		;
	if(*(U32*)&ProcessorL1InstructionCache)
	{
		LogPrintf("Data Cache       : ");
	}
	else
	{
		LogPrintf("Unified Cache    : ");
	}
	DisplayCache(ProcessorL1DataCache)	;

	if(*(U32*)&ProcessorL1InstructionCache)
	{
		LogPrintf("Level 1 Instruction Cache: ");
		DisplayCache(ProcessorL1InstructionCache);
	}

	if(*(U32*)&ProcessorL2Cache)
	{
		LogPrintf("Level 2 Unified Cache    : ");
		DisplayCache(ProcessorL2Cache)		;
	}

	LogPuts("");

	LogPrintf("Cache allocate on Read%s\n", ProcessorSignature.CacheAllocOnWrite ? " and Write" : "");
}

//----------------------------------------------------------------------------
S32	ParamsCPU()
{
	char	str[256];
	char	*ptr	;
	S32	result	;
	S32	i	;

	str[255] = 0	;
	result = FALSE	;

	i = 1	;
	while(i<ArgC)
	{
		strncpy(str, ArgV[i], 255)	;
		if(strncasecmp("/Cpu", str, 4))
		{
			i++		;
			continue	;
		}

		RemoveParam(i)	;

		ptr = str+4	;

		if(!strncasecmp("Manufacturer=", ptr, 12))
		{
			ProcessorSignature.Manufacturer= atoi(ptr+13)	;
			result = TRUE	;
		}
		else if(!strncasecmp("Family=", ptr, 7))
		{
			ProcessorSignature.Family = atoi(ptr+7)		;
			result = TRUE	;
		}
		else if(!strncasecmp("FPU=", ptr, 4))
		{
			ProcessorSignature.FPU = atoi(ptr+4) ? 1 : 0	;
			result = TRUE	;
		}
		else if(!strncasecmp("MMX=", ptr, 4))
		{
			ProcessorFeatureFlags.MMX = atoi(ptr+4) ? 1 : 0	;
			result = TRUE	;
		}
		else
		{
			LogPrintf("Unsupported CPU override: %s", str)	;
		}
	}

	return	result	;
}

//----------------------------------------------------------------------------
