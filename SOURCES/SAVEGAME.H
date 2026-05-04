#ifndef SAVEGAME_H
#define SAVEGAME_H

extern U8 *PtrSave;

#ifndef EDITLBA2
/* Issue #62: bounded reads — set before LoadContexte / load tail; clear after. */
void SaveLoadSetReadLimit(U8 *end_exclusive);
void SaveLoadClearReadLimit(void);
#endif

#ifdef DEMO
#define NB_DEMOSAVE 3
extern S32 CurrentDemoSave;
extern S32 NewDemoSave;
extern char *ListDemoSave[NB_DEMOSAVE];

/*--------------------------------------------------------------------------*/
extern S32 SetDemoSaveGame(S32 num);
/*--------------------------------------------------------------------------*/
#endif

/*--------------------------------------------------------------------------*/
extern void AutoSaveGame(void);
/*--------------------------------------------------------------------------*/
extern void CurrentSaveGame(void);
/*--------------------------------------------------------------------------*/
extern void SaveGame(S32 flagmess);
/*--------------------------------------------------------------------------*/
extern void LoadGameNumCube(void);
/*--------------------------------------------------------------------------*/
extern void LoadGamePlayerName(void);
/*--------------------------------------------------------------------------*/
extern U8 *LoadGameScreen(void);
/*--------------------------------------------------------------------------*/
/* LoadGame(): LoadContexte hit read/bounds failure — partial ListObjet; do not InitLoadedGame. */
#define LOADGAME_ERR_CONTEXT ((S32)(-2))
extern S32 LoadGame(void);
/*--------------------------------------------------------------------------*/
extern S32 LoadGameOldVersion(void);
/*--------------------------------------------------------------------------*/
extern void SaveContexte(S32 savetimerrefhr);
/*--------------------------------------------------------------------------*/
extern S32 LoadContexte(S32 *savetimerrefhr);
/*--------------------------------------------------------------------------*/

#endif // SAVEGAME_H
