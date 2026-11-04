#ifndef GAMEMENU_INTERNAL_H
#define GAMEMENU_INTERNAL_H

/* Cross-translation-unit surface of the menu module. Not a public header:
 * only GAMEMENU*.CPP includes it. The public face stays GAMEMENU.H, which
 * carries the entry points the rest of the engine calls. */

#include "GAMEMENU_MENUS.H"
#include "RES_MENU.H"
#include <SVGA/SCREEN.H>

/* Row geometry shared by the driver and the save list. */
#define HAUTEUR_STANDARD 50
#define MENU_SPACE 6

/* Row-id ranges owned by the runtime builders. */
#define MENU_ID_RESOLUTION_BASE 2200
#define MENU_ID_CHEAT_BASE 2240
#define CHEAT_MENU_MAX_ENTRIES 12

#define SCREEN_SAVE_WIDTH 160
#define SCREEN_SAVE_HEIGHT 120
/* Top-left X of the SCREEN_SAVE_WIDTH-wide save thumbnail when it should be
   horizontally centred on the framebuffer. At 640 this resolves to 240 (the
   historical literal); at wider widths it tracks ModeDesiredX/2. */
#define SAVE_THUMB_X ((S32)ModeDesiredX / 2 - SCREEN_SAVE_WIDTH / 2)
/* Top of the save thumbnail on the save/load list screen (ChoosePlayerName).
   The historical literal is 50, on the 480-tall canvas; UI_VCENTER_OFS floats
   it with the rest of that screen on a taller framebuffer (0 at 480). The
   in-game save (y=10) and load-confirm (y=130) thumbnails are other screens
   and keep their own literals. */
#define SAVE_THUMB_Y (50 + UI_VCENTER_OFS)

/* Save/load state shared with the main menu and the generic driver. */
extern char GamePathname[];
extern char PlayerName[];

/* Text chrome the driver and the save list both draw with. */
extern U8 CoulTextMenu;
extern S32 LargeurMenu;

/* Name-entry caret state, drawn by DrawOneString and moved by InputPlayerName. */
extern S32 CursorActif;
extern S32 CursorX;
extern S32 CursorY;

extern S32 GameInProgress;
extern U16 GameMainMenu[];
extern U16 GameOptionMenu[];

/* Display-owned state the generic driver reads: the resolution catalog rows
   behind the range-id labels, and the green "active value" marker row. */
extern DisplayEntry s_resMenuEntries[];
extern S32 s_resMenuCount;
extern S32 s_menuMarkRow;

/* Sub-pages opened by OptionsMenu. */
extern S32 GereVolumeMenu(void);
extern S32 GereLanguageMenu(void);
extern S32 GereGameplayMenu(void);
extern S32 GereControllerMenu(void);
extern S32 GereCheatMenu(void);
extern S32 GereDisplayMenu(void);

/* Language cycling is driven by DoGameMenu's type 8/9 rows. */
extern void CycleTextLanguage(S32 step);
#ifdef CDROM
extern void CycleVoiceLanguage(S32 step);
#endif

/* Save-list probe used by the main menu's Load/Resume filtering. */
extern S32 IsExistSavedGame(void);

/* Main-menu builder shared by MainGameMenu and the headless capture hook. */
extern void BuildGameMainMenu(S32 firstloop);

/* Seconds-scale timer the pause/menu flow resets. Defined by the driver. */
extern S32 EscTimer;

#endif // GAMEMENU_INTERNAL_H
