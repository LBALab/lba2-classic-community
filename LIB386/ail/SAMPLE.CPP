#include	<system\adeline.h>
#include	<system\logprint.h>
#include	<system\exit.h>

#include 	<stdio.h>
#include 	<stdlib.h>
#include 	<string.h>

#ifdef	YAZ_WIN32
#include 	"mixer.h"
#include	<system\winsys.h>
#include	<system\timerwin.h>
#else
#include	<system\timerwin.h> //yaz
#endif

#include	<ail\common.h>
#include	<ail\sample.h>
#include	<ail\stream.h>

#pragma pack(8)
#include	"soloud.h"
#include	"soloud_wav.h"
#include    "ElasticBuffer.h"
#pragma pack(1)

#include	<assert.h>

//████████████████████████████████████████████████████████████████████████████

#pragma	aux AIL_error	"_*"

//████████████████████████████████████████████████████████████████████████████

#define	MAX_SYNTHESIZER_VOICES	0
#define	MAX_SAMPLE_VOICES	12	// THIS CAN BE CHANGED
#define	MAX_STREAMING_VOICES	2	// DO NOT CHANGE

#define	NB_VOICES	MAX_SAMPLE_VOICES+MAX_SYNTHESIZER_VOICES+MAX_STREAMING_VOICES

struct soundHandle
{
	U32 soloudHandle = 0;
	SoLoud::Wav* soloudWave = nullptr;
};

soundHandle gSoundHandleMapping[NB_VOICES + 1]; // because slot 0 is never used

//████████████████████████████████████████████████████████████████████████████
typedef	struct		{
				U32	handle	;
				U32	sample	;
				U32	timer	;
			}
			SAMP_DESC	;

//████████████████████████████████████████████████████████████████████████████
	S32		SampleOverflow = FALSE		;

//████████████████████████████████████████████████████████████████████████████
	S32		LibWaveMasterVolume = 128	;
#ifdef	YAZ_WIN32
	S32		UseWaveMixer = FALSE		;
#endif//YAZ_WIN32

//████████████████████████████████████████████████████████████████████████████
#ifdef	YAZ_WIN32
static	VOLUMECONTROL	WaveVolume			;
static	S32		OldWaveVolume			;
#endif//YAZ_WIN32

//████████████████████████████████████████████████████████████████████████████
//static	HTIMER		HandleTimer	= NULL		;
static	void		*Timer_ptrsample		;
static	U32		Timer_usernum	     		;
static	S32		Timer_pitchbend			;
static	S32		Timer_nbrepeat			;
static	S32		Timer_volume	     		;
static	S32		Timer_pan	     		;

static	U32		InversePanMask = 0		;

	SAMP_DESC	Sample_Handle[MAX_SAMPLE_VOICES];
static	S32		Nb_Voices			;
static	U32		SampleSeed			;
static	S32		SamplesPaused = 0		;
static	S32		SamplesFade = 0			;
static	S32		FadeStartTime			;

static Ida::ElasticBuffer<char> sampleBuffer;

//████████████████████████████████████████████████████████████████████████████

enum	{ 	USER_DATA_VOLUME,
		USER_DATA_PITCHBEND,
		USER_DATA_RATE,
		USER_DATA_USER
	}	;

//████████████████████████████████████████████████████████████████████████████

#define	Scale(value, scale, range)	( ( (value)*(scale) + (1<<(range-1)) ) >> range )

//████████████████████████████████████████████████████████████████████████████

VOID_FUNC	SpecialInitSampleDriver	;

//████████████████████████████████████████████████████████████████████████████

#ifdef	_WIN32

SoLoud::Soloud* gSoloud = NULL;

S32	InitDriver(S32 rate, S32 bits, S32 chans)
{
	S32 result = 0;

	gSoloud = new SoLoud::Soloud();
	result = gSoloud->init(SoLoud::Soloud::CLIP_ROUNDOFF, SoLoud::Soloud::BACKENDS::AUTO, rate, 0, chans);

	return result;
}

#endif

//████████████████████████████████████████████████████████████████████████████

S32	InitSampleDriver( char *driver_name )
{
	if( Sample_Driver_Enabled )
	{
		return TRUE	;
	}

retry:
	if(InitDriver(SamplingRate, 16, 2))
	{
		if(InitDriver(SamplingRate, 8, 2))
		{
			if(InitDriver(SamplingRate/2, 8, 1))
			{

/*				if(!AIL_get_preference(DIG_USE_WAVEOUT))
				{
					//yaz AIL_set_preference(DIG_USE_WAVEOUT, TRUE) ;
					goto retry	;
				}*/
				LogPuts( "Could find a compatible Digital Audio Driver. Digital Audio Disabled." );
				return TRUE		;
			}
		}
	}

	for (Nb_Voices = 0; Nb_Voices < MAX_SAMPLE_VOICES; Nb_Voices++)
	{
		Sample_Handle[Nb_Voices].handle = Nb_Voices + 1; // so that it's never 0
		Sample_Handle[Nb_Voices].sample = 0;
	}

	SampleSeed		= 0x1000000	;

	Sample_Driver_Enabled	= TRUE 	;

	RegisterInitClear( SpecialInitSampleDriver, ClearSampleDriver );// Automatic clear

#ifdef	YAZ_WIN32
	UseWaveMixer = GetMixerVolDevice(&WaveVolume, MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT) ;
	if(UseWaveMixer)
	{
		LogPuts("Mixer Device found. Used for WAVE volume control.")	;
		OldWaveVolume = GetMixerVolume(&WaveVolume)	;
	}
	else
	{
		LogPuts("Mixer Device not found. Wave volume control handled in software.")	;
	}

        PauseSamplesPtr	= PauseSamples	;
/*yaz	if(PauseMiles)
	{
		PauseMiles	= PauseBothMiles;
	}
	else
	{
	        PauseMiles	= PauseSamples	;
	}*/
        ResumeSamplesPtr= ResumeSamples	;
/*yaz	if(ResumeMiles)
	{
		ResumeMiles	= ResumeBothMiles;
	}
	else
	{
	        ResumeSamplesPtr= ResumeSamples	;
	}*/
#endif

	return TRUE	;
}

//████████████████████████████████████████████████████████████████████████████

static void SpecialInitSampleDriver()
{
	InitSampleDriver(SampleDriverName);
}

//████████████████████████████████████████████████████████████████████████████

void	ClearSampleDriver()
{
	if( !Sample_Driver_Enabled )	return	;

#ifdef	YAZ_WIN32
	// Restore mixer volume
	if(UseWaveMixer)
	{
		 SetMixerVolume(&WaveVolume, OldWaveVolume)	;
	}
#endif//YAZ_WIN32

	Sample_Driver_Enabled = FALSE 		;
}
//████████████████████████████████████████████████████████████████████████████

static S32 GetHandleIndice(U32 sample)
{
	S32	hnum;

	if (sample<0x1000000)
	{
		for(hnum=0; hnum<Nb_Voices; hnum++)
		{
			if(((Sample_Handle[hnum].sample>>8)&0xFFFF)==sample)
			{
				return hnum ;
			}
		}
		return -1 ;
	}
	else
	{
		// get array indice
		hnum = sample & 0xFF	;

		// right sample playing?
		if (Sample_Handle[hnum].sample == sample) 
		{
			return hnum;
		}

		// No already stopped
		return	-1	;
	}
}

//████████████████████████████████████████████████████████████████████████████

static U32 FindFreeHandle(U32 *pnum)
{
	U32	handle = 0	;
	U32		hnum	;

	if( !Sample_Driver_Enabled )	return 0 ;

	// Search a sample with SMP_DONE status
	for(hnum=0; hnum<Nb_Voices; hnum++)
	{
		if(Sample_Handle[hnum].sample==0)
			break;

		if (!gSoloud->isValidVoiceHandle(gSoundHandleMapping[Sample_Handle[hnum].handle].soloudHandle))
		{
			break;
		}
	}

	// Found one?
	if(hnum==Nb_Voices)
	{
		// If none find the oldest

		U32	bestsample	;

		SampleOverflow	= TRUE	;

		bestsample = 0xFFFFFFFF	;

		// search a one shot
		for(hnum=0; hnum<Nb_Voices; hnum++)
		{
			//yaz if(    (bestsample>Sample_Handle[hnum].sample)
			//yaz     && (AIL_sample_loop_count(Sample_Handle[hnum].handle) == 1) )
			//yaz 	bestsample=Sample_Handle[hnum].sample	;
		}
		if(bestsample != 0xFFFFFFFF)
		{
			goto ok	;
		}

		// search a sample that is not in infinite loop
		for(hnum=0; hnum<Nb_Voices; hnum++)
		{
			//yaz if(    (bestsample>Sample_Handle[hnum].sample)
			//yaz     && AIL_sample_loop_count(Sample_Handle[hnum].handle) )
			//yaz 	bestsample=Sample_Handle[hnum].sample	;
		}
		if(bestsample != 0xFFFFFFFF)
		{
			goto ok	;
		}

		// search any sample
		for(hnum=0; hnum<Nb_Voices; hnum++)
		{
			if( bestsample>Sample_Handle[hnum].sample )
				bestsample=Sample_Handle[hnum].sample	;
		}
ok:
		// get array indice
		hnum = bestsample & 0xFF		;

		// No sample associated with handle
		Sample_Handle[hnum].sample = 0		;
	}

	// Get sample
	handle = Sample_Handle[hnum].handle	;

	// stop the sample (security)
	gSoloud->stop(gSoundHandleMapping[handle].soloudHandle);

	// Clear internal structures
	delete gSoundHandleMapping[handle].soloudWave;
	gSoundHandleMapping[handle].soloudWave = nullptr;

	*pnum = hnum	;

	Sample_Handle[hnum].timer = TimerSystemHR;

	return	handle 	;
}

//████████████████████████████████████████████████████████████████████████████

static U32 GenerateSampleHandle(U32 usernum, U32 hnum)
{
	U32		sample	;

	// Compute handle
	sample = SampleSeed | ((usernum&0xFFFF)<<8) | hnum ;

	Sample_Handle[hnum].sample = sample	;

	SampleSeed	+= 0x1000000		;
	if(SampleSeed==0)
	{
		SampleSeed = 0x1000000		;
	}

	return (sample)				;
}

//████████████████████████████████████████████████████████████████████████████

static void ScaleVolumeSamples( S32 fadevol )
{
	U32	hnum	;

	// update all the volumes
	for(hnum=0; hnum<Nb_Voices; hnum++)
	{
		//HSAMPLE	handle	;
		S32	volume	;

//yaz		handle = Sample_Handle[hnum].handle	;
		//yaz volume = AIL_sample_user_data( handle, USER_DATA_VOLUME);
		//yaz AIL_set_sample_volume( handle, Scale(volume, fadevol, 7));
	}
}


//████████████████████████████████████████████████████████████████████████████

void	SetMasterVolumeSample( S32 volume )
{
#ifdef	YAZ_WIN32
	if(UseWaveMixer)
	{
		SetMixerVolume(&WaveVolume, volume)	;
	}
	else
#endif
	{
		if(volume<=0)
		{
			volume = 0	;
		}
		else
		{
			volume++	;

			if(volume>128)
			{
				volume = 128	;
			}
		}

		if(volume == LibWaveMasterVolume)
		{
			return	;
		}

		LibWaveMasterVolume = volume	;

		if(!SamplesFade)	ScaleVolumeSamples(volume)	;

		ChangeVolumeStream(GetVolumeStream())	;
	}
}

//████████████████████████████████████████████████████████████████████████████

S32	GetMasterVolumeSample()
{
#ifdef	YAZ_WIN32
	if(UseWaveMixer)
	{
		return GetMixerVolume(&WaveVolume) ;
	}
	else
#endif
	{
		if(LibWaveMasterVolume)
		{
			return(LibWaveMasterVolume - 1);
		}
		else
		{
			return(0)		;
		}
	}
}

//████████████████████████████████████████████████████████████████████████████

S32	FadeOutSamples( S32 delay )
{
	S32	delta	;

	if(!SamplesFade)
	{
		ManageTime()	;

		FadeStartTime = TimerSystemHR ;

		SamplesFade = TRUE ;
	}

	ManageTime()	;

	delta = FadeStartTime + delay - TimerSystemHR ;

	if(delta<0)	delta = 0	;

	if(Sample_Driver_Enabled)
	{
		S32	ratio	;

		if(delay)
		{
			ratio = delta*128/delay	;
		}
		else
		{
			ratio = 128 ;
		}

		ScaleVolumeSamples(ratio);
	}

	return delta	;
}

//████████████████████████████████████████████████████████████████████████████

S32	FadeInSamples( S32 delay )
{
	S32	delta	;

	if(SamplesFade)
	{
		ManageTime()	;

		FadeStartTime = TimerSystemHR ;

		SamplesFade = FALSE ;
	}

	ManageTime()	;

	delta = TimerSystemHR - FadeStartTime;

	if(delta>delay)	delta = delay	;

	if(Sample_Driver_Enabled)
	{
		S32	ratio	;

		if(delay)
		{
			ratio = delta*128/delay	;
		}
		else
		{
			ratio = 128 ;
		}

		ScaleVolumeSamples(ratio);
	}

	return delta	;
}

//████████████████████████████████████████████████████████████████████████████

void	InverseStereoSample( S32 inverse )
{
	InversePanMask = inverse ? 127 : 0 ;
}

//████████████████████████████████████████████████████████████████████████████

void dsp_adpcm_ima_decode(S16 *out, void *in, int size, S16 *step_index, S16 *predicted_value);

extern "C" {
#include "../adpcm.h"
};

U32	PlaySample(void *ptrsample, U32 usernum, S32 pitchbend, S32 nbrepeat, S32 volume, S32 pan)
{
	U32		handle	;
	U32		hnum	;

	// get one available handle
	handle = FindFreeHandle(&hnum)	;

	// if none, exit
	if(!handle)	return	NULL	;

	int numChannels = *(short*)((char*)ptrsample + 22);
	int sampleRate = *(U32*)((char*)ptrsample + 24);
	int bitsPerSample = *(short*)((char*)ptrsample + 34);

	char* dataPtr =  ((char*)ptrsample + 36);
	while(*(U32*)dataPtr != 'atad')
	{
		dataPtr++;
	}

	int samplesSize = *(U32*)(dataPtr + 4);
	char * samplesPtr = dataPtr + 8;

	SoLoud::Wav* pAudioSource = new SoLoud::Wav();
	if (numChannels == 1 && bitsPerSample == 16)
	{
		pAudioSource->loadRawWave16((short*)samplesPtr, samplesSize, sampleRate, 1);
	}
	if (numChannels == 1 && bitsPerSample == 8)
	{
		pAudioSource->loadRawWave8((unsigned char*)samplesPtr, samplesSize, sampleRate, 1);
	}
	if (numChannels == 1 && bitsPerSample == 4)
	{
		sampleBuffer.ensureCapacity(samplesSize * 4);
		samplesPtr = sampleBuffer.getBuffer();

		S16 stepIndex = 0;
		S16 predicted = 0;

		int adpcm_decode_init(int numChannels);
		int adpcm_decode_frame(void *data, int *data_size, unsigned char *buf, int buf_size);

		short* decompressedData = (short*)samplesPtr;
		unsigned char* inputData = (unsigned char*)dataPtr+8;

		int sizeLeftToDecode = samplesSize;

		adpcm_decode_init(1);
		while(sizeLeftToDecode)
		{
			int frameSize = 0x200;

			if(frameSize > sizeLeftToDecode)
				frameSize = sizeLeftToDecode;

			int decodedSize;
			adpcm_decode_frame(decompressedData, &decodedSize, inputData, frameSize);

			inputData+=frameSize;
			sizeLeftToDecode-=frameSize;
			decompressedData+=decodedSize/2;
		}

		int actualSamplesSize = ((char*)decompressedData - samplesPtr) / 2;

		pAudioSource->loadRawWave16((short*)samplesPtr, actualSamplesSize, sampleRate, 1);
	}

	gSoundHandleMapping[handle].soloudHandle = gSoloud->play(*pAudioSource, -1, 0, true, 0);
	gSoloud->setAutoStop(gSoundHandleMapping[handle].soloudHandle, true);

	// link sample data with sample handle
	gSoundHandleMapping[handle].soloudWave = pAudioSource;
	
	// Pichpend
	if(pitchbend != 4096)
	{
		gSoloud->setSamplerate(gSoundHandleMapping[handle].soloudHandle, Scale((int)gSoloud->getSamplerate(gSoundHandleMapping[handle].soloudHandle), pitchbend, 12));
	}

	gSoloud->setLooping(gSoundHandleMapping[handle].soloudHandle, nbrepeat != 1);

	// GO !!!
	gSoloud->setVolume(gSoundHandleMapping[handle].soloudHandle, volume / 127.0f);
	gSoloud->setPause(gSoundHandleMapping[handle].soloudHandle, false);

	// Compute handle
	return GenerateSampleHandle(usernum, hnum);
}

//████████████████████████████████████████████████████████████████████████████

#ifdef YAZ_WIN32
static void WINAPI
#else
static void //yazpascal
#endif
SampleCallBack(U32 user)
{
	user = user	;

	if(!SamplesPaused)
	{
		PlaySample(Timer_ptrsample, Timer_usernum, Timer_pitchbend, Timer_nbrepeat, Timer_volume, Timer_pan^InversePanMask);
	}
}

//████████████████████████████████████████████████████████████████████████████

U32	TimerPlaySample(void *ptrsample, U32 usernum, S32 pitchbend, S32 nbrepeat, S32 volume, S32 pan, S32 period)
{
	if (!Sample_Driver_Enabled)	return FALSE;

	Timer_ptrsample	= ptrsample	;
	Timer_usernum	= usernum	;
	Timer_pitchbend	= pitchbend	;
	Timer_nbrepeat	= nbrepeat	;
	Timer_volume	= volume	;
	Timer_pan	= pan		;

	return TRUE			;
}

//████████████████████████████████████████████████████████████████████████████

void	TimerStopSample()
{
	if(!Sample_Driver_Enabled )	return;

	//if(HandleTimer)
	{
		//yaz AIL_stop_timer( HandleTimer ) 	;
	}
}

//████████████████████████████████████████████████████████████████████████████
void ChangePitchbendSample( U32 sample, S32 pitchbend)
{
	if (!Sample_Driver_Enabled)	return;

	// check if user or lib handle and do conversion
	S32 hnum = GetHandleIndice(sample);
	if (hnum == -1)	return;

	// Get sample handle
	auto handle = Sample_Handle[hnum].handle;
	
	// Ida - refactored code starts here, using SoLoud instead of AIL
	// Pitchbend uses fixed-point math with this bit range (12 bits = 4096 scale factor for 1.0x)
	constexpr S32 pitchbendBitRange = 12;
	constexpr S32 pitchbendNeutral = 1 << pitchbendBitRange;  // 4096 = no pitch change

	auto soloudHandle = gSoundHandleMapping[handle].soloudHandle;

	if (!gSoloud->isValidVoiceHandle(soloudHandle))
	{
		return;
	}

	SoLoud::Wav *wav = gSoundHandleMapping[handle].soloudWave;
	if (wav == nullptr) return;
	float baseSampleRate = wav->mBaseSamplerate;

	if (pitchbend != pitchbendNeutral)
	{
		gSoloud->setSamplerate(soloudHandle, Scale((int)baseSampleRate, pitchbend, pitchbendBitRange));
	}
	else
	{
		gSoloud->setSamplerate(soloudHandle, baseSampleRate);
	}

	// Old AIL code:
	// // check sample status
	// if (AIL_sample_status(handle) == SMP_DONE)
	// {
	// 	return;
	// }
	//
	// // pitchend
	// if (pitchbend != AIL_sample_user_data( handle, USER_DATA_PITCHBEND))
	// {
	// 	AIL_set_sample_playback_rate(handle, Scale(AIL_sample_user_data( handle, USER_DATA_RATE), pitchbend, 12));
	// 	AIL_set_sample_user_data( handle, USER_DATA_PITCHBEND, pitchbend);
	// }
}

//████████████████████████████████████████████████████████████████████████████

void	ChangeVolumePanSample( U32 sample, S32 volume, S32 pan)
{
	//HSAMPLE	handle	;
	S32	hnum	;

	if( !Sample_Driver_Enabled )	return	;

	// check if user or lib handle and do conversion
	hnum = GetHandleIndice(sample)		;

	// found the sample ?
	if(hnum==-1)			return 	;

	// Get sample handle
//yaz	handle = Sample_Handle[hnum].handle	;

	// check sample status
	//yaz if(AIL_sample_status( handle )==SMP_DONE)
	//yaz 	return	;

	// Volume
	//yaz AIL_set_sample_user_data( handle, USER_DATA_VOLUME, volume);
	//yaz AIL_set_sample_volume( handle,
	//yaz 			SamplesFade?0:
#ifdef	YAZ_WIN32
	//yaz 			UseWaveMixer?volume:
#endif//YAZ_WIN32
	//yaz 			Scale(volume, LibWaveMasterVolume, 7));

	// Pan
	if(pan>127)		pan = 127	;
	else if(pan<0)		pan = 0		;
	//yaz AIL_set_sample_pan( handle, pan^InversePanMask);
}

//████████████████████████████████████████████████████████████████████████████
/*
FMOD::Sound*	GetSampleMSSHandle( U32 sample )
{
	S32	hnum	;

	if( !Sample_Driver_Enabled )	return NULL	;

	// check if user or lib handle and do conversion
	hnum = GetHandleIndice(sample)	;

	// found the sample ?
	if(hnum==-1)			return NULL	;

	// stop the sample
	return Sample_Handle[hnum].handle		;
}
*/
//████████████████████████████████████████████████████████████████████████████

void	StopOneSample( U32 sample )
{
	U32	handle	;
	S32	hnum	;

	if( !Sample_Driver_Enabled )	return	;

	// check if user or lib handle and do conversion
	hnum = GetHandleIndice(sample)	;

	// found the sample ?
	if(hnum==-1)			return ;

	// stop the sample
	gSoloud->stop(gSoundHandleMapping[Sample_Handle[hnum].handle].soloudHandle);
	Sample_Handle[hnum].sample = 0			;
}

//████████████████████████████████████████████████████████████████████████████

void	StopSamples()
{
	U32		hnum	;

	if( !Sample_Driver_Enabled )	return	;

	// Stop sample on timer
	TimerStopSample()	;

	// Stop everything!
	for(hnum=0; hnum<Nb_Voices; hnum++)
	{
		gSoloud->stop(gSoundHandleMapping[Sample_Handle[hnum].handle].soloudHandle);
		Sample_Handle[hnum].sample = 0		;
	}

	// Reset seed to 0x100 to limit risks of overflow
	SampleSeed = 0x1000000	;
	SamplesPaused = 0	;
}

//████████████████████████████████████████████████████████████████████████████

void	PauseOneSample( U32 sample )
{
	S32	hnum	;

	if( !Sample_Driver_Enabled )	return	;

	// check if user or lib handle and do conversion
	hnum = GetHandleIndice(sample)		;

	// found the sample ?
	if(hnum==-1)			return	;

	// stop the sample
	gSoloud->setPause(gSoundHandleMapping[Sample_Handle[hnum].handle].soloudHandle, true);
}

//████████████████████████████████████████████████████████████████████████████

void	ResumeOneSample( U32 sample )
{
	S32	hnum	;

	if( !Sample_Driver_Enabled )	return	;

	// check if user or lib handle and do conversion
	hnum = GetHandleIndice(sample)		;

	// found the sample ?
	if(hnum==-1)			return	;

	// stop the sample
	gSoloud->setPause(gSoundHandleMapping[Sample_Handle[hnum].handle].soloudHandle, false);
}

//████████████████████████████████████████████████████████████████████████████

void	PauseSamples()
{
	U32		hnum	;

	if( !Sample_Driver_Enabled )	return	;

	SamplesPaused++;

	if(SamplesPaused>1)	//  already in pause ?
	{
		return	;	// Yes -> exit
	}

	// Pause everything
	for(hnum=0; hnum<Nb_Voices; hnum++)
	{
		if(Sample_Handle[hnum].sample)
		{
			gSoloud->setPause(gSoundHandleMapping[Sample_Handle[hnum].handle].soloudHandle, true);
		}
	}
}

//████████████████████████████████████████████████████████████████████████████

void	ResumeSamples()
{
	U32		hnum	;

	if( !Sample_Driver_Enabled )	return	;

	if(!SamplesPaused)	// in pause ?
	{
		return	;	// No -> exit
	}

	SamplesPaused--	;	// decrement pause

	if(SamplesPaused)	// end of pause ?
	{
		return	;	// No -> exit
	}

	// Resume everything
	for(hnum=0; hnum<Nb_Voices; hnum++)
	{
		if(Sample_Handle[hnum].sample)
		{
			gSoloud->setPause(gSoundHandleMapping[Sample_Handle[hnum].handle].soloudHandle, false);
		}
	}
}

//████████████████████████████████████████████████████████████████████████████
U32	IsSamplePlaying( U32 sample )
{
	S32	hnum;

	if (!Sample_Driver_Enabled)
		return FALSE;

	// check if user or lib handle and do conversion
	hnum = GetHandleIndice(sample)			;

	// found the sample ?
	if (hnum==-1) return FALSE;

	// get sample status
	if (!gSoloud->isValidVoiceHandle(gSoundHandleMapping[Sample_Handle[hnum].handle].soloudHandle))
	{
		return 0;
	}
	else
	{
		return Sample_Handle[hnum].timer;
	}
}

//████████████████████████████████████████████████████████████████████████████

S32	GetSamplePosition( U32 sample )
{
	S32	hnum	;

	if( !Sample_Driver_Enabled )
		return	-1	;

	// check if user or lib handle and do conversion
	hnum = GetHandleIndice(sample)			;

	// found the sample ?
	if(hnum==-1)			return 	-1	;

	// get sample pos
	//yaz return(AIL_sample_position(Sample_Handle[hnum].handle))	;
	return 0;

}

//████████████████████████████████████████████████████████████████████████████

S32	GetPlayingSamples(SAMPLE_PLAYING tab[], S32 max)
{
	S32	i, hnum	;

	hnum = 0	;

	for(i=0; i<Nb_Voices; i++)
	{
		if(hnum==max)
		{
			break	;	// list full
		}

		if(!Sample_Handle[i].sample)
		{
			continue;	// not playing
		}
		//yaz tab[hnum].Usernum 	= (Sample_Handle[i].sample>>8)&0xFFFF		;
		//yaz tab[hnum].Pitch  	= AIL_sample_user_data(Sample_Handle[i].handle, USER_DATA_PITCHBEND);
		//yaz tab[hnum].Repeat	= (U8)AIL_sample_loop_count(Sample_Handle[i].handle)	;
		//yaz tab[hnum].Volume	= (U8)AIL_sample_user_data(Sample_Handle[i].handle, USER_DATA_VOLUME);
		//yaz tab[hnum].Pan		= (U8)(AIL_sample_pan(Sample_Handle[i].handle)^InversePanMask);

		//yaz if(AIL_sample_status(Sample_Handle[i].handle) == SMP_DONE)
		{
			Sample_Handle[i].sample = 0	;
			continue;	// not playing
		}
		hnum++	;
	}

	return	hnum	;
}

//████████████████████████████████████████████████████████████████████████████

S32	GetSamplePoly()
{
	S32	hnum, count	;

	if( !Sample_Driver_Enabled )
		return	-1	;

	count = 0	;

	for(hnum=0; hnum<Nb_Voices; hnum++)
	{
		if(Sample_Handle[hnum].sample==0)
			continue	;
		//yaz if(AIL_sample_status(Sample_Handle[hnum].handle)==SMP_DONE)
		//yaz 	continue	;

		count++	;
	}

	return count	;
}

//████████████████████████████████████████████████████████████████████████████

void	SetDataSample( U32 sample, S32 index, S32 value )
{
	S32	hnum	;

	if( !Sample_Driver_Enabled || (index>8-USER_DATA_USER))
		return		;

	// check if user or lib handle and do conversion
	hnum = GetHandleIndice(sample)			;

	// found the sample ?
	if(hnum==-1)			return 		;

	// set data
	//yaz AIL_set_sample_user_data(Sample_Handle[hnum].handle, USER_DATA_USER+index, value)	;
}

//████████████████████████████████████████████████████████████████████████████

S32	GetDataSample( U32 sample, S32 index )
{
	S32	hnum	;

	if( !Sample_Driver_Enabled || (index>8-USER_DATA_USER))
		return	0	;

	// check if user or lib handle and do conversion
	hnum = GetHandleIndice(sample)			;

	// found the sample ?
	if(hnum==-1)			return 	0	;

	// get data
	//yaz return(AIL_sample_user_data(Sample_Handle[hnum].handle, USER_DATA_USER+index))	;
	return 0;
}

//████████████████████████████████████████████████████████████████████████████
