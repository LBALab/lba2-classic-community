#include <SYSTEM/MOUSE.H>

#include <SVGA/SCREEN.H> // For ModeDesiredX, ModeDesiredY
#include <SYSTEM/WINDOW.H>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>
#include <math.h>
#include <assert.h>

/** Hide framebuffer pointer after this many ms without mouse activity (motion / buttons / wheel). */
#define MOUSE_SPRITE_IDLE_HIDE_MS 3000U

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Public state ------------------------------------------------------------
bool FlagMouse = false;
volatile S32 Click = 0;
volatile U32 MouseX = 0;
volatile U32 MouseY = 0;
S32 MouseXDep = 0;
S32 MouseYDep = 0;

S32 MouseSpriteGraphicNum = 0;
const void *MouseSpriteGraphic = NULL;

// --- Private state -----------------------------------------------------------
S32 LibMouseXDep, LibMouseYDep;
U32 MouseBoxX0, MouseBoxX1, MouseBoxY0, MouseBoxY1;

volatile S32 MouseWheelYAccum = 0;

static bool s_showSpriteAfterFirstMotion = false;
static Uint32 s_lastPointerActivityMs = 0;

static void MousePointerMarkActivity(void) {
    s_lastPointerActivityMs = SDL_GetTicks();
}

// --- Extern state ------------------------------------------------------------
// TODO: Convert the asm to .h to hold the data (currently on MOUSEDAT.ASM)
extern U8 BinGphMouse[];

// --- Initialization ----------------------------------------------------------
void InitMouse() {
    ClearMouseBox();

    MouseSpriteGraphic = (const void *)BinGphMouse;
    MouseSpriteGraphicNum = 0;
}

void EndMouse() {
    // Empty
}

bool MouseSpriteShouldDraw(void) {
    if (!FlagMouse)
        return false;
    if (s_showSpriteAfterFirstMotion)
        return false;
    if (MouseSpriteGraphic == NULL)
        return false;
    {
        Uint32 now = SDL_GetTicks();
        Uint32 elapsed = now - s_lastPointerActivityMs;

        if (elapsed >= MOUSE_SPRITE_IDLE_HIDE_MS)
            return false;
    }
    return true;
}

// --- Interface ---------------------------------------------------------------
void ShowMouse(bool show) {
    if (!show) {
        FlagMouse = false;
        s_showSpriteAfterFirstMotion = false;
        return;
    }
    s_showSpriteAfterFirstMotion = false;
    FlagMouse = true;
    MousePointerMarkActivity();
}

void ShowMouseAfterFirstMotion(void) {
    s_showSpriteAfterFirstMotion = true;
    FlagMouse = false;
}

void SetMousePos(U32 x, U32 y) {
    MouseX = x;
    MouseY = y;

    // Clamp to limits, X
    if (MouseX < MouseBoxX0) {
        MouseX = MouseBoxX0;
    } else if (MouseX > MouseBoxX1) {
        MouseX = MouseBoxX1;
    }

    // Clamp to limits, Y
    if (MouseY < MouseBoxY0) {
        MouseY = MouseBoxY0;
    } else if (MouseY > MouseBoxY1) {
        MouseY = MouseBoxY1;
    }
}

void SetMouseBox(U32 x0, U32 y0, U32 x1, U32 y1) {
    assert((x0 <= x1) && (y0 <= y1));

    // Limits can't be greater than screen
    if (x1 >= ModeDesiredX) {
        x1 = ModeDesiredX - 1;
    }
    if (y1 >= ModeDesiredY) {
        y1 = ModeDesiredY - 1;
    }

    MouseBoxX0 = x0;
    MouseBoxY0 = y0;
    MouseBoxX1 = x1;
    MouseBoxY1 = y1;

    SetMousePos(MouseX, MouseY);
}

void ClearMouseBox() { SetMouseBox(0, 0, ModeDesiredX - 1, ModeDesiredY - 1); }

void ManageMouse() {
    MouseXDep = LibMouseXDep;
    MouseYDep = LibMouseYDep;

    LibMouseXDep = 0;
    LibMouseYDep = 0;
}

void HandleEventsMouse(const void *event) {
    assert(event != NULL);

    const SDL_Event *sdlEvent = (SDL_Event *)(event);

    switch (sdlEvent->type) {
    case SDL_EVENT_MOUSE_MOTION: {
        S32 prevX;
        S32 prevY;
        S32 curX;
        S32 curY;

        WindowToSurfaceCoords((S32)(sdlEvent->motion.x - sdlEvent->motion.xrel),
                              (S32)(sdlEvent->motion.y - sdlEvent->motion.yrel),
                              &prevX, &prevY);
        WindowToSurfaceCoords((S32)sdlEvent->motion.x, (S32)sdlEvent->motion.y,
                              &curX, &curY);

        LibMouseXDep += curX - prevX;
        LibMouseYDep += curY - prevY;
        SetMousePos((U32)curX, (U32)curY);

        MousePointerMarkActivity();

        if (s_showSpriteAfterFirstMotion) {
            s_showSpriteAfterFirstMotion = false;
            FlagMouse = true;
        }
    } break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        MousePointerMarkActivity();
        Click |= (sdlEvent->button.button == 1) ? 1 : 0;
        Click |= (sdlEvent->button.button == 3) ? 2 : 0;
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        MousePointerMarkActivity();
        Click &= (sdlEvent->button.button == 1) ? ~1 : Click;
        Click &= (sdlEvent->button.button == 3) ? ~2 : Click;
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        MousePointerMarkActivity();
        MouseWheelYAccum += (S32)lround((double)sdlEvent->wheel.y);
        break;
    }
}

S32 ConsumeMouseWheelY(void) {
    S32 v;

    v = MouseWheelYAccum;
    MouseWheelYAccum = 0;
    return v;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
