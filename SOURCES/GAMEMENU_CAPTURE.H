#ifndef GAMEMENU_CAPTURE_H
#define GAMEMENU_CAPTURE_H

/* Headless UI capture hooks for the console `ui` verbs.
 *
 * These are test scaffolding, not game API. They live in their own header so
 * GAMEMENU.H stays the menu module's real surface rather than carrying six
 * near-identical capture entry points.
 *
 * Each function arms a one-shot SavePNG inside the next DoGameMenu (or
 * SlideShow) iteration and returns the ESC sentinel / exits cleanly, so a
 * `ui ... <path>` verb in a --headless run does not have to drive the menu by
 * input. Pass NULL or "" to disarm. */

/* Arm a one-shot screenshot inside the next DoGameMenu call. After a
   countdown of iterations (long enough for the menu to fade in and settle),
   SavePNG is called with the supplied path and DoGameMenu returns 1000 (the
   "ESC" sentinel) so the wrapping menu loop exits cleanly. */
extern void Menu_RequestCapture(const char *path);

/* True while a capture is armed. OptionsMenu's pre-pass uses it to decide
   whether to run the harness-only setup. */
extern S32 Menu_IsCaptureArmed(void);

/* Capture the boot-time main game menu (Resume / New Game / Load / Options /
   Quit -- the screen the player sees at launch). Does the minimal
   MainGameMenu setup (loads the PCR_MENU background bitmap, InitPlasmaMenu,
   BuildGameMainMenu, InitDial(0)), arms Menu_RequestCapture, then drives
   DoGameMenu(GameMainMenu) which captures and returns 1000. Skips the
   surrounding MainGameMenu while-loop, fade transitions, and any case-driven
   cinematic playback so the verb is non-blocking. */
extern void Menu_MainCapture(const char *path);

/* Capture the Display submenu (Phase 2 runtime-resolution UX). Mirrors
   Menu_MainCapture: does the minimal pre-pass the regular OptionsMenu ->
   GereDisplayMenu path does (InitPlasmaMenu, palette sync, dial bank), arms
   Menu_RequestCapture, drives DoGameMenu on the DisplayMenu array. The capture
   site inside DoGameMenu fires and returns 1000. Background pixels come from
   the caller's Log/Screen state -- `ui --black-bg display` clears them first;
   the plain verb composites over the live scene. */
extern void Display_MainCapture(const char *path);

/* Capture the resolution sub-page (the list opened from the Display submenu's
   Resolution row). Like Display_MainCapture but builds the runtime
   ResolutionMenu first, then drives DoGameMenu on it. */
extern void Resolution_MainCapture(const char *path);

/* Capture the Gameplay submenu (the list opened from Options -> Gameplay).
   Arms Menu_RequestCapture and drives DoGameMenu on the GameplayMenu array;
   the capture site inside DoGameMenu fires and returns 1000. Background pixels
   come from the caller's Log/Screen state -- `ui --black-bg gameplay` clears
   them first; the plain verb composites over the live scene. */
extern void Gameplay_MainCapture(const char *path);

/* Capture the Cheat submenu (the list opened from Options -> Cheats). Like
   Gameplay_MainCapture but rebuilds the rows from the shared cheat table in
   CHEATCOD.CPP first. */
extern void Cheat_MainCapture(const char *path);

/* Arm a one-shot SavePNG inside SlideShow's first frame and exit cleanly
   after capturing. Mirrors Inv_RequestCapture / Menu_RequestCapture /
   Dial_RequestCapture etc. Wire into the harness via `ui slideshow <path>`. */
extern void SlideShow_RequestCapture(const char *path);

#endif // GAMEMENU_CAPTURE_H
