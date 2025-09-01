#ifndef	ANIMTEX_H
#define	ANIMTEX_H

/*--------------------------------------------------------------------------*/
//				fonctions ASM

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

/*--------------------------------------------------------------------------*/
//				fonctions C (ANIMTEX.CPP)

extern	S32     	NbTexEffects ;

/*--------------------------------------------------------------------------*/
extern	void	ScanTextureAnimation( U8 *ptrtex, S32 nbscanlines ) ;
/*--------------------------------------------------------------------------*/
extern	void	InitTextureAnimation( void ) ;
/*--------------------------------------------------------------------------*/
extern	void	DoTextureAnimation() ;
/*--------------------------------------------------------------------------*/
extern	void	InitPlasmaMenu() ;
/*--------------------------------------------------------------------------*/
extern	void	SelectPlasmaBank( U8 bank ) ;
/*--------------------------------------------------------------------------*/
extern	void	ClearPlasmaMenu( void ) ;
/*--------------------------------------------------------------------------*/
extern	void	FixePlasmaPtrMap( void ) ;
/*--------------------------------------------------------------------------*/

#endif	// ANIMTEX_H
