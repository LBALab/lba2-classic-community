#ifndef	CHEATCOD_H
#define	CHEATCOD_H

extern	void	GereCheatCode( void ) ;

#ifdef CONSOLE_MODULE
/* Execute cheat by name (e.g. "life", "magic"). Returns 1 if matched and run, 0 otherwise. */
int	TryExecuteCheatByName( const char *name ) ;
#endif

#endif	// CHEATCOD_H
