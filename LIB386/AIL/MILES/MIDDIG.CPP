#include <system/adeline.h>

#ifdef _WIN32
#include <ail/mssw.h>
#else
#include <ail/mss.h>
#endif

#include <stdlib.h>

#include "COMMON_MILES.H"
#include "MIDDIG.H"
#include "MIDI_MILES.H"
#include "SAMPLE_MILES.H"

// ████████████████████████████████████████████████████████████████████████████

extern HDIGDRIVER DigDriver; // Handle to digital driver
extern HMDIDRIVER MdiDriver; // Handle to midi driver

static HWAVESYNTH WaveSynthesizer = NULL;
static void *WaveLib = NULL;
static S32 SizeLib = 0;

// ████████████████████████████████████████████████████████████████████████████

void InitSampleSynthesizer(void *wavelib, S32 sizelib, S32 voices)
{
    if ((!Midi_Driver_Enabled) || (!Sample_Driver_Enabled))
        return;

    if (WaveLib)
    {
        AIL_destroy_wave_synthesizer((WAVE_SYNTH *)WaveSynthesizer);
        AIL_vmm_unlock(WaveLib, SizeLib);
    }

    // lock wave table
    AIL_vmm_lock(wavelib, sizelib);

    // link table to both drivers
    WaveSynthesizer = AIL_create_wave_synthesizer(DigDriver, MdiDriver, wavelib, voices);
    WaveLib = wavelib;
    SizeLib = sizelib;
}

// ████████████████████████████████████████████████████████████████████████████

void ClearSampleSynthesizer()
{
    if ((!Midi_Driver_Enabled) || (!Sample_Driver_Enabled))
        return;

    if (!WaveLib)
        return;

    AIL_destroy_wave_synthesizer((WAVE_SYNTH *)WaveSynthesizer);
    AIL_vmm_unlock(WaveLib, SizeLib);

    WaveLib = NULL;
    SizeLib = 0;
}

// ████████████████████████████████████████████████████████████████████████████
