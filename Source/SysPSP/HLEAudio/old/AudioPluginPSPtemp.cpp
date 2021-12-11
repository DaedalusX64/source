/*
Copyright (C) 2003 Azimer
Copyright (C) 2001,2006 StrmnNrmn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

//
//	N.B. This source code is derived from Azimer's Audio plugin (v0.55?)
//	and modified by StrmnNrmn to work with Daedalus PSP. Thanks Azimer!
//	Drop me a line if you get chance :)
//

#include "stdafx.h"
#include <stdio.h>
#include <new>

#include <pspkernel.h>
#include <pspaudiolib.h>
#include <pspaudio.h>

#include "Config/ConfigOptions.h"
#include "Core/CPU.h"
#include "Core/Interrupt.h"
#include "Core/Memory.h"
#include "Core/ROM.h"
#include "Core/RSP_HLE.h"
#include "Debug/DBGConsole.h"
#include "HLEAudio/HLEAudioInternal.h"
#include "HLEAudio/AudioBuffer.h"
#include "HLEAudio/AudioPlugin.h"
#include "SysPSP/Utility/JobManager.h"
#include "SysPSP/Utility/CacheUtil.h"
#include "SysPSP/Utility/JobManager.h"
#include "Core/FramerateLimiter.h"
#include "System/Thread.h"

EAudioPluginMode gAudioPluginEnabled( APM_DISABLED );

#define DEBUG_AUDIO  0

#if DEBUG_AUDIO
#define DPF_AUDIO(...)	do { printf(__VA_ARGS__); } while(0)
#else
#define DPF_AUDIO(...)	do { (void)sizeof(__VA_ARGS__); } while(0)
#endif


static const u32	kOutputFrequency = 44100;

// OSX Uses a Circular Buffer of 1024 * 1024
// A Large kAudioBufferSize will create a large delay on sound - Corn
static const u32	kAudioBufferSize = 1024 * 2; 

static const u32 	kNumChannels = 2;

// How much input we try to keep buffered in the synchronisation code.
// Setting this too low and we run the risk of skipping.
// Setting this too high and we run the risk of being very laggy.
static const u32 kMaxBufferLengthMs = 30;

// AudioQueue buffer object count and length.
// Increasing either of these numbers increases the amount of buffered
// audio which can help reduce crackling (empty buffers) at the cost of lag.
static const u32 kNumBuffers = 3;
static const u32 kAudioQueueBufferLength = 1 * 1024;
extern u32 gSoundSync;

static const u32	DESIRED_OUTPUT_FREQUENCY {44100};
static const u32	MAX_OUTPUT_FREQUENCY {DESIRED_OUTPUT_FREQUENCY * 4};

//static const u32	ADAPTIVE_FREQUENCY_ADJUST = 2000;
// Large BUFFER_SIZE creates huge delay on sound //Corn
static const u32	BUFFER_SIZE {1024 * 2};

static const u32	PSP_NUM_SAMPLES {512};

// Global variables
static SceUID bufferEmpty {};

static s32 sound_channel {PSP_AUDIO_NEXT_CHANNEL};
static volatile s32 sound_volume {PSP_VOLUME_MAX};
static volatile u32 sound_status {0};

static volatile int pcmflip {0};
static s16 __attribute__((aligned(16))) pcmout1[PSP_NUM_AUDIO_SAMPLES]; // # of stereo samples
static s16 __attribute__((aligned(16))) pcmout2[PSP_NUM_AUDIO_SAMPLES];


static bool audio_open = false;


#define RSP_AUDIO_INTR_CYCLES 1 



class AudioPluginPSP : public CAudioPlugin
{
public:

 	AudioPluginPSP();
	virtual ~AudioPluginPSP();
	virtual bool			StartEmulation();
	virtual void			StopEmulation();

	virtual void			AiDacrateChanged( int system_type );
	virtual void			AiLenChanged();
	virtual u32				AiReadLength() {return 0;}
	virtual EProcessResult	ProcessAList();

	//virtual void SetFrequency(u32 frequency);
	virtual void AddBuffer( u8 * start, u32 length);
	// virtual void FillBuffer( Sample * buffer, u32 num_samples);

	virtual void StopAudio();
	virtual void StartAudio();

	// static void AudioSyncFunction(void * arg);
	// static void AudioCallback(void * arg);
	// static void AudioThreadd(void * arg);

public:
  CAudioBuffer *		mAudioBufferUncached;

private:
	CAudioBuffer * mAudioBuffer;
	bool mKeepRunning;
	bool mExitAudioThread;
	u32 mFrequency;
	s32 mAudioThread;
	s32 mSemaphore;
//	u32 mBufferLenMs;
};

class SHLEStartJob : public SJob
{
public:
	SHLEStartJob()
	{
		InitJob = nullptr;
		DoJob = &DoHLEStartStatic;
		FiniJob = &DoHLEFinishedStatic;
	}

	static int DoHLEStartStatic( SJob * arg )
	{
		SHLEStartJob *  job( static_cast< SHLEStartJob * >( arg ) );
		job->DoHLEStart();
		return 0;
	}

	static int DoHLEFinishedStatic( SJob * arg )
	{
		SHLEStartJob *  job( static_cast< SHLEStartJob * >( arg ) );
		job->DoHLEFinish();
		return 0;
	}

	int DoHLEStart()
	{
		HLEStart();
		return 0;
	}

	int DoHLEFinish()
	{
		CPU_AddEvent(RSP_AUDIO_INTR_CYCLES, CPU_EVENT_AUDIO);
		return 0;
	}
};

class SAddSamplesJob : public SJob
{
	CAudioBuffer *		mBuffer;
	const Sample *		mSamples;
	u32					mNumSamples;
	u32					mFrequency;
	u32					mOutputFreq;

public:
	SAddSamplesJob( CAudioBuffer * buffer, const Sample * samples, u32 num_samples, u32 frequency, u32 output_freq )
		:	mBuffer( buffer )
		,	mSamples( samples )
		,	mNumSamples( num_samples )
		,	mFrequency( frequency )
		,	mOutputFreq( output_freq )
	{
		InitJob = nullptr;
		DoJob = &DoAddSamplesStatic;
		FiniJob = &DoJobComplete;
	}

	~SAddSamplesJob() {}

	static int DoAddSamplesStatic( SJob * arg )
	{
		SAddSamplesJob *    job( static_cast< SAddSamplesJob * >( arg ) );
		job->DoAddSamples();
		return 0;
	}

	int DoAddSamples()
	{
		mBuffer->AddSamples( mSamples, mNumSamples, mFrequency, mOutputFreq );
		return 0;
	}

	static int DoJobComplete( SJob * arg )
	{
		return 0;
	}
};


static int audioOutput(SceSize args, void *argp)
{
	s16 *playbuf {0};

	while(sound_status != 0xDEADBEEF)
	{
		playbuf = pcmflip ? pcmout1 : pcmout2;
		pcmflip ^= 1;
		sceKernelSignalSema(bufferEmpty, 1);
		sceAudioOutputBlocking(sound_channel, sound_volume, playbuf);
	}
	sceKernelExitDeleteThread(0);
	return 0;
}

static int fillBuffer(SceSize args, void *argp)
{
	s16 *fillbuf {0};
		static AudioPluginPSP * ac;

	while(sound_status != 0xDEADBEEF)
	{
		sceKernelWaitSema(bufferEmpty, 1, 0);
		fillbuf = pcmflip ? pcmout2 : pcmout1;

		ac->mAudioBufferUncached->Drain( reinterpret_cast< Sample * >( fillbuf ), PSP_NUM_SAMPLES );
	}
	sceKernelExitDeleteThread(0);
	return 0;
}


AudioPluginPSP::AudioPluginPSP()
:mKeepRunning (false)
//: mAudioBuffer( kAudioBufferSize )
,	mFrequency( kOutputFrequency )
,	mSemaphore( sceKernelCreateSema( "AudioPluginPSP", 0, 1, 1, nullptr ) )
//, mAudioThread ( kInvalidThreadHandle )
//, mKeepRunning( false )
//, mBufferLenMs ( 0 )
{
	// Allocate audio buffer with malloc_64 to avoid cached/uncached aliasing
	void * mem = malloc_64( sizeof( CAudioBuffer ) );
	mAudioBuffer = new( mem ) CAudioBuffer( kAudioBufferSize );
	mAudioBufferUncached = (CAudioBuffer*)MAKE_UNCACHED_PTR(mem);
	// Ideally we could just invalidate this range?
	dcache_wbinv_range_unaligned( mAudioBuffer, mAudioBuffer+sizeof( CAudioBuffer ) );
}


AudioPluginPSP::~AudioPluginPSP( )
{
	mAudioBuffer->~CAudioBuffer();
  free(mAudioBuffer);
  sceKernelDeleteSema(mSemaphore);
//   pspAudioEnd();
}



static void AudioInit()
{
		// Init semaphore
	bufferEmpty = sceKernelCreateSema("Buffer Empty", 0, 1, 1, 0);

	// reserve audio channel
	sound_channel = sceAudioChReserve(sound_channel, PSP_NUM_SAMPLES, PSP_AUDIO_FORMAT_STEREO);

	sound_status = 0; // threads running

	// create audio playback thread to provide timing
	int audioThid = sceKernelCreateThread("audioOutput", audioOutput, 0x15, 0x1800, PSP_THREAD_ATTR_USER, NULL);
	if(audioThid < 0)
	{
		printf("FATAL: Cannot create audioOutput thread\n");
		return; // no audio
	}
	sceKernelStartThread(audioThid, 0, NULL);

	// Start streaming thread
	int bufferThid = sceKernelCreateThread("bufferFilling", fillBuffer, 0x14, 0x1800, PSP_THREAD_ATTR_USER, NULL);
	if(bufferThid < 0)
	{
		sound_status = 0xDEADBEEF; // kill the audioOutput thread
		sceKernelDelayThread(100*1000);
		sceAudioChRelease(sound_channel);
		sound_channel = PSP_AUDIO_NEXT_CHANNEL;
		printf("FATAL: Cannot create bufferFilling thread\n");
		return;
	}
	sceKernelStartThread(bufferThid, 0, NULL);

	// Everything OK
	audio_open = true;
}


bool	AudioPluginPSP::StartEmulation()
{
	return true;
}

void	AudioPluginPSP::StopEmulation()
{
    Audio_Reset();
  	StopAudio();
    sceKernelDeleteSema(mSemaphore);

}


void	AudioPluginPSP::AiDacrateChanged( int system_type )
{
	u32 clock = (system_type == ST_NTSC) ? VI_NTSC_CLOCK : VI_PAL_CLOCK;
	u32 dacrate = Memory_AI_GetRegister(AI_DACRATE_REG);
	u32 frequency = clock / (dacrate + 1);

	DBGConsole_Msg(0, "Audio frequency: %d", frequency);
	mFrequency = frequency;
}



void	AudioPluginPSP::AiLenChanged()
{
	if( gAudioPluginEnabled > APM_DISABLED )
	{
		u32 address = Memory_AI_GetRegister(AI_DRAM_ADDR_REG) & 0xFFFFFF;
		u32	length = Memory_AI_GetRegister(AI_LEN_REG);

		AddBuffer( g_pu8RamBase + address, length );
	}
	else
	{
		StopAudio();
	}
}




EProcessResult	AudioPluginPSP::ProcessAList()
{
	Memory_SP_SetRegisterBits(SP_STATUS_REG, SP_STATUS_HALT);

	EProcessResult	result = PR_NOT_STARTED;

	switch( gAudioPluginEnabled )
	{
		case APM_DISABLED:
			result = PR_COMPLETED;
			break;
		case APM_ENABLED_ASYNC:
			{
				SHLEStartJob	job;
				gJobManager.AddJob( &job, sizeof( job ) );
			}
			result = PR_STARTED;
			break;
		case APM_ENABLED_SYNC:
			HLEStart();
			result = PR_COMPLETED;
			break;
	}

	return result;
}

void AudioPluginPSP::AddBuffer( u8 *start, u32 length )
{
	if (length == 0)
		return;

	if (!mKeepRunning)
		StartAudio();

	u32 num_samples = length / sizeof( Sample );

	switch( gAudioPluginEnabled )
	{
	case APM_DISABLED:
		break;

	case APM_ENABLED_ASYNC:
		{
			SAddSamplesJob	job( mAudioBufferUncached, reinterpret_cast< const Sample * >( start ), num_samples, mFrequency, kOutputFrequency );

			gJobManager.AddJob( &job, sizeof( job ) );
		}
		break;

	case APM_ENABLED_SYNC:
		{
			mAudioBufferUncached->AddSamples( reinterpret_cast< const Sample * >( start ), num_samples, mFrequency, kOutputFrequency );
		}
		break;
	}

	/*
	u32 remaining_samples = mAudioBuffer.GetNumBufferedSamples();
	mBufferLenMs = (1000 * remaining_samples) / kOutputFrequency);
	float ms = (float) num_samples * 1000.f / (float)mFrequency;
	#ifdef DAEDALUS_DEBUG_CONSOLE
	DPF_AUDIO("Queuing %d samples @%dHz - %.2fms - bufferlen now %d\n", num_samples, mFrequency, ms, mBufferLenMs);
	#endif
	*/
}

// void audioCallback( void * buf, unsigned int length, void * userdata )
// {
// 	AudioPluginPSP * ac( reinterpret_cast< AudioPluginPSP * >( userdata ) );

// 	ac->FillBuffer( reinterpret_cast< Sample * >( buf ), length );
// }


// void AudioPluginPSP::FillBuffer(Sample * buffer, u32 num_samples)
// {
// 	sceKernelWaitSema( mSemaphore, 1, nullptr );

// 	mAudioBufferUncached->Drain( buffer, num_samples );

// 	sceKernelSignalSema( mSemaphore, 1 );
// }


// void AudioPluginPSP::AudioSyncFunction(void * arg) {}


void AudioPluginPSP::StartAudio()
{
	static AudioPluginPSP * ac;
	if (mKeepRunning)
		return;

	mKeepRunning = true;

	ac = this;


AudioInit();


}



void AudioPluginPSP::StopAudio()
{
	if (!mKeepRunning)
		return;

	mKeepRunning = false;

	audio_open = false;
}

CAudioPlugin *		CreateAudioPlugin()
{
	return new AudioPluginPSP();
}
