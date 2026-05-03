#ifndef CONSOLE_STATE_H
#define CONSOLE_STATE_H

#include <SYSTEM/ADELINE_TYPES.H>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Indices / sentinels — must match SOURCES/COMMON.H (CONSOLE_CMD.CPP #errors if not). */
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

/** Legacy three-way label "video" | "menu" | "game" (host tests only). */
const char *Console_ModeString_FromState(int flag_play_acf,
                                         const void *ptr_scene,
                                         S32 num_cube,
                                         S32 num_cube_phantom);

/**
 * First line of the status command: island, cube, chapter from globals passed in.
 * Matches cmd_status formatting (raw values; no give-style scene masking).
 *
 * \a list_var_game may be NULL; chapter is 0 then. \a flag_chapter_index is
 * ignored if out of range 0..255 (defensive bound, not the real ListVarGame size).
 */
void Console_FormatStatusIslandLine_FromState(char *buf, size_t buflen,
                                              S32 island,
                                              S32 num_cube,
                                              const S16 *list_var_game,
                                              int flag_chapter_index);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_STATE_H */
