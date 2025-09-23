//--------------------------------------------------------------------------
#ifndef	LIB_AIL_SAMPLE_MILES
#define	LIB_AIL_SAMPLE_MILES

// This header contains declarations private to MILES

//--------------------------------------------------------------------------
#include <AIL/SAMPLE.H>

//--------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------
extern	S32		SampleOverflow		;

//--------------------------------------------------------------------------
extern	VOID_FUNC	ClearSampleDriver	;

//--------------------------------------------------------------------------
//	GetMasterVolumeSample :		Get master volume for all samples
//
//	Returns	: 			Master Volume
//--------------------------------------------------------------------------
S32	GetMasterVolumeSample(	void			)	;

//--------------------------------------------------------------------------
//	TimerPlaySample :		Play a Sample and repeat with period
//
//			  buffer    :	Pointer to the sample in mem
//			  userhandle: 	user handle (0 - 0xFFFF)
//			  pitchbend : 	4096 is no bend
//			  repeat    : 	number of times to play the sample
//			  volume    : 	Volume (127 max)
//			  pan	    : 	Paning (0-127, 64 dead center)
//			  period    :   in ms
//
//	Returns	: 			TRUE / FALSE
//--------------------------------------------------------------------------
U32	TimerPlaySample(	void *ptrsample,
				U32 usernum,
				S32 pitchbend,
				S32 nbrepeat,
				S32 volume,
				S32 pan,
				S32 period	);

//--------------------------------------------------------------------------
//	TimerStopSample :		stop TimerPlaySample
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void	TimerStopSample(	void		)	;

//--------------------------------------------------------------------------
//	GetSampleMSSHandle :		return MSS handle for a sample
//
//			  sample :	sample handle
//
//	Returns	:			HSAMPLE (see MSS doc)
//--------------------------------------------------------------------------
void*	GetSampleMSSHandle( 	U32 sample 	)	;

//--------------------------------------------------------------------------
//	PauseOneSample :		pause a sample
//
//			  sample :	sample handle
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void	PauseOneSample(		U32 sample 		)	;

//--------------------------------------------------------------------------
//	ResumeOneSample :		resume a sample
//
//			  sample :	sample handle
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void	ResumeOneSample(	U32 sample 		)	;

//--------------------------------------------------------------------------
//	GetPlayingSamples :		get list of all playing samples
//
//			  tab :		array of sample descriptor
//			  max :		max number of samples in list
//
//	Returns	:			number of samples playing
//--------------------------------------------------------------------------
S32	GetPlayingSamples(	SAMPLE_PLAYING tab[],
				S32 max			)	;

//--------------------------------------------------------------------------
//	GetSamplePostion :		get sample number currently playing
//
//			  sample :	sample handle
//
//	Returns	:			sample position
//--------------------------------------------------------------------------
S32	GetSamplePosition( U32 sample )				;

//--------------------------------------------------------------------------
//	GetSamplePostion :		get sample polyphonie
//
//	Returns	:			number of samples playing
//--------------------------------------------------------------------------
S32	GetSamplePoly( void )					;

//--------------------------------------------------------------------------
//	GetSamplePostion :		get a piece of data
//
//			  sample :	sample handle
//			  index  :	data index (0-6)
//			  value  :	data value
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void	SetDataSample( U32 sample, S32 index, S32 value )	;

//--------------------------------------------------------------------------
//	GetSamplePostion :		get a piece of data
//
//			  sample :	sample handle
//			  index :	data index (0-6)
//
//	Returns	:			value
//--------------------------------------------------------------------------
S32	GetDataSample( U32 sample, S32 index )			;

//--------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------
#endif//LIB_AIL_SAMPLE_MILES

//--------------------------------------------------------------------------
