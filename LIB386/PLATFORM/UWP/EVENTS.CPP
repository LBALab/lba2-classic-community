#include "PRIVATE.H"
#include "NAVIGATION.H"
#include "QUEUE.H"
#include "TRANSLATE.H"

#include <string.h>

using namespace ABI::Windows::UI::Core;

/* Every CoreWindow event is raised on the window's thread from inside
   ProcessEvents, which the engine's own thread calls from pump: one thread,
   no locks. */

namespace {
UwpQueue s_queue;
bool s_overflowLogged = false;

ICoreWindow *s_window = NULL;
ICoreDispatcher *s_dispatcher = NULL;
EventRegistrationToken s_tokens[7];
UwpSystemNavigationManager *s_navigation = NULL;
EventRegistrationToken s_backToken;
bool s_visible = true;
bool s_closed = false;

uint8_t s_keys[PLATFORM_SCANCODE_COUNT];
uint16_t s_mods = 0;
bool s_textInput = false;
WCHAR s_highSurrogate = 0;

void Push(const PlatformEvent *event, const char *text) {
    if (!UwpQueuePush(&s_queue, event, text) && !s_overflowLogged) {
        Platform_Log(PLATFORM_LOG_WARN, "UWP event queue full; dropping events");
        s_overflowLogged = true;
    }
}

void PushType(PlatformEventType type) {
    PlatformEvent event;
    memset(&event, 0, sizeof event);
    event.type = type;
    Push(&event, NULL);
}

/* Keys held when focus goes are released, or they would stay down. */
void ReleaseAllKeys() {
    PlatformEvent event;
    for (int i = 0; i < PLATFORM_SCANCODE_COUNT; ++i) {
        if (!s_keys[i])
            continue;
        s_keys[i] = 0;
        s_mods &= (uint16_t)~UwpModifierBit(i);
        memset(&event, 0, sizeof event);
        event.type = PLATFORM_EVENT_KEY_UP;
        event.key.scancode = i;
        event.key.mod = s_mods;
        Push(&event, NULL);
    }
}

void OnKey(IKeyEventArgs *args, bool down) {
    ABI::Windows::System::VirtualKey vk;
    CorePhysicalKeyStatus status;
    if (FAILED(args->get_VirtualKey(&vk)) || FAILED(args->get_KeyStatus(&status)))
        return;
    const int32_t code = UwpKeyScancode((uint32_t)vk, status.ScanCode, status.IsExtendedKey != 0);
    if (code == PLATFORM_SCANCODE_UNKNOWN)
        return;
    ICoreWindowEventArgs *handled = NULL;
    if (SUCCEEDED(args->QueryInterface(__uuidof(ICoreWindowEventArgs), (void **)&handled)))
        handled->put_Handled(TRUE);
    Uwp_Release(handled);
    const uint16_t bit = UwpModifierBit(code);
    if (down) {
        s_keys[code] = 1;
        s_mods |= bit;
    } else {
        s_keys[code] = 0;
        s_mods &= (uint16_t)~bit;
    }
    PlatformEvent event;
    memset(&event, 0, sizeof event);
    event.type = down ? PLATFORM_EVENT_KEY_DOWN : PLATFORM_EVENT_KEY_UP;
    event.key.scancode = code;
    event.key.mod = s_mods;
    event.key.repeat = down && status.WasKeyDown;
    Push(&event, NULL);
}

void OnCharacter(UINT32 unit) {
    if (unit >= 0xD800 && unit <= 0xDBFF) {
        s_highSurrogate = (WCHAR)unit;
        return;
    }
    uint32_t cp = unit;
    if (unit >= 0xDC00 && unit <= 0xDFFF) {
        if (!s_highSurrogate)
            return;
        cp = 0x10000 + (((uint32_t)s_highSurrogate - 0xD800) << 10) + (unit - 0xDC00);
    }
    s_highSurrogate = 0;
    if (!s_textInput || cp < 0x20 || cp == 0x7F)
        return;
    char text[UWP_TEXT_SIZE] = {0};
    if (cp < 0x80) {
        text[0] = (char)cp;
    } else if (cp < 0x800) {
        text[0] = (char)(0xC0 | (cp >> 6));
        text[1] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        text[0] = (char)(0xE0 | (cp >> 12));
        text[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        text[2] = (char)(0x80 | (cp & 0x3F));
    } else {
        text[0] = (char)(0xF0 | (cp >> 18));
        text[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        text[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        text[3] = (char)(0x80 | (cp & 0x3F));
    }
    PlatformEvent event;
    memset(&event, 0, sizeof event);
    event.type = PLATFORM_EVENT_TEXT_INPUT;
    Push(&event, text);
}

/* One struct per CoreWindow event: the event's argument class, the
   interface its handler receives, and what to do with it. */
struct KeyDown {
    typedef KeyEventArgs Args;
    typedef IKeyEventArgs Abi;
    static void Handle(Abi *args) { OnKey(args, true); }
};

struct KeyUp {
    typedef KeyEventArgs Args;
    typedef IKeyEventArgs Abi;
    static void Handle(Abi *args) { OnKey(args, false); }
};

struct CharacterReceived {
    typedef CharacterReceivedEventArgs Args;
    typedef ICharacterReceivedEventArgs Abi;
    static void Handle(Abi *args) {
        UINT32 unit = 0;
        if (SUCCEEDED(args->get_KeyCode(&unit)))
            OnCharacter(unit);
    }
};

struct Activated {
    typedef WindowActivatedEventArgs Args;
    typedef IWindowActivatedEventArgs Abi;
    static void Handle(Abi *args) {
        CoreWindowActivationState state = CoreWindowActivationState_CodeActivated;
        args->get_WindowActivationState(&state);
        if (state == CoreWindowActivationState_Deactivated) {
            ReleaseAllKeys();
            PushType(PLATFORM_EVENT_WINDOW_FOCUS_LOST);
        } else {
            PushType(PLATFORM_EVENT_WINDOW_FOCUS_GAINED);
        }
    }
};

struct VisibilityChanged {
    typedef VisibilityChangedEventArgs Args;
    typedef IVisibilityChangedEventArgs Abi;
    static void Handle(Abi *args) {
        boolean visible = TRUE;
        args->get_Visible(&visible);
        s_visible = visible != 0;
    }
};

/* After Closed the dispatcher must not be pumped again; the engine is told
   to leave and does so at its next poll. */
struct Closed {
    typedef CoreWindowEventArgs Args;
    typedef ICoreWindowEventArgs Abi;
    static void Handle(Abi *) {
        s_closed = true;
        PushType(PLATFORM_EVENT_QUIT);
    }
};

struct SizeChanged {
    typedef WindowSizeChangedEventArgs Args;
    typedef IWindowSizeChangedEventArgs Abi;
    static void Handle(Abi *) {
        Uwp_WindowSizeChanged();
        PushType(PLATFORM_EVENT_WINDOW_RESIZED);
    }
};

/* The delegate for one of them: TypedEventHandler<CoreWindow, Args>. The
   typedefs keep two closing angle brackets apart, which C++98 needs and
   clang-format would otherwise join. */
template <class Event>
struct WindowDelegate {
    typedef ABI::Windows::Foundation::ITypedEventHandler<CoreWindow *, typename Event::Args *> Interface;
};

template <class Event>
class WindowHandler : public UwpDelegate<typename WindowDelegate<Event>::Interface> {
  public:
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWindow *, typename Event::Abi *args) {
        Event::Handle(args);
        return S_OK;
    }
};

typedef WindowHandler<KeyDown> OnKeyDown;
typedef WindowHandler<KeyUp> OnKeyUp;
typedef WindowHandler<CharacterReceived> OnCharacterReceived;
typedef WindowHandler<Activated> OnActivated;
typedef WindowHandler<VisibilityChanged> OnVisibilityChanged;
typedef WindowHandler<Closed> OnClosed;
typedef WindowHandler<SizeChanged> OnSizeChanged;

/* B on an Xbox, Back elsewhere. Unhandled, it closes the app. The pad's B
   still reaches the game as a button, so this only claims the request. */
class OnBackRequested : public UwpDelegate<UwpBackRequestedHandler> {
  public:
    HRESULT STDMETHODCALLTYPE Invoke(IInspectable *, UwpBackRequestedEventArgs *args) {
        args->put_Handled(TRUE);
        return S_OK;
    }
};

void WatchBack() {
    UwpSystemNavigationManagerStatics *statics = NULL;
    if (SUCCEEDED(Uwp_Factory(UWP_RUNTIMECLASS_SYSTEM_NAVIGATION_MANAGER,
                              __uuidof(UwpSystemNavigationManagerStatics), (void **)&statics)) &&
        SUCCEEDED(statics->GetForCurrentView(&s_navigation))) {
        OnBackRequested *handler = new OnBackRequested();
        if (FAILED(s_navigation->add_BackRequested(handler, &s_backToken)))
            Platform_Log(PLATFORM_LOG_WARN, "UWP: cannot claim the Back button; B will close the app");
        handler->Release();
    } else {
        Platform_Log(PLATFORM_LOG_WARN, "UWP: no SystemNavigationManager; B will close the app");
    }
    Uwp_Release(statics);
}

template <class Handler, class Add>
void Subscribe(Add add, EventRegistrationToken *token) {
    Handler *handler = new Handler();
    if (FAILED((s_window->*add)(handler, token)))
        Platform_Log(PLATFORM_LOG_WARN, "UWP: a window event subscription failed");
    handler->Release();
}
} // namespace

void Uwp_AttachWindow(ICoreWindow *window) {
    s_window = window;
    s_window->AddRef();
    if (FAILED(s_window->get_Dispatcher(&s_dispatcher)))
        Platform_Log(PLATFORM_LOG_ERROR, "UWP: the window has no dispatcher");
    Subscribe<OnKeyDown>(&ICoreWindow::add_KeyDown, &s_tokens[0]);
    Subscribe<OnKeyUp>(&ICoreWindow::add_KeyUp, &s_tokens[1]);
    Subscribe<OnCharacterReceived>(&ICoreWindow::add_CharacterReceived, &s_tokens[2]);
    Subscribe<OnActivated>(&ICoreWindow::add_Activated, &s_tokens[3]);
    Subscribe<OnVisibilityChanged>(&ICoreWindow::add_VisibilityChanged, &s_tokens[4]);
    Subscribe<OnClosed>(&ICoreWindow::add_Closed, &s_tokens[5]);
    Subscribe<OnSizeChanged>(&ICoreWindow::add_SizeChanged, &s_tokens[6]);
    WatchBack();
}

void Uwp_DetachWindow(void) {
    if (s_window) {
        s_window->remove_KeyDown(s_tokens[0]);
        s_window->remove_KeyUp(s_tokens[1]);
        s_window->remove_CharacterReceived(s_tokens[2]);
        s_window->remove_Activated(s_tokens[3]);
        s_window->remove_VisibilityChanged(s_tokens[4]);
        s_window->remove_Closed(s_tokens[5]);
        s_window->remove_SizeChanged(s_tokens[6]);
    }
    if (s_navigation)
        s_navigation->remove_BackRequested(s_backToken);
    Uwp_Release(s_navigation);
    Uwp_Release(s_dispatcher);
    Uwp_Release(s_window);
}

ICoreWindow *Uwp_CoreWindow(void) { return s_window; }

void Uwp_PushEvent(const PlatformEvent *event) { Push(event, NULL); }

bool Uwp_EventsInit(void) {
    if (!s_window)
        return Uwp_Fail("UWP: events need the app's window, which the entry point attaches");
    return true;
}

void Uwp_EventsShutdown(void) { UwpQueueClear(&s_queue); }

void Uwp_Pump(void) {
    if (s_dispatcher && !s_closed) {
        /* Hidden, the game has nothing to show: wait for the next window
           event instead of spinning. A suspended app stops inside here. */
        const CoreProcessEventsOption option = s_visible ? CoreProcessEventsOption_ProcessAllIfPresent
                                                         : CoreProcessEventsOption_ProcessOneAndAllPending;
        s_dispatcher->ProcessEvents(option);
    }
    Uwp_PollGamepads();
}

bool Uwp_Poll(PlatformEvent *event) {
    if (!event)
        return Uwp_Fail("UWP event output is missing");
    return UwpQueuePoll(&s_queue, event);
}

const uint8_t *Uwp_KeyboardState(void) { return s_keys; }
uint32_t Uwp_Keycode(int32_t scancode, uint16_t modifiers) { return UwpKeycode(scancode, modifiers); }
const char *Uwp_ScancodeName(int32_t scancode) { return UwpScancodeName(scancode); }
uint16_t Uwp_Modifiers(void) { return s_mods; }
void Uwp_TextInput(bool enabled) { s_textInput = enabled; }
