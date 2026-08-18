#ifndef PERSO_H
#define PERSO_H

#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
/*--------------------------------------------------------------------------*/
extern void AffDebugMenu(void);
/*--------------------------------------------------------------------------*/
#endif // DEBUG_TOOLS

/*--------------------------------------------------------------------------*/
extern void CheckSavePcx(void);
/*--------------------------------------------------------------------------*/
extern void CheckSaveLogPcx(U8 *pal);
/*--------------------------------------------------------------------------*/
extern void LoadListAnim3DS(void);
/*--------------------------------------------------------------------------*/
extern void InitGameLists(void);
/*--------------------------------------------------------------------------*/
extern void InitGame(int argc);
/*--------------------------------------------------------------------------*/
extern void Introduction(void);
/*--------------------------------------------------------------------------*/
extern void GamePaused(U32 flags);
/*--------------------------------------------------------------------------*/
#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
/* Memory at start and the frame-rate tally, both written by the main loop and
   read by the exit report. Declared because that report is a separate module
   now; without this a DEBUG_TOOLS build does not link, and no CI job sets the
   macro, so nothing would have said so. */
extern U32 MemoMemory;
extern U32 MemEnd;
extern U32 MemoDosMemory;
extern U32 MemoMinDosMemory;
extern S32 MaxNbf;
extern S32 NbNbf;
extern S32 TotalNbf;
/*--------------------------------------------------------------------------*/
#endif
extern S32 MainLoop(void);
/*--------------------------------------------------------------------------*/
extern void InitProgram();
/*--------------------------------------------------------------------------*/
extern void Message(const char *mess, S32 flag);
/*--------------------------------------------------------------------------*/
extern void WriteConfigFile(void);
/*--------------------------------------------------------------------------*/
/* Persist one key into the config this run owns, returning FALSE if it could
   not be stored.

   For the settings that are changed one at a time rather than saved together:
   the console verbs that persist a choice. A bare DefFileBufferWriteValue is
   refused while the game data's config is stacked under the player's, which is
   every install that ships one, so it is not the way to write a setting. */
extern S32 WriteConfigValue(const char *ident, S32 value);
/*--------------------------------------------------------------------------*/

#endif // PERSO_H
