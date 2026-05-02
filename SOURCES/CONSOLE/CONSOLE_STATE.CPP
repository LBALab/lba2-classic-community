#include "CONSOLE_STATE.H"

#include <stdio.h>
#include <string.h>

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

void Console_FormatContextLine_FromState(char *buf, size_t buflen,
                                         int flag_play_acf,
                                         const void *ptr_scene,
                                         S32 num_cube,
                                         S32 num_cube_phantom,
                                         S32 island,
                                         const S16 *list_var_game,
                                         int flag_chapter_index) {
    const char *mode = Console_ModeString_FromState(flag_play_acf, ptr_scene, num_cube, num_cube_phantom);
    const int in_scene = (ptr_scene != NULL && num_cube != num_cube_phantom && !flag_play_acf);
    char isla_str[32];
    char cube_str[32];
    char chap_str[32];

    if (in_scene) {
        snprintf(isla_str, sizeof(isla_str), "%d", (int)island);
        snprintf(cube_str, sizeof(cube_str), "%d", (int)num_cube);
        if (list_var_game != NULL && flag_chapter_index >= 0 && flag_chapter_index <= 255) {
            snprintf(chap_str, sizeof(chap_str), "%d", (int)list_var_game[flag_chapter_index]);
        } else {
            strncpy(chap_str, "-", sizeof(chap_str));
            chap_str[sizeof(chap_str) - 1] = '\0';
        }
    } else {
        strncpy(isla_str, "-", sizeof(isla_str));
        isla_str[sizeof(isla_str) - 1] = '\0';
        strncpy(cube_str, "-", sizeof(cube_str));
        cube_str[sizeof(cube_str) - 1] = '\0';
        strncpy(chap_str, "-", sizeof(chap_str));
        chap_str[sizeof(chap_str) - 1] = '\0';
    }

    if (buflen == 0)
        return;
    snprintf(buf, buflen, "[Context] mode=%s island=%s cube=%s chapter=%s", mode, isla_str, cube_str, chap_str);
    buf[buflen - 1] = '\0';
}
