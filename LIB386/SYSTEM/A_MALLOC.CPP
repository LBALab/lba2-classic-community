//──────────────────────────────────────────────────────────────────────────
#include	<system\adeline.h>
#include	<system\dosmallo.h>
#include	<system\a_malloc.h>

#include	<malloc.h>

//──────────────────────────────────────────────────────────────────────────
void	*SmartMalloc(S32 size)
{
	void 	*addr ;

	addr = DosMalloc(size)		;
	if( !addr )
	{
		addr = Malloc(size)	;
	}

	return(addr)	;
}

//──────────────────────────────────────────────────────────────────────────
