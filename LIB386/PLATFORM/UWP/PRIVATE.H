#pragma once

/* Shared by the UWP backend's files. WinRT is called as plain COM interfaces
   from the llvm-mingw headers: no C++/CX, no C++/WinRT, C++98. */

#include <PLATFORM/PLATFORM.H>

#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.applicationmodel.core.h>
#include <windows.ui.core.h>

extern "C" {
/* NULL/BACKEND.CPP built with LBA2_NULL_AS_BASE. */
PlatformInterface NullPlatform_Interface(void);
bool NullPlatform_Fail(const char *message);
}

/* Reports through the null backend's error slot, so get_error sees it. */
bool Uwp_Fail(const char *format, ...);

/* RoGetActivationFactory for a runtime class name. */
HRESULT Uwp_Factory(const WCHAR *className, REFIID iid, void **out);

template <class T>
inline void Uwp_Release(T *&p) {
    if (p) {
        p->Release();
        p = NULL;
    }
}

/* A WinRT object of our own. Views live until CoreApplication::Run returns. */
template <class I>
class UwpInspectable : public I {
  public:
    UwpInspectable() : m_refs(1) {}
    virtual ~UwpInspectable() {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IInspectable) || iid == __uuidof(I)) {
            *out = static_cast<I *>(this);
            AddRef();
            return S_OK;
        }
        *out = NULL;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() { return (ULONG)InterlockedIncrement(&m_refs); }
    ULONG STDMETHODCALLTYPE Release() {
        LONG left = InterlockedDecrement(&m_refs);
        if (left == 0)
            delete this;
        return (ULONG)left;
    }
    HRESULT STDMETHODCALLTYPE GetIids(ULONG *count, IID **iids) {
        *count = 0;
        *iids = NULL;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING *name) {
        *name = NULL;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel *level) {
        *level = BaseTrust;
        return S_OK;
    }

  private:
    LONG m_refs;
};

/* An event delegate: plain COM, agile so WinRT may call it from any thread. */
template <class I>
class UwpDelegate : public I {
  public:
    UwpDelegate() : m_refs(1) {}
    virtual ~UwpDelegate() {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IAgileObject) || iid == __uuidof(I)) {
            *out = static_cast<I *>(this);
            AddRef();
            return S_OK;
        }
        *out = NULL;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() { return (ULONG)InterlockedIncrement(&m_refs); }
    ULONG STDMETHODCALLTYPE Release() {
        LONG left = InterlockedDecrement(&m_refs);
        if (left == 0)
            delete this;
        return (ULONG)left;
    }

  private:
    LONG m_refs;
};

/* Entry: the app's writable folder, UTF-8, with a trailing backslash. */
const char *Uwp_LocalState(void);

/* EVENTS.CPP: the window's events, set up once the CoreWindow exists. */
void Uwp_AttachWindow(ABI::Windows::UI::Core::ICoreWindow *window);
void Uwp_DetachWindow(void);
void Uwp_PushEvent(const PlatformEvent *event);

/* INPUT.CPP: subscribed at startup (see Uwp_WatchGamepads); polled from pump. */
void Uwp_WatchGamepads(void);
void Uwp_PollGamepads(void);

/* WINDOW.CPP */
ABI::Windows::UI::Core::ICoreWindow *Uwp_CoreWindow(void);
void Uwp_WindowSizeChanged(void);
void Uwp_WindowTrim(void);

/* Contract callbacks, gathered into the table by BACKEND.CPP. */
bool Uwp_EventsInit(void);
void Uwp_EventsShutdown(void);
void Uwp_Pump(void);
bool Uwp_Poll(PlatformEvent *event);
const uint8_t *Uwp_KeyboardState(void);
uint32_t Uwp_Keycode(int32_t scancode, uint16_t modifiers);
const char *Uwp_ScancodeName(int32_t scancode);
uint16_t Uwp_Modifiers(void);
void Uwp_TextInput(bool enabled);

bool Uwp_InitGamepads(void);
void Uwp_ShutdownGamepads(void);
uint32_t *Uwp_Gamepads(int32_t *count);
PlatformGamepad *Uwp_OpenGamepad(uint32_t id);
void Uwp_CloseGamepad(PlatformGamepad *pad);
const char *Uwp_GamepadName(PlatformGamepad *pad);
bool Uwp_GamepadButton(PlatformGamepad *pad, PlatformGamepadButton button);
int16_t Uwp_GamepadAxis(PlatformGamepad *pad, PlatformGamepadAxis axis);

bool Uwp_WindowInit(const char *title);
void Uwp_WindowShutdown(void);
bool Uwp_WindowInitialized(void);
bool Uwp_WindowCreate(uint32_t width, uint32_t height, bool fullscreen);
void Uwp_WindowDestroy(void);
bool Uwp_WindowResize(uint32_t width, uint32_t height, bool keepWindowSize);
bool Uwp_WindowSize(int32_t *width, int32_t *height);
bool Uwp_DisplaySize(uint32_t *width, uint32_t *height);
void Uwp_SurfaceCoords(int32_t x, int32_t y, int32_t *sx, int32_t *sy);
void Uwp_SetVSync(bool enabled);
bool Uwp_SetPresentFilter(PlatformPresentFilter mode);
bool Uwp_Present(const PlatformFrame *frame);
