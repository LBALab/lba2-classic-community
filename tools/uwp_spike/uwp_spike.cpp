/* UWP spike for an Xbox Dev Mode backend: no SDL, no C++/CX, C++98.
 *
 * Opens a CoreWindow through hand-written IFrameworkViewSource and
 * IFrameworkView objects, presents a 640x480 BGRA frame each tick through a
 * D3D11 swap chain that DXGI scales with letterboxing, moves a square with
 * the first gamepad's left stick, and writes a log to LocalState. B quits.
 *
 * It answers one question: can llvm-mingw build the pieces a lba2cc UWP
 * platform backend needs. It is not part of the game build. */

#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.applicationmodel.core.h>
#include <windows.ui.core.h>
#include <windows.gaming.input.h>
#include <windows.storage.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <stdio.h>
#include <string.h>

using namespace ABI::Windows::ApplicationModel::Core;
using namespace ABI::Windows::UI::Core;
namespace WGI = ABI::Windows::Gaming::Input;
namespace WS = ABI::Windows::Storage;

enum { FRAME_W = 640,
       FRAME_H = 480 };

static FILE *g_log = NULL;
static UINT32 g_frame[FRAME_W * FRAME_H];

static void Log(const char *msg) {
    if (g_log) {
        fprintf(g_log, "%s\n", msg);
        fflush(g_log);
    }
}

template <class T>
static void SafeRelease(T *&p) {
    if (p) {
        p->Release();
        p = NULL;
    }
}

static HRESULT Factory(const WCHAR *name, REFIID iid, void **out) {
    HSTRING_HEADER header;
    HSTRING cls;
    HRESULT hr = WindowsCreateStringReference(name, (UINT32)wcslen(name), &header, &cls);
    if (FAILED(hr))
        return hr;
    return RoGetActivationFactory(cls, iid, out);
}

/* LocalState is the only folder a UWP app can write besides temp. */
static void OpenLog(void) {
    WS::IApplicationDataStatics *statics = NULL;
    WS::IApplicationData *data = NULL;
    WS::IStorageFolder *folder = NULL;
    WS::IStorageItem *item = NULL;
    HSTRING path = NULL;
    if (SUCCEEDED(Factory(RuntimeClass_Windows_Storage_ApplicationData,
                          __uuidof(WS::IApplicationDataStatics), (void **)&statics)) &&
        SUCCEEDED(statics->get_Current(&data)) && SUCCEEDED(data->get_LocalFolder(&folder)) &&
        SUCCEEDED(folder->QueryInterface(__uuidof(WS::IStorageItem), (void **)&item)) &&
        SUCCEEDED(item->get_Path(&path))) {
        WCHAR file[MAX_PATH];
        _snwprintf(file, MAX_PATH, L"%s\\uwp_spike.log", WindowsGetStringRawBuffer(path, NULL));
        file[MAX_PATH - 1] = 0;
        g_log = _wfopen(file, L"w");
    }
    if (path)
        WindowsDeleteString(path);
    SafeRelease(item);
    SafeRelease(folder);
    SafeRelease(data);
    SafeRelease(statics);
}

/* --- Presentation ------------------------------------------------------ */

static ID3D11Device *g_device = NULL;
static ID3D11DeviceContext *g_context = NULL;
static IDXGISwapChain1 *g_swap = NULL;

static bool CreateSwapChain(ICoreWindow *window) {
    D3D_FEATURE_LEVEL level;
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   NULL, 0, D3D11_SDK_VERSION, &g_device, &level, &g_context);
    if (FAILED(hr))
        return false;
    IDXGIDevice *dxgiDevice = NULL;
    IDXGIAdapter *adapter = NULL;
    IDXGIFactory2 *factory = NULL;
    bool ok = SUCCEEDED(g_device->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgiDevice)) &&
              SUCCEEDED(dxgiDevice->GetAdapter(&adapter)) &&
              SUCCEEDED(adapter->GetParent(__uuidof(IDXGIFactory2), (void **)&factory));
    if (ok) {
        /* The back buffer is the game's own size; DXGI scales it to the
           window and keeps the aspect ratio, so no shader is needed. */
        DXGI_SWAP_CHAIN_DESC1 desc;
        memset(&desc, 0, sizeof desc);
        desc.Width = FRAME_W;
        desc.Height = FRAME_H;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        ok = SUCCEEDED(factory->CreateSwapChainForCoreWindow(g_device, window, &desc, NULL, &g_swap));
    }
    SafeRelease(factory);
    SafeRelease(adapter);
    SafeRelease(dxgiDevice);
    return ok;
}

static void Present(void) {
    ID3D11Texture2D *back = NULL;
    if (SUCCEEDED(g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back))) {
        g_context->UpdateSubresource(back, 0, NULL, g_frame, FRAME_W * 4, 0);
        SafeRelease(back);
    }
    g_swap->Present(1, 0);
}

/* --- Gamepad ------------------------------------------------------------ */

static WGI::IGamepadStatics *g_pads = NULL;

static bool ReadPad(WGI::GamepadReading *reading) {
    __FIVectorView_1_Windows__CGaming__CInput__CGamepad *list = NULL;
    WGI::IGamepad *pad = NULL;
    unsigned int count = 0;
    bool ok = g_pads && SUCCEEDED(g_pads->get_Gamepads(&list)) && SUCCEEDED(list->get_Size(&count)) &&
              count > 0 && SUCCEEDED(list->GetAt(0, &pad)) && SUCCEEDED(pad->GetCurrentReading(reading));
    SafeRelease(pad);
    SafeRelease(list);
    return ok;
}

/* --- The app ------------------------------------------------------------ */

static void Draw(int x, int y) {
    for (int i = 0; i < FRAME_W * FRAME_H; ++i)
        g_frame[i] = 0xFF203040;
    for (int row = y; row < y + 32; ++row)
        for (int col = x; col < x + 32; ++col)
            if (row >= 0 && row < FRAME_H && col >= 0 && col < FRAME_W)
                g_frame[row * FRAME_W + col] = 0xFFE0C040;
}

/* Hand-written WinRT objects. A view is created once and lives until
   CoreApplication::Run returns, so reference counts only need to be right,
   not thread-safe. */
template <class I>
class Inspectable : public I {
  public:
    Inspectable() : m_refs(1) {
    }
    virtual ~Inspectable() {
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IInspectable) || iid == __uuidof(I)) {
            *out = static_cast<I *>(this);
            AddRef();
            return S_OK;
        }
        *out = NULL;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() {
        return (ULONG)InterlockedIncrement(&m_refs);
    }
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

class View : public Inspectable<IFrameworkView> {
  public:
    View() : m_window(NULL) {
    }
    ~View() {
        SafeRelease(m_window);
    }
    HRESULT STDMETHODCALLTYPE Initialize(ICoreApplicationView *) {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetWindow(ICoreWindow *window) {
        m_window = window;
        m_window->AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Load(HSTRING) {
        return S_OK;
    }
    /* The engine's blocking loop would run here, pumping the dispatcher
       from its event poll, as this loop does. */
    HRESULT STDMETHODCALLTYPE Run() {
        OpenLog();
        Log("run");
        m_window->Activate();
        if (!CreateSwapChain(m_window)) {
            Log("swap chain failed");
            return E_FAIL;
        }
        Factory(RuntimeClass_Windows_Gaming_Input_Gamepad, __uuidof(WGI::IGamepadStatics), (void **)&g_pads);
        ICoreDispatcher *dispatcher = NULL;
        m_window->get_Dispatcher(&dispatcher);
        int x = FRAME_W / 2, y = FRAME_H / 2;
        bool padSeen = false;
        for (;;) {
            dispatcher->ProcessEvents(CoreProcessEventsOption_ProcessAllIfPresent);
            WGI::GamepadReading pad;
            if (ReadPad(&pad)) {
                if (!padSeen) {
                    Log("gamepad seen");
                    padSeen = true;
                }
                if (pad.Buttons & WGI::GamepadButtons_B)
                    break;
                x += (int)(pad.LeftThumbstickX * 8.0);
                y -= (int)(pad.LeftThumbstickY * 8.0);
            }
            Draw(x, y);
            Present();
        }
        Log("quit");
        SafeRelease(dispatcher);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Uninitialize() {
        return S_OK;
    }

  private:
    ICoreWindow *m_window;
};

class ViewSource : public Inspectable<IFrameworkViewSource> {
  public:
    HRESULT STDMETHODCALLTYPE CreateView(IFrameworkView **view) {
        *view = new View();
        return S_OK;
    }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    if (FAILED(RoInitialize(RO_INIT_MULTITHREADED)))
        return 1;
    ICoreApplication *app = NULL;
    if (FAILED(Factory(RuntimeClass_Windows_ApplicationModel_Core_CoreApplication, __uuidof(ICoreApplication),
                       (void **)&app)))
        return 1;
    ViewSource *source = new ViewSource();
    HRESULT hr = app->Run(source);
    source->Release();
    SafeRelease(app);
    SafeRelease(g_pads);
    SafeRelease(g_swap);
    SafeRelease(g_context);
    SafeRelease(g_device);
    if (g_log)
        fclose(g_log);
    RoUninitialize();
    return SUCCEEDED(hr) ? 0 : 1;
}
