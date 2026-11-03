#include <SYSTEM/WINDOW.H>

#include <SYSTEM/ANDROID.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/LOGPRINT.H>
#include <SVGA/SCREEN.H> /* ModeDesiredX/Y for the GPU letterbox remap */
#include <SVGA/SDL.H>    /* GpuPresentIsActive: which presenter owns the window */

#include <SDL3/SDL.h>
#include <assert.h>
#include <cstdlib>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// TODO: Implement detection of app instance already running

// --- Public state ------------------------------------------------------------
bool AppActive = false;

// --- Private state -----------------------------------------------------------
bool windowSystemInitialized = false;
const char *windowTitle = NULL;
SDL_Window *sdlWindow = NULL;
SDL_Renderer *sdlRenderer = NULL;
SDL_Texture *sdlTexture = NULL;
U32 windowSurfaceResX = 0;
U32 windowSurfaceResY = 0;
bool windowFullscreenEnabled = false;

static void ApplyGameWindowHideSystemCursor(void) {
    SDL_HideCursor();
    /* Clear default cursor; some WMs still show an arrow until this runs. */
    SDL_SetCursor(NULL);
}

/* Tell SDL to present the framebuffer centered and aspect-fit (letterbox /
   pillarbox) at the given logical size. Applied whenever the renderer or
   texture is (re)created. SDL then owns both the on-screen present rect and the
   window<->framebuffer coordinate mapping (SDL_RenderCoordinatesFromWindow), so
   the present and input transforms can't drift apart and high-DPI displays map
   correctly. */
static void SetLogicalPresentation(U32 resX, U32 resY) {
    assert(sdlRenderer != NULL);
    if (!SDL_SetRenderLogicalPresentation(sdlRenderer, (int)resX, (int)resY,
                                          SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        Log_Warn("Unable to set SDL logical presentation to %ux%u.\n"
                 "\tSDL message: %s",
                 resX, resY, SDL_GetError());
    }
}

/* The GL 2.1 / GLES 2.0 attribute set the renderer shaders are written for.
   SDL reads these when a window flagged SDL_WINDOW_OPENGL is created, so they
   must be set before every such creation (boot and runtime toggle alike). */
static void SetGLContextAttributes(void) {
#ifdef USE_GLES2
    /* The ES profile is the whole request: on Android a desktop compatibility
       profile has no EGL config behind it, so window creation fails and the
       engine drops to the software renderer. */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
}

static void PresentCurrentTexture() {
    assert(sdlRenderer != NULL);
    assert(sdlTexture != NULL);

    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer);
    SDL_RenderTexture(sdlRenderer, sdlTexture, NULL, NULL);
    SDL_RenderPresent(sdlRenderer);
}

// --- Initialization ----------------------------------------------------------
bool InitWindow(const char *title) {
    assert(title != NULL);
    assert(windowSystemInitialized == false);

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        const char *errorMsg = SDL_GetError();
        Log_Error("Unable to initialize SDL Window/Video subsystem.\n"
                  "\tSDL Message: %s",
                  errorMsg);

        windowSystemInitialized = false;
        return false;
    }

    windowTitle = title;
    windowSystemInitialized = true;
    return true;
}

void EndWindow() {
    if (windowSystemInitialized) {
        DestroyWindowSurface();

        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        windowSystemInitialized = false;
    }
}

bool IsWindowInitialized() {
    return windowSystemInitialized;
}

SDL_Window *Window_GetSDLWindow(void) {
    return sdlWindow;
}

/* ── OpenGL context (GL renderer backend) ────────────────────────────────── */

static SDL_GLContext s_glContext = NULL;

S32 Window_CreateGLContext(void) {
    if (s_glContext != NULL) {
        return TRUE;
    }
    if (sdlWindow == NULL) {
        Log_Warn("Window_CreateGLContext: no window to attach a context to");
        return FALSE;
    }
    if ((SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_OPENGL) == 0) {
        /* The window was created for the software present path. The GL backend
           must be selected before window creation so the present mode can add
           the OpenGL flag; see INITMODE. */
        Log_Warn("Window_CreateGLContext: window is not an OpenGL window "
                 "(select the GL renderer before creating the window)");
        return FALSE;
    }

    /* Attributes were set before the window was created (SetGLContextAttributes
       runs in CreateWindowSurface / Window_RecreateWindowForGL); nothing else
       to configure on the context itself. */

    s_glContext = SDL_GL_CreateContext(sdlWindow);
    if (s_glContext == NULL) {
        Log_Warn("Window_CreateGLContext: SDL_GL_CreateContext failed: %s",
                 SDL_GetError());
        return FALSE;
    }

    if (!Window_GLMakeCurrent()) {
        SDL_GL_DestroyContext(s_glContext);
        s_glContext = NULL;
        return FALSE;
    }

    return TRUE;
}

S32 Window_GLMakeCurrent(void) {
    if (s_glContext == NULL || sdlWindow == NULL) {
        return FALSE;
    }
    if (SDL_GL_MakeCurrent(sdlWindow, s_glContext) != 0) {
        /* Some SDL3/EGL stacks return non-zero here with no error message
           while the context still becomes current (verified with a raw
           SDL3+GL probe: GetCurrentContext equals ours and rendering works).
           Judge by the actual current-context state rather than the return
           code, so a spurious error cannot take down a live context. */
        if (SDL_GL_GetCurrentContext() == s_glContext) {
            return TRUE;
        }
        Log_Warn("Window_GLMakeCurrent: %s", SDL_GetError());
        return FALSE;
    }
    return TRUE;
}

void Window_GL_SwapWindow(void) {
    if (s_glContext != NULL && sdlWindow != NULL) {
        SDL_GL_SwapWindow(sdlWindow);
    }
}

void Window_DestroyGLContext(void) {
    if (s_glContext == NULL) {
        return;
    }
    if (sdlWindow != NULL) {
        SDL_GL_MakeCurrent(sdlWindow, NULL);
    }
    SDL_GL_DestroyContext(s_glContext);
    s_glContext = NULL;
}

/* The Display menu's renderer switch toggles between software and GL at
   runtime. The two presenters cannot share one window (the software path owns
   an SDL_Renderer streamed to; the GL path owns a GL context swapped to), so
   the window is torn down and recreated with or without the OpenGL flag. */

/* CoerceFullscreenForPlatform is defined below with the windowed-mode feature
   reporting it relies on; declare it here so the window-recreate paths below
   can call it without reordering the display-mode feature code it sits with. */
static bool CoerceFullscreenForPlatform(bool fullscreen);

/* Destroy the window for a recreate. On macOS a fullscreen window lives in its
   own Space, entered through an animation: destroying it mid-transition leaves
   that Space behind (an empty gray screen) and the next window comes up on the
   desktop instead. Finish any pending transition and leave fullscreen first. */
static void DestroyWindowForRecreate(void) {
    if (sdlWindow == NULL) {
        return;
    }
    SDL_SyncWindow(sdlWindow);
    if ((SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_FULLSCREEN) != 0) {
        SDL_SetWindowFullscreen(sdlWindow, false);
        SDL_SyncWindow(sdlWindow);
    }
    SDL_DestroyWindow(sdlWindow);
    sdlWindow = NULL;
}

/* Recreate the SDL window flagged SDL_WINDOW_OPENGL (with the GL attributes
   set before creation) for the GL renderer's present path. */
bool Window_RecreateWindowForGL(U32 resX, U32 resY, bool fullscreen) {
    fullscreen = CoerceFullscreenForPlatform(fullscreen);

    if (s_glContext != NULL) {
        Window_DestroyGLContext();
    }
    if (sdlTexture) {
        SDL_DestroyTexture(sdlTexture);
        sdlTexture = NULL;
    }
    if (sdlRenderer) {
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = NULL;
    }
    DestroyWindowForRecreate();

    windowFullscreenEnabled = fullscreen;

    SetGLContextAttributes();
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, windowTitle);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, resX);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, resY);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
#if defined(__ANDROID__)
    if (IsAndroidTVDevice())
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
#else
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, fullscreen);
#endif
    sdlWindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (sdlWindow == NULL) {
        Log_Error("Unable to create SDL window for GL.\n"
                  "\tResolution: %ux%u.\n"
                  "\tSDL message: %s",
                  resX, resY, SDL_GetError());
        return false;
    }

    windowSurfaceResX = resX;
    windowSurfaceResY = resY;

    if (windowFullscreenEnabled && !SDL_SetWindowFullscreen(sdlWindow, true)) {
        Log_Warn("GL window: fullscreen fallback failed.\n"
                 "\tSDL message: %s",
                 SDL_GetError());
        windowFullscreenEnabled = false;
    }

    ApplyGameWindowHideSystemCursor();
    return true;
}

/* Recreate the window without the OpenGL flag and restore the software
   present path (SDL_Renderer + streaming texture + logical presentation). */
bool Window_RecreateWindowForSW(U32 resX, U32 resY, bool fullscreen) {
    fullscreen = CoerceFullscreenForPlatform(fullscreen);
    Window_DestroyGLContext();

    DestroyWindowForRecreate();

    windowFullscreenEnabled = fullscreen;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, windowTitle);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, resX);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, resY);
#if defined(__ANDROID__)
    if (IsAndroidTVDevice())
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
#else
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, fullscreen);
#endif
    sdlWindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (sdlWindow == NULL) {
        Log_Error("Unable to recreate SDL window for SW renderer.\n"
                  "\tResolution: %ux%u.\n"
                  "\tSDL message: %s",
                  resX, resY, SDL_GetError());
        return false;
    }

    sdlRenderer = SDL_CreateRenderer(sdlWindow, NULL);
    if (sdlRenderer == NULL) {
        Log_Error("Unable to recreate SDL renderer.\n"
                  "\tSDL message: %s",
                  SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(sdlRenderer, 1);

    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, resX, resY);
    if (sdlTexture == NULL) {
        Log_Error("Unable to recreate SDL streaming texture.\n"
                  "\tSDL message: %s",
                  SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(sdlTexture, SDL_SCALEMODE_NEAREST);

    windowSurfaceResX = resX;
    windowSurfaceResY = resY;
    SetLogicalPresentation(resX, resY);

    if (windowFullscreenEnabled && !SDL_SetWindowFullscreen(sdlWindow, true)) {
        Log_Warn("SW window: fullscreen fallback failed.\n"
                 "\tSDL message: %s",
                 SDL_GetError());
        windowFullscreenEnabled = false;
    }

    ApplyGameWindowHideSystemCursor();
    return true;
}

/* Recreate the window plain: no OpenGL flag, no SDL_Renderer, no streaming
   texture. The SDL3 GPU backend claims the window for a GPU device, which
   refuses a window that already carries a renderer or a GL context, so this
   is the third shape the renderer switch recreates for. It mirrors the
   software recreate minus the present path plumbing; the bookkeeping the
   other two keep (surface size, fullscreen, cursor) is shared. */
bool Window_RecreateWindowForGPU(U32 resX, U32 resY, bool fullscreen) {
    fullscreen = CoerceFullscreenForPlatform(fullscreen);
    Window_DestroyGLContext();

    /* The previous window shape (software) still carried a renderer and a
       streaming texture. SDL_DestroyWindow frees whatever renderer/texture is
       attached to it without telling us, so null them first or they dangle
       into the GPU window's lifetime -- the same teardown order the GL
       recreate uses. */
    if (sdlTexture) {
        SDL_DestroyTexture(sdlTexture);
        sdlTexture = NULL;
    }
    if (sdlRenderer) {
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = NULL;
    }
    DestroyWindowForRecreate();

    windowFullscreenEnabled = fullscreen;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, windowTitle);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, resX);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, resY);
#if defined(__ANDROID__)
    if (IsAndroidTVDevice())
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
#else
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, fullscreen);
#endif
    sdlWindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (sdlWindow == NULL) {
        Log_Error("Unable to recreate SDL window for the GPU renderer.\n"
                  "\tResolution: %ux%u.\n"
                  "\tSDL message: %s",
                  resX, resY, SDL_GetError());
        return false;
    }

    windowSurfaceResX = resX;
    windowSurfaceResY = resY;

    if (windowFullscreenEnabled && !SDL_SetWindowFullscreen(sdlWindow, true)) {
        Log_Warn("GPU window: fullscreen fallback failed.\n"
                 "\tSDL message: %s",
                 SDL_GetError());
        windowFullscreenEnabled = false;
    }

    ApplyGameWindowHideSystemCursor();
    return true;
}

/* GL-path window resize. The software path resizes through ResizeWindowSurface;
   the GL renderer re-creates its own viewport on the new backing size. */
void Window_SetWindowSize(U32 w, U32 h) {
    if (sdlWindow != NULL) {
        SDL_SetWindowSize(sdlWindow, (int)w, (int)h);
    }
    windowSurfaceResX = w;
    windowSurfaceResY = h;
}

// --- Interface ---------------------------------------------------------------
/* A platform with no windowed mode has no way back out of a windowed window:
   the Display submenu drops its Fullscreen row (Window_SupportsWindowedMode),
   so a cfg asking for windowed strands the player behind the system bars with
   no control to undo it. Honour only fullscreen there. The cfg value itself is
   left alone, so a profile carried to desktop keeps the player's choice. */
static bool CoerceFullscreenForPlatform(bool fullscreen) {
    return Window_SupportsWindowedMode() ? fullscreen : true;
}

bool CreateWindowSurface(U32 resX, U32 resY, bool fullscreen, bool opengl) {
    assert(windowSystemInitialized == true);
    assert(sdlWindow == NULL);

    fullscreen = CoerceFullscreenForPlatform(fullscreen);

    /* Seed the cached state from the caller's choice so a fullscreen boot is
       fullscreen from the first frame. The create-time property below asks SDL
       to bring the window up fullscreen directly (no windowed flash); the
       post-create apply is kept as a fallback for backends that ignore it. */
    windowFullscreenEnabled = fullscreen;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, windowTitle);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, resX);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, resY);
    /* SDL_GL_CreateContext (Window_CreateGLContext) can only attach to a window
       created with the OpenGL flag; the flag is only requested by the present
       mode selection when a GL backend is active. The attributes must land
       before this creation too (SDL picks the context profile at window
       creation for an OpenGL window). */
    if (opengl) {
        SetGLContextAttributes();
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    }
    /* Resizable allows OS-driven window resizing on desktop. On Android,
     * SDL translates the resizable flag into SCREEN_ORIENTATION_UNSPECIFIED,
     * which permits portrait rotation and triggers a SDL internal
     * use-after-free on mutex during surfaceChanged. Lock landscape via
     * manifest + non-resizable window. */
#if defined(__ANDROID__)
    /* Android TV: fullscreen at creation avoids a later surface transition
     * (1920×984 → 1920×1080 when system UI hides on focus gain) that
     * triggers EGL_BAD_DISPLAY on some Mali GPUs. Phone: skip the property
     * here to avoid a hwuiTask crash (pthread_mutex_lock on destroyed mutex). */
    if (IsAndroidTVDevice())
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
#else
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, fullscreen);
#endif
    sdlWindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (sdlWindow == NULL) {
        const char *errorMsg = SDL_GetError();
        Log_Error("Unable to create SDL window.\n"
                  "\tResolution: %ux%u.\n"
                  "\tSDL message: %s",
                  resX, resY, errorMsg);
        return false;
    }

    if (windowFullscreenEnabled && !SDL_SetWindowFullscreen(sdlWindow, true)) {
        const char *errorMsg = SDL_GetError();
        Log_Warn("Unable to switch SDL window to fullscreen.\n"
                 "\tSDL message: %s",
                 errorMsg);
        windowFullscreenEnabled = false;
    }

    /* An OpenGL window needs SDL's OpenGL renderer: the default on macOS is
       Metal, and SDL recreates the window without the OpenGL flag to host it,
       which leaves the GL backend nothing to attach a context to. */
    sdlRenderer = SDL_CreateRenderer(sdlWindow, opengl ? "opengl" : NULL);
    if (sdlRenderer == NULL) {
        const char *errorMsg = SDL_GetError();
        Log_Error("Unable to create SDL renderer.\n"
                  "\tSDL message: %s",
                  errorMsg);
        SDL_DestroyWindow(sdlWindow);
        sdlWindow = NULL;
        return false;
    }

    SDL_SetRenderVSync(sdlRenderer, 1);

    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, resX, resY);
    if (sdlTexture == NULL) {
        const char *errorMsg = SDL_GetError();
        Log_Error("Unable to create SDL streaming texture.\n"
                  "\tSize: %ux%u.\n"
                  "\tSDL message: %s",
                  resX, resY, errorMsg);
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = NULL;
        SDL_DestroyWindow(sdlWindow);
        sdlWindow = NULL;
        return false;
    }

    SDL_SetTextureScaleMode(sdlTexture, SDL_SCALEMODE_NEAREST);
    windowSurfaceResX = resX;
    windowSurfaceResY = resY;
    SetLogicalPresentation(resX, resY);

    /* Game draws the pointer in the framebuffer; hide the OS cursor to avoid double cursors. */
    ApplyGameWindowHideSystemCursor();

    return true;
}

bool Window_GetDisplayDimensions(U32 *outW, U32 *outH) {
    if (outW == NULL || outH == NULL || sdlWindow == NULL) {
        return false;
    }
    SDL_DisplayID display = SDL_GetDisplayForWindow(sdlWindow);
    if (display == 0) {
        return false;
    }
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);
    if (mode == NULL || mode->w <= 0 || mode->h <= 0) {
        return false;
    }
    *outW = (U32)mode->w;
    *outH = (U32)mode->h;
    return true;
}

bool Window_GetPrimaryDisplayDimensions(U32 *outW, U32 *outH) {
    if (outW == NULL || outH == NULL) {
        return false;
    }
    /* Boot-path query: no window exists yet, so go straight to the primary
       display. Requires the video subsystem to be up (the caller brings it
       up before sizing the framebuffer). */
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        return false;
    }
    /* Never auto-detect under the headless drivers. The test harness runs
       with SDL_VIDEODRIVER=dummy and pins --resolution, but guard anyway so a
       stray dummy boot can't pick up the driver's synthetic display mode. */
    const char *driver = SDL_GetCurrentVideoDriver();
    if (driver == NULL || SDL_strcmp(driver, "dummy") == 0 ||
        SDL_strcmp(driver, "offscreen") == 0) {
        return false;
    }
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (display == 0) {
        return false;
    }
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);
    if (mode == NULL || mode->w <= 0 || mode->h <= 0) {
        return false;
    }
    *outW = (U32)mode->w;
    *outH = (U32)mode->h;
    return true;
}

bool ResizeWindowSurface(U32 newW, U32 newH, bool keepWindowSize) {
    /* No-op if the surface was never created.  Caller should fall through to
       CreateWindowSurface in that case (boot path), not Resize. */
    if (sdlWindow == NULL || sdlRenderer == NULL) {
        return false;
    }

    /* In windowed mode this resizes the visible window; in fullscreen SDL
       ignores the dimension change but the texture below still needs the new
       size so the renderer scales the correct pixel count to fit the
       physical display.

       Framebuffer size and window size are independent -- SDL logical
       presentation owns the mapping between them and scales either way. A
       render size the player did not ask for therefore leaves the window
       exactly as they left it, including a manual resize; their own choice
       still moves both. */
    if (!keepWindowSize) {
        SDL_SetWindowSize(sdlWindow, (int)newW, (int)newH);
    }

    /* The streaming texture is fixed-dimension at creation; recreate at the
       new size.  Renderer and window survive untouched, so fullscreen state,
       focus, and multi-monitor placement are preserved. */
    if (sdlTexture) {
        SDL_DestroyTexture(sdlTexture);
        sdlTexture = NULL;
    }
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, (int)newW, (int)newH);
    if (sdlTexture == NULL) {
        const char *errorMsg = SDL_GetError();
        Log_Error("Unable to recreate SDL streaming texture at %ux%u.\n"
                  "\tSDL message: %s",
                  newW, newH, errorMsg);
        return false;
    }
    SDL_SetTextureScaleMode(sdlTexture, SDL_SCALEMODE_NEAREST);

    windowSurfaceResX = newW;
    windowSurfaceResY = newH;
    SetLogicalPresentation(newW, newH);
    return true;
}

void DestroyWindowSurface() {
    if (sdlTexture) {
        SDL_DestroyTexture(sdlTexture);
        sdlTexture = NULL;
    }
    if (sdlRenderer) {
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = NULL;
    }
    if (sdlWindow) {
        SDL_DestroyWindow(sdlWindow);
        sdlWindow = NULL;
    }

    windowSurfaceResX = 0;
    windowSurfaceResY = 0;
}

void PresentRendererFrame(const void *pixels, U32 pitch) {
    assert(sdlRenderer != NULL);
    assert(sdlTexture != NULL);
    assert(pixels != NULL);

    SDL_UpdateTexture(sdlTexture, NULL, pixels, (int)pitch);
    PresentCurrentTexture();
}

bool LockRendererTexture(void **pixels, int *pitch) {
    assert(sdlRenderer != NULL);
    assert(sdlTexture != NULL);
    assert(pixels != NULL);
    assert(pitch != NULL);

    return SDL_LockTexture(sdlTexture, NULL, pixels, pitch);
}

void UnlockAndPresentRendererTexture() {
    assert(sdlRenderer != NULL);
    assert(sdlTexture != NULL);

    SDL_UnlockTexture(sdlTexture);
    PresentCurrentTexture();
}

/* Map a window pixel to a framebuffer pixel via SDL's logical presentation (set
   in CreateWindowSurface / ResizeWindowSurface), then clamp to a valid index.
   SDL extrapolates past the framebuffer for points in the letterbox / pillarbox
   bars, so the clamp keeps callers (mouse / touch picking) in range. */
void WindowToSurfaceCoords(S32 windowX, S32 windowY, S32 *surfaceX, S32 *surfaceY) {
    assert(surfaceX != NULL);
    assert(surfaceY != NULL);

    /* GPU path: the renderer owns the letterbox (the GL and SDL3 GPU backends
       compute their own centred viewport because the present goes through their
       API, not SDL's renderer), so remap through the same centred rectangle for
       mouse picking. */
    if (GpuPresentIsActive() && sdlWindow != NULL &&
        ModeDesiredX > 0 && ModeDesiredY > 0) {
        int winW = 0, winH = 0;
        SDL_GetWindowSize(sdlWindow, &winW, &winH);
        if (winW > 0 && winH > 0) {
            double windowAspect = (double)winW / (double)winH;
            double contentAspect = (double)ModeDesiredX / (double)ModeDesiredY;
            int vpX = 0, vpY = 0, vpW = winW, vpH = winH;
            if (windowAspect > contentAspect) {
                vpH = winH;
                vpW = (int)((double)winH * contentAspect + 0.5);
                vpW = (vpW + 1) & ~1;
                if (vpW > winW)
                    vpW = winW;
                vpX = (winW - vpW) / 2;
                vpY = 0;
            } else {
                vpW = winW;
                vpH = (int)((double)winW / contentAspect + 0.5);
                vpH = (vpH + 1) & ~1;
                if (vpH > winH)
                    vpH = winH;
                vpX = 0;
                vpY = (winH - vpH) / 2;
            }
            S32 sx = (S32)((double)(windowX - vpX) * (double)ModeDesiredX / (double)vpW);
            S32 sy = (S32)((double)(windowY - vpY) * (double)ModeDesiredY / (double)vpH);
            if (sx < 0)
                sx = 0;
            else if (sx >= (S32)ModeDesiredX)
                sx = (S32)ModeDesiredX - 1;
            if (sy < 0)
                sy = 0;
            else if (sy >= (S32)ModeDesiredY)
                sy = (S32)ModeDesiredY - 1;
            *surfaceX = sx;
            *surfaceY = sy;
            return;
        }
    }

    if ((sdlRenderer == NULL) || (windowSurfaceResX == 0) || (windowSurfaceResY == 0)) {
        *surfaceX = windowX;
        *surfaceY = windowY;
        return;
    }

    float logicalX = 0.0f;
    float logicalY = 0.0f;
    SDL_RenderCoordinatesFromWindow(sdlRenderer, (float)windowX, (float)windowY, &logicalX,
                                    &logicalY);

    S32 sx = (S32)logicalX;
    S32 sy = (S32)logicalY;
    if (sx < 0) {
        sx = 0;
    } else if (sx >= (S32)windowSurfaceResX) {
        sx = (S32)windowSurfaceResX - 1;
    }
    if (sy < 0) {
        sy = 0;
    } else if (sy >= (S32)windowSurfaceResY) {
        sy = (S32)windowSurfaceResY - 1;
    }
    *surfaceX = sx;
    *surfaceY = sy;
}

void SetWindowRelativeMouse(bool enable) {
    if (sdlWindow != NULL)
        SDL_SetWindowRelativeMouseMode(sdlWindow, enable);
}

void SetWindowFullscreen(bool fullscreen) {
    fullscreen = CoerceFullscreenForPlatform(fullscreen);
    windowFullscreenEnabled = fullscreen;

    if (sdlWindow == NULL) {
        return;
    }

    if (!SDL_SetWindowFullscreen(sdlWindow, fullscreen)) {
        const char *errorMsg = SDL_GetError();
        Log_Warn("Unable to change SDL fullscreen state to %d.\n"
                 "\tSDL message: %s",
                 fullscreen ? 1 : 0, errorMsg);
        windowFullscreenEnabled = !fullscreen;
        return;
    }

    if ((sdlRenderer != NULL) && (sdlTexture != NULL)) {
        PresentCurrentTexture();
    }
}

bool GetWindowFullscreen() {
    return windowFullscreenEnabled;
}

/* The window's *actual* fullscreen state per SDL, rather than the cached request
   flag above (which can disagree when a mode change fails — e.g. SDL refusing to
   leave fullscreen). For accurate reporting / diagnostics. */
bool Window_IsFullscreen(void) {
    return (sdlWindow != NULL) &&
           ((SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_FULLSCREEN) != 0);
}

bool Window_CanLoseNativeWindow(void) {
#if defined(__ANDROID__)
    /* The activity owns the surface, so anything Android puts in front takes it
       away: SDL_CreateWindow then fails with "Could not fetch native window".
       The engine asks for All Files Access during boot, which opens Settings and
       causes exactly that. */
    return true;
#else
    return false;
#endif
}

bool Window_SupportsWindowedMode(void) {
#if defined(__ANDROID__)
    /* Android phones and TVs run the game fullscreen (landscape-locked via
       manifest + non-resizable window, see CreateWindowSurface). There is no
       windowed mode to switch to, so the fullscreen toggle is meaningless. */
    return false;
#else
    return true;
#endif
}

void Window_SetVSync(int enabled) {
    /* Record for the GPU backend first: it may not exist yet (config load
       precedes renderer init) and the seam replays the value on registration. */
    GpuPresentSetVSync(enabled ? 1 : 0);

    /* The GL context and the SDL renderer are mutually exclusive presenters;
       the swap interval has a GL-specific control. */
    if (s_glContext != NULL) {
        SDL_GL_SetSwapInterval(enabled ? 1 : 0);
        return;
    }
    if (sdlRenderer != NULL) {
        SDL_SetRenderVSync(sdlRenderer, enabled ? 1 : 0);
    }
}

void ManageWindow() {
    // Empty
}

void HandleEventsWindow(const void *event) {
    assert(event != NULL);

    // Handle nothing if system was not initialized yet
    if (!windowSystemInitialized) {
        return;
    }

    const SDL_Event *sdlEvent = (SDL_Event *)(event);
    switch (sdlEvent->type) {
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        AppActive = true;
        AppFocusAudio_OnChange(true);
        if (sdlWindow != NULL) {
            ApplyGameWindowHideSystemCursor();
        }
        break;
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
        if (sdlWindow != NULL) {
            ApplyGameWindowHideSystemCursor();
        }
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        AppActive = false;
        AppFocusAudio_OnChange(false);
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        if ((sdlRenderer != NULL) && (sdlTexture != NULL)) {
            PresentCurrentTexture();
        }
        break;
    case SDL_EVENT_QUIT:
        exit(0); // TODO: Implement graceful exit
        break;
    default:
        break;
    }
}

// =============================================================================
#ifdef __cplusplus
}
#endif
