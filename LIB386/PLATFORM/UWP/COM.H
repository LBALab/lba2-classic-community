#pragma once

/* WinRT called as plain COM interfaces from the llvm-mingw headers: no C++/CX,
   no C++/WinRT, C++98. These are the helpers that needs. */

#include <windows.h>
#include <roapi.h>
#include <winstring.h>

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
