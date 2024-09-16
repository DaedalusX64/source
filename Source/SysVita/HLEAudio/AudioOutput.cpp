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

#include "stdafx.h"
#include "AudioOutput.h"

#include <stdio.h>
#include <new>

#include <vitasdk.h>

#include "Config/ConfigOptions.h"
#include "Debug/DBGConsole.h"
#include "HLEAudio/AudioBuffer.h"
#include "Utility/FramerateLimiter.h"
#include "Utility/Thread.h"

extern u32 gSoundSync;

static const u32	VITA_NUM_SAMPLES = 512;
static const u32	BUFFER_SIZE  = VITA_NUM_SAMPLES * 4;

volatile u32 sound_status;
extern bool async_boot;

static bool audio_open = false;

static AudioOutput *ac;

CAudioBuffer *mAudioBuffer;

static int audioOutput(unsigned int args, void *argp)
{
	Sample *playbuf = (Sample*)malloc(BUFFER_SIZE);
	
	// reserve audio channel
	SceUID sound_channel = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, VITA_NUM_SAMPLES, DESIRED_OUTPUT_FREQUENCY, SCE_AUDIO_OUT_MODE_STEREO);
	sceAudioOutSetConfig(sound_channel, -1, -1, (SceAudioOutMode)-1);
	
	int vol_stereo[] = {32767, 32767};
	sceAudioOutSetVolume(sound_channel, (SceAudioOutChannelFlag)(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), vol_stereo);

	while (sound_status != 0xDEADBEEF)
	{
		mAudioBuffer->Drain(playbuf, VITA_NUM_SAMPLES);
		sceAudioOutOutput(sound_channel, playbuf);
	}
	free(playbuf);
	sceAudioOutReleasePort(sound_channel);
	sceKernelExitDeleteThread(0);
	return 0;
}

static void AudioInit()
{
	sound_status = 0; // threads running

	// create audio playback thread
	SceUID audioThid = sceKernelCreateThread("audioOutput", &audioOutput, 0x10000100, 0x10000, 0, 0, NULL);
	sceKernelStartThread(audioThid, 0, NULL);
	
	// Everything OK
	audio_open = true;
}

static void AudioExit()
{
	// Stop stream
	if (audio_open)
	{
		sound_status = 0xDEADBEEF;
		sceKernelDelayThread(100*1000);
		async_boot = false;
	}

	audio_open = false;
}

AudioOutput::AudioOutput()
:	mAudioPlaying( false )
,	mFrequency( DESIRED_OUTPUT_FREQUENCY )
{
	void * mem = malloc( sizeof( CAudioBuffer ) );
	mAudioBuffer = new( mem ) CAudioBuffer( BUFFER_SIZE );
}

AudioOutput::~AudioOutput( )
{
	StopAudio();

	mAudioBuffer->~CAudioBuffer();
	free( mAudioBuffer );
}

void AudioOutput::SetFrequency( u32 frequency )
{
	mFrequency = frequency;
}

void AudioOutput::AddBuffer( u8 *start, u32 length )
{
	if (length == 0)
		return;

	if (!mAudioPlaying)
		StartAudio();

	u32 num_samples = length / sizeof( Sample );
	
	//Adapt Audio to sync% //Corn
	u32 output_freq = DESIRED_OUTPUT_FREQUENCY;
	if (gAudioRateMatch)
	{
		if (gSoundSync > DESIRED_OUTPUT_FREQUENCY * 2) output_freq = DESIRED_OUTPUT_FREQUENCY * 2;	//limit upper rate
		else if (gSoundSync < DESIRED_OUTPUT_FREQUENCY)	output_freq = DESIRED_OUTPUT_FREQUENCY;	//limit lower rate
		else output_freq = gSoundSync;
	}


	switch( gAudioPluginEnabled )
	{
	case APM_DISABLED:
		break;
	case APM_ENABLED_ASYNC:
		mAudioBuffer->AddSamples( reinterpret_cast< const Sample * >( start ), num_samples, mFrequency, output_freq );		
		break;
	case APM_ENABLED_SYNC:
		mAudioBuffer->AddSamples( reinterpret_cast< const Sample * >( start ), num_samples, mFrequency, output_freq );
		break;
	}
}

void AudioOutput::StartAudio()
{
	if (mAudioPlaying)
		return;

	mAudioPlaying = true;

	ac = this;

	AudioInit();
}

void AudioOutput::StopAudio()
{
	if (!mAudioPlaying)
		return;

	mAudioPlaying = false;

	AudioExit();
}
