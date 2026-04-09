#include "GAMEMENU_MOUSE.H"

#include "C_EXTERN.H"

#include <SYSTEM/MOUSE.H>
#include <SYSTEM/WINDOW.H>

extern U16 GameMainMenu[];

#define GM_HAUTEUR_STANDARD 50
#define GM_MENU_SPACE 6
#define GM_MENU_XMID 320
#define GM_MENU_HALF_WIDTH 275

static int s_menuMouseRefCount = 0;
static S32 s_pressRow = -1;
static S32 s_prevClick = 0;
static bool s_prevAppActive = true;

void GameMenuMouseResetPress(void) {
    s_pressRow = -1;
    s_prevClick = 0;
}

void MenuMouseApplyCursorSprite(void) {
    /* No-op: menus use the default BinGphMouse arrow set by InitMouse(). */
}

void MenuMouseAcquire(void) {
    if (!FlagMenuMouse)
        return;
    s_menuMouseRefCount++;
    if (s_menuMouseRefCount == 1) {
        ShowMouseAfterFirstMotion();
    }
}

void MenuMouseRelease(void) {
    if (!FlagMenuMouse)
        return;
    if (s_menuMouseRefCount > 0)
        s_menuMouseRefCount--;
    if (s_menuMouseRefCount == 0) {
        GameMenuMouseResetPress();
        ShowMouse(false);
        InitMouse();
    }
}

S32 MenuMouse_GetRefCount(void) {
    return s_menuMouseRefCount;
}

void GameMenuMousePollFocus(void) {
    bool active = AppActive;

    if (s_prevAppActive && !active)
        GameMenuMouseResetPress();
    s_prevAppActive = active;
}

S32 GameMenuHitTestRow(const U16 *ptrmenu, U32 mx, U32 my, const U16 *gameMainMenuPtr) {
    S32 nb = ptrmenu[1];
    S32 ycenter = ptrmenu[2];
    S32 y;
    S32 n;
    S32 x0 = GM_MENU_XMID - GM_MENU_HALF_WIDTH;
    S32 x1 = GM_MENU_XMID + GM_MENU_HALF_WIDTH;

    if (nb <= 0)
        return -1;

    if (ycenter == 0)
        y = GM_HAUTEUR_STANDARD / 2 + 10;
    else
        y = ycenter - (GM_HAUTEUR_STANDARD * nb + (nb - 1) * GM_MENU_SPACE) / 2;

    for (n = 0; n < nb; n++) {
        S32 num = ptrmenu[4 + n * 2 + 1];

#ifdef DEMO
        if (gameMainMenuPtr != NULL && ptrmenu == gameMainMenuPtr && (num == 72 || num == 73)) {
            y += GM_HAUTEUR_STANDARD + GM_MENU_SPACE;
            continue;
        }
#else
        (void)gameMainMenuPtr;
#endif

        {
            S32 y0 = y - GM_HAUTEUR_STANDARD / 2;
            S32 y1 = y + GM_HAUTEUR_STANDARD / 2;
            if ((S32)mx >= x0 && (S32)mx <= x1 && (S32)my >= y0 && (S32)my <= y1)
                return n;
        }
        y += GM_HAUTEUR_STANDARD + GM_MENU_SPACE;
    }
    return -1;
}

S32 GameMenuMouseProcessDoGameMenu(U16 *ptrmenu, S16 *selected, S32 *flag) {
    S32 row;
    S32 cur;
    S32 leftDown;
    S32 prevLeft;
    S32 leftDownEdge;
    S32 leftUpEdge;
    S32 rel;
    S32 mess;
    bool mouseMoved;

    if (!FlagMenuMouse || s_menuMouseRefCount <= 0)
        return -1;

    GameMenuMousePollFocus();
    mouseMoved = MouseConsumeMotionFlag();

    cur = Click;
    leftDown = (cur & 1) != 0;
    prevLeft = (s_prevClick & 1) != 0;
    leftDownEdge = leftDown && !prevLeft;
    leftUpEdge = !leftDown && prevLeft;
    s_prevClick = cur;

    row = GameMenuHitTestRow(ptrmenu, MouseX, MouseY, GameMainMenu);

    if (leftDownEdge) {
        if (row >= 0) {
            s_pressRow = row;
            if (*selected != row) {
                *selected = row;
                *flag = 1;
                ptrmenu[0] = *selected;
            }
        }
    }

    if (leftDown && s_pressRow >= 0) {
        if (*selected != s_pressRow) {
            *selected = s_pressRow;
            *flag = 1;
            ptrmenu[0] = *selected;
        }
    } else if (!leftDown && s_pressRow < 0) {
        /* Hover: only update selection when the mouse actually moved. */
        if (mouseMoved && row >= 0 && row != *selected) {
            *selected = row;
            *flag = 1;
            ptrmenu[0] = *selected;
        }
    }

    if (leftUpEdge && s_pressRow >= 0) {
        rel = GameMenuHitTestRow(ptrmenu, MouseX, MouseY, GameMainMenu);
        if (rel == s_pressRow) {
            mess = ptrmenu[4 + 1 + s_pressRow * 2];
            s_pressRow = -1;
            return mess;
        }
        s_pressRow = -1;
    }

    return -1;
}
