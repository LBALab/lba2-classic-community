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
extern S32 MainLoop(void);
/*--------------------------------------------------------------------------*/
extern void InitProgram();
/*--------------------------------------------------------------------------*/
extern LBA_NORETURN void TheEndCheckFile(const char *filename);
/*--------------------------------------------------------------------------*/
extern void InitTheEnd(S32 num, char *error);
/*--------------------------------------------------------------------------*/
extern void TheEnd(S32 num, const char *error);
/*--------------------------------------------------------------------------*/
/* Boot-time fatal exit for the InitAdeline init failures (events, window,
   video, screen, graphics, config, MIDI). These run before atexit(TheEndInfo)
   is registered and use a bare exit(), so they bypass TheEndInfo's error
   footer + native dialog entirely -- silent on a double-clicked launcher.
   BootFatal logs the reason, shows the same native dialog as TheEndInfo (when
   there is no terminal to read), and exits non-zero. It deliberately skips
   TheEndInfo's heavy teardown (WriteConfigFile / Clear3dExt), which would
   touch subsystems that an early failure never finished initialising. */
extern LBA_NORETURN void BootFatal(const char *fmt, ...);
/*--------------------------------------------------------------------------*/
extern void TheEndInfo(void);
/*--------------------------------------------------------------------------*/
extern void Message(const char *mess, S32 flag);
/*--------------------------------------------------------------------------*/
extern void WriteConfigFile(void);
/*--------------------------------------------------------------------------*/

#endif // PERSO_H
