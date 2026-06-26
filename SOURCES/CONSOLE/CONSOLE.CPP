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
#include <SYSTEM/WINDOW.H> /* WindowToSurfaceCoords for mouse selection */
#include <SDL3/SDL.h>
#include <SDL3/SDL_keyboard.h>

#define CONSOLE_TEXT_COLOUR 15

/* Bumped from 128 so the boot log (now fed in via the Log_* console sink) isn't
 * evicted by gameplay output before the player opens the console. */
#define CONSOLE_SCROLLBACK_LINES 512
#define CONSOLE_INPUT_MAX 256
#define CONSOLE_HISTORY_SIZE 32
#define CONSOLE_LINE_MAX 256
#define CONSOLE_WRAP_WIDTH 78
#define CONSOLE_PANEL_HEIGHT 140
#define CONSOLE_LINE_HEIGHT 9
#define CONSOLE_MAX_CMDS 48 /* ~27 registered today; headroom before the loud drop */
#define CONSOLE_MAX_CVARS 32
#define CONSOLE_MAX_ARGV 16
#define CONSOLE_KEY_QUEUE_SIZE 32
/* Sentinel scancode for a queued text-input character (vs a physical key). */
#define CONSOLE_TEXT_CHAR (-1)
/* Max console rows we snapshot for mouse selection (panel is height/2; 1024px -> ~56). */
#define CONSOLE_MAX_ROWS 80
/* Double-click window for word selection. */
#define CONSOLE_DBLCLICK_MS 400

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
    int mod; /* SDL_Keymod bitmask captured at key-down (Ctrl/Shift for clipboard) */
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
static U8 s_scrollback_col[CONSOLE_SCROLLBACK_LINES]; /* per-line ink (CONSOLE_COL_*) */
static int s_scrollback_count = 0;
static int s_scrollback_head = 0; /* next write position */
/* The editable input line: its text, cursor, and (Shift / Ctrl-A) selection. */
typedef struct {
    char line[CONSOLE_INPUT_MAX];
    int len;
    int cursor;
    int sel_anchor;     /* selection anchor in the line; -1 = no selection */
    unsigned int blink; /* cursor blink phase */
} ConsoleInput;
static ConsoleInput s_in = {{0}, 0, 0, -1, 0};
static char s_history[CONSOLE_HISTORY_SIZE][CONSOLE_LINE_MAX];
static int s_history_count = 0;
static int s_history_index = -1; /* -1 = not browsing */
static int s_registered = 0;
static int s_just_closed = 0; /* suppress game input for one frame after closing */

/* Own buffer for overlay (independent of game Log) */
/* The overlay render state: the 8-bit panel buffer and its size, the per-frame
 * snapshot of drawn rows (for mouse hit-testing and clipboard extraction), and the
 * per-row selection spans the compositor inverts (-1 = no selection on that row). */
typedef struct {
    U8 *buf;                                           /* 8-bit overlay pixels (own allocation, independent of game Log) */
    U32 w, h;                                          /* overlay size in pixels */
    int row_count;                                     /* rows currently snapshotted */
    int input_row;                                     /* screen-row index of the input line */
    char row_text[CONSOLE_MAX_ROWS][CONSOLE_LINE_MAX]; /* rows as drawn (CP437 bytes) */
    int row_len[CONSOLE_MAX_ROWS];                     /* cell count per row */
    int row_sel_start[CONSOLE_MAX_ROWS];
    int row_sel_end[CONSOLE_MAX_ROWS];
} ConsoleRender;
static ConsoleRender s_r = {NULL, 0, CONSOLE_PANEL_HEIGHT, 0, -1};

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

/* Mouse scrollback selection in screen row/col space. Read-only: copy with Ctrl+C.
 * Mutually exclusive with the editable input-line selection (s_in.sel_anchor). */
typedef struct {
    int active;                         /* a scrollback selection exists */
    int a_row, a_col;                   /* anchor cell */
    int c_row, c_col;                   /* caret cell */
    int dragging;                       /* left mouse button held */
    Uint64 last_click_ms;               /* double-click timing */
    int last_click_row, last_click_col; /* double-click target */
} ConsoleScreenSel;
static ConsoleScreenSel s_scrsel = {0, 0, 0, 0, 0, 0, 0, -1, -1};

/* Reset to "no selection" with fresh double-click tracking. Anchor/caret are left
 * as-is since they are only read while active. */
static void scrsel_reset(void) {
    s_scrsel.active = 0;
    s_scrsel.dragging = 0;
    s_scrsel.last_click_ms = 0;
    s_scrsel.last_click_row = -1;
    s_scrsel.last_click_col = -1;
}

/* Screenshot without console: path and format. PNG = game performs SavePNG; else PrePresent writes PPM. */
#define CONSOLE_SCREENSHOT_PATH_MAX 256
static char s_screenshot_path[CONSOLE_SCREENSHOT_PATH_MAX] = "";
static int s_screenshot_png = 0;
static Console_LineSinkFn s_line_sink_for_tests = NULL;

static void Console_SubmitLine(void);
static void input_clear(void); /* defined with the input-editing primitives below */

/* Enable/disable SDL text input so the console receives correctly-cased,
 * layout-aware characters via SDL_EVENT_TEXT_INPUT. SDL_GetKeyFromScancode does
 * not apply Shift on every platform (hence the old '-'->'_' workaround), so the
 * keycode path alone can't produce capitals or shifted symbols. Targets the
 * focused window to stay decoupled from the video layer; a no-op when there is
 * no window (e.g. host tests with no SDL video). */
static void console_set_text_input(int on) {
    SDL_Window *w = SDL_GetKeyboardFocus();
    if (!w)
        return;
    if (on)
        SDL_StartTextInput(w);
    else
        SDL_StopTextInput(w);
}

/* The engine hides the OS cursor at window creation and draws its own software
 * pointer sprite into the game frame, which composites *under* the console overlay.
 * So show the OS cursor (drawn on top by the window system) while the console is
 * open, for click/drag selection, and restore the hidden default on close. */
static void console_set_os_cursor(int on) {
    if (on)
        SDL_ShowCursor();
    else
        SDL_HideCursor();
}

/* Tear down the visible console: stop text input, restore the hidden OS cursor,
 * drop any selection, free the overlay buffer, clear the input line, and arm the
 * one-frame input suppression. Shared by Toggle (close), Close, and the screenshot
 * path so the reset lives in one place. Does not touch s_console_open; callers own
 * the open flag. The set/stop calls are no-ops when already off. */
static void console_teardown(void) {
    console_set_text_input(0);
    console_set_os_cursor(0);
    scrsel_reset();
    free(s_r.buf);
    s_r.buf = NULL;
    s_r.w = 0;
    input_clear();
    s_just_closed = 1;
}

void Console_SetSlideShowActive(int active) {
    s_slide_show_active = (active != 0);
}

/* Cleanroom UI-capture-mode flag. See doc in CONSOLE.H. */
static int s_capture_black_bg = 0;

void Console_SetCaptureBlackBg(int active) {
    s_capture_black_bg = (active != 0);
}

int Console_GetCaptureBlackBg(void) {
    return s_capture_black_bg;
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
    console_teardown();
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

void Console_EnsureRegistered(void) {
    if (!s_registered) {
        Console_RegisterAll();
        s_registered = 1;
    }
}

void Console_Toggle(void) {
    s_console_open = !s_console_open;
    if (s_console_open) {
        Console_EnsureRegistered();
        s_history_index = -1;
        s_in.sel_anchor = -1;
        scrsel_reset();
        console_set_text_input(1);
        console_set_os_cursor(1);
    } else {
        console_teardown();
    }
}

void Console_Close(void) {
    if (!s_console_open)
        return;
    s_console_open = 0;
    console_teardown();
}

/* Push one line to scrollback (no wrap). */
static void scrollback_push_one(const char *line, U8 colour);

static void scrollback_push_one(const char *line, U8 colour) {
    size_t n = strlen(line);
    if (n >= CONSOLE_LINE_MAX)
        n = CONSOLE_LINE_MAX - 1;
    memcpy(s_scrollback[s_scrollback_head], line, n);
    s_scrollback[s_scrollback_head][n] = '\0';
    s_scrollback_col[s_scrollback_head] = colour;
    s_scrollback_head = (s_scrollback_head + 1) % CONSOLE_SCROLLBACK_LINES;
    if (s_scrollback_count < CONSOLE_SCROLLBACK_LINES)
        s_scrollback_count++;
}

/* Break long lines at CONSOLE_WRAP_WIDTH (at space when possible) and push each. */
static void scrollback_push_wrapped(const char *line, U8 colour) {
    size_t len = strlen(line);
    if (len == 0)
        return;
    if (len <= (size_t)CONSOLE_WRAP_WIDTH) {
        scrollback_push_one(line, colour);
        return;
    }
    char seg[CONSOLE_LINE_MAX];
    const char *p = line;
    while (p < line + len) {
        size_t remain = (size_t)(line + len - p);
        if (remain <= (size_t)CONSOLE_WRAP_WIDTH) {
            memcpy(seg, p, remain);
            seg[remain] = '\0';
            scrollback_push_one(seg, colour);
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
        scrollback_push_one(seg, colour);
        p = br;
        while (p < line + len && (*p == ' ' || *p == '\t'))
            p++;
    }
}

/* Split buf on '\n' so callers can pass multi-line strings naturally (matches
 * printf intuition: '\n' means "advance to a new line"). Empty segments — from
 * leading, trailing, or repeated '\n' — are skipped (no spurious blank lines).
 * The sink-for-tests sees exactly what gets pushed to the scrollback.
 *
 * Exception: a wholly-empty message is an intentional blank-line spacer. The
 * boot log groups itself with Log_Raw("") (and LogPrintf's internal blanks
 * arrive as their own empty record), which the terminal and file sinks render
 * as a blank line; preserve it here so the F12 console keeps the same vertical
 * rhythm. Blanks *within* a non-empty message still collapse, per the rule
 * above. */
static void console_push_multiline(char *buf, U8 colour) {
    if (*buf == '\0') {
        scrollback_push_one("", colour);
        if (s_line_sink_for_tests)
            s_line_sink_for_tests("");
        return;
    }
    char *p = buf;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (nl)
            *nl = '\0';
        if (*p) {
            scrollback_push_wrapped(p, colour);
            if (s_line_sink_for_tests)
                s_line_sink_for_tests(p);
        }
        if (!nl)
            break;
        p = nl + 1;
    }
}

void Console_Print(const char *fmt, ...) {
    char buf[CONSOLE_LINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, CONSOLE_LINE_MAX, fmt, ap);
    va_end(ap);
    buf[CONSOLE_LINE_MAX - 1] = '\0';
    console_push_multiline(buf, CONSOLE_COL_DEFAULT);
}

void Console_PrintColored(int colour, const char *fmt, ...) {
    char buf[CONSOLE_LINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, CONSOLE_LINE_MAX, fmt, ap);
    va_end(ap);
    buf[CONSOLE_LINE_MAX - 1] = '\0';
    console_push_multiline(buf, (U8)colour);
}

void Console_SetLineSinkForTests(Console_LineSinkFn sink) {
    s_line_sink_for_tests = sink;
}

int Console_GetScrollback(const char **out, int max) {
    int k, i, start;
    if (!out || max <= 0)
        return 0;
    k = (s_scrollback_count < max) ? s_scrollback_count : max;
    start = s_scrollback_head - k;
    while (start < 0)
        start += CONSOLE_SCROLLBACK_LINES;
    for (i = 0; i < k; i++)
        out[i] = s_scrollback[(start + i) % CONSOLE_SCROLLBACK_LINES];
    return k;
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

/* --- Input-line editing primitives. All operate at s_in.cursor so the line can
 * be edited mid-string (Left/Right/Home/End/Delete), not just appended to. --- */
static void input_insert_char(char c) {
    if (s_in.len >= CONSOLE_INPUT_MAX - 1)
        return;
    memmove(&s_in.line[s_in.cursor + 1], &s_in.line[s_in.cursor],
            (size_t)(s_in.len - s_in.cursor));
    s_in.line[s_in.cursor] = c;
    s_in.len++;
    s_in.cursor++;
    s_in.line[s_in.len] = '\0';
}

static void input_backspace(void) {
    if (s_in.cursor <= 0)
        return;
    memmove(&s_in.line[s_in.cursor - 1], &s_in.line[s_in.cursor],
            (size_t)(s_in.len - s_in.cursor));
    s_in.cursor--;
    s_in.len--;
    s_in.line[s_in.len] = '\0';
}

static void input_delete(void) { /* delete the char *at* the cursor (Del key) */
    if (s_in.cursor >= s_in.len)
        return;
    memmove(&s_in.line[s_in.cursor], &s_in.line[s_in.cursor + 1],
            (size_t)(s_in.len - s_in.cursor - 1));
    s_in.len--;
    s_in.line[s_in.len] = '\0';
}

static void input_clear(void) {
    s_in.len = 0;
    s_in.cursor = 0;
    s_in.line[0] = '\0';
    s_history_index = -1; /* a cleared line starts history browsing fresh */
    s_in.sel_anchor = -1;
}

/* Resolve the active selection into [lo, hi) input-line indices. Returns 0 when
 * there is no selection (no anchor, or anchor == cursor). */
static int sel_span(int *lo, int *hi) {
    if (s_in.sel_anchor < 0 || s_in.sel_anchor == s_in.cursor)
        return 0;
    if (s_in.sel_anchor < s_in.cursor) {
        *lo = s_in.sel_anchor;
        *hi = s_in.cursor;
    } else {
        *lo = s_in.cursor;
        *hi = s_in.sel_anchor;
    }
    return 1;
}

/* Delete the selected span (if any). Returns 1 if it removed something. */
static int sel_delete(void) {
    int lo, hi;
    if (!sel_span(&lo, &hi))
        return 0;
    memmove(&s_in.line[lo], &s_in.line[hi], (size_t)(s_in.len - hi));
    s_in.len -= (hi - lo);
    s_in.line[s_in.len] = '\0';
    s_in.cursor = lo;
    s_in.sel_anchor = -1;
    return 1;
}

/* Order the mouse-selection anchor/caret into reading order (start <= end). */
static void scrsel_normalize(int *sr, int *sc, int *er, int *ec) {
    int before = (s_scrsel.a_row < s_scrsel.c_row) ||
                 (s_scrsel.a_row == s_scrsel.c_row && s_scrsel.a_col <= s_scrsel.c_col);
    if (before) {
        *sr = s_scrsel.a_row;
        *sc = s_scrsel.a_col;
        *er = s_scrsel.c_row;
        *ec = s_scrsel.c_col;
    } else {
        *sr = s_scrsel.c_row;
        *sc = s_scrsel.c_col;
        *er = s_scrsel.a_row;
        *ec = s_scrsel.a_col;
    }
}

/* A word char for double-click selection (letters, digits, underscore). */
static int is_word_char(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/* A bare modifier / lock key, which must not dismiss a mouse selection on its own
 * (otherwise pressing Ctrl to start Ctrl+C would clear what you mean to copy). */
static int is_modifier_scancode(int sc) {
    switch (sc) {
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
    case SDL_SCANCODE_LGUI:
    case SDL_SCANCODE_RGUI:
    case SDL_SCANCODE_CAPSLOCK:
    case SDL_SCANCODE_NUMLOCKCLEAR:
    case SDL_SCANCODE_SCROLLLOCK:
        return 1;
    default:
        return 0;
    }
}

/* CP437 <-> Unicode for the Latin-1 range the console actually draws (the boot
 * log's "·" separator, accented language names, etc.). Single source of truth so
 * the draw-time fold (cp_to_cp437) and the copy-time unfold (cp437_to_unicode)
 * cannot disagree. All these codepoints are U+0080..U+07FF (2-byte UTF-8). */
static const struct cp437_pair {
    unsigned int cp; /* Unicode codepoint */
    unsigned char b; /* CP437 byte */
} cp437_map[] = {
    {0x00B7, 0xFA}, /* · */ {0x00B0, 0xF8}, /* ° */ {0x00A1, 0xAD}, /* ¡ */
    {0x00BF, 0xA8},
    /* ¿ */ {0x00AB, 0xAE},
    /* « */ {0x00BB, 0xAF}, /* » */
    {0x00A3, 0x9C},
    /* £ */ {0x00C7, 0x80},
    /* Ç */ {0x00FC, 0x81}, /* ü */
    {0x00E9, 0x82},
    /* é */ {0x00E2, 0x83},
    /* â */ {0x00E4, 0x84}, /* ä */
    {0x00E0, 0x85},
    /* à */ {0x00E5, 0x86},
    /* å */ {0x00E7, 0x87}, /* ç */
    {0x00EA, 0x88},
    /* ê */ {0x00EB, 0x89},
    /* ë */ {0x00E8, 0x8A}, /* è */
    {0x00EF, 0x8B},
    /* ï */ {0x00EE, 0x8C},
    /* î */ {0x00EC, 0x8D}, /* ì */
    {0x00C4, 0x8E},
    /* Ä */ {0x00C5, 0x8F},
    /* Å */ {0x00C9, 0x90}, /* É */
    {0x00E6, 0x91},
    /* æ */ {0x00C6, 0x92},
    /* Æ */ {0x00F4, 0x93}, /* ô */
    {0x00F6, 0x94},
    /* ö */ {0x00F2, 0x95},
    /* ò */ {0x00FB, 0x96}, /* û */
    {0x00F9, 0x97},
    /* ù */ {0x00FF, 0x98},
    /* ÿ */ {0x00D6, 0x99}, /* Ö */
    {0x00DC, 0x9A},
    /* Ü */ {0x00E1, 0xA0},
    /* á */ {0x00ED, 0xA1}, /* í */
    {0x00F3, 0xA2},
    /* ó */ {0x00FA, 0xA3},
    /* ú */ {0x00F1, 0xA4}, /* ñ */
    {0x00D1, 0xA5},
    /* Ñ */ {0x00DF, 0xE1}, /* ß */
};

/* Unicode codepoint for a CP437 high byte, or 0 if unmapped. Turns the CP437 row
 * snapshot back into proper UTF-8 for the clipboard. */
static unsigned int cp437_to_unicode(unsigned char b) {
    for (size_t i = 0; i < sizeof(cp437_map) / sizeof(cp437_map[0]); i++)
        if (cp437_map[i].b == b)
            return cp437_map[i].cp;
    return 0;
}

/* Append one display cell (a CP437 byte) to the clipboard buffer as UTF-8: ASCII
 * passes through, mapped high bytes expand to 2-byte UTF-8, anything else -> '?'. */
static void copybuf_put_cell(char *buf, size_t cap, size_t *o, unsigned char cell) {
    if (cell >= 0x20 && cell < 0x7F) {
        if (*o + 1 < cap)
            buf[(*o)++] = (char)cell;
        return;
    }
    unsigned int cp = cp437_to_unicode(cell);
    if (cp == 0) {
        if (*o + 1 < cap)
            buf[(*o)++] = '?';
    } else if (*o + 2 < cap) {
        buf[(*o)++] = (char)(0xC0 | (cp >> 6));
        buf[(*o)++] = (char)(0x80 | (cp & 0x3F));
    }
}

/* Copy the active mouse (scrollback) selection to the clipboard as a flow: the
 * first row from its start column, full middle rows, the last row to its end
 * column, joined with newlines. */
static void scrsel_copy(void) {
    static char buf[CONSOLE_MAX_ROWS * CONSOLE_LINE_MAX];
    size_t o = 0;
    int sr, sc, er, ec, r;
    if (!s_scrsel.active)
        return;
    scrsel_normalize(&sr, &sc, &er, &ec);
    for (r = sr; r <= er && r < s_r.row_count; r++) {
        int c0 = (r == sr) ? sc : 0;
        int c1 = (r == er) ? ec : s_r.row_len[r];
        int c;
        if (c0 < 0)
            c0 = 0;
        if (c1 > s_r.row_len[r])
            c1 = s_r.row_len[r];
        for (c = c0; c < c1; c++)
            copybuf_put_cell(buf, sizeof buf, &o, (unsigned char)s_r.row_text[r][c]);
        if (r != er && o + 1 < sizeof buf)
            buf[o++] = '\n'; /* real newline between rows (not folded) */
    }
    buf[o < sizeof buf ? o : sizeof buf - 1] = '\0';
    SDL_SetClipboardText(buf);
}

/* Copy the selection to the clipboard: the mouse scrollback selection if one is
 * active, otherwise the input-line selection (or the whole line if none). */
static void clipboard_copy(void) {
    int lo, hi;
    if (s_scrsel.active) {
        scrsel_copy();
    } else if (sel_span(&lo, &hi)) {
        char tmp[CONSOLE_INPUT_MAX];
        int n = hi - lo;
        memcpy(tmp, &s_in.line[lo], (size_t)n);
        tmp[n] = '\0';
        SDL_SetClipboardText(tmp);
    } else {
        SDL_SetClipboardText(s_in.line);
    }
}

/* Scroll the scrollback view; delta > 0 reveals older lines. Clamped to the
 * visible window exactly as Console_Draw clamps. Shared by PgUp/PgDn and wheel. */
static void Console_ScrollLines(int delta) {
    int n = (int)(s_r.h / CONSOLE_LINE_HEIGHT) - 1;
    int maxScroll;
    if (n < 0)
        n = 0;
    maxScroll = s_scrollback_count - n;
    if (maxScroll < 0)
        maxScroll = 0;
    s_scroll_offset += delta;
    if (s_scroll_offset > maxScroll)
        s_scroll_offset = maxScroll;
    if (s_scroll_offset < 0)
        s_scroll_offset = 0;
}

/* Recall command history into the input line. dir < 0 = older, dir > 0 = newer.
 * Down past the newest entry restores an empty line and stops browsing. */
static void Console_HistoryRecall(int dir) {
    if (s_history_count == 0)
        return;
    if (s_history_index < 0) {
        if (dir >= 0)
            return;                        /* Down with nothing recalled yet: keep the in-progress line */
        s_history_index = s_history_count; /* step back from one-past-newest */
    }
    s_history_index += dir;
    if (s_history_index < 0)
        s_history_index = 0;
    if (s_history_index >= s_history_count) {
        input_clear(); /* walked past the newest entry: restore an empty line */
        return;
    }
    strncpy(s_in.line, s_history[s_history_index], CONSOLE_INPUT_MAX - 1);
    s_in.line[CONSOLE_INPUT_MAX - 1] = '\0';
    s_in.len = (int)strlen(s_in.line);
    s_in.cursor = s_in.len;
    s_in.sel_anchor = -1; /* a recalled line carries no selection */
}

/* Insert the printable chars of the system clipboard at the cursor (drops control
 * chars / newlines so a multi-line paste can't smuggle a submit). */
static void clipboard_paste(void) {
    char *txt = SDL_GetClipboardText();
    const char *p;
    if (!txt)
        return;
    for (p = txt; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c >= 32 && c < 127)
            input_insert_char((char)c);
    }
    SDL_free(txt);
}

/* Accumulate one completion candidate: record the match and shrink `out` to the
 * longest common prefix of all matches seen so far. */
static void complete_consider(const char *name, const char *prefix, size_t plen,
                              char *out, size_t outsz, const char **matches, int max,
                              int *count, int *have_first) {
    size_t k;
    if (strncmp(name, prefix, plen) != 0)
        return;
    if (matches && *count < max)
        matches[*count] = name;
    if (!*have_first) {
        strncpy(out, name, outsz - 1);
        out[outsz - 1] = '\0';
        *have_first = 1;
    } else {
        for (k = 0; out[k] && name[k] && out[k] == name[k]; k++)
            ;
        out[k] = '\0';
    }
    (*count)++;
}

/* Complete `prefix` against registered commands, cvars and the built-ins. `out`
 * receives the longest common prefix of all matches (the full name for a unique
 * match); `matches` receives up to `max` name pointers. Returns the match count.
 * Non-static so host tests can exercise it against a registered table. */
int Console_CompletePrefix(const char *prefix, char *out, size_t outsz, const char **matches, int max) {
    static const char *const builtins[] = {"help", "cmdlist", "varlist"};
    size_t plen = prefix ? strlen(prefix) : 0;
    int count = 0;
    int have_first = 0;
    int i;
    if (out && outsz)
        out[0] = '\0';
    if (!prefix || !out || outsz == 0)
        return 0;
    for (i = 0; i < s_cmd_count; i++)
        complete_consider(s_cmds[i].name, prefix, plen, out, outsz, matches, max, &count, &have_first);
    for (i = 0; i < s_cvar_count; i++)
        complete_consider(s_cvars[i].name, prefix, plen, out, outsz, matches, max, &count, &have_first);
    for (i = 0; i < (int)(sizeof(builtins) / sizeof(builtins[0])); i++)
        complete_consider(builtins[i], prefix, plen, out, outsz, matches, max, &count, &have_first);
    return count;
}

/* Tab-complete the command at the start of the line. Argument completion (after a
 * space) is intentionally left for later. */
static void Console_TabComplete(void) {
    char token[CONSOLE_INPUT_MAX];
    char lcp[CONSOLE_INPUT_MAX];
    const char *matches[CONSOLE_MAX_CMDS + CONSOLE_MAX_CVARS + 4];
    const int maxmatch = (int)(sizeof(matches) / sizeof(matches[0]));
    int nmatch, i, tlen, newlen, tail;

    for (i = 0; i < s_in.cursor; i++)
        if (s_in.line[i] == ' ' || s_in.line[i] == '\t')
            return; /* cursor is past the command word */
    tlen = s_in.cursor;
    if (tlen <= 0)
        return;
    if (tlen >= (int)sizeof(token))
        tlen = (int)sizeof(token) - 1;
    memcpy(token, s_in.line, (size_t)tlen);
    token[tlen] = '\0';

    nmatch = Console_CompletePrefix(token, lcp, sizeof lcp, matches, maxmatch);
    if (nmatch <= 0)
        return;

    /* Replace the typed token with the common prefix (or the full unique name). */
    newlen = (int)strlen(lcp);
    tail = s_in.len - s_in.cursor;
    if (newlen + tail >= CONSOLE_INPUT_MAX)
        return;
    memmove(&s_in.line[newlen], &s_in.line[s_in.cursor], (size_t)tail);
    memcpy(s_in.line, lcp, (size_t)newlen);
    s_in.len = newlen + tail;
    s_in.cursor = newlen;
    s_in.line[s_in.len] = '\0';

    if (nmatch == 1) {
        if (s_in.cursor == s_in.len && s_in.len < CONSOLE_INPUT_MAX - 1) {
            s_in.line[s_in.len++] = ' ';
            s_in.line[s_in.len] = '\0';
            s_in.cursor = s_in.len;
        }
    } else {
        /* Ambiguous: list the candidates (Quake-style). */
        char line[CONSOLE_LINE_MAX];
        size_t off = 0;
        line[0] = '\0';
        for (i = 0; i < nmatch && i < maxmatch; i++) {
            int w = snprintf(line + off, sizeof(line) - off, "%s%s", off ? "  " : "", matches[i]);
            if (w < 0 || off + (size_t)w >= sizeof(line))
                break;
            off += (size_t)w;
        }
        Console_Print("%s", line);
    }
}

static void Console_UpdateKey(int scancode, int keycode, int mod) {
    /* AltGr is Ctrl+Alt on many layouts; treat Ctrl as a command modifier only
     * when Alt is not also held so AltGr text entry still works. Lowercase the
     * keycode in-range without tolower() (it may be a large SDL keysym). */
    int ctrl = (mod & SDL_KMOD_CTRL) && !(mod & SDL_KMOD_ALT);
    int kl = (keycode >= 'A' && keycode <= 'Z') ? (keycode - 'A' + 'a') : keycode;

    s_in.blink = 0; /* any keypress makes the cursor immediately visible */

    /* A mouse (scrollback) selection is dismissed by editing/navigation keys, but
     * not by the copy shortcut or by a bare modifier press (so Ctrl, then C, keeps
     * the selection long enough to copy it). */
    if (s_scrsel.active && !(ctrl && kl == 'c') && !is_modifier_scancode(scancode))
        s_scrsel.active = 0;

    /* Text-input characters arrive already cased and layout-resolved, queued with
     * the CONSOLE_TEXT_CHAR sentinel; insert the printable ASCII ones. Physical
     * keys (below) are handled by scancode and never insert text themselves. */
    if (scancode == CONSOLE_TEXT_CHAR) {
        if (keycode >= 32 && keycode < 127) {
            sel_delete(); /* typing over a selection replaces it */
            input_insert_char((char)keycode);
        }
        return;
    }

    /* Control combos are commands, never text: handle the few we support and
     * swallow the rest so e.g. Ctrl+A doesn't type 'a'. */
    if (ctrl) {
        if (kl == 'v') {
            sel_delete(); /* paste replaces the selection */
            clipboard_paste();
        } else if (kl == 'c') {
            clipboard_copy();
            s_scrsel.active = 0; /* copying clears the selection */
            s_in.sel_anchor = -1;
        } else if (kl == 'x') {
            clipboard_copy();  /* copies the selection, or the whole line */
            if (!sel_delete()) /* delete the selection; if there was none, */
                input_clear(); /* cut the whole line we just copied */
        } else if (kl == 'a') {
            s_in.sel_anchor = 0; /* select all */
            s_in.cursor = s_in.len;
        } else if (kl == 'l') {
            input_clear();
        }
        return;
    }
    /* Shift+Insert also pastes (X11/terminal convention). */
    if (scancode == (int)SDL_SCANCODE_INSERT && (mod & SDL_KMOD_SHIFT)) {
        clipboard_paste();
        return;
    }

    if (scancode == K_ENTER || scancode == K_NUMPAD_ENTER) {
        Console_SubmitLine();
        return;
    }
    if (scancode == K_ESC) {
        if (s_in.len > 0)
            input_clear(); /* first Esc clears the line ... */
        else
            Console_Close(); /* ... Esc on an empty line closes the console */
        return;
    }
    if (scancode == K_TAB) {
        Console_TabComplete();
        return;
    }
    /* Backspace: match by scancode or keycode (8 = ASCII BS) for different layouts.
     * With a selection, Backspace/Delete remove the whole selection. */
    if (scancode == K_BACKSPACE || keycode == 8) {
        if (!sel_delete())
            input_backspace();
        return;
    }
    if (scancode == K_SUPPR) {
        if (!sel_delete())
            input_delete();
        return;
    }
    /* Left/Right/Home/End: with Shift held, extend the selection from where it
     * started; without Shift, drop any selection (collapsing to its near edge for
     * Left/Right) and move the cursor. */
    if (scancode == K_LEFT) {
        int lo, hi;
        if (mod & SDL_KMOD_SHIFT) {
            if (s_in.sel_anchor < 0)
                s_in.sel_anchor = s_in.cursor;
            if (s_in.cursor > 0)
                s_in.cursor--;
        } else if (sel_span(&lo, &hi)) {
            s_in.cursor = lo;
            s_in.sel_anchor = -1;
        } else if (s_in.cursor > 0) {
            s_in.cursor--;
        }
        return;
    }
    if (scancode == K_RIGHT) {
        int lo, hi;
        if (mod & SDL_KMOD_SHIFT) {
            if (s_in.sel_anchor < 0)
                s_in.sel_anchor = s_in.cursor;
            if (s_in.cursor < s_in.len)
                s_in.cursor++;
        } else if (sel_span(&lo, &hi)) {
            s_in.cursor = hi;
            s_in.sel_anchor = -1;
        } else if (s_in.cursor < s_in.len) {
            s_in.cursor++;
        }
        return;
    }
    if (scancode == K_HOME) {
        if (mod & SDL_KMOD_SHIFT) {
            if (s_in.sel_anchor < 0)
                s_in.sel_anchor = s_in.cursor;
        } else {
            s_in.sel_anchor = -1;
        }
        s_in.cursor = 0;
        return;
    }
    if (scancode == K_END) {
        if (mod & SDL_KMOD_SHIFT) {
            if (s_in.sel_anchor < 0)
                s_in.sel_anchor = s_in.cursor;
        } else {
            s_in.sel_anchor = -1;
        }
        s_in.cursor = s_in.len;
        return;
    }
    if (scancode == K_UP) {
        Console_HistoryRecall(-1); /* older */
        return;
    }
    if (scancode == K_DOWN) {
        Console_HistoryRecall(1); /* newer */
        return;
    }
    if (scancode == K_PAGE_UP || scancode == K_PAGE_DOWN) {
        int page = (int)(s_r.h / CONSOLE_LINE_HEIGHT) - 1;
        if (page < 1)
            page = 1;
        Console_ScrollLines(scancode == K_PAGE_UP ? page : -page);
        return;
    }
    /* Printable characters are not inserted here: they arrive via text input as
     * CONSOLE_TEXT_CHAR events (handled at the top), so casing and shifted symbols
     * are resolved by the OS keyboard layout. The keypad types digits when Num Lock
     * is on (text input); with it off the keypad keeps its OS navigation role. */
}

void Console_Update(void) {
    s_in.blink++; /* advance the cursor-blink phase once per frame */
    while (s_key_head != s_key_tail) {
        struct key_event *e = &s_key_queue[s_key_tail];
        Console_UpdateKey(e->scancode, e->keycode, e->mod);
        s_key_tail = (s_key_tail + 1) % CONSOLE_KEY_QUEUE_SIZE;
    }
}

/* --- Host-test hooks for the input/selection state machine. These drive the same
 * Console_UpdateKey path the live console runs after draining the event queue, so
 * tests exercise the real code without needing SDL scancode/modifier constants. --- */
static int testkey_scancode(Console_TestKey k) {
    switch (k) {
    case CONSOLE_TKEY_LEFT:
        return K_LEFT;
    case CONSOLE_TKEY_RIGHT:
        return K_RIGHT;
    case CONSOLE_TKEY_HOME:
        return K_HOME;
    case CONSOLE_TKEY_END:
        return K_END;
    case CONSOLE_TKEY_BACKSPACE:
        return K_BACKSPACE;
    case CONSOLE_TKEY_DELETE:
        return K_SUPPR;
    case CONSOLE_TKEY_ENTER:
        return K_ENTER;
    case CONSOLE_TKEY_ESC:
        return K_ESC;
    case CONSOLE_TKEY_TAB:
        return K_TAB;
    case CONSOLE_TKEY_UP:
        return K_UP;
    case CONSOLE_TKEY_DOWN:
        return K_DOWN;
    }
    return 0;
}

void Console_TestType(const char *s) {
    if (!s)
        return;
    for (; *s; s++)
        Console_UpdateKey(CONSOLE_TEXT_CHAR, (unsigned char)*s, 0);
}

void Console_TestSpecial(Console_TestKey key, int shift) {
    Console_UpdateKey(testkey_scancode(key), 0, shift ? SDL_KMOD_SHIFT : 0);
}

void Console_TestCtrl(char letter) {
    Console_UpdateKey(0, (unsigned char)letter, SDL_KMOD_CTRL);
}

const char *Console_TestInput(void) {
    return s_in.line;
}

int Console_TestCursor(void) {
    return s_in.cursor;
}

int Console_TestSelection(int *lo, int *hi) {
    return sel_span(lo, hi);
}

/* Map a Unicode codepoint to its CP437 byte (the AffStringToBuffer font), or 0 if
 * CP437 has no glyph for it. Covers the printable Latin-1 range the boot log and
 * command output use (the "·" separator, accented language names). Shares cp437_map
 * with cp437_to_unicode so the fold and unfold stay in lockstep. */
static unsigned char cp_to_cp437(unsigned int cp) {
    for (size_t i = 0; i < sizeof(cp437_map) / sizeof(cp437_map[0]); i++)
        if (cp437_map[i].cp == cp)
            return cp437_map[i].b;
    return 0;
}

/* Fold a UTF-8 string into the CP437 bytes the font draws. ASCII passes through;
 * mapped Latin-1 chars become their CP437 byte; anything unmapped becomes '?'.
 * Runs at draw time only — the stored scrollback stays UTF-8 so
 * Console_GetScrollback / --dump-state remain valid UTF-8. */
static void utf8_to_cp437(const char *src, char *dst, size_t dstsz) {
    size_t o = 0;
    const unsigned char *s = (const unsigned char *)src;
    while (*s && o + 1 < dstsz) {
        unsigned char c = *s;
        if (c < 0x80) { /* ASCII */
            dst[o++] = (char)c;
            s++;
            continue;
        }
        unsigned int cp = 0;
        if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
            cp = ((unsigned int)(c & 0x1F) << 6) | (s[1] & 0x3F);
            s += 2;
        } else if ((c & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
            s += 3; /* 3-byte: outside CP437's reach */
            dst[o++] = '?';
            continue;
        } else {
            s++; /* stray continuation / invalid byte */
            dst[o++] = '?';
            continue;
        }
        unsigned char mapped = cp_to_cp437(cp);
        dst[o++] = mapped ? (char)mapped : '?';
    }
    dst[o] = '\0';
}

/* Palette-independent RGB for the reserved console inks (see CONSOLE_COL_*). Any
 * other ink is drawn from the scene palette by the compositor; only these
 * reserved values resolve here, so "warn is yellow" / "banner is cyan" holds in
 * every scene. */
static void console_fixed_rgb(U8 idx, U8 *r, U8 *g, U8 *b) {
    switch (idx) {
    case CONSOLE_COL_BANNER:
        *r = 90;
        *g = 215;
        *b = 220;
        break; /* cyan (matches the terminal sink's bookend colour) */
    case CONSOLE_COL_DEBUG:
        *r = 130;
        *g = 130;
        *b = 130;
        break; /* dim grey */
    case CONSOLE_COL_WARN:
        *r = 235;
        *g = 205;
        *b = 50;
        break; /* yellow */
    case CONSOLE_COL_ERROR:
        *r = 235;
        *g = 70;
        *b = 60;
        break; /* red */
    default:
        *r = 255;
        *g = 255;
        *b = 255;
        break;
    }
}

/* Record a drawn row (CP437 bytes) into the snapshot used for mouse selection. */
static void record_row(int rowIdx, const char *cp437, int len) {
    if (rowIdx < 0 || rowIdx >= CONSOLE_MAX_ROWS)
        return;
    if (len >= CONSOLE_LINE_MAX)
        len = CONSOLE_LINE_MAX - 1;
    memcpy(s_r.row_text[rowIdx], cp437, (size_t)len);
    s_r.row_text[rowIdx][len] = '\0';
    s_r.row_len[rowIdx] = len;
}

/* Fill the per-row selection spans for the active mouse (scrollback) selection. */
static void fill_scrsel_spans(void) {
    int sr, sc, er, ec, r;
    scrsel_normalize(&sr, &sc, &er, &ec);
    for (r = sr; r <= er && r < s_r.row_count; r++) {
        int c0, c1;
        if (r < 0 || r >= CONSOLE_MAX_ROWS)
            continue;
        c0 = (r == sr) ? sc : 0;
        c1 = (r == er) ? ec : s_r.row_len[r];
        if (c0 < 0)
            c0 = 0;
        if (c1 > s_r.row_len[r])
            c1 = s_r.row_len[r];
        if (c1 > c0) {
            s_r.row_sel_start[r] = c0;
            s_r.row_sel_end[r] = c1;
        }
    }
}

/** Draw scrollback + input line into s_r.buf (call when open and buffer ready). */
static void Console_Draw(void) {
    int i, y, start, n;
    if (!s_r.buf || s_r.w == 0)
        return;

    memset(s_r.buf, 0, s_r.w * s_r.h);

    /* Visible lines for scrollback (leave one line for the input prompt). */
    n = (int)(s_r.h / CONSOLE_LINE_HEIGHT) - 1;
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

    /* Reset the per-frame row snapshot and selection spans. */
    s_r.row_count = 0;
    s_r.input_row = -1;
    for (i = 0; i < CONSOLE_MAX_ROWS; i++) {
        s_r.row_sel_start[i] = -1;
        s_r.row_sel_end[i] = -1;
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
            char rendered[CONSOLE_LINE_MAX];
            utf8_to_cp437(s_scrollback[idx], rendered, sizeof rendered);
            U8 col = s_scrollback_col[idx];
            AffStringToBuffer(s_r.buf, s_r.w, 8, y, rendered, col);
            record_row(y / CONSOLE_LINE_HEIGHT, rendered, (int)strlen(rendered));
            y += CONSOLE_LINE_HEIGHT;
        }

        /* Ellipsis at bottom when there are earlier lines above the current window. */
        if (showEllipsis) {
            AffStringToBuffer(s_r.buf, s_r.w, 8, y, "...", CONSOLE_TEXT_COLOUR);
            record_row(y / CONSOLE_LINE_HEIGHT, "...", 3);
            y += CONSOLE_LINE_HEIGHT;
        }
    }
    {
        char prompt[CONSOLE_INPUT_MAX + 8];
        char rendered[CONSOLE_INPUT_MAX + 8];
        snprintf(prompt, sizeof(prompt), "]> %s", s_in.line);
        utf8_to_cp437(prompt, rendered, sizeof rendered);
        AffStringToBuffer(s_r.buf, s_r.w, 8, y, rendered, CONSOLE_TEXT_COLOUR);
        s_r.input_row = y / CONSOLE_LINE_HEIGHT;
        record_row(s_r.input_row, rendered, (int)strlen(rendered));
    }
    s_r.row_count = s_r.input_row + 1; /* rows recorded, including the input row */

    /* Selection spans. The editable input-line selection ("]> " is a 3-char prefix)
     * occupies the input row; a mouse scrollback selection (mutually exclusive)
     * flows across its rows. The compositor inverts the cells in these spans. */
    {
        int lo, hi;
        if (sel_span(&lo, &hi) && s_r.input_row >= 0 && s_r.input_row < CONSOLE_MAX_ROWS) {
            s_r.row_sel_start[s_r.input_row] = 3 + lo;
            s_r.row_sel_end[s_r.input_row] = 3 + hi;
        } else if (s_scrsel.active) {
            fill_scrsel_spans();
        }
    }

    /* Blinking underline cursor at the edit position, off for ~half of each cycle.
     * Hidden while the input row is selected (the block marks the spot). */
    {
        int sel_on_input = (s_r.input_row >= 0 && s_r.input_row < CONSOLE_MAX_ROWS && s_r.row_sel_start[s_r.input_row] >= 0);
        if (!sel_on_input && s_r.input_row >= 0 && ((s_in.blink >> 5) & 1u) == 0u) {
            S32 cx = 8 + (3 + s_in.cursor) * 8;
            S32 cy = (S32)s_r.input_row * CONSOLE_LINE_HEIGHT;
            if (cx + 8 <= (S32)s_r.w)
                AffStringToBuffer(s_r.buf, s_r.w, cx, cy, "_", CONSOLE_TEXT_COLOUR);
        }
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

    if (s_r.buf == NULL || s_r.w != width) {
        free(s_r.buf);
        s_r.w = width;
        /* Top half of the screen, with a small minimum so at least a few lines show. */
        s_r.h = height / 2;
        if (s_r.h < (U32)(CONSOLE_LINE_HEIGHT * 4))
            s_r.h = (height < (U32)(CONSOLE_LINE_HEIGHT * 4)) ? height : (U32)(CONSOLE_LINE_HEIGHT * 4);
        s_r.buf = (U8 *)malloc(s_r.w * s_r.h);
        if (!s_r.buf)
            return;
    }

    Console_Draw();

    const U8 *borderCol = palette_rgb + (CONSOLE_TEXT_COLOUR * 3);

    /* Use byte-based row offset so pitch need not be a multiple of sizeof(U32). */
    for (y = 0; y < s_r.h; y++) {
        U32 *dstRow = (U32 *)((U8 *)frameBuffer + y * pitch);
        U8 *srcRow = s_r.buf + y * s_r.w;
        /* Per-row selection span (cells in [rs, re) invert). Hoisted out of the x
         * loop because the row index is constant across the scanline. */
        int srow = (int)(y / CONSOLE_LINE_HEIGHT);
        int rs = (srow < s_r.row_count && srow < CONSOLE_MAX_ROWS) ? s_r.row_sel_start[srow] : -1;
        int re = (rs >= 0) ? s_r.row_sel_end[srow] : -1;
        for (x = 0; x < s_r.w; x++) {
            U8 idx = srcRow[x];
            U32 base = dstRow[x];
            U8 br = (U8)((base >> 16) & 0xFF);
            U8 bg = (U8)((base >> 8) & 0xFF);
            U8 bb = (U8)(base & 0xFF);
            U8 r, g, b;
            int scol = ((S32)x >= 8) ? (int)((x - 8) / 8) : -1;
            int selected = (rs >= 0 && scol >= rs && scol < re);

            /* Bottom border line of the console panel (drawn first so a selection on
             * the input row can't paint over it). */
            if (y + 1 == s_r.h) {
                r = borderCol[0];
                g = borderCol[1];
                b = borderCol[2];
            }
            /* Selection: inverted block. Selected cells render opaque (paper bright,
             * glyph dark), so the highlight reads the same over any scene. */
            else if (selected) {
                if (idx == 0)
                    r = g = b = 230; /* selection bar (bright paper) */
                else
                    r = g = b = 20; /* selected glyph (dark ink) */
            } else if (idx == 0) {
                /* Background: darken underlying game image for a translucent effect. */
                r = (U8)((br * 3) / 5);
                g = (U8)((bg * 3) / 5);
                b = (U8)((bb * 3) / 5);
            } else if (idx >= CONSOLE_COL_BANNER && idx <= CONSOLE_COL_ERROR) {
                /* Reserved console inks (banner/severity): fixed RGB. */
                console_fixed_rgb(idx, &r, &g, &b);
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

/* Map a window-space mouse position to a console (row, col) cell. Returns 1 when
 * the point is inside the console panel, with col clamped to the row's length. */
static int mouse_to_cell(float winx, float winy, int *row, int *col) {
    S32 sx, sy;
    int r, c;
    WindowToSurfaceCoords((S32)winx, (S32)winy, &sx, &sy);
    if (sy < 0 || (U32)sy >= s_r.h || s_r.row_count <= 0)
        return 0;
    r = sy / CONSOLE_LINE_HEIGHT;
    if (r >= s_r.row_count)
        r = s_r.row_count - 1;
    if (r < 0)
        return 0;
    c = (sx >= 8) ? (sx - 8) / 8 : 0;
    if (c < 0)
        c = 0;
    if (c > s_r.row_len[r])
        c = s_r.row_len[r];
    *row = r;
    *col = c;
    return 1;
}

int Console_FeedEvent(const void *sdlEvent) {
    const SDL_Event *ev = (const SDL_Event *)sdlEvent;

    /* Mouse drag selects text in the console; the OS cursor (shown on open) is the
     * pointer. Left button down anchors, motion extends, double-click selects a word,
     * and a click on the input row places the edit cursor. Copy with Ctrl+C; a right
     * click copies and clears. While open the console owns the mouse. */
    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        int row, col, lo, hi;
        Uint64 now;
        int dbl;
        if (!s_console_open)
            return 0;
        if (ev->button.button == SDL_BUTTON_RIGHT) {
            /* Right-click copies the current selection, then clears it. */
            if (s_scrsel.active || sel_span(&lo, &hi))
                clipboard_copy();
            s_scrsel.active = 0;
            s_in.sel_anchor = -1;
            return 1;
        }
        if (ev->button.button != SDL_BUTTON_LEFT)
            return 1;
        if (!mouse_to_cell(ev->button.x, ev->button.y, &row, &col)) {
            /* Left-click below the panel (in the game area) closes the console. */
            Console_Close();
            return 1;
        }
        now = SDL_GetTicks();
        dbl = (now - s_scrsel.last_click_ms <= CONSOLE_DBLCLICK_MS && row == s_scrsel.last_click_row &&
               (col >= s_scrsel.last_click_col - 1 && col <= s_scrsel.last_click_col + 1));
        s_scrsel.last_click_ms = now;
        s_scrsel.last_click_row = row;
        s_scrsel.last_click_col = col;
        s_in.sel_anchor = -1; /* drop any editable input-line selection */

        if (dbl) {
            /* Select the word under the cell. */
            const char *t = s_r.row_text[row];
            int len = s_r.row_len[row];
            int a = col, e = col;
            if (col < len && is_word_char((unsigned char)t[col])) {
                while (a > 0 && is_word_char((unsigned char)t[a - 1]))
                    a--;
                while (e < len && is_word_char((unsigned char)t[e]))
                    e++;
            } else if (col < len) {
                e = col + 1; /* a single non-word cell */
            }
            s_scrsel.a_row = s_scrsel.c_row = row;
            s_scrsel.a_col = a;
            s_scrsel.c_col = e;
            s_scrsel.active = (e > a);
            s_scrsel.dragging = 0;
        } else {
            s_scrsel.a_row = s_scrsel.c_row = row;
            s_scrsel.a_col = s_scrsel.c_col = col;
            s_scrsel.active = 0; /* nothing until the caret moves */
            s_scrsel.dragging = 1;
            if (row == s_r.input_row) {
                int p = col - 3; /* input text starts at column 3 ("]> ") */
                if (p < 0)
                    p = 0;
                if (p > s_in.len)
                    p = s_in.len;
                s_in.cursor = p;
            }
        }
        return 1;
    }
    if (ev->type == SDL_EVENT_MOUSE_MOTION) {
        int row, col;
        if (!s_console_open) {
            s_scrsel.dragging = 0;
            return 0;
        }
        if (!s_scrsel.dragging)
            return 0; /* let the game keep tracking the pointer */
        if (mouse_to_cell(ev->motion.x, ev->motion.y, &row, &col)) {
            s_scrsel.c_row = row;
            s_scrsel.c_col = col;
            s_scrsel.active = !(s_scrsel.a_row == row && s_scrsel.a_col == col);
        }
        return 1;
    }
    if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (!s_console_open)
            return 0;
        if (ev->button.button == SDL_BUTTON_LEFT)
            s_scrsel.dragging = 0;
        return 1; /* console owns the mouse while open */
    }

    /* Mouse wheel scrolls the scrollback while the console is open. */
    if (ev->type == SDL_EVENT_MOUSE_WHEEL) {
        if (!s_console_open)
            return 0;
        if (ev->wheel.y > 0)
            Console_ScrollLines(3); /* wheel up -> older lines */
        else if (ev->wheel.y < 0)
            Console_ScrollLines(-3);
        return 1;
    }

    /* Typed characters arrive here already cased and layout-resolved (uppercase,
     * shifted symbols, AZERTY, numpad-with-NumLock). Queue the printable ASCII
     * ones as CONSOLE_TEXT_CHAR events; physical keys come through KEY_DOWN below. */
    if (ev->type == SDL_EVENT_TEXT_INPUT) {
        const char *t;
        if (!s_console_open)
            return 0;
        for (t = ev->text.text; t && *t; t++) {
            unsigned char c = (unsigned char)*t;
            int next;
            if (c < 32 || c >= 127)
                continue; /* ASCII printable only, like paste */
            next = (s_key_head + 1) % CONSOLE_KEY_QUEUE_SIZE;
            if (next == s_key_tail)
                break;
            s_key_queue[s_key_head].scancode = CONSOLE_TEXT_CHAR;
            s_key_queue[s_key_head].keycode = (int)c;
            s_key_queue[s_key_head].mod = 0;
            s_key_head = next;
        }
        return 1;
    }

    if (ev->type != SDL_EVENT_KEY_DOWN)
        return 0;

    SDL_Scancode scan = ev->key.scancode;
    /* Keycode is used only for the Ctrl combos below (paste/copy/clear); character
     * entry is handled by text input above, so no Shift/layout fix-up is needed. */
    SDL_Keycode key = SDL_GetKeyFromScancode(scan, ev->key.mod, true);

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
    s_key_queue[s_key_head].mod = (int)ev->key.mod;
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
    if (s_cmd_count >= CONSOLE_MAX_CMDS) {
        fprintf(stderr, "console: command table full (%d), dropped '%s' (raise CONSOLE_MAX_CMDS)\n",
                CONSOLE_MAX_CMDS, name ? name : "(null)");
        return;
    }
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
    if (s_cvar_count >= CONSOLE_MAX_CVARS) {
        fprintf(stderr, "console: cvar table full (%d), dropped '%s' (raise CONSOLE_MAX_CVARS)\n",
                CONSOLE_MAX_CVARS, name ? name : "(null)");
        return;
    }
    s_cvars[s_cvar_count].name = name;
    s_cvars[s_cvar_count].ptr = ptr;
    s_cvars[s_cvar_count].type = type;
    s_cvars[s_cvar_count].help = help;
    s_cvar_count++;
}

void Console_Execute(const char *line);

static void Console_SubmitLine(void) {
    if (s_in.len <= 0)
        return;
    s_in.line[s_in.len] = '\0';
    scrollback_push_one(s_in.line, CONSOLE_COL_DEFAULT);
    /* Add to history, skipping a verbatim repeat of the previous entry. */
    if (!(s_history_count > 0 && strcmp(s_history[s_history_count - 1], s_in.line) == 0)) {
        if (s_history_count < CONSOLE_HISTORY_SIZE) {
            strncpy(s_history[s_history_count], s_in.line, CONSOLE_LINE_MAX - 1);
            s_history[s_history_count][CONSOLE_LINE_MAX - 1] = '\0';
            s_history_count++;
        } else {
            int j;
            for (j = 0; j < CONSOLE_HISTORY_SIZE - 1; j++)
                strcpy(s_history[j], s_history[j + 1]);
            strncpy(s_history[CONSOLE_HISTORY_SIZE - 1], s_in.line, CONSOLE_LINE_MAX - 1);
            s_history[CONSOLE_HISTORY_SIZE - 1][CONSOLE_LINE_MAX - 1] = '\0';
        }
    }
    Console_Execute(s_in.line);
    input_clear();
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
