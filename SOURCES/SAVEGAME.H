#ifndef SAVEGAME_H
#define SAVEGAME_H

extern U8 *PtrSave;

/* Layout of the object records LoadContexte reads. AUTO tries the legacy
 * 64-bit stride first and falls back to the 32-bit wire one; that guess is
 * unreliable when the context holds one or two objects, so a caller that knows
 * the layout passes it. SaveLoadLastAbi() reports what the last LoadContexte
 * read (WIRE32 or NATIVE). */
#define SAVELOAD_ABI_AUTO 0
#define SAVELOAD_ABI_WIRE32 1
#define SAVELOAD_ABI_NATIVE 2

#ifndef EDITLBA2
/* Issue #62: bounded reads — set before LoadContexte / load tail; clear after. */
void SaveLoadSetReadLimit(U8 *end_exclusive);
void SaveLoadClearReadLimit(void);

void SaveLoadSetAbi(S32 abi);
S32 SaveLoadLastAbi(void);
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
extern void Save_SetAutoDisabled(S32 disabled);
extern S32 Save_AutoDisabled(void);
extern void AutoSaveGame(void);
/*--------------------------------------------------------------------------*/
extern void CurrentSaveGame(void);
/*--------------------------------------------------------------------------*/
/* TRUE when the whole save reached the file. */
extern S32 SaveGame(S32 flagmess);
/*--------------------------------------------------------------------------*/
extern void LoadGameNumCube(void);
/*--------------------------------------------------------------------------*/
extern void LoadGamePlayerName(void);
/*--------------------------------------------------------------------------*/
extern U8 *LoadGameScreen(void);
/*--------------------------------------------------------------------------*/
/* LoadContexte() returns SAVELOAD_CTX_ERR on bounds/truncation failure (-1).
 * LoadGame() translates that to LOADGAME_ERR_CONTEXT (-2) for ChangeCube and
 * RestartValidePos so they can skip post-load steps that deref ListObjet. */
#define SAVELOAD_CTX_ERR ((S32)(-1))
#define LOADGAME_ERR_CONTEXT ((S32)(-2))
extern S32 LoadGame(void);
/*--------------------------------------------------------------------------*/
extern S32 LoadGameOldVersion(void);
/*--------------------------------------------------------------------------*/
extern void SaveContexte(S32 savetimerrefhr);
/*--------------------------------------------------------------------------*/
extern S32 LoadContexte(S32 *savetimerrefhr);
/*--------------------------------------------------------------------------*/

#ifndef EDITLBA2
/* Issue #62 host harness: --save-load-test <path> on lba2 sets these so
 * ChangeCube can emit SAVE_LOAD_TEST stage lines for the driver script. */
extern S32 SaveLoadTestMode;
#endif

#endif // SAVEGAME_H
