//****************************************************************************
#ifndef	LIB_SYSTEM_DEFFILE
#define	LIB_SYSTEM_DEFFILE

//****************************************************************************
#include	<SYSTEM/ITOA.H>

//****************************************************************************
#ifdef	__cplusplus
extern	"C"	{
#endif


//****************************************************************************
extern	char	*OrgPtrDef		;
extern	char	*EndPtrDef		;

//****************************************************************************
extern	S32	DefFileReadSkipComments	;

//****************************************************************************
S32	DefFileBufferInit(char *file, void *buffer, S32 maxsize)	;

//****************************************************************************
char	*DefFileBufferReadString(const char *ident)				;

//****************************************************************************
const char *DefFileBufferReadStringDefault(const char *ident, const char *def)  ;

//****************************************************************************
S32	DefFileBufferReadValue(const char *ident)				;

//****************************************************************************
S32	DefFileBufferReadValueDefault(const char *ident, S32 def)		;

//****************************************************************************
S32	DefFileBufferReadValue2(const char *ident, S32 *result)		;

//****************************************************************************
S32	DefFileBufferWriteString(const char *ident, const char *string)		;

//****************************************************************************
#define	DefFileBufferWriteValue(ident, value)			\
	DefFileBufferWriteString(ident, Itoa(value))

//****************************************************************************
#ifdef	__cplusplus
}
#endif

//****************************************************************************
#endif//LIB_SYSTEM_DEFFILE

//****************************************************************************

