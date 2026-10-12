#pragma once

#include <PLATFORM/INPUT.H>

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PlatformEventType {
    PLATFORM_EVENT_OTHER,
    PLATFORM_EVENT_QUIT,
    PLATFORM_EVENT_WINDOW_FOCUS_GAINED,
    PLATFORM_EVENT_WINDOW_FOCUS_LOST,
    PLATFORM_EVENT_WINDOW_MOUSE_ENTER,
    PLATFORM_EVENT_WINDOW_RESIZED,
    PLATFORM_EVENT_KEY_DOWN,
    PLATFORM_EVENT_KEY_UP,
    PLATFORM_EVENT_TEXT_INPUT,
    PLATFORM_EVENT_MOUSE_MOTION,
    PLATFORM_EVENT_MOUSE_BUTTON_DOWN,
    PLATFORM_EVENT_MOUSE_BUTTON_UP,
    PLATFORM_EVENT_MOUSE_WHEEL,
    PLATFORM_EVENT_GAMEPAD_ADDED,
    PLATFORM_EVENT_GAMEPAD_REMOVED,
    PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN,
    PLATFORM_EVENT_GAMEPAD_AXIS_MOTION,
    PLATFORM_EVENT_FINGER_DOWN,
    PLATFORM_EVENT_FINGER_UP,
    PLATFORM_EVENT_FINGER_MOTION
} PlatformEventType;

/* Text is borrowed until the next event poll. Coordinates are window pixels;
   finger coordinates are normalised to the whole display, including bars. */
typedef struct PlatformEvent {
    PlatformEventType type;
    union {
        struct {
            int32_t scancode;
            uint16_t mod;
            bool repeat;
        } key;
        struct {
            const char *text;
        } text;
        struct {
            float x, y, xrel, yrel;
        } motion;
        struct {
            float x, y;
            uint8_t button;
        } button;
        struct {
            float y;
        } wheel;
        struct {
            uint32_t which;
        } gdevice;
        struct {
            uint32_t which;
            PlatformGamepadAxis axis;
            int16_t value;
        } gaxis;
        struct {
            uint64_t fingerID;
            float x, y;
        } tfinger;
    };
} PlatformEvent;

typedef void (*PlatformOverlayFn)(uint32_t *pixels, uint32_t pitch,
                                  uint32_t width, uint32_t height,
                                  const uint8_t *palette_rgb);

/* Indexed pixels and the 256 RGB triples are borrowed for this call only.
   The optional overlay operates on 0xAARRGGBB pixels, with a byte pitch.
   Backends without an overlay may present indexed/RGB565 pixels directly. */
typedef struct PlatformFrame {
    const uint8_t *pixels;
    uint32_t width, height, pitch;
    const uint8_t *palette_rgb;
    PlatformOverlayFn overlay;
} PlatformFrame;

typedef struct PlatformGamepad PlatformGamepad;
typedef struct PlatformFolderDialog PlatformFolderDialog;

typedef enum PlatformDialogResult {
    PLATFORM_DIALOG_PENDING,
    PLATFORM_DIALOG_SELECTED,
    PLATFORM_DIALOG_CANCELLED,
    PLATFORM_DIALOG_ERROR
} PlatformDialogResult;

typedef enum PlatformMessageKind {
    PLATFORM_MESSAGE_INFORMATION,
    PLATFORM_MESSAGE_WARNING,
    PLATFORM_MESSAGE_ERROR
} PlatformMessageKind;

typedef enum PlatformLogLevel {
    PLATFORM_LOG_DEBUG,
    PLATFORM_LOG_INFO,
    PLATFORM_LOG_WARN,
    PLATFORM_LOG_ERROR
} PlatformLogLevel;

typedef void (*PlatformLogFn)(void *userdata, PlatformLogLevel level, const char *message);

typedef struct PlatformCapabilities {
    bool windowed;
    bool transient_window_loss;
    bool touch_overlay;
} PlatformCapabilities;

typedef struct PlatformSystem {
    bool (*init)(void);
    void (*shutdown)(void);
    bool (*set_headless)(void); /* before init; retain a present-capable offscreen target */
    const char *(*get_error)(void);
    void (*clear_error)(void);
    uint64_t (*ticks_ms)(void); /* wall clock; the engine owns its fixed-dt overlay */
    void (*delay_ms)(uint32_t ms);
    uint64_t (*performance_counter)(void);
    uint64_t (*performance_frequency)(void);
    uint64_t (*thread_id)(void);
    int32_t (*cpu_count)(void);
    int32_t (*ram_mb)(void);
    const char *(*platform_name)(void);
    void (*set_log_output)(PlatformLogFn callback, void *userdata);
} PlatformSystem;

typedef struct PlatformEvents {
    bool (*init)(void);
    void (*shutdown)(void);
    void (*pump)(void);
    bool (*poll)(PlatformEvent *event);
} PlatformEvents;

typedef struct PlatformWindow {
    bool (*init)(const char *title);
    void (*shutdown)(void);
    bool (*initialized)(void);
    bool (*create)(uint32_t width, uint32_t height, bool fullscreen);
    void (*destroy)(void);
    bool (*resize)(uint32_t width, uint32_t height, bool keep_window_size);
    bool (*size)(int32_t *width, int32_t *height);
    bool (*display_size)(uint32_t *width, uint32_t *height);
    bool (*primary_display_size)(uint32_t *width, uint32_t *height);
    void (*surface_coords)(int32_t x, int32_t y, int32_t *sx, int32_t *sy);
    void (*set_fullscreen)(bool enabled);
    bool (*get_fullscreen)(void); /* requested state, including failed-change rollback */
    bool (*is_fullscreen)(void);  /* actual host state */
    void (*set_vsync)(bool enabled);
    void (*set_relative_mouse)(bool enabled);
    bool (*present)(const PlatformFrame *frame);
} PlatformWindow;

typedef struct PlatformInput {
    /* The sampled state has PLATFORM_SCANCODE_COUNT entries, 0 or 1 per key. */
    const uint8_t *(*keyboard_state)(void);
    uint32_t (*keycode)(int32_t scancode, uint16_t modifiers);
    const char *(*scancode_name)(int32_t scancode);
    uint16_t (*modifiers)(void);
    void (*text_input)(bool enabled);
    void (*cursor_visible)(bool visible);
    bool (*init_gamepads)(void);
    void (*shutdown_gamepads)(void);
    uint32_t *(*gamepads)(int32_t *count); /* owned; free_memory */
    PlatformGamepad *(*open_gamepad)(uint32_t id);
    void (*close_gamepad)(PlatformGamepad *pad);
    const char *(*gamepad_name)(PlatformGamepad *pad); /* borrowed while pad is open */
    bool (*gamepad_button)(PlatformGamepad *pad, PlatformGamepadButton button);
    int16_t (*gamepad_axis)(PlatformGamepad *pad, PlatformGamepadAxis axis);
} PlatformInput;

typedef struct PlatformHost {
    const char *(*base_path)(void);                       /* borrowed for backend lifetime */
    char *(*current_directory)(void);                     /* owned; free_memory */
    char *(*pref_path)(const char *org, const char *app); /* owned; free_memory */
    const char *(*home_path)(void);                       /* borrowed */
    bool (*create_directory)(const char *path);
    bool (*remove_path)(const char *path);
    bool (*message_box)(PlatformMessageKind kind, const char *title, const char *message);
    /* Results are delivered on the polling thread. Path is borrowed until end_dialog;
       an error must be available through system.get_error on that same thread. */
    PlatformFolderDialog *(*begin_folder_dialog)(const char *initial_path);
    PlatformDialogResult (*poll_folder_dialog)(PlatformFolderDialog *dialog, const char **path);
    void (*end_folder_dialog)(PlatformFolderDialog *dialog); /* completed requests only */
    bool (*set_clipboard)(const char *text);
    char *(*get_clipboard)(void); /* owned; free_memory */
    void (*free_memory)(void *ptr);
} PlatformHost;

/* Exactly one implementation is selected at link time. Every callback is
   supplied: unsupported host services report an error, never a success stub. */
typedef struct PlatformInterface {
    const char *name;
    PlatformCapabilities capabilities;
    PlatformSystem system;
    PlatformEvents events;
    PlatformWindow window;
    PlatformInput input;
    PlatformHost host;
} PlatformInterface;

extern const PlatformInterface g_platform;

/* Backend diagnostics have no dependency on the game's logger. */
void Platform_SetLogOutput(PlatformLogFn callback, void *userdata);
void Platform_Log(PlatformLogLevel level, const char *fmt, ...);

/* Shared software-present conversion; pitches are bytes. Padding is untouched. */
bool Platform_ConvertIndexedFrame(const PlatformFrame *frame, uint32_t *pixels, uint32_t pitch);

#ifdef __cplusplus
}
#endif
