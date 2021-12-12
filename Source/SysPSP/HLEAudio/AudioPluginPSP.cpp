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
#include <pspuser.h>

#include "Config/ConfigOptions.h"
#include "Core/CPU.h"
#include "Core/Interrupt.h"
#include "Core/Memory.h"
#include "Core/ROM.h"
#include "Core/RSP_HLE.h"
#include "Debug/DBGConsole.h"
#include "HLEAudio/audiohle.h"
#include "HLEAudio/AudioBuffer.h"
#include "HLEAudio/AudioPlugin.h"
#include "SysPSP/Utility/CacheUtil.h"
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
	virtual void FillBuffer( Sample * buffer, u32 num_samples);

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

#ifdef DAEDALUS_PSP_USE_ME

#include "SysPSP/PRX/MediaEngine/me.h"
#include "SysPSP/Utility/ModulePSP.h"

bool gLoadedMediaEnginePRX {false};

volatile me_struct *mei;

bool InitialiseMediaEngine()
{

	if( CModule::Load("mediaengine.prx") < 0 )	return false;

	mei = (volatile struct me_struct *)malloc_64(sizeof(struct me_struct));
	mei = (volatile struct me_struct *)(MAKE_UNCACHED_PTR(mei));
	sceKernelDcacheWritebackInvalidateAll();

	if (InitME(mei) == 0)
	{
		gLoadedMediaEnginePRX = true;
		return true;
	}
	else
	{
		#ifdef DAEDALUS_DEBUG_CONSOLE
		printf(" Couldn't initialize MediaEngine Instance\n");
		#endif
		return false;
	}

}

#endif


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
  pspAudioEnd();
}


bool	AudioPluginPSP::StartEmulation()
{
	return true;
}

void	AudioPluginPSP::StopEmulation()
{
  	StopAudio();
    sceKernelDeleteSema(mSemaphore);
    pspAudioEndPre();
    sceKernelDelayThread(100000);
    pspAudioEnd();

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
				// sceKernelDcacheWritebackInvalidateAll();
				if(BeginME( mei, (int)&HLEStart, (int)NULL, -1, NULL, -1, NULL) < 0)
				{
						HLEStart();
						result = PR_COMPLETED;
						break;
				}
			}
			result = PR_COMPLETED;
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
			mAudioBufferUncached->AddSamples( reinterpret_cast< const Sample * >( start ), num_samples, mFrequency, kOutputFrequency );

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

void audioCallback( void * buf, unsigned int length, void * userdata )
{
	AudioPluginPSP * ac( reinterpret_cast< AudioPluginPSP * >( userdata ) );

	ac->FillBuffer( reinterpret_cast< Sample * >( buf ), length );
}


void AudioPluginPSP::FillBuffer(Sample * buffer, u32 num_samples)
{
	sceKernelWaitSema( mSemaphore, 1, nullptr );

	mAudioBufferUncached->Drain( buffer, num_samples );

	sceKernelSignalSema( mSemaphore, 1 );
}


// void AudioPluginPSP::AudioSyncFunction(void * arg) {}


void AudioPluginPSP::StartAudio()
{
	static AudioPluginPSP * ac;
	if (mKeepRunning)
		return;

	mKeepRunning = true;

	ac = this;


pspAudioInit();
pspAudioSetChannelCallback( 0, audioCallback, this );

	// Everything OK
	audio_open = true;
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
