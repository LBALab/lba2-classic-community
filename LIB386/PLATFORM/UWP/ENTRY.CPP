/* The app's entry: CoreApplication runs a view, and the view's Run calls the
   game on the window's own thread. The game's loop pumps the window from its
   event poll, so nothing else has to run. The app ends by returning from Run. */

#include "PRIVATE.H"

#include <windows.applicationmodel.h>

#include <stdio.h>
#include <string.h>

int Lba2Main(int argc, char *argv[]);

using namespace ABI::Windows::ApplicationModel::Core;
using namespace ABI::Windows::UI::Core;
namespace AM = ABI::Windows::ApplicationModel;

namespace {
enum { MAX_ARGS = 32,
       ARG_SIZE = 1024 };

int s_exitCode = 0;

/* The window is shown from the view's Activated event. */
class OnViewActivated
    : public UwpDelegate<__FITypedEventHandler_2_Windows__CApplicationModel__CCore__CCoreApplicationView_Windows__CApplicationModel__CActivation__CIActivatedEventArgs> {
  public:
    HRESULT STDMETHODCALLTYPE Invoke(ICoreApplicationView *view,
                                     ABI::Windows::ApplicationModel::Activation::IActivatedEventArgs *) {
        ICoreWindow *window = NULL;
        if (SUCCEEDED(view->get_CoreWindow(&window)) && window)
            window->Activate();
        Uwp_Release(window);
        return S_OK;
    }
};

/* Suspension waits on the deferral; the driver's spare memory goes back
   first, as every suspended app is asked to. The game is not told: its loop
   simply stops inside the event pump until the app resumes. */
class OnSuspending : public UwpDelegate<__FIEventHandler_1_Windows__CApplicationModel__CSuspendingEventArgs> {
  public:
    HRESULT STDMETHODCALLTYPE Invoke(IInspectable *, AM::ISuspendingEventArgs *args) {
        AM::ISuspendingOperation *operation = NULL;
        AM::ISuspendingDeferral *deferral = NULL;
        if (SUCCEEDED(args->get_SuspendingOperation(&operation)))
            operation->GetDeferral(&deferral);
        Platform_Log(PLATFORM_LOG_INFO, "UWP: suspending");
        Uwp_WindowTrim();
        if (deferral)
            deferral->Complete();
        Uwp_Release(deferral);
        Uwp_Release(operation);
        return S_OK;
    }
};

/* The arguments the game gets: the game data in LocalState\Game when that
   folder exists (otherwise the game's own search, which starts in the app's
   install folder), and any lines of LocalState\args.txt, one argument per
   line, for runs set up from a computer through the Device Portal. */
int BuildArguments(char storage[MAX_ARGS][ARG_SIZE], char *argv[MAX_ARGS]) {
    int argc = 0;
    WCHAR exe[MAX_PATH];
    const DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (n == 0 || n >= MAX_PATH ||
        !WideCharToMultiByte(CP_UTF8, 0, exe, -1, storage[argc], ARG_SIZE, NULL, NULL))
        strcpy(storage[argc], "lba2cc.exe");
    argv[argc] = storage[argc];
    ++argc;

    const char *local = Uwp_LocalState();
    if (!*local)
        return argc;
    char path[ARG_SIZE];
    snprintf(path, sizeof path, "%sGame", local);
    const DWORD attributes = GetFileAttributesA(path);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        strcpy(storage[argc], "--game-dir");
        argv[argc] = storage[argc];
        ++argc;
        strcpy(storage[argc], path);
        argv[argc] = storage[argc];
        ++argc;
    }

    snprintf(path, sizeof path, "%sargs.txt", local);
    FILE *file = fopen(path, "r");
    if (!file)
        return argc;
    while (argc < MAX_ARGS && fgets(storage[argc], ARG_SIZE, file)) {
        size_t len = strlen(storage[argc]);
        while (len > 0 && (storage[argc][len - 1] == '\n' || storage[argc][len - 1] == '\r'))
            storage[argc][--len] = '\0';
        if (len == 0)
            continue;
        argv[argc] = storage[argc];
        ++argc;
    }
    fclose(file);
    return argc;
}

class View : public UwpInspectable<IFrameworkView> {
  public:
    HRESULT STDMETHODCALLTYPE Initialize(ICoreApplicationView *view) {
        EventRegistrationToken token;
        OnViewActivated *activated = new OnViewActivated();
        view->add_Activated(activated, &token);
        activated->Release();

        ICoreApplication *app = NULL;
        if (SUCCEEDED(Uwp_Factory(RuntimeClass_Windows_ApplicationModel_Core_CoreApplication,
                                  __uuidof(ICoreApplication), (void **)&app))) {
            OnSuspending *suspending = new OnSuspending();
            app->add_Suspending(suspending, &token);
            suspending->Release();
        }
        Uwp_Release(app);
        Uwp_WatchGamepads();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetWindow(ICoreWindow *window) {
        Uwp_AttachWindow(window);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Load(HSTRING) { return S_OK; }
    HRESULT STDMETHODCALLTYPE Run() {
        static char storage[MAX_ARGS][ARG_SIZE];
        char *argv[MAX_ARGS + 1];
        const int argc = BuildArguments(storage, argv);
        argv[argc] = NULL;
        s_exitCode = Lba2Main(argc, argv);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Uninitialize() {
        Uwp_DetachWindow();
        return S_OK;
    }
};

class ViewSource : public UwpInspectable<IFrameworkViewSource> {
  public:
    HRESULT STDMETHODCALLTYPE CreateView(IFrameworkView **view) {
        *view = new View();
        return S_OK;
    }
};
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    if (FAILED(RoInitialize(RO_INIT_MULTITHREADED)))
        return 1;
    ICoreApplication *app = NULL;
    HRESULT hr = Uwp_Factory(RuntimeClass_Windows_ApplicationModel_Core_CoreApplication,
                             __uuidof(ICoreApplication), (void **)&app);
    if (SUCCEEDED(hr)) {
        ViewSource *source = new ViewSource();
        hr = app->Run(source);
        source->Release();
    }
    Uwp_Release(app);
    RoUninitialize();
    return SUCCEEDED(hr) ? s_exitCode : 1;
}
