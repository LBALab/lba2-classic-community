#pragma once

/* Windows.UI.Core.SystemNavigationManager, which the mingw-w64 headers do not
   declare. On an Xbox, B raises its BackRequested event, and an app that does
   not mark it handled is closed.

   Interface IDs and method order are from the Windows metadata, as the
   windows crate (0.62.2, src/Windows/UI/Core/mod.rs) prints them. The
   handler's ID is the parameterized ID of EventHandler<BackRequestedEventArgs>,
   computed by the WinRT rule (SHA-1 over the type signature); the same rule
   reproduces the headers' EventHandler<Gamepad> ID, 8a7639ee-624a-501a-bb53-562d1ec11b52. */

#include <windows.h>
#include <inspectable.h>
#include <eventtoken.h>

struct UwpBackRequestedEventArgs : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Handled(boolean *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Handled(boolean value) = 0;
};

struct UwpBackRequestedHandler : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Invoke(IInspectable *sender, UwpBackRequestedEventArgs *args) = 0;
};

struct UwpSystemNavigationManager : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE add_BackRequested(UwpBackRequestedHandler *handler,
                                                        EventRegistrationToken *token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_BackRequested(EventRegistrationToken token) = 0;
};

struct UwpSystemNavigationManagerStatics : public IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetForCurrentView(UwpSystemNavigationManager **manager) = 0;
};

__CRT_UUID_DECL(UwpBackRequestedEventArgs, 0xd603d28a, 0xe411, 0x4a4e, 0xba, 0x41, 0x6a, 0x32, 0x7a, 0x86, 0x75, 0xbc)
__CRT_UUID_DECL(UwpBackRequestedHandler, 0xca821060, 0x002b, 0x526d, 0x81, 0x22, 0x98, 0x26, 0x30, 0xd7, 0xcd, 0xbe)
__CRT_UUID_DECL(UwpSystemNavigationManager, 0x93023118, 0xcf50, 0x42a6, 0x97, 0x06, 0x69, 0x10, 0x7f, 0xa1, 0x22, 0xe1)
__CRT_UUID_DECL(UwpSystemNavigationManagerStatics, 0xdc52b5ce, 0xbee0, 0x4305, 0x8c, 0x54, 0x68, 0x22, 0x8e, 0xd6, 0x83, 0xb5)

#define UWP_RUNTIMECLASS_SYSTEM_NAVIGATION_MANAGER L"Windows.UI.Core.SystemNavigationManager"
