//----------------------------------------------------------------------------
#include	<system\adeline.h>
#include	<system\dossys.h>

//----------------------------------------------------------------------------
VOID_FUNC	*ManageProc	;

//----------------------------------------------------------------------------
void	ManageEvents()
{
	if(ManageProc)
	{
		ManageProc()	;
	}
}

//----------------------------------------------------------------------------
