#include "PRIVATE.H"
#include "TRANSLATE.H"

#include <windows.gaming.input.h>

#include <stdlib.h>
#include <string.h>

namespace WGI = ABI::Windows::Gaming::Input;

/* Gamepads are read, not evented: each pump lists the connected pads,
   reports arrivals and departures, takes one reading per pad and reports
   button presses and stick moves from the change since the last pump. */

struct PlatformGamepad {
    bool used;    /* the slot holds a pad, present or still open */
    bool present; /* in the system's list at the last pump */
    bool opened;  /* the engine holds this handle */
    uint32_t id;
    WGI::IGamepad *pad;
    IUnknown *identity; /* compared to recognise the same pad across lists */
    uint32_t buttons;
    int16_t axes[PLATFORM_GAMEPAD_AXIS_COUNT];
};

namespace {
enum { MAX_PADS = 8 };

WGI::IGamepadStatics *s_statics = NULL;
PlatformGamepad s_pads[MAX_PADS];
uint32_t s_nextId = 1;
bool s_init = false;

/* An Xbox lists no gamepads at all unless something subscribes to
   GamepadAdded (SDL_winrtapp_direct3d.cpp, "HACK ALERT"). It does nothing. */
class OnGamepadAdded : public UwpDelegate<__FIEventHandler_1_Windows__CGaming__CInput__CGamepad> {
  public:
    HRESULT STDMETHODCALLTYPE Invoke(IInspectable *, WGI::IGamepad *) { return S_OK; }
};

void PushDevice(PlatformEventType type, uint32_t id) {
    PlatformEvent event;
    memset(&event, 0, sizeof event);
    event.type = type;
    event.gdevice.which = id;
    Uwp_PushEvent(&event);
}

void ClearSlot(PlatformGamepad *slot) {
    Uwp_Release(slot->pad);
    Uwp_Release(slot->identity);
    memset(slot, 0, sizeof *slot);
}

void Read(PlatformGamepad *slot, bool report) {
    WGI::GamepadReading reading;
    memset(&reading, 0, sizeof reading);
    if (slot->present && slot->pad)
        slot->pad->GetCurrentReading(&reading);
    /* All zero while the app has no focus: that reads as centred and released. */
    const uint32_t buttons = (uint32_t)reading.Buttons;
    int16_t axes[PLATFORM_GAMEPAD_AXIS_COUNT];
    axes[PLATFORM_GAMEPAD_AXIS_LEFTX] = UwpPadUnit(reading.LeftThumbstickX, false);
    axes[PLATFORM_GAMEPAD_AXIS_LEFTY] = UwpPadUnit(reading.LeftThumbstickY, true);
    axes[PLATFORM_GAMEPAD_AXIS_RIGHTX] = UwpPadUnit(reading.RightThumbstickX, false);
    axes[PLATFORM_GAMEPAD_AXIS_RIGHTY] = UwpPadUnit(reading.RightThumbstickY, true);
    axes[PLATFORM_GAMEPAD_AXIS_LEFT_TRIGGER] = UwpPadUnit(reading.LeftTrigger, false);
    axes[PLATFORM_GAMEPAD_AXIS_RIGHT_TRIGGER] = UwpPadUnit(reading.RightTrigger, false);
    if (report) {
        PlatformEvent events[UWP_PAD_EVENTS_MAX];
        const int n = UwpPadEvents(slot->id, slot->buttons, slot->axes, buttons, axes, events);
        for (int e = 0; e < n; ++e)
            Uwp_PushEvent(&events[e]);
    }
    slot->buttons = buttons;
    memcpy(slot->axes, axes, sizeof axes);
}

/* Matches the system's list against the slots, reporting changes. */
void Scan() {
    if (!s_statics)
        return;
    bool seen[MAX_PADS] = {false};
    __FIVectorView_1_Windows__CGaming__CInput__CGamepad *list = NULL;
    unsigned int count = 0;
    if (FAILED(s_statics->get_Gamepads(&list)) || FAILED(list->get_Size(&count))) {
        Uwp_Release(list);
        return;
    }
    for (unsigned int i = 0; i < count; ++i) {
        WGI::IGamepad *pad = NULL;
        IUnknown *identity = NULL;
        if (FAILED(list->GetAt(i, &pad)) || FAILED(pad->QueryInterface(__uuidof(IUnknown), (void **)&identity))) {
            Uwp_Release(pad);
            continue;
        }
        int found = -1;
        for (int s = 0; s < MAX_PADS && found < 0; ++s)
            if (s_pads[s].used && s_pads[s].present && s_pads[s].identity == identity)
                found = s;
        if (found < 0) {
            for (int s = 0; s < MAX_PADS && found < 0; ++s)
                if (!s_pads[s].used)
                    found = s;
            if (found < 0) {
                Uwp_Release(identity);
                Uwp_Release(pad);
                continue;
            }
            PlatformGamepad *slot = &s_pads[found];
            slot->used = slot->present = true;
            slot->id = s_nextId++;
            slot->pad = pad;
            slot->identity = identity;
            Read(slot, false);
            PushDevice(PLATFORM_EVENT_GAMEPAD_ADDED, slot->id);
        } else {
            Uwp_Release(identity);
            Uwp_Release(pad);
        }
        seen[found] = true;
    }
    Uwp_Release(list);
    for (int s = 0; s < MAX_PADS; ++s) {
        PlatformGamepad *slot = &s_pads[s];
        if (!slot->used || !slot->present || seen[s])
            continue;
        const uint32_t id = slot->id;
        slot->present = false;
        Uwp_Release(slot->pad);
        Read(slot, false);
        if (!slot->opened)
            ClearSlot(slot);
        PushDevice(PLATFORM_EVENT_GAMEPAD_REMOVED, id);
    }
}

PlatformGamepad *Find(uint32_t id) {
    for (int s = 0; s < MAX_PADS; ++s)
        if (s_pads[s].used && s_pads[s].present && s_pads[s].id == id)
            return &s_pads[s];
    return NULL;
}

bool Valid(PlatformGamepad *pad) {
    return pad >= s_pads && pad < s_pads + MAX_PADS && pad->used && pad->opened;
}
} // namespace

void Uwp_WatchGamepads(void) {
    if (s_statics)
        return;
    if (FAILED(Uwp_Factory(RuntimeClass_Windows_Gaming_Input_Gamepad, __uuidof(WGI::IGamepadStatics),
                           (void **)&s_statics))) {
        Platform_Log(PLATFORM_LOG_WARN, "UWP: Windows.Gaming.Input is unavailable");
        return;
    }
    EventRegistrationToken token;
    OnGamepadAdded *added = new OnGamepadAdded();
    s_statics->add_GamepadAdded(added, &token);
    added->Release();
}

void Uwp_PollGamepads(void) {
    if (!s_init)
        return;
    Scan();
    for (int s = 0; s < MAX_PADS; ++s)
        if (s_pads[s].used && s_pads[s].present)
            Read(&s_pads[s], true);
}

bool Uwp_InitGamepads(void) {
    if (!s_statics)
        return Uwp_Fail("UWP: Windows.Gaming.Input is unavailable");
    s_init = true;
    return true;
}

void Uwp_ShutdownGamepads(void) {
    for (int s = 0; s < MAX_PADS; ++s)
        ClearSlot(&s_pads[s]);
    s_init = false;
}

uint32_t *Uwp_Gamepads(int32_t *count) {
    if (!count) {
        Uwp_Fail("UWP gamepad count output is missing");
        return NULL;
    }
    Scan();
    uint32_t *ids = (uint32_t *)calloc(MAX_PADS, sizeof(uint32_t));
    if (!ids) {
        Uwp_Fail("Unable to allocate the gamepad list");
        return NULL;
    }
    int32_t n = 0;
    for (int s = 0; s < MAX_PADS; ++s)
        if (s_pads[s].used && s_pads[s].present)
            ids[n++] = s_pads[s].id;
    *count = n;
    return ids;
}

PlatformGamepad *Uwp_OpenGamepad(uint32_t id) {
    PlatformGamepad *pad = Find(id);
    if (!pad) {
        Uwp_Fail("UWP: no gamepad %u", id);
        return NULL;
    }
    pad->opened = true;
    return pad;
}

void Uwp_CloseGamepad(PlatformGamepad *pad) {
    if (!Valid(pad)) {
        if (pad)
            Uwp_Fail("UWP: closing a gamepad that is not open");
        return;
    }
    pad->opened = false;
    if (!pad->present)
        ClearSlot(pad);
}

const char *Uwp_GamepadName(PlatformGamepad *pad) {
    if (!Valid(pad)) {
        Uwp_Fail("UWP: no such gamepad");
        return "";
    }
    return "Xbox Controller";
}

bool Uwp_GamepadButton(PlatformGamepad *pad, PlatformGamepadButton button) {
    if (!Valid(pad))
        return Uwp_Fail("UWP: no such gamepad");
    return UwpPadButton(pad->buttons, button);
}

int16_t Uwp_GamepadAxis(PlatformGamepad *pad, PlatformGamepadAxis axis) {
    if (!Valid(pad)) {
        Uwp_Fail("UWP: no such gamepad");
        return 0;
    }
    if (axis < 0 || axis >= PLATFORM_GAMEPAD_AXIS_COUNT) {
        Uwp_Fail("UWP: invalid gamepad axis %d", (int)axis);
        return 0;
    }
    return pad->axes[axis];
}
