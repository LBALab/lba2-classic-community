#ifndef MENU_LABELS_H
#define MENU_LABELS_H

/* In-source, community-added menu strings, localized across the engine's six UI
   languages. These are the strings the retail TEXT.HQR banks can't supply,
   because we can't extend that binary asset (see GAMEMENU.CPP).

   Layout: ONE ROW PER LABEL, not one array per language. All six translations
   of a label sit together, so adding a label is a single-row edit and reviewing
   one is a single-line read. Each row also carries its own enum id, which
   tests/menu_labels asserts equals the row's index -- so inserting a row in the
   wrong place (which would silently shift every later label's text) fails CI
   instead of shipping a menu full of wrong strings.

   Contract, all pinned by tests/menu_labels:
     - every row's id equals its index (no misalignment);
     - every translation is non-NULL and non-empty (C++ brace elision would
       otherwise quietly fill a short row with NULL, and GetLocalizedMenuLabel()
       would hand that NULL to CopyUtf8ToCp850());
     - a row's printf specifiers are identical in all six languages (a stray
       "%s" where the caller passes an unsigned is a crash, not a typo). */

/* Width of every row. This header stays free of engine includes so the host test
   can link the table on its own, so it cannot reference NB_LANGUAGES directly.
   GAMEMENU.CPP, which sees both headers, pins MENU_LANGUAGE_COUNT == NB_LANGUAGES
   at compile time. */
#define MENU_LANGUAGE_COUNT 6

enum MenuLocalizedLabel {
    MENU_LABEL_CHOOSE_LANGUAGE,
    MENU_LABEL_ADVANCED_OPTIONS,
    MENU_LABEL_TEXT_LANGUAGE,
    MENU_LABEL_VOICE_LANGUAGE,
    MENU_LABEL_DISPLAY_WINDOWED,
    MENU_LABEL_DISPLAY_FULLSCREEN,
    MENU_LABEL_AUTO_CAMERA,
    MENU_LABEL_CLASSIC_CAMERA,
    MENU_LABEL_CONTROLLER,
    MENU_LABEL_GAME_CONTROLLER,
    MENU_LABEL_GAME_SAVED,
    MENU_LABEL_BUG_SAVED,
    MENU_LABEL_RES_TITLE,
    MENU_LABEL_RES_NOW_AT,
    MENU_LABEL_RES_KEEP_Q,
    MENU_LABEL_RES_BTN_KEEP,
    MENU_LABEL_RES_BTN_REVERT_FMT,
    MENU_LABEL_DISPLAY,
    MENU_LABEL_RESOLUTION,
    MENU_LABEL_VSYNC_ON,
    MENU_LABEL_VSYNC_OFF,
    MENU_LABEL_MOVIE_ORIGINAL,
    MENU_LABEL_MOVIE_FIT,
    MENU_LABEL_KEYCFG_FOOTER,
    MENU_LABEL_COUNT,
};

/* lang[] is indexed by the engine's Language value, in the order it uses:
   0 English, 1 French, 2 German, 3 Spanish, 4 Italian, 5 Portuguese. */
struct MenuLabelRow {
    enum MenuLocalizedLabel id; /* must equal this row's index */
    const char *lang[MENU_LANGUAGE_COUNT];
};

extern const struct MenuLabelRow LocalizedMenuLabels[MENU_LABEL_COUNT];

#endif // MENU_LABELS_H
