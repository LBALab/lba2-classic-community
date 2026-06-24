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
#define CONSOLE_MAX_CMDS 32
#define CONSOLE_MAX_CVARS 16
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
static char s_input_line[CONSOLE_INPUT_MAX];
static int s_input_len = 0;
static int s_cursor_pos = 0;
static int s_sel_anchor = -1; /* selection anchor in the input line; -1 = no selection */
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

/* Snapshot of the rows as drawn (CP437 bytes, one per cell), filled by Console_Draw
 * each frame. Row r sits at pixel y = r * CONSOLE_LINE_HEIGHT. Used to map mouse
 * clicks to cells and to extract selected text for the clipboard. */
static char s_row_text[CONSOLE_MAX_ROWS][CONSOLE_LINE_MAX];
static int s_row_len[CONSOLE_MAX_ROWS];
static int s_row_count = 0;
static int s_input_row = -1; /* screen-row index of the input line */

/* Per-row selection column span [start,end), consumed by the compositor (inverts the
 * cells). -1 = no selection on that row. Holds either the input-line selection (one
 * row) or a mouse scrollback selection (a flow across rows). */
static int s_row_sel_start[CONSOLE_MAX_ROWS];
static int s_row_sel_end[CONSOLE_MAX_ROWS];

/* Mouse scrollback selection in screen row/col space. Read-only: copy with Ctrl+C.
 * Mutually exclusive with the editable input-line selection (s_sel_anchor). */
static int s_scrsel_active = 0;
static int s_scrsel_a_row = 0, s_scrsel_a_col = 0; /* anchor cell */
static int s_scrsel_c_row = 0, s_scrsel_c_col = 0; /* caret cell */
static int s_mouse_dragging = 0;
static Uint64 s_last_click_ms = 0;
static int s_last_click_row = -1, s_last_click_col = -1;

/* Screenshot without console: path and format. PNG = game performs SavePNG; else PrePresent writes PPM. */
#define CONSOLE_SCREENSHOT_PATH_MAX 256
static char s_screenshot_path[CONSOLE_SCREENSHOT_PATH_MAX] = "";
static int s_screenshot_png = 0;
static Console_LineSinkFn s_line_sink_for_tests = NULL;

static void Console_SubmitLine(void);

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
    if (s_console_open) {
        console_set_text_input(0);
        console_set_os_cursor(0);
    }
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
        s_sel_anchor = -1;
        s_scrsel_active = 0;
        s_mouse_dragging = 0;
        console_set_text_input(1);
        console_set_os_cursor(1);
    } else {
        console_set_text_input(0);
        console_set_os_cursor(0);
        s_scrsel_active = 0;
        s_mouse_dragging = 0;
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
    console_set_text_input(0);
    console_set_os_cursor(0);
    s_scrsel_active = 0;
    s_mouse_dragging = 0;
    free(s_console_buf);
    s_console_buf = NULL;
    s_console_w = 0;
    s_input_len = 0;
    s_input_line[0] = '\0';
    s_cursor_pos = 0;
    s_just_closed = 1;
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

/* --- Input-line editing primitives. All operate at s_cursor_pos so the line can
 * be edited mid-string (Left/Right/Home/End/Delete), not just appended to. --- */
static void input_insert_char(char c) {
    if (s_input_len >= CONSOLE_INPUT_MAX - 1)
        return;
    memmove(&s_input_line[s_cursor_pos + 1], &s_input_line[s_cursor_pos],
            (size_t)(s_input_len - s_cursor_pos));
    s_input_line[s_cursor_pos] = c;
    s_input_len++;
    s_cursor_pos++;
    s_input_line[s_input_len] = '\0';
}

static void input_backspace(void) {
    if (s_cursor_pos <= 0)
        return;
    memmove(&s_input_line[s_cursor_pos - 1], &s_input_line[s_cursor_pos],
            (size_t)(s_input_len - s_cursor_pos));
    s_cursor_pos--;
    s_input_len--;
    s_input_line[s_input_len] = '\0';
}

static void input_delete(void) { /* delete the char *at* the cursor (Del key) */
    if (s_cursor_pos >= s_input_len)
        return;
    memmove(&s_input_line[s_cursor_pos], &s_input_line[s_cursor_pos + 1],
            (size_t)(s_input_len - s_cursor_pos - 1));
    s_input_len--;
    s_input_line[s_input_len] = '\0';
}

static void input_clear(void) {
    s_input_len = 0;
    s_cursor_pos = 0;
    s_input_line[0] = '\0';
    s_history_index = -1; /* a cleared line starts history browsing fresh */
    s_sel_anchor = -1;
}

/* Resolve the active selection into [lo, hi) input-line indices. Returns 0 when
 * there is no selection (no anchor, or anchor == cursor). */
static int sel_span(int *lo, int *hi) {
    if (s_sel_anchor < 0 || s_sel_anchor == s_cursor_pos)
        return 0;
    if (s_sel_anchor < s_cursor_pos) {
        *lo = s_sel_anchor;
        *hi = s_cursor_pos;
    } else {
        *lo = s_cursor_pos;
        *hi = s_sel_anchor;
    }
    return 1;
}

/* Delete the selected span (if any). Returns 1 if it removed something. */
static int sel_delete(void) {
    int lo, hi;
    if (!sel_span(&lo, &hi))
        return 0;
    memmove(&s_input_line[lo], &s_input_line[hi], (size_t)(s_input_len - hi));
    s_input_len -= (hi - lo);
    s_input_line[s_input_len] = '\0';
    s_cursor_pos = lo;
    s_sel_anchor = -1;
    return 1;
}

/* Order the mouse-selection anchor/caret into reading order (start <= end). */
static void scrsel_normalize(int *sr, int *sc, int *er, int *ec) {
    int before = (s_scrsel_a_row < s_scrsel_c_row) ||
                 (s_scrsel_a_row == s_scrsel_c_row && s_scrsel_a_col <= s_scrsel_c_col);
    if (before) {
        *sr = s_scrsel_a_row;
        *sc = s_scrsel_a_col;
        *er = s_scrsel_c_row;
        *ec = s_scrsel_c_col;
    } else {
        *sr = s_scrsel_c_row;
        *sc = s_scrsel_c_col;
        *er = s_scrsel_a_row;
        *ec = s_scrsel_a_col;
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

/* Unicode codepoint for a CP437 high byte (the inverse of cp_to_cp437, for the
 * bytes the console renders), or 0 when there is no mapping. Lets the CP437 row
 * snapshot copy back out as proper UTF-8 (e.g. the boot log's "·" separator). */
static unsigned int cp437_to_unicode(unsigned char b) {
    switch (b) {
    case 0xFA:
        return 0x00B7; /* · */
    case 0xF8:
        return 0x00B0; /* ° */
    case 0xAD:
        return 0x00A1; /* ¡ */
    case 0xA8:
        return 0x00BF; /* ¿ */
    case 0xAE:
        return 0x00AB; /* « */
    case 0xAF:
        return 0x00BB; /* » */
    case 0x9C:
        return 0x00A3; /* £ */
    case 0x80:
        return 0x00C7; /* Ç */
    case 0x81:
        return 0x00FC; /* ü */
    case 0x82:
        return 0x00E9; /* é */
    case 0x83:
        return 0x00E2; /* â */
    case 0x84:
        return 0x00E4; /* ä */
    case 0x85:
        return 0x00E0; /* à */
    case 0x86:
        return 0x00E5; /* å */
    case 0x87:
        return 0x00E7; /* ç */
    case 0x88:
        return 0x00EA; /* ê */
    case 0x89:
        return 0x00EB; /* ë */
    case 0x8A:
        return 0x00E8; /* è */
    case 0x8B:
        return 0x00EF; /* ï */
    case 0x8C:
        return 0x00EE; /* î */
    case 0x8D:
        return 0x00EC; /* ì */
    case 0x8E:
        return 0x00C4; /* Ä */
    case 0x8F:
        return 0x00C5; /* Å */
    case 0x90:
        return 0x00C9; /* É */
    case 0x91:
        return 0x00E6; /* æ */
    case 0x92:
        return 0x00C6; /* Æ */
    case 0x93:
        return 0x00F4; /* ô */
    case 0x94:
        return 0x00F6; /* ö */
    case 0x95:
        return 0x00F2; /* ò */
    case 0x96:
        return 0x00FB; /* û */
    case 0x97:
        return 0x00F9; /* ù */
    case 0x98:
        return 0x00FF; /* ÿ */
    case 0x99:
        return 0x00D6; /* Ö */
    case 0x9A:
        return 0x00DC; /* Ü */
    case 0xA0:
        return 0x00E1; /* á */
    case 0xA1:
        return 0x00ED; /* í */
    case 0xA2:
        return 0x00F3; /* ó */
    case 0xA3:
        return 0x00FA; /* ú */
    case 0xA4:
        return 0x00F1; /* ñ */
    case 0xA5:
        return 0x00D1; /* Ñ */
    case 0xE1:
        return 0x00DF; /* ß */
    default:
        return 0;
    }
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
    if (!s_scrsel_active)
        return;
    scrsel_normalize(&sr, &sc, &er, &ec);
    for (r = sr; r <= er && r < s_row_count; r++) {
        int c0 = (r == sr) ? sc : 0;
        int c1 = (r == er) ? ec : s_row_len[r];
        int c;
        if (c0 < 0)
            c0 = 0;
        if (c1 > s_row_len[r])
            c1 = s_row_len[r];
        for (c = c0; c < c1; c++)
            copybuf_put_cell(buf, sizeof buf, &o, (unsigned char)s_row_text[r][c]);
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
    if (s_scrsel_active) {
        scrsel_copy();
    } else if (sel_span(&lo, &hi)) {
        char tmp[CONSOLE_INPUT_MAX];
        int n = hi - lo;
        memcpy(tmp, &s_input_line[lo], (size_t)n);
        tmp[n] = '\0';
        SDL_SetClipboardText(tmp);
    } else {
        SDL_SetClipboardText(s_input_line);
    }
}

/* Scroll the scrollback view; delta > 0 reveals older lines. Clamped to the
 * visible window exactly as Console_Draw clamps. Shared by PgUp/PgDn and wheel. */
static void Console_ScrollLines(int delta) {
    int n = (int)(s_console_h / CONSOLE_LINE_HEIGHT) - 1;
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
    strncpy(s_input_line, s_history[s_history_index], CONSOLE_INPUT_MAX - 1);
    s_input_line[CONSOLE_INPUT_MAX - 1] = '\0';
    s_input_len = (int)strlen(s_input_line);
    s_cursor_pos = s_input_len;
    s_sel_anchor = -1; /* a recalled line carries no selection */
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

    for (i = 0; i < s_cursor_pos; i++)
        if (s_input_line[i] == ' ' || s_input_line[i] == '\t')
            return; /* cursor is past the command word */
    tlen = s_cursor_pos;
    if (tlen <= 0)
        return;
    if (tlen >= (int)sizeof(token))
        tlen = (int)sizeof(token) - 1;
    memcpy(token, s_input_line, (size_t)tlen);
    token[tlen] = '\0';

    nmatch = Console_CompletePrefix(token, lcp, sizeof lcp, matches, maxmatch);
    if (nmatch <= 0)
        return;

    /* Replace the typed token with the common prefix (or the full unique name). */
    newlen = (int)strlen(lcp);
    tail = s_input_len - s_cursor_pos;
    if (newlen + tail >= CONSOLE_INPUT_MAX)
        return;
    memmove(&s_input_line[newlen], &s_input_line[s_cursor_pos], (size_t)tail);
    memcpy(s_input_line, lcp, (size_t)newlen);
    s_input_len = newlen + tail;
    s_cursor_pos = newlen;
    s_input_line[s_input_len] = '\0';

    if (nmatch == 1) {
        if (s_cursor_pos == s_input_len && s_input_len < CONSOLE_INPUT_MAX - 1) {
            s_input_line[s_input_len++] = ' ';
            s_input_line[s_input_len] = '\0';
            s_cursor_pos = s_input_len;
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

    s_cursor_blink = 0; /* any keypress makes the cursor immediately visible */

    /* A mouse (scrollback) selection is dismissed by editing/navigation keys, but
     * not by the copy shortcut or by a bare modifier press (so Ctrl, then C, keeps
     * the selection long enough to copy it). */
    if (s_scrsel_active && !(ctrl && kl == 'c') && !is_modifier_scancode(scancode))
        s_scrsel_active = 0;

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
            s_scrsel_active = 0; /* copying clears the selection */
            s_sel_anchor = -1;
        } else if (kl == 'x') {
            clipboard_copy();  /* copies the selection, or the whole line */
            if (!sel_delete()) /* delete the selection; if there was none, */
                input_clear(); /* cut the whole line we just copied */
        } else if (kl == 'a') {
            s_sel_anchor = 0; /* select all */
            s_cursor_pos = s_input_len;
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
        if (s_input_len > 0)
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
            if (s_sel_anchor < 0)
                s_sel_anchor = s_cursor_pos;
            if (s_cursor_pos > 0)
                s_cursor_pos--;
        } else if (sel_span(&lo, &hi)) {
            s_cursor_pos = lo;
            s_sel_anchor = -1;
        } else if (s_cursor_pos > 0) {
            s_cursor_pos--;
        }
        return;
    }
    if (scancode == K_RIGHT) {
        int lo, hi;
        if (mod & SDL_KMOD_SHIFT) {
            if (s_sel_anchor < 0)
                s_sel_anchor = s_cursor_pos;
            if (s_cursor_pos < s_input_len)
                s_cursor_pos++;
        } else if (sel_span(&lo, &hi)) {
            s_cursor_pos = hi;
            s_sel_anchor = -1;
        } else if (s_cursor_pos < s_input_len) {
            s_cursor_pos++;
        }
        return;
    }
    if (scancode == K_HOME) {
        if (mod & SDL_KMOD_SHIFT) {
            if (s_sel_anchor < 0)
                s_sel_anchor = s_cursor_pos;
        } else {
            s_sel_anchor = -1;
        }
        s_cursor_pos = 0;
        return;
    }
    if (scancode == K_END) {
        if (mod & SDL_KMOD_SHIFT) {
            if (s_sel_anchor < 0)
                s_sel_anchor = s_cursor_pos;
        } else {
            s_sel_anchor = -1;
        }
        s_cursor_pos = s_input_len;
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
        int page = (int)(s_console_h / CONSOLE_LINE_HEIGHT) - 1;
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
    s_cursor_blink++; /* advance the cursor-blink phase once per frame */
    while (s_key_head != s_key_tail) {
        struct key_event *e = &s_key_queue[s_key_tail];
        Console_UpdateKey(e->scancode, e->keycode, e->mod);
        s_key_tail = (s_key_tail + 1) % CONSOLE_KEY_QUEUE_SIZE;
    }
}

/* Map a Unicode codepoint to its CP437 byte (the AffStringToBuffer font), or 0
 * if CP437 has no glyph for it. Covers the printable Latin-1 range — enough for
 * the boot log's "·" separator and accented language names (Français, Español)
 * and any Western-European text a command might print. */
static unsigned char cp_to_cp437(unsigned int cp) {
    switch (cp) {
    case 0x00B7:
        return 0xFA; /* · middle dot (the title separator) */
    case 0x00B0:
        return 0xF8; /* ° */
    case 0x00A1:
        return 0xAD; /* ¡ */
    case 0x00BF:
        return 0xA8; /* ¿ */
    case 0x00AB:
        return 0xAE; /* « */
    case 0x00BB:
        return 0xAF; /* » */
    case 0x00A3:
        return 0x9C; /* £ */
    case 0x00C7:
        return 0x80; /* Ç */
    case 0x00FC:
        return 0x81; /* ü */
    case 0x00E9:
        return 0x82; /* é */
    case 0x00E2:
        return 0x83; /* â */
    case 0x00E4:
        return 0x84; /* ä */
    case 0x00E0:
        return 0x85; /* à */
    case 0x00E5:
        return 0x86; /* å */
    case 0x00E7:
        return 0x87; /* ç (Français) */
    case 0x00EA:
        return 0x88; /* ê */
    case 0x00EB:
        return 0x89; /* ë */
    case 0x00E8:
        return 0x8A; /* è */
    case 0x00EF:
        return 0x8B; /* ï */
    case 0x00EE:
        return 0x8C; /* î */
    case 0x00EC:
        return 0x8D; /* ì */
    case 0x00C4:
        return 0x8E; /* Ä */
    case 0x00C5:
        return 0x8F; /* Å */
    case 0x00C9:
        return 0x90; /* É */
    case 0x00E6:
        return 0x91; /* æ */
    case 0x00C6:
        return 0x92; /* Æ */
    case 0x00F4:
        return 0x93; /* ô */
    case 0x00F6:
        return 0x94; /* ö */
    case 0x00F2:
        return 0x95; /* ò */
    case 0x00FB:
        return 0x96; /* û */
    case 0x00F9:
        return 0x97; /* ù */
    case 0x00FF:
        return 0x98; /* ÿ */
    case 0x00D6:
        return 0x99; /* Ö */
    case 0x00DC:
        return 0x9A; /* Ü */
    case 0x00E1:
        return 0xA0; /* á */
    case 0x00ED:
        return 0xA1; /* í */
    case 0x00F3:
        return 0xA2; /* ó */
    case 0x00FA:
        return 0xA3; /* ú */
    case 0x00F1:
        return 0xA4; /* ñ (Español) */
    case 0x00D1:
        return 0xA5; /* Ñ */
    case 0x00DF:
        return 0xE1; /* ß */
    default:
        return 0;
    }
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
    memcpy(s_row_text[rowIdx], cp437, (size_t)len);
    s_row_text[rowIdx][len] = '\0';
    s_row_len[rowIdx] = len;
}

/* Fill the per-row selection spans for the active mouse (scrollback) selection. */
static void fill_scrsel_spans(void) {
    int sr, sc, er, ec, r;
    scrsel_normalize(&sr, &sc, &er, &ec);
    for (r = sr; r <= er && r < s_row_count; r++) {
        int c0, c1;
        if (r < 0 || r >= CONSOLE_MAX_ROWS)
            continue;
        c0 = (r == sr) ? sc : 0;
        c1 = (r == er) ? ec : s_row_len[r];
        if (c0 < 0)
            c0 = 0;
        if (c1 > s_row_len[r])
            c1 = s_row_len[r];
        if (c1 > c0) {
            s_row_sel_start[r] = c0;
            s_row_sel_end[r] = c1;
        }
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

    /* Reset the per-frame row snapshot and selection spans. */
    s_row_count = 0;
    s_input_row = -1;
    for (i = 0; i < CONSOLE_MAX_ROWS; i++) {
        s_row_sel_start[i] = -1;
        s_row_sel_end[i] = -1;
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
            AffStringToBuffer(s_console_buf, s_console_w, 8, y, rendered, col);
            record_row(y / CONSOLE_LINE_HEIGHT, rendered, (int)strlen(rendered));
            y += CONSOLE_LINE_HEIGHT;
        }

        /* Ellipsis at bottom when there are earlier lines above the current window. */
        if (showEllipsis) {
            AffStringToBuffer(s_console_buf, s_console_w, 8, y, "...", CONSOLE_TEXT_COLOUR);
            record_row(y / CONSOLE_LINE_HEIGHT, "...", 3);
            y += CONSOLE_LINE_HEIGHT;
        }
    }
    {
        char prompt[CONSOLE_INPUT_MAX + 8];
        char rendered[CONSOLE_INPUT_MAX + 8];
        snprintf(prompt, sizeof(prompt), "]> %s", s_input_line);
        utf8_to_cp437(prompt, rendered, sizeof rendered);
        AffStringToBuffer(s_console_buf, s_console_w, 8, y, rendered, CONSOLE_TEXT_COLOUR);
        s_input_row = y / CONSOLE_LINE_HEIGHT;
        record_row(s_input_row, rendered, (int)strlen(rendered));
    }
    s_row_count = s_input_row + 1; /* rows recorded, including the input row */

    /* Selection spans. The editable input-line selection ("]> " is a 3-char prefix)
     * occupies the input row; a mouse scrollback selection (mutually exclusive)
     * flows across its rows. The compositor inverts the cells in these spans. */
    {
        int lo, hi;
        if (sel_span(&lo, &hi) && s_input_row >= 0 && s_input_row < CONSOLE_MAX_ROWS) {
            s_row_sel_start[s_input_row] = 3 + lo;
            s_row_sel_end[s_input_row] = 3 + hi;
        } else if (s_scrsel_active) {
            fill_scrsel_spans();
        }
    }

    /* Blinking underline cursor at the edit position, off for ~half of each cycle.
     * Hidden while the input row is selected (the block marks the spot). */
    {
        int sel_on_input = (s_input_row >= 0 && s_input_row < CONSOLE_MAX_ROWS && s_row_sel_start[s_input_row] >= 0);
        if (!sel_on_input && s_input_row >= 0 && ((s_cursor_blink >> 5) & 1u) == 0u) {
            S32 cx = 8 + (3 + s_cursor_pos) * 8;
            S32 cy = (S32)s_input_row * CONSOLE_LINE_HEIGHT;
            if (cx + 8 <= (S32)s_console_w)
                AffStringToBuffer(s_console_buf, s_console_w, cx, cy, "_", CONSOLE_TEXT_COLOUR);
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
        /* Per-row selection span (cells in [rs, re) invert). Hoisted out of the x
         * loop because the row index is constant across the scanline. */
        int srow = (int)(y / CONSOLE_LINE_HEIGHT);
        int rs = (srow < s_row_count && srow < CONSOLE_MAX_ROWS) ? s_row_sel_start[srow] : -1;
        int re = (rs >= 0) ? s_row_sel_end[srow] : -1;
        for (x = 0; x < s_console_w; x++) {
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
            if (y + 1 == s_console_h) {
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
    if (sy < 0 || (U32)sy >= s_console_h || s_row_count <= 0)
        return 0;
    r = sy / CONSOLE_LINE_HEIGHT;
    if (r >= s_row_count)
        r = s_row_count - 1;
    if (r < 0)
        return 0;
    c = (sx >= 8) ? (sx - 8) / 8 : 0;
    if (c < 0)
        c = 0;
    if (c > s_row_len[r])
        c = s_row_len[r];
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
            if (s_scrsel_active || sel_span(&lo, &hi))
                clipboard_copy();
            s_scrsel_active = 0;
            s_sel_anchor = -1;
            return 1;
        }
        if (ev->button.button != SDL_BUTTON_LEFT)
            return 1;
        if (!mouse_to_cell(ev->button.x, ev->button.y, &row, &col)) {
            /* Click outside the panel clears the selection. */
            s_scrsel_active = 0;
            s_sel_anchor = -1;
            s_mouse_dragging = 0;
            return 1;
        }
        now = SDL_GetTicks();
        dbl = (now - s_last_click_ms <= CONSOLE_DBLCLICK_MS && row == s_last_click_row &&
               (col >= s_last_click_col - 1 && col <= s_last_click_col + 1));
        s_last_click_ms = now;
        s_last_click_row = row;
        s_last_click_col = col;
        s_sel_anchor = -1; /* drop any editable input-line selection */

        if (dbl) {
            /* Select the word under the cell. */
            const char *t = s_row_text[row];
            int len = s_row_len[row];
            int a = col, e = col;
            if (col < len && is_word_char((unsigned char)t[col])) {
                while (a > 0 && is_word_char((unsigned char)t[a - 1]))
                    a--;
                while (e < len && is_word_char((unsigned char)t[e]))
                    e++;
            } else if (col < len) {
                e = col + 1; /* a single non-word cell */
            }
            s_scrsel_a_row = s_scrsel_c_row = row;
            s_scrsel_a_col = a;
            s_scrsel_c_col = e;
            s_scrsel_active = (e > a);
            s_mouse_dragging = 0;
        } else {
            s_scrsel_a_row = s_scrsel_c_row = row;
            s_scrsel_a_col = s_scrsel_c_col = col;
            s_scrsel_active = 0; /* nothing until the caret moves */
            s_mouse_dragging = 1;
            if (row == s_input_row) {
                int p = col - 3; /* input text starts at column 3 ("]> ") */
                if (p < 0)
                    p = 0;
                if (p > s_input_len)
                    p = s_input_len;
                s_cursor_pos = p;
            }
        }
        return 1;
    }
    if (ev->type == SDL_EVENT_MOUSE_MOTION) {
        int row, col;
        if (!s_console_open) {
            s_mouse_dragging = 0;
            return 0;
        }
        if (!s_mouse_dragging)
            return 0; /* let the game keep tracking the pointer */
        if (mouse_to_cell(ev->motion.x, ev->motion.y, &row, &col)) {
            s_scrsel_c_row = row;
            s_scrsel_c_col = col;
            s_scrsel_active = !(s_scrsel_a_row == row && s_scrsel_a_col == col);
        }
        return 1;
    }
    if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (!s_console_open)
            return 0;
        if (ev->button.button == SDL_BUTTON_LEFT)
            s_mouse_dragging = 0;
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
    scrollback_push_one(s_input_line, CONSOLE_COL_DEFAULT);
    /* Add to history, skipping a verbatim repeat of the previous entry. */
    if (!(s_history_count > 0 && strcmp(s_history[s_history_count - 1], s_input_line) == 0)) {
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
    }
    Console_Execute(s_input_line);
    s_input_len = 0;
    s_input_line[0] = '\0';
    s_cursor_pos = 0;
    s_history_index = -1;
    s_sel_anchor = -1;
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
