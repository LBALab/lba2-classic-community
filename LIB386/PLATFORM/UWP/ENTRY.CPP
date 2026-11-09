/* The app's entry: CoreApplication runs a view, and the view's Run calls the
   game on the window's own thread. The game's loop pumps the window from its
   event poll, so nothing else has to run. The app ends by returning from Run. */

#include "PRIVATE.H"

#include <windows.applicationmodel.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int Lba2Main(int argc, char *argv[]);

using namespace ABI::Windows::ApplicationModel::Core;
using namespace ABI::Windows::UI::Core;
namespace AM = ABI::Windows::ApplicationModel;
namespace AA = ABI::Windows::ApplicationModel::Activation;

namespace {
enum { MAX_ARGS = 128,
       ARG_SIZE = 1024 };

int s_exitCode = 0;
DWORD s_gameThread = 0;

/* A scripted run (tools/uwp/xbox-lba2cc.py): LocalState\env.txt exists. */
bool s_scripted = false;
char s_exitPath[ARG_SIZE] = "";

enum { MAX_NOTES = 40,
       NOTE_SIZE = 256 };
char s_notes[MAX_NOTES][NOTE_SIZE];
int s_noteCount = 0;
bool s_logReady = false;

const char *ExecutionStateName(AA::ApplicationExecutionState state) {
    switch (state) {
    case AA::ApplicationExecutionState_NotRunning:
        return "not running (first launch, or it crashed)";
    case AA::ApplicationExecutionState_Running:
        return "running";
    case AA::ApplicationExecutionState_Suspended:
        return "suspended";
    case AA::ApplicationExecutionState_Terminated:
        return "ended by the system while suspended";
    case AA::ApplicationExecutionState_ClosedByUser:
        return "closed by the player";
    }
    return "unknown";
}

/* The window is shown from the view's Activated event. */
class OnViewActivated
    : public UwpDelegate<__FITypedEventHandler_2_Windows__CApplicationModel__CCore__CCoreApplicationView_Windows__CApplicationModel__CActivation__CIActivatedEventArgs> {
  public:
    HRESULT STDMETHODCALLTYPE Invoke(ICoreApplicationView *view,
                                     ABI::Windows::ApplicationModel::Activation::IActivatedEventArgs *args) {
        AA::ApplicationExecutionState previous = AA::ApplicationExecutionState_NotRunning;
        if (args)
            args->get_PreviousExecutionState(&previous);
        /* Says how the last run ended, the only record left when the system
           ended a suspended app: that run's log stops at "suspending". */
        Uwp_Note("UWP: activated; the previous run was %s", ExecutionStateName(previous));
        ICoreWindow *window = NULL;
        if (SUCCEEDED(view->get_CoreWindow(&window)) && window)
            window->Activate();
        Uwp_Release(window);
        return S_OK;
    }
};

/* Suspension waits on the deferral; the game keeps what it registered with
   Platform_SetSuspendHandler (the settings), since a suspended app may be
   ended without another event, and the driver's spare memory goes back, as
   every suspended app is asked to. The game's loop simply stops inside the
   event pump until the app resumes. Everything here assumes the event arrives
   on the game's thread, inside its pump; the log says if it ever does not. */
class OnSuspending : public UwpDelegate<__FIEventHandler_1_Windows__CApplicationModel__CSuspendingEventArgs> {
  public:
    HRESULT STDMETHODCALLTYPE Invoke(IInspectable *, AM::ISuspendingEventArgs *args) {
        AM::ISuspendingOperation *operation = NULL;
        AM::ISuspendingDeferral *deferral = NULL;
        if (SUCCEEDED(args->get_SuspendingOperation(&operation)))
            operation->GetDeferral(&deferral);
        const DWORD thread = GetCurrentThreadId();
        if (thread == s_gameThread) {
            Platform_Log(PLATFORM_LOG_INFO, "UWP: suspending");
            /* The game is stopped inside its pump, so its code may run here. */
            Platform_NotifySuspend();
        } else {
            Platform_Log(PLATFORM_LOG_WARN, "UWP: suspending on thread %lu, not the game's %lu; nothing saved",
                         (unsigned long)thread, (unsigned long)s_gameThread);
        }
        Uwp_WindowTrim();
        if (deferral)
            deferral->Complete();
        Uwp_Release(deferral);
        Uwp_Release(operation);
        return S_OK;
    }
};

/* Logged so a run's log shows whether the app came back from a suspension
   or was ended in it. */
class OnResuming : public UwpDelegate<__FIEventHandler_1_IInspectable> {
  public:
    HRESULT STDMETHODCALLTYPE Invoke(IInspectable *, IInspectable *) {
        Uwp_Note("UWP: resuming");
        return S_OK;
    }
};

/* The arguments the game gets: the game data in LocalState\Game when that
   folder exists (otherwise the game's own search, which starts in the app's
   install folder), and any lines of LocalState\args.txt, one argument per
   line, for runs set up from a computer through the Device Portal. */
int BuildArguments(char storage[MAX_ARGS][ARG_SIZE], char *argv[MAX_ARGS]) {
    int argc = 0;
    /* The narrow name, as base_path reads it too. */
    const DWORD n = GetModuleFileNameA(NULL, storage[argc], ARG_SIZE);
    if (n == 0 || n >= ARG_SIZE)
        strcpy(storage[argc], "lba2cc.exe");
    argv[argc] = storage[argc];
    ++argc;

    const char *local = Uwp_LocalState();
    if (!*local)
        return argc;
    char path[ARG_SIZE];
    snprintf(path, sizeof path, "%sGame", local);
    const DWORD attributes = GetFileAttributesA(path);
    /* A scripted run passes every argument itself, the game data's included. */
    if (!s_scripted && attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
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
        /* Said once per run, so a file left behind is not a silent surprise. */
        Uwp_Note("UWP: argument from args.txt: %s", storage[argc]);
        argv[argc] = storage[argc];
        ++argc;
    }
    fclose(file);
    return argc;
}

/* A run set up from a computer by tools/uwp/xbox-lba2cc.py, which writes
   LocalState\env.txt: its NAME=VALUE lines are the environment, the console
   having none to pass, and stdout, stderr and the exit code, which the
   console discards, go to stdout.txt, stderr.txt and exit.txt. Without
   env.txt nothing here happens. */
void StartScriptedRun(void) {
    const char *local = Uwp_LocalState();
    if (!*local)
        return;
    char path[ARG_SIZE];
    snprintf(path, sizeof path, "%senv.txt", local);
    FILE *file = fopen(path, "r");
    if (!file)
        return;
    s_scripted = true;
    char line[ARG_SIZE];
    while (fgets(line, sizeof line, file)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        char *equals = strchr(line, '=');
        if (!equals || equals == line)
            continue;
        *equals = '\0';
        _putenv_s(line, equals + 1);
    }
    fclose(file);

    snprintf(s_exitPath, sizeof s_exitPath, "%sexit.txt", local);
    remove(s_exitPath);
    /* Binary, so lines end as they do on the hosts the output is compared on.
       Unbuffered: the C runtime takes _IOLBF as full buffering, and a crash
       would lose the lines that say where it happened. */
    snprintf(path, sizeof path, "%sstdout.txt", local);
    /* The caller sent both streams to one file: both append to stdout.txt,
       so their lines stay in the order they were written. */
    const char *one = getenv("LBA2_UWP_ONE_STREAM");
    if (one && strcmp(one, "1") == 0) {
        remove(path);
        if (freopen(path, "ab", stdout))
            setvbuf(stdout, NULL, _IONBF, 0);
        if (freopen(path, "ab", stderr))
            setvbuf(stderr, NULL, _IONBF, 0);
    } else {
        if (freopen(path, "wb", stdout))
            setvbuf(stdout, NULL, _IONBF, 0);
        snprintf(path, sizeof path, "%sstderr.txt", local);
        if (freopen(path, "wb", stderr))
            setvbuf(stderr, NULL, _IONBF, 0);
    }
    Uwp_Note("UWP: scripted run (env.txt)");
}

void RecordExit(int code) {
    if (!s_exitPath[0])
        return;
    fflush(NULL);
    FILE *file = fopen(s_exitPath, "w");
    if (file) {
        fprintf(file, "%d\n", code);
        fclose(file);
    }
    s_exitPath[0] = '\0';
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
            OnResuming *resuming = new OnResuming();
            app->add_Resuming(resuming, &token);
            resuming->Release();
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
        StartScriptedRun();
        const int argc = BuildArguments(storage, argv);
        argv[argc] = NULL;
        s_gameThread = GetCurrentThreadId();
        s_exitCode = Lba2Main(argc, argv);
        /* The system may end the process once Run returns, without exit(). */
        RecordExit(s_exitCode);
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

void Uwp_Note(const char *format, ...) {
    char line[NOTE_SIZE];
    va_list ap;
    va_start(ap, format);
    vsnprintf(line, sizeof line, format, ap);
    va_end(ap);
    if (s_logReady)
        Platform_Log(PLATFORM_LOG_INFO, "%s", line);
    else if (s_noteCount < MAX_NOTES)
        strcpy(s_notes[s_noteCount++], line);
}

/* The first pump comes after the game has opened its log. */
void Uwp_FlushNotes(void) {
    if (s_logReady)
        return;
    s_logReady = true;
    for (int i = 0; i < s_noteCount; ++i)
        Platform_Log(PLATFORM_LOG_INFO, "%s", s_notes[i]);
    s_noteCount = 0;
}

/* The game's other ways out call exit(), so a scripted run's exit code is
   caught here too. Linked with --wrap=exit. */
extern "C" void __real_exit(int code);
extern "C" void __wrap_exit(int code) {
    RecordExit(code);
    __real_exit(code);
}

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
