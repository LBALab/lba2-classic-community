#ifndef GAMEMENU_H
#define GAMEMENU_H

#ifdef DEMO
extern S32 FlagShowEndDemo;
#endif

/*--------------------------------------------------------------------------*/
extern Func_Init3DView Init3DView;
/*--------------------------------------------------------------------------*/
extern void Init3DGame(void);
/*--------------------------------------------------------------------------*/
extern void SetDetailLevel(void);
/*--------------------------------------------------------------------------*/
extern void MessageTransp(char *mess, S32 coul, S32 moving);
/*--------------------------------------------------------------------------*/
extern void InitMenuIncrustText(const char *text, S32 sprite, S32 time);
/*--------------------------------------------------------------------------*/
extern void DrawFireBar(S32 xmin, S32 ymin, S32 xmax, S32 ymax, S32 coul);
/*--------------------------------------------------------------------------*/
/* Plasma/fire highlight bar on the active menu option. Sits horizontally
   centred on the screen, LargeurMenu wide. The bar's x-range mirrors the
   DrawOneString/DrawOneChoice box (x ± LargeurMenu/2) — those callers
   already centre at ModeDesiredX/2 post-C2, so this macro tracks the same
   anchor or the highlight slides off the menu items at wider widths. */
#define DrawFire(y, coul) DrawFireBar((S32)ModeDesiredX / 2 - LargeurMenu / 2, \
                                      y,                                       \
                                      (S32)ModeDesiredX / 2 + LargeurMenu / 2, \
                                      y + HAUTEUR_STANDARD,                    \
                                      coul)
/*--------------------------------------------------------------------------*/
extern int my_sort_function(const void *a, const void *b);
/*--------------------------------------------------------------------------*/
extern S32 PlayerGameList(char **ptrlistname, char *listname, S32 flagnew);
/*--------------------------------------------------------------------------*/
extern S32 FindPlayerFile(void);
/*--------------------------------------------------------------------------*/
extern void DrawOneString(S32 x, S32 y, char *playername, S32 draw);
/*--------------------------------------------------------------------------*/
extern void ClearOneString(S32 x, S32 y);
/*--------------------------------------------------------------------------*/
extern void DrawSingleString(S32 x, S32 y, char *string);
/*--------------------------------------------------------------------------*/
extern S32 InputPlayerName(S32 choice, char *name);
/*--------------------------------------------------------------------------*/
extern S32 ChoosePlayerName(S32 mess, S32 flagflip, S32 flagnew);
/*--------------------------------------------------------------------------*/
extern void DrawOneChoice(S32 x, S32 y, S32 type, S32 num, S32 select);
/*--------------------------------------------------------------------------*/
extern void DrawGameMenu(U16 *ptrmenu, S32 justone);
/*--------------------------------------------------------------------------*/
extern S32 DoGameMenu(U16 *ptrmenu);
/*--------------------------------------------------------------------------*/
extern S32 OptionsMenu(S32 flagflip);
/* Harness hook: arm a one-shot screenshot inside the next DoGameMenu call.
   After a countdown of iterations (long enough for the menu to fade in and
   settle), SavePNG is called with the supplied path and DoGameMenu returns
   1000 (the "ESC" sentinel) so the wrapping menu loop exits cleanly.
   Pass NULL or "" to disarm.  Used by the console "ui menu-options <path>"
   verb. */
extern void Menu_RequestCapture(const char *path);
/* Harness hook: capture the boot-time main game menu (Resume / New Game /
   Load / Options / Quit -- the screen the player sees at launch).  Does
   the minimal MainGameMenu setup (loads the PCR_MENU background bitmap,
   InitPlasmaMenu, BuildGameMainMenu, InitDial(0)), arms Menu_RequestCapture,
   then drives DoGameMenu(GameMainMenu) which captures and returns 1000.
   Skips the surrounding MainGameMenu while-loop, fade transitions, and any
   case-driven cinematic playback so the verb is non-blocking. */
extern void Menu_MainCapture(const char *path);
/*--------------------------------------------------------------------------*/
extern void VolumeOptions(void);
/*--------------------------------------------------------------------------*/
extern void SavedGameManagement(void);
/*--------------------------------------------------------------------------*/
extern S32 DeleteSavedGame(void);
/*--------------------------------------------------------------------------*/
extern void ZoomSavedGame(S32 x, S32 y);
/*--------------------------------------------------------------------------*/
extern void DrawScreenSave(U8 *ptr, S32 x, S32 y);
/*--------------------------------------------------------------------------*/
extern void DrawCadreSave(S32 x, S32 y);
/*--------------------------------------------------------------------------*/
extern S32 IsFirstGameLaunched(void);
/*--------------------------------------------------------------------------*/
extern S32 MainGameMenu(S32 load);
/*--------------------------------------------------------------------------*/
extern void GameAskChoice(S32 nummess);
/*--------------------------------------------------------------------------*/
extern void GameOver(void);
/*--------------------------------------------------------------------------*/
extern void Help(U8 flaggame);
/*--------------------------------------------------------------------------*/
extern void Credits(S32 mode);
/*--------------------------------------------------------------------------*/
extern S32 SlideShow(void);
/*--------------------------------------------------------------------------*/
extern void SlideDemo(int argc, char *argv[]);
/*--------------------------------------------------------------------------*/
extern S32 AdelineLogo(void);
/*--------------------------------------------------------------------------*/
extern void DistribLogo(void);
/*--------------------------------------------------------------------------*/
extern S32 BumperLogo(void);
/*--------------------------------------------------------------------------*/
extern S32 ShowLogo(S32 numscr, S32 nsec);
/*--------------------------------------------------------------------------*/
#ifdef DEMO
extern void DemoLogo(void);
/*--------------------------------------------------------------------------*/
extern void DemoBumper(void);
#endif
/*--------------------------------------------------------------------------*/
extern void EffectPcx(U8 num, U8 *screen, U8 effect, U8 flagbcl);
/*--------------------------------------------------------------------------*/
extern void EffectPcxMess(U8 numpcx, U8 *screen, U8 effect, S32 mess);
/*--------------------------------------------------------------------------*/
extern void Introduction(void);
/*--------------------------------------------------------------------------*/

#endif // GAMEMENU_H
