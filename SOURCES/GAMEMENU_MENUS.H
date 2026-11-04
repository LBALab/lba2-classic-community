#ifndef GAMEMENU_MENUS_H
#define GAMEMENU_MENUS_H

#include <SYSTEM/ADELINE_TYPES.H>

/* Shared vocabulary for the menu pages.
 *
 * A runtime menu is a flat U16 array:
 *
 *     [selected, count, y_center, dia_num, (type, id) * count]
 *
 * The helpers below keep a page's count in step with the rows it actually
 * wrote. The old builders advanced a U16 * by hand and then recomputed the
 * count with `(p - base - 4) / 2`; one missing pair silently made DoGameMenu
 * read past the intended rows, so pages write by index instead.
 *
 * The static pages describe their rows as a MenuRowDef table, which lets
 * MENU_ROW_COUNT derive the count from the initializer at compile time.
 * Runtime-conditional pages (Display, Resolution, Cheats) append by index and
 * finish with MenuSetCount. */

#define MENU_HEADER_WORDS 4

/* Largest page currently built at runtime (Cheats: Back + every cheat). */
#define MENU_ROW_MAX 16

struct MenuRowDef {
    U16 type;
    U16 id;
};

#define MENU_ROW_COUNT(rows) ((S32)(sizeof(rows) / sizeof((rows)[0])))

static inline void MenuSetRow(U16 *menu, S32 row, U16 type, U16 id) {
    menu[MENU_HEADER_WORDS + row * 2] = type;
    menu[MENU_HEADER_WORDS + row * 2 + 1] = id;
}

static inline void MenuSetCount(U16 *menu, S32 count) {
    menu[1] = (U16)count;
}

/* Seed a page from a compile-time row table. yCenter is the 480-tall-canvas
 * centre DrawGameMenu expects; selected and dia are left alone so a page
 * keeps its cursor across re-entry, matching the static initializers. */
static inline void MenuBuildRows(U16 *menu, S32 yCenter, const struct MenuRowDef *rows, S32 count) {
    S32 i;

    menu[1] = (U16)count;
    menu[2] = (U16)yCenter;
    menu[3] = 0;
    for (i = 0; i < count; i++) {
        menu[MENU_HEADER_WORDS + i * 2] = rows[i].type;
        menu[MENU_HEADER_WORDS + i * 2 + 1] = rows[i].id;
    }
}

/* Row ids. The plain numbers are text.hqr SYS-bank ids; the MENU_ID_* block
 * from 2100 up is synthetic and resolved by BuildCustomMenuText. Both are
 * "ids" to DoGameMenu, hence the shared prefix. */
#define MENU_ID_RETURN_TO_GAME 15
#define MENU_ID_CANCEL 24
#define MENU_ID_BACK 26
#define MENU_ID_DELETE_SAVE 48

#define MENU_ID_KEYBOARD_CONFIG 14
#define MENU_ID_DETAIL_LEVEL 45
#define MENU_ID_VOLUME_SAMPLES 19
#define MENU_ID_VOLUME_VOICE 20
#define MENU_ID_VOLUME_MUSIC 21

#define MENU_ID_STEREO_OFF 12
#define MENU_ID_STEREO_ON 13
#define MENU_ID_MOVIE_CAMERA_OFF 46
#define MENU_ID_MOVIE_CAMERA_ON 47
#define MENU_ID_VIDEO_SMALL 27
#define MENU_ID_VIDEO_FULL 28
#define MENU_ID_TEXT_HIDE 16
#define MENU_ID_TEXT_SHOW 17

#define MENU_ID_MAIN_RESUME 70
#define MENU_ID_MAIN_NEW 71
#define MENU_ID_MAIN_LOAD 72
#define MENU_ID_MAIN_SAVE 73
#define MENU_ID_MAIN_OPTIONS 74
#define MENU_ID_MAIN_QUIT 75

#define MENU_ID_SOUND_VOLUME 2100
#define MENU_ID_CHOOSE_LANGUAGE 2101
#define MENU_ID_GAMEPLAY 2102
#define MENU_ID_TEXT_LANGUAGE 2103
#define MENU_ID_VOICE_LANGUAGE 2104
#define MENU_ID_STEREO_CHANNELS 2105
#define MENU_ID_MOVIE_CAMERA 2106
#define MENU_ID_VIDEO_SIZE 2107
#define MENU_ID_DISPLAY_FULLSCREEN 2108
#define MENU_ID_DISPLAY_TEXT 2109
#define MENU_ID_GENERAL_VOLUME 2110
#define MENU_ID_FOLLOW_CAMERA 2111
#define MENU_ID_CONTROLLER 2112
#define MENU_ID_GAMEPAD_CONFIG 2113
#define MENU_ID_DISPLAY 2114
#define MENU_ID_RESOLUTION 2115
#define MENU_ID_DISPLAY_VSYNC 2116
#ifdef USE_GPURENDERER
#define MENU_ID_DISPLAY_RENDERER 2117
#define MENU_ID_DISPLAY_TEXFILTER 2118
#define MENU_ID_DISPLAY_QUALITY 2119
#define MENU_ID_DISPLAY_STEREO3D 2120 /* Quest OpenXR native stereo toggle */
#define MENU_ID_DISPLAY_VRZOOM 2128   /* Quest stereo world zoom percent */
#define MENU_ID_DISPLAY_VRSUPERSAMPLE 2135
#define MENU_ID_DISPLAY_VRHOVER 2138
#define MENU_ID_DISPLAY_VRPASSTHROUGH 2139
#define MENU_ID_DISPLAY_VRBODY 2140
#define MENU_ID_DISPLAY_VRFULLBODY 2141
#define MENU_ID_DISPLAY_VRWORLDSCALE 2142
#define MENU_ID_DISPLAY_VRINTERIOR 2148
#define MENU_ID_DISPLAY_DITHER 2143
#define MENU_ID_DISPLAY_SHARPEN 2145
#endif
#define MENU_ID_DISPLAY_FPS 2121
#define MENU_ID_FPS_LIMIT 2122
#define MENU_ID_DISPLAY_ANTIALIAS 2123
#define MENU_ID_CHEATS 2124
#define MENU_ID_DISPLAY_BRIGHTNESS 2125
#define MENU_ID_DISPLAY_CONTRAST 2126
#define MENU_ID_DISPLAY_SATURATION 2127
#define MENU_ID_DISPLAY_GRAPHICS 2130
#define MENU_ID_DISPLAY_IMAGE 2131
#define MENU_ID_DISPLAY_VR 2132
#define MENU_ID_DISPLAY_WINDOW 2133
#define MENU_ID_DISPLAY_SOFTSHADOWS 2137
#define MENU_ID_DISPLAY_COLORGRADE 2144
#define MENU_ID_DISPLAY_SUBPIXEL 2147
#define MENU_ID_MOVIE_UPSCALE 2149

/* Synthetic row types handled by DoGameMenu's slider/cycler branches. */
#define MENU_TYPE_TEXT_LANGUAGE 8
#define MENU_TYPE_VOICE_LANGUAGE 9
#define MENU_TYPE_IMAGE_BRIGHTNESS 10
#define MENU_TYPE_IMAGE_CONTRAST 11
#define MENU_TYPE_IMAGE_SATURATION 12

/* Sentinel returns from DoGameMenu. */
#define MENU_RET_ESC 1000
#define MENU_RET_DEMO_REEL 9999

#endif // GAMEMENU_MENUS_H
