#include "CONSOLE.H"
#include "CHEATCOD.H"
#include <ctype.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SVGA/AFFSTR.H>
#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SDL3/SDL.h>
#include <SDL3/SDL_keyboard.h>

#define CONSOLE_TEXT_COLOUR 15

#define CONSOLE_SCROLLBACK_LINES 128
#define CONSOLE_INPUT_MAX 256
#define CONSOLE_HISTORY_SIZE 32
#define CONSOLE_LINE_MAX 256
#define CONSOLE_WRAP_WIDTH 78
#define CONSOLE_PANEL_HEIGHT 140
#define CONSOLE_LINE_HEIGHT 9
#define CONSOLE_MAX_CMDS 32
#define CONSOLE_MAX_CVARS 16
#define CONSOLE_MAX_ARGV 16
#define CONSOLE_KEY_QUEUE_SIZE 32

struct cmd_entry {
    const char *name;
    Console_CmdFn fn;
    const char *help;
    const char *usage;
    const char *context;
};
struct key_event {
    int scancode;
    int keycode;
};
struct cvar_entry {
    const char *name;
    void *ptr;
    Console_CvarType type;
    const char *help;
};

static int s_console_open = 0;
static struct cmd_entry s_cmds[CONSOLE_MAX_CMDS];
static int s_cmd_count = 0;
static struct cvar_entry s_cvars[CONSOLE_MAX_CVARS];
static int s_cvar_count = 0;
static char s_scrollback[CONSOLE_SCROLLBACK_LINES][CONSOLE_LINE_MAX];
static int s_scrollback_count = 0;
static int s_scrollback_head = 0; /* next write position */
static char s_input_line[CONSOLE_INPUT_MAX];
static int s_input_len = 0;
static int s_cursor_pos = 0;
static char s_history[CONSOLE_HISTORY_SIZE][CONSOLE_LINE_MAX];
static int s_history_count = 0;
static int s_history_index = -1; /* -1 = not browsing */
static int s_registered = 0;
static unsigned int s_cursor_blink = 0;
static int s_just_closed = 0; /* suppress game input for one frame after closing */

/* Own buffer for overlay (independent of game Log) */
static U8 *s_console_buf = NULL;
static U32 s_console_w = 0;
static U32 s_console_h = CONSOLE_PANEL_HEIGHT;

/* Key queue for event-driven input */
static struct key_event s_key_queue[CONSOLE_KEY_QUEUE_SIZE];
static int s_key_head = 0;
static int s_key_tail = 0;

/* Set by game when entering/leaving demo slide show so we don't open console there (avoids segfault). */
static int s_slide_show_active = 0;
/* Configurable toggle scancode; defaults to F12. */
static int s_toggle_scancode = K_F12;

/* Scrollback view offset: 0 = newest, >0 = scrolled up into history. */
static int s_scroll_offset = 0;

/* Screenshot without console: path and format. PNG = game performs SavePNG; else PrePresent writes PPM. */
#define CONSOLE_SCREENSHOT_PATH_MAX 256
static char s_screenshot_path[CONSOLE_SCREENSHOT_PATH_MAX] = "";
static int s_screenshot_png = 0;
static Console_LineSinkFn s_line_sink_for_tests = NULL;

static void Console_SubmitLine(void);

void Console_SetSlideShowActive(int active) {
    s_slide_show_active = (active != 0);
}

void Console_SetToggleScancode(int scancode) {
    if (scancode <= 0 || scancode >= SDL_SCANCODE_COUNT) {
        s_toggle_scancode = K_F12;
        return;
    }
    s_toggle_scancode = scancode;
}

int Console_GetToggleScancode(void) {
    return s_toggle_scancode;
}

void Console_RequestScreenshot(const char *path, int use_png) {
    if (!path)
        return;
    s_screenshot_path[0] = '\0';
    s_screenshot_png = use_png ? 1 : 0;
    strncpy(s_screenshot_path, path, CONSOLE_SCREENSHOT_PATH_MAX - 1);
    s_screenshot_path[CONSOLE_SCREENSHOT_PATH_MAX - 1] = '\0';
    s_console_open = 0;
    free(s_console_buf);
    s_console_buf = NULL;
    s_console_w = 0;
    s_input_len = 0;
    s_input_line[0] = '\0';
    s_cursor_pos = 0;
    s_just_closed = 1;
}

int Console_HasPendingScreenshot(char *path_out, unsigned int path_max, int *out_is_png) {
    if (!s_screenshot_path[0])
        return 0;
    if (path_out && path_max) {
        strncpy(path_out, s_screenshot_path, path_max - 1);
        path_out[path_max - 1] = '\0';
    }
    if (out_is_png)
        *out_is_png = s_screenshot_png;
    return 1;
}

void Console_ClearPendingScreenshot(void) {
    s_screenshot_path[0] = '\0';
    s_screenshot_png = 0;
}

int Console_IsOpen(void) {
    return s_console_open;
}

void Console_Toggle(void) {
    s_console_open = !s_console_open;
    if (s_console_open) {
        if (!s_registered) {
            Console_RegisterAll();
            s_registered = 1;
        }
        s_history_index = -1;
    } else {
        /* Clear input line when closing so it doesn't persist on next open */
        free(s_console_buf);
        s_console_buf = NULL;
        s_console_w = 0;
        s_input_len = 0;
        s_input_line[0] = '\0';
        s_cursor_pos = 0;
        s_just_closed = 1;
    }
}

void Console_Close(void) {
    if (!s_console_open)
        return;
    s_console_open = 0;
    free(s_console_buf);
    s_console_buf = NULL;
    s_console_w = 0;
    s_input_len = 0;
    s_input_line[0] = '\0';
    s_cursor_pos = 0;
    s_just_closed = 1;
}

/* Push one line to scrollback (no wrap). */
static void scrollback_push_one(const char *line);

static void scrollback_push(const char *line) {
    scrollback_push_one(line);
}

static void scrollback_push_one(const char *line) {
    size_t n = strlen(line);
    if (n >= CONSOLE_LINE_MAX)
        n = CONSOLE_LINE_MAX - 1;
    memcpy(s_scrollback[s_scrollback_head], line, n);
    s_scrollback[s_scrollback_head][n] = '\0';
    s_scrollback_head = (s_scrollback_head + 1) % CONSOLE_SCROLLBACK_LINES;
    if (s_scrollback_count < CONSOLE_SCROLLBACK_LINES)
        s_scrollback_count++;
}

/* Break long lines at CONSOLE_WRAP_WIDTH (at space when possible) and push each. */
static void scrollback_push_wrapped(const char *line) {
    size_t len = strlen(line);
    if (len == 0)
        return;
    if (len <= (size_t)CONSOLE_WRAP_WIDTH) {
        scrollback_push_one(line);
        return;
    }
    char seg[CONSOLE_LINE_MAX];
    const char *p = line;
    while (p < line + len) {
        size_t remain = (size_t)(line + len - p);
        if (remain <= (size_t)CONSOLE_WRAP_WIDTH) {
            memcpy(seg, p, remain);
            seg[remain] = '\0';
            scrollback_push_one(seg);
            break;
        }
        size_t take = CONSOLE_WRAP_WIDTH;
        const char *br = p + take;
        /* Prefer break at space */
        for (; br > p; br--)
            if (*br == ' ' || *br == '\t')
                break;
        if (br == p)
            br = p + take;
        size_t n = (size_t)(br - p);
        memcpy(seg, p, n);
        seg[n] = '\0';
        scrollback_push_one(seg);
        p = br;
        while (p < line + len && (*p == ' ' || *p == '\t'))
            p++;
    }
}

void Console_Print(const char *fmt, ...) {
    char buf[CONSOLE_LINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, CONSOLE_LINE_MAX, fmt, ap);
    va_end(ap);
    buf[CONSOLE_LINE_MAX - 1] = '\0';

    /* Split on '\n' so callers can pass multi-line strings naturally
     * (matches printf intuition: '\n' in the format means "advance to
     * a new line"). Empty segments — from leading, trailing, or
     * repeated '\n' — are skipped, matching the behavior console
     * users expect (no spurious blank lines). The sink-for-tests
     * sees exactly what gets pushed to the scrollback. */
    char *p = buf;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (nl)
            *nl = '\0';
        if (*p) {
            scrollback_push_wrapped(p);
            if (s_line_sink_for_tests)
                s_line_sink_for_tests(p);
        }
        if (!nl)
            break;
        p = nl + 1;
    }
}

void Console_SetLineSinkForTests(Console_LineSinkFn sink) {
    s_line_sink_for_tests = sink;
}

/* Helpers to sort commands/cvars alphabetically for help/cmdlist/varlist. */
static int cmd_index_cmp(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    if (ia < 0 || ib < 0 || ia >= s_cmd_count || ib >= s_cmd_count)
        return 0;
    return strcmp(s_cmds[ia].name, s_cmds[ib].name);
}

static int cvar_index_cmp(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    if (ia < 0 || ib < 0 || ia >= s_cvar_count || ib >= s_cvar_count)
        return 0;
    return strcmp(s_cvars[ia].name, s_cvars[ib].name);
}

static void Console_UpdateKey(int scancode, int keycode) {
    (void)keycode;
    if (scancode == K_ENTER || scancode == K_NUMPAD_ENTER) {
        Console_SubmitLine();
        return;
    }
    /* Backspace: match by scancode or keycode (8 = ASCII BS) for different layouts */
    if (scancode == K_BACKSPACE || keycode == 8) {
        if (s_input_len > 0) {
            s_input_len--;
            s_cursor_pos = s_input_len;
            s_input_line[s_input_len] = '\0';
        }
        return;
    }
    if (scancode == K_UP) {
        /* Scroll up through scrollback. */
        int n = (int)(s_console_h / CONSOLE_LINE_HEIGHT) - 1;
        if (n < 0)
            n = 0;
        if (n < s_scrollback_count) {
            int maxScroll = s_scrollback_count - n;
            if (maxScroll < 0)
                maxScroll = 0;
            if (s_scroll_offset < maxScroll)
                s_scroll_offset++;
        }
        return;
    }
    if (scancode == K_DOWN) {
        /* Scroll down toward the newest lines. */
        if (s_scroll_offset > 0)
            s_scroll_offset--;
        return;
    }
    /* Printable character from event keycode (SDL keycode is Unicode-ish) */
    if (keycode >= 32 && keycode < 127 && s_input_len < CONSOLE_INPUT_MAX - 1) {
        s_input_line[s_input_len++] = (char)keycode;
        s_input_line[s_input_len] = '\0';
        s_cursor_pos = s_input_len;
    }
    (void)s_cursor_blink;
}

void Console_Update(void) {
    while (s_key_head != s_key_tail) {
        struct key_event *e = &s_key_queue[s_key_tail];
        Console_UpdateKey(e->scancode, e->keycode);
        s_key_tail = (s_key_tail + 1) % CONSOLE_KEY_QUEUE_SIZE;
    }
}

/** Draw scrollback + input line into s_console_buf (call when open and buffer ready). */
static void Console_Draw(void) {
    int i, y, start, n;
    if (!s_console_buf || s_console_w == 0)
        return;

    memset(s_console_buf, 0, s_console_w * s_console_h);

    /* Visible lines for scrollback (leave one line for the input prompt). */
    n = (int)(s_console_h / CONSOLE_LINE_HEIGHT) - 1;
    if (n < 0)
        n = 0;
    if (n > s_scrollback_count)
        n = s_scrollback_count;

    /* Clamp scroll offset based on total lines and visible window. */
    if (n > 0) {
        int maxScroll = s_scrollback_count - n;
        if (maxScroll < 0)
            maxScroll = 0;
        if (s_scroll_offset > maxScroll)
            s_scroll_offset = maxScroll;
        if (s_scroll_offset < 0)
            s_scroll_offset = 0;
    }

    y = 0;
    if (n > 0) {
        /* Oldest line currently visible = (head - n - offset) modulo buffer size. */
        start = s_scrollback_head - n - s_scroll_offset;
        while (start < 0)
            start += CONSOLE_SCROLLBACK_LINES;

        /* If scrolled up and there is room, reserve the last visible line for an ellipsis. */
        int drawLines = n;
        int showEllipsis = (s_scroll_offset > 0 && n > 0) ? 1 : 0;
        if (showEllipsis && drawLines > 1)
            drawLines--;

        for (i = 0; i < drawLines; i++) {
            int idx = (start + i) % CONSOLE_SCROLLBACK_LINES;
            AffStringToBuffer(s_console_buf, s_console_w, 8, y,
                              s_scrollback[idx], CONSOLE_TEXT_COLOUR);
            y += CONSOLE_LINE_HEIGHT;
        }

        /* Ellipsis at bottom when there are earlier lines above the current window. */
        if (showEllipsis) {
            AffStringToBuffer(s_console_buf, s_console_w, 8, y, "...", CONSOLE_TEXT_COLOUR);
            y += CONSOLE_LINE_HEIGHT;
        }
    }
    {
        char prompt[CONSOLE_INPUT_MAX + 8];
        snprintf(prompt, sizeof(prompt), "]> %s_", s_input_line);
        AffStringToBuffer(s_console_buf, s_console_w, 8, y, prompt, CONSOLE_TEXT_COLOUR);
    }
}

void Console_PrePresent(U32 *frameBuffer, U32 pitch, U32 width, U32 height, const U8 *palette_rgb) {
    U32 y, x;
    /* Screenshot without console: PNG is done by game (SavePNG); only PPM here. */
    if (s_screenshot_path[0] && !s_screenshot_png && frameBuffer && width > 0 && height > 0) {
        FILE *f = fopen(s_screenshot_path, "wb");
        if (f) {
            fprintf(f, "P6\n%u %u\n255\n", width, height);
            for (U32 row = 0; row < height; row++) {
                U32 *src = (U32 *)((U8 *)frameBuffer + row * pitch);
                for (U32 col = 0; col < width; col++) {
                    U32 p = src[col];
                    unsigned char r = (unsigned char)((p >> 16) & 0xFF);
                    unsigned char g = (unsigned char)((p >> 8) & 0xFF);
                    unsigned char b = (unsigned char)(p & 0xFF);
                    fputc(r, f);
                    fputc(g, f);
                    fputc(b, f);
                }
            }
            fclose(f);
        }
        Console_ClearPendingScreenshot();
        return;
    }
    if (!s_console_open || !frameBuffer || !palette_rgb)
        return;

    if (s_console_buf == NULL || s_console_w != width) {
        free(s_console_buf);
        s_console_w = width;
        /* Top half of the screen, with a small minimum so at least a few lines show. */
        s_console_h = height / 2;
        if (s_console_h < (U32)(CONSOLE_LINE_HEIGHT * 4))
            s_console_h = (height < (U32)(CONSOLE_LINE_HEIGHT * 4)) ? height : (U32)(CONSOLE_LINE_HEIGHT * 4);
        s_console_buf = (U8 *)malloc(s_console_w * s_console_h);
        if (!s_console_buf)
            return;
    }

    Console_Draw();

    const U8 *borderCol = palette_rgb + (CONSOLE_TEXT_COLOUR * 3);

    /* Use byte-based row offset so pitch need not be a multiple of sizeof(U32). */
    for (y = 0; y < s_console_h; y++) {
        U32 *dstRow = (U32 *)((U8 *)frameBuffer + y * pitch);
        U8 *srcRow = s_console_buf + y * s_console_w;
        for (x = 0; x < s_console_w; x++) {
            U8 idx = srcRow[x];
            U32 base = dstRow[x];
            U8 br = (U8)((base >> 16) & 0xFF);
            U8 bg = (U8)((base >> 8) & 0xFF);
            U8 bb = (U8)(base & 0xFF);
            U8 r, g, b;

            /* Bottom border line of the console panel. */
            if (y + 1 == s_console_h) {
                r = borderCol[0];
                g = borderCol[1];
                b = borderCol[2];
            } else if (idx == 0) {
                /* Background: darken underlying game image for a translucent effect. */
                r = (U8)((br * 3) / 5);
                g = (U8)((bg * 3) / 5);
                b = (U8)((bb * 3) / 5);
            } else {
                /* Text: solid colour from current palette. */
                const U8 *c = palette_rgb + (idx * 3);
                r = c[0];
                g = c[1];
                b = c[2];
            }
            dstRow[x] = ((U32)0xFF << 24) | ((U32)r << 16) | ((U32)g << 8) | b;
        }
    }
}

int Console_FeedEvent(const void *sdlEvent) {
    const SDL_Event *ev = (const SDL_Event *)sdlEvent;
    if (ev->type != SDL_EVENT_KEY_DOWN)
        return 0;

    SDL_Scancode scan = ev->key.scancode;
    SDL_Keycode key = SDL_GetKeyFromScancode(scan, ev->key.mod, true);
    /* Fallback: some layouts don't report Shift; ensure minus+Shift gives underscore. */
    if ((key == 45 || key == (SDL_Keycode)'-') && (ev->key.mod & SDL_KMOD_SHIFT))
        key = (SDL_Keycode)'_';

    /* Toggle only on first key down, not key repeat. */
    if ((scan == (SDL_Scancode)s_toggle_scancode) && !ev->key.repeat) {
        if (s_slide_show_active && !s_console_open)
            return 0;
        Console_Toggle();
        return 1;
    }
    if (!s_console_open)
        return 0;

    /* Don't queue key repeat for Enter so we only submit once per keypress */
    if (ev->key.repeat && (scan == (SDL_Scancode)K_ENTER || scan == (SDL_Scancode)K_NUMPAD_ENTER))
        return 1;

    /* Push to queue */
    int next = (s_key_head + 1) % CONSOLE_KEY_QUEUE_SIZE;
    if (next == s_key_tail)
        return 1;
    s_key_queue[s_key_head].scancode = (int)scan;
    s_key_queue[s_key_head].keycode = (int)key;
    s_key_head = next;
    return 1;
}

int Console_ShouldSuppressInputThisFrame(void) {
    if (!s_just_closed)
        return 0;
    s_just_closed = 0;
    return 1;
}

void Console_RegisterCommandEx(const char *name, Console_CmdFn callback, const char *help, const char *usage, const char *context) {
    if (s_cmd_count >= CONSOLE_MAX_CMDS)
        return;
    s_cmds[s_cmd_count].name = name;
    s_cmds[s_cmd_count].fn = callback;
    s_cmds[s_cmd_count].help = help;
    s_cmds[s_cmd_count].usage = usage;
    s_cmds[s_cmd_count].context = context;
    s_cmd_count++;
}

void Console_RegisterCommand(const char *name, Console_CmdFn callback, const char *help) {
    Console_RegisterCommandEx(name, callback, help, NULL, NULL);
}

void Console_RegisterCvar(const char *name, void *ptr, Console_CvarType type, const char *help) {
    if (s_cvar_count >= CONSOLE_MAX_CVARS)
        return;
    s_cvars[s_cvar_count].name = name;
    s_cvars[s_cvar_count].ptr = ptr;
    s_cvars[s_cvar_count].type = type;
    s_cvars[s_cvar_count].help = help;
    s_cvar_count++;
}

void Console_Execute(const char *line);

static void Console_SubmitLine(void) {
    if (s_input_len <= 0)
        return;
    s_input_line[s_input_len] = '\0';
    scrollback_push(s_input_line);
    /* Add to history */
    if (s_history_count < CONSOLE_HISTORY_SIZE) {
        strncpy(s_history[s_history_count], s_input_line, CONSOLE_LINE_MAX - 1);
        s_history[s_history_count][CONSOLE_LINE_MAX - 1] = '\0';
        s_history_count++;
    } else {
        int j;
        for (j = 0; j < CONSOLE_HISTORY_SIZE - 1; j++)
            strcpy(s_history[j], s_history[j + 1]);
        strncpy(s_history[CONSOLE_HISTORY_SIZE - 1], s_input_line, CONSOLE_LINE_MAX - 1);
        s_history[CONSOLE_HISTORY_SIZE - 1][CONSOLE_LINE_MAX - 1] = '\0';
    }
    Console_Execute(s_input_line);
    s_input_len = 0;
    s_input_line[0] = '\0';
    s_cursor_pos = 0;
    s_history_index = -1;
}

void Console_Execute(const char *line) {
    static char buf[CONSOLE_LINE_MAX];
    static const char *argv[CONSOLE_MAX_ARGV];
    int argc = 0;
    char *p;
    int i;

    strncpy(buf, line, CONSOLE_LINE_MAX - 1);
    buf[CONSOLE_LINE_MAX - 1] = '\0';
    p = buf;
    while (argc < CONSOLE_MAX_ARGV) {
        while (*p && (unsigned char)*p <= ' ')
            p++;
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && (unsigned char)*p > ' ')
            p++;
        if (*p)
            *p++ = '\0';
    }

    if (argc == 0)
        return;

    if (TryExecuteCheatByName(argv[0]))
        return;

    /* Built-in: help, cmdlist, varlist */
    if (strcmp(argv[0], "help") == 0) {
        if (argc >= 2 && argv[1][0]) {
            const char *topic = argv[1];
            for (i = 0; i < s_cmd_count; i++) {
                if (strcmp(s_cmds[i].name, topic) == 0) {
                    Console_Print("%s - %s", s_cmds[i].name, s_cmds[i].help ? s_cmds[i].help : "");
                    if (s_cmds[i].usage && s_cmds[i].usage[0])
                        Console_Print("  Usage: %s", s_cmds[i].usage);
                    if (s_cmds[i].context && s_cmds[i].context[0])
                        Console_Print("  Context: %s", s_cmds[i].context);
                    return;
                }
            }
            for (i = 0; i < s_cvar_count; i++) {
                if (strcmp(s_cvars[i].name, topic) == 0) {
                    Console_Print("cvar %s - %s", s_cvars[i].name, s_cvars[i].help ? s_cvars[i].help : "");
                    if (s_cvars[i].type == CONSOLE_CVAR_INT)
                        Console_Print("  Usage: %s <integer>", s_cvars[i].name);
                    else if (s_cvars[i].type == CONSOLE_CVAR_BOOL_U8)
                        Console_Print("  Usage: %s  (0|1)", s_cvars[i].name);
                    else
                        Console_Print("  Usage: %s  (no arg toggles; or %s 0|1)", s_cvars[i].name, s_cvars[i].name);
                    return;
                }
            }
            Console_Print("Unknown help topic: %s (try: cmdlist, varlist)", topic);
            return;
        }
        int idx[CONSOLE_MAX_CMDS];
        int n = s_cmd_count;
        if (n > CONSOLE_MAX_CMDS)
            n = CONSOLE_MAX_CMDS;
        for (i = 0; i < n; i++)
            idx[i] = i;
        qsort(idx, (size_t)n, sizeof(int), cmd_index_cmp);
        for (i = 0; i < n; i++) {
            const struct cmd_entry *ce = &s_cmds[idx[i]];
            Console_Print("  %s - %s", ce->name, ce->help ? ce->help : "");
        }
        Console_Print("Use: help <name> for usage and context. Try: cmdlist, varlist");
        return;
    }
    if (strcmp(argv[0], "cmdlist") == 0) {
        int idx[CONSOLE_MAX_CMDS];
        int n = s_cmd_count;
        if (n > CONSOLE_MAX_CMDS)
            n = CONSOLE_MAX_CMDS;
        for (i = 0; i < n; i++)
            idx[i] = i;
        qsort(idx, (size_t)n, sizeof(int), cmd_index_cmp);
        for (i = 0; i < n; i++)
            Console_Print("  %s", s_cmds[idx[i]].name);
        return;
    }
    if (strcmp(argv[0], "varlist") == 0) {
        int idx[CONSOLE_MAX_CVARS];
        int n = s_cvar_count;
        if (n > CONSOLE_MAX_CVARS)
            n = CONSOLE_MAX_CVARS;
        for (i = 0; i < n; i++)
            idx[i] = i;
        qsort(idx, (size_t)n, sizeof(int), cvar_index_cmp);
        for (i = 0; i < n; i++) {
            const struct cvar_entry *cv = &s_cvars[idx[i]];
            Console_Print("  %s - %s", cv->name, cv->help ? cv->help : "");
        }
        return;
    }

    for (i = 0; i < s_cmd_count; i++) {
        if (strcmp(s_cmds[i].name, argv[0]) == 0) {
            s_cmds[i].fn(argc, argv);
            return;
        }
    }

    /* Cvar get/set: "name" or "name value"; empty value => show current. */
    for (i = 0; i < s_cvar_count; i++) {
        if (strcmp(s_cvars[i].name, argv[0]) != 0)
            continue;
        if (argc >= 2 && argv[1][0] != '\0') {
            if (s_cvars[i].type == CONSOLE_CVAR_INT) {
                *(int *)s_cvars[i].ptr = (int)strtol(argv[1], NULL, 0);
                Console_Print("%s = %d", s_cvars[i].name, *(int *)s_cvars[i].ptr);
            } else if (s_cvars[i].type == CONSOLE_CVAR_BOOL) {
                int v = (argv[1][0] == '1' || argv[1][0] == 'y' || argv[1][0] == 'Y' || (argv[1][0] == 'o' && argv[1][1] == 'n'));
                *(int *)s_cvars[i].ptr = v;
                Console_Print("%s = %s", s_cvars[i].name, v ? "1" : "0");
            } else if (s_cvars[i].type == CONSOLE_CVAR_BOOL_U8) {
                int v = (argv[1][0] == '1' || argv[1][0] == 'y' || argv[1][0] == 'Y' || (argv[1][0] == 'o' && argv[1][1] == 'n'));
                *(unsigned char *)s_cvars[i].ptr = (unsigned char)v;
                Console_Print("%s = %s", s_cvars[i].name, v ? "1" : "0");
            }
        } else {
            /* No value: for bools toggle, otherwise show current value. */
            if (s_cvars[i].type == CONSOLE_CVAR_INT)
                Console_Print("%s = %d", s_cvars[i].name, *(int *)s_cvars[i].ptr);
            else if (s_cvars[i].type == CONSOLE_CVAR_BOOL) {
                int v = *(int *)s_cvars[i].ptr ? 0 : 1;
                *(int *)s_cvars[i].ptr = v;
                Console_Print("%s = %s", s_cvars[i].name, v ? "1" : "0");
            } else if (s_cvars[i].type == CONSOLE_CVAR_BOOL_U8) {
                unsigned char v = *(unsigned char *)s_cvars[i].ptr ? (unsigned char)0 : (unsigned char)1;
                *(unsigned char *)s_cvars[i].ptr = v;
                Console_Print("%s = %s", s_cvars[i].name, v ? "1" : "0");
            }
        }
        return;
    }

    Console_Print("Unknown command: %s (try: cmdlist)", argv[0]);
}
