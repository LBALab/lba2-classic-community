//--------------------------------------------------------------------------
#ifndef	LIB_SVGA_INITMODE
#define	LIB_SVGA_INITMODE

//--------------------------------------------------------------------------
#ifdef	__cplusplus
extern	"C"	{
#endif

//--------------------------------------------------------------------------
#define	MAX_SCREEN_Y_RES	1024

//--------------------------------------------------------------------------
extern	U32	ModeDesiredX		;
extern	U32	ModeDesiredY		;
extern	U32	ModeDesiredDepth	;
extern	U32	ModeDesiredSize		;
extern	U32	ModePages		;

//--------------------------------------------------------------------------
extern	void 	*PhysHard		;// Hardware location (for info only)

#pragma	aux	PhysHard	"*"

//--------------------------------------------------------------------------
extern	void	*MemoLog		;

#pragma	aux	MemoLog		"*"

//--------------------------------------------------------------------------
extern	void	*MemoScreen		;

#pragma	aux	MemoScreen	"*"

//--------------------------------------------------------------------------
extern	S32	ParamsSvga()		;

//--------------------------------------------------------------------------
extern	S32	InitGraphSvga(U32 resx, U32 resy, U32 depth)	;

//--------------------------------------------------------------------------
extern	void	ClearGraphSvga()				;

//--------------------------------------------------------------------------
#ifdef	__cplusplus
}
#endif

//--------------------------------------------------------------------------
#endif//LIB_SVGA_INITMODE

//--------------------------------------------------------------------------

