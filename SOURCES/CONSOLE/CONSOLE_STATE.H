#ifndef CONSOLE_STATE_H
#define CONSOLE_STATE_H

#include <SYSTEM/ADELINE_TYPES.H>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Indices / sentinel values must stay aligned with SOURCES/COMMON.H */
#define CONSOLE_LISTVAR_FLAG_CHAPTER 253
#define CONSOLE_NUM_CUBE_PHANTOM_DEFAULT 94

/**
 * Pure helpers mirroring game globals. Used by the in-game console and by
 * host-only unit tests (no retail assets, no full engine init).
 */

/** NULL if the command may run; else a user-facing reason string (literal). */
const char *Console_AvailInGameScene_FromState(int flag_play_acf,
                                               const void *ptr_scene,
                                               S32 num_cube,
                                               S32 num_cube_phantom);

/** Short mode label for the context line: "video" | "menu" | "game". */
const char *Console_ModeString_FromState(int flag_play_acf,
                                         const void *ptr_scene,
                                         S32 num_cube,
                                         S32 num_cube_phantom);

/**
 * Writes "[Context] mode=... island=... cube=... chapter=..." into buf.
 * When there is no loaded in-scene state (PtrScene NULL, phantom cube, or
 * video playing), island/cube/chapter are shown as '-' so stale globals are
 * not mistaken for live play state.
 *
 * \a list_var_game may be NULL; \a flag_chapter_index is ignored then or if
 * out of supported range (0..255).
 */
void Console_FormatContextLine_FromState(char *buf, size_t buflen,
                                         int flag_play_acf,
                                         const void *ptr_scene,
                                         S32 num_cube,
                                         S32 num_cube_phantom,
                                         S32 island,
                                         const S16 *list_var_game,
                                         int flag_chapter_index);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_STATE_H */
