//--------------------------------------------------------------------------
#ifndef	LIB_AIL_SAMPLE
#define	LIB_AIL_SAMPLE

//--------------------------------------------------------------------------
#include <SYSTEM/ADELINE_TYPES.H>

//--------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------
typedef	struct {
		S32	Usernum	;
		S32	Pitch  	;
		U8	Repeat	;
		U8	Volume	;
		U8	Pan	;
		U8	Dummy	;
	}	SAMPLE_PLAYING	;

//--------------------------------------------------------------------------
extern	VOID_FUNC	PauseSamples		;
extern	VOID_FUNC	ResumeSamples		;

//--------------------------------------------------------------------------
extern	S32		Sample_Driver_Enabled	;
extern	S32		UseWaveMixer		;

//--------------------------------------------------------------------------
S32	InitSampleDriver(	char *driver_name	)	;

//--------------------------------------------------------------------------
//	SetMasterVolumeSample :		Set master volume for all samples
//
//			  volume : 	master volume (0-127)
//
//	Returns	: 			Nothing
//--------------------------------------------------------------------------
void	SetMasterVolumeSample(	S32 volume 		)	;

//--------------------------------------------------------------------------
//	FadeOutSamples :		Fade out all samples (except stream)
//
//			  delay : 	delay for fade (in ms)
//
//	Returns	: 			scaling factor (from delay to 0)
//--------------------------------------------------------------------------
S32	FadeOutSamples(		S32 delay 		)	;

//--------------------------------------------------------------------------
//	FadeInSamples :			Fade in all samples (except stream)
//
//			  delay : 	delay for fade (in ms)
//
//	Returns	: 			scaling factor (from 0 to delay)
//--------------------------------------------------------------------------
S32	FadeInSamples(		S32 delay 		)	;

//--------------------------------------------------------------------------
//	InverseStereoSample :		Reverse left and right channels
//
//			  inverse : 	TRUE for inverse, FALSE to reset
//
//	Returns	: 			Nothing
//--------------------------------------------------------------------------
void	InverseStereoSample(	S32 inverse 		)	;

//--------------------------------------------------------------------------
//	PlaySample : 			Play a Sample
//
//			  buffer    :	Pointer to the sample in mem
//			  sizeBytes :	Sample size pointed by buffer, in bytes
//			  userhandle: 	user handle (0 - 0xFFFF)
//			  pitchbend : 	4096 is no bend
//			  repeat    : 	number of times to play the sample
//			  volume    : 	Volume (127 max, clipped if > 127)
//			  pan	    : 	Paning (0-127, 64 dead center)
//
//	Returns	: 			lib sample handle
//--------------------------------------------------------------------------
U32	PlaySample( 		void *buffer,
				S32 sizeBytes,
				U32 userhandle,
				S32 pitchbend,
				S32 repeat,
				S32 volume,
				S32 pan		)	;

//--------------------------------------------------------------------------
//	ChangePitchbendSample :		Change a sample pitchbend
//
//			  sample :	sample handle
//			  pitchbend : 	4096 is no bend
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void	ChangePitchbendSample(	U32 sample,
				S32 pitchbend	)	;

//--------------------------------------------------------------------------
//	ChangeVolumePanSample :		Change a sample	volume and paning
//
//			  sample :	sample handle
//			  volume : 	Volume (127 max)
//			  pan    : 	Paning (0-127, 64 dead center)
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void	ChangeVolumePanSample(	U32 sample,
				S32 volume,
				S32 pan		)	;

//--------------------------------------------------------------------------
//	StopOneSample :			stop a sample
//
//			  sample :	sample handle
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void	StopOneSample( 		U32 sample 		)	;

//--------------------------------------------------------------------------
//	StopSample :			stop all samples
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void	StopSamples(		void			)	;

//--------------------------------------------------------------------------
//	PauseSample :			pause all samples
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void	PauseSamples(		void			)	;

//--------------------------------------------------------------------------
//	ResumeSample :			resume all samples
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void	ResumeSamples(		void			)	;

//--------------------------------------------------------------------------
//	IsSamplePlaying :		check if a sample is playing
//
//			  sample :	sample handle
//
//	Returns	:			FALSE or starting TimerRefHR
//--------------------------------------------------------------------------
U32	IsSamplePlaying( 	U32 sample 		)	;

//--------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------
#endif//LIB_AIL_SAMPLE

//--------------------------------------------------------------------------
