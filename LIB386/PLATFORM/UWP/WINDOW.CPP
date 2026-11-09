#include "PRIVATE.H"
#include "TRANSLATE.H"

#include <windows.graphics.display.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <d2d1_1.h>

#include <stdlib.h>
#include <string.h>

using namespace ABI::Windows::UI::Core;

/* The swap chain is the window's size and never scaled by DXGI. Direct2D
   draws the game's frame into it, letterboxed, with the filter the player
   chose, so no shader has to be compiled. The window is always fullscreen. */

namespace {
bool s_initialized = false;
ID3D11Device *s_device = NULL;
ID3D11DeviceContext *s_context = NULL;
IDXGISwapChain1 *s_swap = NULL;
ID2D1Factory1 *s_d2dFactory = NULL;
ID2D1Device *s_d2dDevice = NULL;
ID2D1DeviceContext *s_d2d = NULL;
ID2D1Bitmap1 *s_target = NULL;
ID2D1Bitmap1 *s_frame = NULL;
uint32_t *s_pixels = NULL;
uint32_t s_frameW = 0, s_frameH = 0;
uint32_t s_windowW = 0, s_windowH = 0;
bool s_vsync = true;
PlatformPresentFilter s_filter = PLATFORM_FILTER_NEAREST;

/* The window's size in pixels: its bounds are in DIPs. */
bool MeasureWindow(uint32_t *width, uint32_t *height) {
    ICoreWindow *window = Uwp_CoreWindow();
    ABI::Windows::Foundation::Rect bounds;
    if (!window || FAILED(window->get_Bounds(&bounds)))
        return false;
    float dpi = 96.0f;
    ABI::Windows::Graphics::Display::IDisplayInformationStatics *statics = NULL;
    ABI::Windows::Graphics::Display::IDisplayInformation *info = NULL;
    if (SUCCEEDED(Uwp_Factory(RuntimeClass_Windows_Graphics_Display_DisplayInformation,
                              __uuidof(ABI::Windows::Graphics::Display::IDisplayInformationStatics),
                              (void **)&statics)) &&
        SUCCEEDED(statics->GetForCurrentView(&info)))
        info->get_LogicalDpi(&dpi);
    Uwp_Release(info);
    Uwp_Release(statics);
    *width = (uint32_t)(bounds.Width * dpi / 96.0f + 0.5f);
    *height = (uint32_t)(bounds.Height * dpi / 96.0f + 0.5f);
    return *width > 0 && *height > 0;
}

D2D1_BITMAP_PROPERTIES1 BitmapProperties(D2D1_BITMAP_OPTIONS options) {
    D2D1_BITMAP_PROPERTIES1 props;
    memset(&props, 0, sizeof props);
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    props.dpiX = props.dpiY = 96.0f;
    props.bitmapOptions = options;
    return props;
}

bool CreateTarget() {
    IDXGISurface *surface = NULL;
    HRESULT hr = s_swap->GetBuffer(0, __uuidof(IDXGISurface), (void **)&surface);
    if (SUCCEEDED(hr)) {
        D2D1_BITMAP_PROPERTIES1 props = BitmapProperties(
            (D2D1_BITMAP_OPTIONS)(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW));
        hr = s_d2d->CreateBitmapFromDxgiSurface(surface, &props, &s_target);
    }
    Uwp_Release(surface);
    if (FAILED(hr))
        return Uwp_Fail("UWP: cannot draw into the swap chain (0x%08lx)", (unsigned long)hr);
    s_d2d->SetTarget(s_target);
    return true;
}

bool CreateFrameBitmap() {
    Uwp_Release(s_frame);
    free(s_pixels);
    s_pixels = (uint32_t *)calloc((size_t)s_frameW * s_frameH, sizeof(uint32_t));
    if (!s_pixels)
        return Uwp_Fail("UWP: cannot allocate a %ux%u frame", s_frameW, s_frameH);
    D2D1_SIZE_U size = {s_frameW, s_frameH};
    D2D1_BITMAP_PROPERTIES1 props = BitmapProperties(D2D1_BITMAP_OPTIONS_NONE);
    HRESULT hr = s_d2d->CreateBitmap(size, NULL, 0, &props, &s_frame);
    if (FAILED(hr))
        return Uwp_Fail("UWP: cannot create a %ux%u frame bitmap (0x%08lx)", s_frameW, s_frameH, (unsigned long)hr);
    return true;
}

void ReleaseDevice() {
    if (s_d2d)
        s_d2d->SetTarget(NULL);
    Uwp_Release(s_frame);
    Uwp_Release(s_target);
    Uwp_Release(s_d2d);
    Uwp_Release(s_d2dDevice);
    Uwp_Release(s_d2dFactory);
    Uwp_Release(s_swap);
    if (s_context) {
        s_context->ClearState();
        s_context->Flush();
    }
    Uwp_Release(s_context);
    Uwp_Release(s_device);
}

bool CreateDevice() {
    ICoreWindow *window = Uwp_CoreWindow();
    if (!window)
        return Uwp_Fail("UWP: no window to present to");
    if (!MeasureWindow(&s_windowW, &s_windowH))
        return Uwp_Fail("UWP: cannot measure the window");
    D3D_FEATURE_LEVEL level;
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   NULL, 0, D3D11_SDK_VERSION, &s_device, &level, &s_context);
    if (FAILED(hr))
        return Uwp_Fail("UWP: no Direct3D 11 device (0x%08lx)", (unsigned long)hr);

    IDXGIDevice *dxgiDevice = NULL;
    IDXGIAdapter *adapter = NULL;
    IDXGIFactory2 *factory = NULL;
    hr = s_device->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgiDevice);
    if (SUCCEEDED(hr))
        hr = dxgiDevice->GetAdapter(&adapter);
    if (SUCCEEDED(hr))
        hr = adapter->GetParent(__uuidof(IDXGIFactory2), (void **)&factory);
    if (SUCCEEDED(hr)) {
        DXGI_SWAP_CHAIN_DESC1 desc;
        memset(&desc, 0, sizeof desc);
        desc.Width = s_windowW;
        desc.Height = s_windowH;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_NONE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        hr = factory->CreateSwapChainForCoreWindow(s_device, window, &desc, NULL, &s_swap);
    }
    if (SUCCEEDED(hr)) {
        /* One queued frame: the game paces itself and should not run ahead. */
        IDXGIDevice1 *device1 = NULL;
        if (SUCCEEDED(dxgiDevice->QueryInterface(__uuidof(IDXGIDevice1), (void **)&device1)))
            device1->SetMaximumFrameLatency(1);
        Uwp_Release(device1);
        D2D1_FACTORY_OPTIONS options;
        options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options,
                               (void **)&s_d2dFactory);
    }
    if (SUCCEEDED(hr))
        hr = s_d2dFactory->CreateDevice(dxgiDevice, &s_d2dDevice);
    if (SUCCEEDED(hr))
        hr = s_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &s_d2d);
    Uwp_Release(factory);
    Uwp_Release(adapter);
    Uwp_Release(dxgiDevice);
    if (FAILED(hr)) {
        ReleaseDevice();
        return Uwp_Fail("UWP: cannot set up presentation (0x%08lx)", (unsigned long)hr);
    }
    if (!CreateTarget()) {
        ReleaseDevice();
        return false;
    }
    Platform_Log(PLATFORM_LOG_INFO, "UWP: presenting to a %ux%u window", s_windowW, s_windowH);
    return true;
}

/* A lost device (driver update, GPU reset) is recreated whole. */
bool RecreateDevice() {
    Platform_Log(PLATFORM_LOG_WARN, "UWP: the graphics device was lost; recreating it");
    ReleaseDevice();
    return CreateDevice() && (s_frameW == 0 || CreateFrameBitmap());
}

bool DeviceLost(HRESULT hr) {
    return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == D2DERR_RECREATE_TARGET;
}

bool Draw() {
    s_d2d->BeginDraw();
    D2D1_COLOR_F black = {0.0f, 0.0f, 0.0f, 1.0f};
    s_d2d->Clear(&black);
    const UwpRect rect = UwpLetterbox(s_windowW, s_windowH, s_frameW, s_frameH);
    D2D1_RECT_F dest = {(float)rect.x, (float)rect.y, (float)(rect.x + rect.w), (float)(rect.y + rect.h)};
    s_d2d->DrawBitmap(s_frame, &dest, 1.0f,
                      s_filter == PLATFORM_FILTER_LINEAR ? D2D1_INTERPOLATION_MODE_LINEAR
                                                         : D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                      NULL, NULL);
    HRESULT hr = s_d2d->EndDraw(NULL, NULL);
    if (SUCCEEDED(hr))
        hr = s_swap->Present(s_vsync ? 1 : 0, 0);
    if (DeviceLost(hr))
        return RecreateDevice() || Uwp_Fail("UWP: the graphics device could not be recreated");
    if (FAILED(hr))
        return Uwp_Fail("UWP: present failed (0x%08lx)", (unsigned long)hr);
    return true;
}
} // namespace

void Uwp_WindowSizeChanged(void) {
    if (!s_swap)
        return;
    uint32_t width, height;
    if (!MeasureWindow(&width, &height) || (width == s_windowW && height == s_windowH))
        return;
    s_d2d->SetTarget(NULL);
    Uwp_Release(s_target);
    HRESULT hr = s_swap->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (DeviceLost(hr)) {
        RecreateDevice();
        return;
    }
    if (FAILED(hr)) {
        Uwp_Fail("UWP: cannot resize the swap chain (0x%08lx)", (unsigned long)hr);
        return;
    }
    s_windowW = width;
    s_windowH = height;
    CreateTarget();
}

/* Asked of every app being suspended: give back the driver's spare memory. */
void Uwp_WindowTrim(void) {
    if (!s_device)
        return;
    s_context->ClearState();
    IDXGIDevice3 *device3 = NULL;
    if (SUCCEEDED(s_device->QueryInterface(__uuidof(IDXGIDevice3), (void **)&device3)))
        device3->Trim();
    Uwp_Release(device3);
}

bool Uwp_WindowInit(const char *title) {
    if (!title)
        return Uwp_Fail("UWP presentation requires a title");
    if (!Uwp_CoreWindow())
        return Uwp_Fail("UWP: no app window; the entry point attaches it");
    s_initialized = true;
    return true;
}

void Uwp_WindowShutdown(void) {
    Uwp_WindowDestroy();
    ReleaseDevice();
    s_initialized = false;
}

bool Uwp_WindowInitialized(void) { return s_initialized; }

bool Uwp_WindowCreate(uint32_t width, uint32_t height, bool) {
    if (!s_initialized)
        return Uwp_Fail("UWP window is not initialised");
    if (s_frame)
        return Uwp_Fail("UWP presentation surface already exists");
    if (width == 0 || height == 0 || width > 16384 || height > 16384)
        return Uwp_Fail("UWP: invalid frame size %ux%u", width, height);
    if (!s_swap && !CreateDevice())
        return false;
    s_frameW = width;
    s_frameH = height;
    return CreateFrameBitmap();
}

void Uwp_WindowDestroy(void) {
    Uwp_Release(s_frame);
    free(s_pixels);
    s_pixels = NULL;
    s_frameW = s_frameH = 0;
}

bool Uwp_WindowResize(uint32_t width, uint32_t height, bool) {
    if (!s_d2d)
        return Uwp_Fail("UWP presentation surface is unavailable");
    if (width == 0 || height == 0 || width > 16384 || height > 16384)
        return Uwp_Fail("UWP: invalid frame size %ux%u", width, height);
    s_frameW = width;
    s_frameH = height;
    return CreateFrameBitmap();
}

bool Uwp_WindowSize(int32_t *width, int32_t *height) {
    if (!width || !height)
        return Uwp_Fail("UWP window size outputs are missing");
    if (!s_swap && !MeasureWindow(&s_windowW, &s_windowH))
        return Uwp_Fail("UWP: cannot measure the window");
    *width = (int32_t)s_windowW;
    *height = (int32_t)s_windowH;
    return true;
}

/* The app always fills the screen, and DXGI may report no outputs at all
   (a known certification failure), so the window stands for the display. */
bool Uwp_DisplaySize(uint32_t *width, uint32_t *height) {
    if (!width || !height)
        return Uwp_Fail("UWP display size outputs are missing");
    if (s_swap) {
        *width = s_windowW;
        *height = s_windowH;
        return true;
    }
    return MeasureWindow(width, height) || Uwp_Fail("UWP: cannot measure the window");
}

void Uwp_SurfaceCoords(int32_t x, int32_t y, int32_t *sx, int32_t *sy) {
    if (!sx || !sy) {
        Uwp_Fail("UWP coordinate outputs are missing");
        return;
    }
    if (!s_frameW || !s_frameH) {
        *sx = x;
        *sy = y;
        return;
    }
    UwpWindowToFrame(UwpLetterbox(s_windowW, s_windowH, s_frameW, s_frameH), s_frameW, s_frameH, x, y, sx, sy);
}

void Uwp_SetVSync(bool enabled) { s_vsync = enabled; }

bool Uwp_SetPresentFilter(PlatformPresentFilter mode) {
    if (mode != PLATFORM_FILTER_NEAREST && mode != PLATFORM_FILTER_LINEAR)
        return Uwp_Fail("UWP presentation has nearest and linear filters only");
    s_filter = mode;
    return true;
}

bool Uwp_Present(const PlatformFrame *frame) {
    if (!frame || !s_frame || frame->width != s_frameW || frame->height != s_frameH)
        return Uwp_Fail("Indexed frame does not match the UWP presentation surface");
    const uint32_t pitch = s_frameW * sizeof(uint32_t);
    if (!Platform_ConvertIndexedFrame(frame, s_pixels, pitch))
        return false;
    if (frame->overlay)
        frame->overlay(s_pixels, pitch, s_frameW, s_frameH, frame->palette_rgb);
    HRESULT hr = s_frame->CopyFromMemory(NULL, s_pixels, pitch);
    if (FAILED(hr))
        return Uwp_Fail("UWP: frame upload failed (0x%08lx)", (unsigned long)hr);
    return Draw();
}
