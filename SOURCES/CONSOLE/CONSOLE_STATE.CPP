#include "CONSOLE_STATE.H"

#include <stdio.h>

const char *Console_AvailInGameScene_FromState(int flag_play_acf,
                                               const void *ptr_scene,
                                               S32 num_cube,
                                               S32 num_cube_phantom) {
    if (flag_play_acf)
        return "Not available while a video is playing.";
    if (ptr_scene == NULL)
        return "Not available until a scene is loaded.";
    if (num_cube == num_cube_phantom)
        return "Not available in this state (not in a loaded cube).";
    return NULL;
}

const char *Console_ModeString_FromState(int flag_play_acf,
                                         const void *ptr_scene,
                                         S32 num_cube,
                                         S32 num_cube_phantom) {
    if (flag_play_acf)
        return "video";
    if (ptr_scene == NULL || num_cube == num_cube_phantom)
        return "menu";
    return "game";
}

void Console_FormatStatusIslandLine_FromState(char *buf, size_t buflen,
                                              S32 island,
                                              S32 num_cube,
                                              const S16 *list_var_game,
                                              int flag_chapter_index) {
    int chapter = 0;
    if (list_var_game != NULL && flag_chapter_index >= 0 && flag_chapter_index <= 255)
        chapter = (int)list_var_game[flag_chapter_index];
    if (buflen == 0)
        return;
    snprintf(buf, buflen, "Island: %d  Cube: %d  Chapter: %d", (int)island, (int)num_cube, chapter);
    buf[buflen - 1] = '\0';
}
