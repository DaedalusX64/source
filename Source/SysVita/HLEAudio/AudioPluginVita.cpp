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

#include <vitasdk.h>

#include "AudioPluginVita.h"
#include "AudioOutput.h"
#include "HLEAudio/audiohle.h"

#include "Config/ConfigOptions.h"
#include "Core/CPU.h"
#include "Core/Interrupt.h"
#include "Core/Memory.h"
#include "Core/ROM.h"
#include "Core/RSP_HLE.h"

#define DEFAULT_FREQUENCY 44100	// Taken from Mupen64 : )
#define RSP_AUDIO_INTR_CYCLES     1

extern volatile u32 sound_status;

bool async_boot = true;

static SceUID audio_mutex = 0xDEADBEEF;

static int audioProcess(unsigned int args, void *argp)
{
	while (sound_status != 0xDEADBEEF)
	{
		sceKernelWaitSema(audio_mutex, 1, NULL);
		Audio_Ucode();
		CPU_AddEvent(RSP_AUDIO_INTR_CYCLES, CPU_EVENT_AUDIO);
	}
	sceKernelExitDeleteThread(0);
	return 0;
}

//*****************************************************************************
//
//*****************************************************************************
EAudioPluginMode gAudioPluginEnabled( APM_ENABLED_SYNC );
//bool gAdaptFrequency( false );

//*****************************************************************************
//
//*****************************************************************************
CAudioPluginVita::CAudioPluginVita()
:	mAudioOutput( new AudioOutput )
{
	//mAudioOutput->SetAdaptFrequency( gAdaptFrequency );
	//gAudioPluginEnabled = enable_audio;
}

//*****************************************************************************
//
//*****************************************************************************
CAudioPluginVita::~CAudioPluginVita()
{
	delete mAudioOutput;
}

//*****************************************************************************
//
//*****************************************************************************
CAudioPluginVita *	CAudioPluginVita::Create()
{
	return new CAudioPluginVita();
}

//*****************************************************************************
//
//*****************************************************************************
/*
void	CAudioPluginVita::SetAdaptFrequecy( bool adapt )
{
	mAudioOutput->SetAdaptFrequency( adapt );
}
*/
//*****************************************************************************
//
//*****************************************************************************
bool		CAudioPluginVita::StartEmulation()
{
	return true;
}

//*****************************************************************************
//
//*****************************************************************************
void	CAudioPluginVita::StopEmulation()
{
	Audio_Reset();
	mAudioOutput->StopAudio();
}

void	CAudioPluginVita::DacrateChanged( int SystemType )
{
	u32 type = (u32)((SystemType == ST_NTSC) ? VI_NTSC_CLOCK : VI_PAL_CLOCK);
	u32 dacrate = Memory_AI_GetRegister(AI_DACRATE_REG);
	u32	frequency = type / (dacrate + 1);

	mAudioOutput->SetFrequency( frequency );
}

//*****************************************************************************
//
//*****************************************************************************
void	CAudioPluginVita::LenChanged()
{
	if( gAudioPluginEnabled > APM_DISABLED )
	{
		//mAudioOutput->SetAdaptFrequency( gAdaptFrequency );

		u32		address( Memory_AI_GetRegister(AI_DRAM_ADDR_REG) & 0xFFFFFF );
		u32		length(Memory_AI_GetRegister(AI_LEN_REG));

		mAudioOutput->AddBuffer( g_pu8RamBase + address, length );
	}
	else
	{
		mAudioOutput->StopAudio();
	}
}

//*****************************************************************************
//
//*****************************************************************************
u32		CAudioPluginVita::ReadLength()
{
	return 0;
}

//*****************************************************************************
//
//*****************************************************************************
EProcessResult	CAudioPluginVita::ProcessAList()
{
	Memory_SP_SetRegisterBits(SP_STATUS_REG, SP_STATUS_HALT);

	EProcessResult	result( PR_NOT_STARTED );
	
	// FIXME: We would want this to be on constructor but it somehow breaks everything
	if (async_boot) {
		if (audio_mutex == 0xDEADBEEF) audio_mutex = sceKernelCreateSema("Audio Mutex", 0, 0, 1, NULL);

		// create audio processing thread
		SceUID audioThid = sceKernelCreateThread("audioProcess", &audioProcess, 0x10000100, 0x10000, 0, 0, NULL);
		sceKernelStartThread(audioThid, 0, NULL);

		async_boot = false;
	}
		
	switch( gAudioPluginEnabled )
	{
		case APM_DISABLED:
			result = PR_COMPLETED;
			break;
		case APM_ENABLED_ASYNC:
			sceKernelSignalSema(audio_mutex, 1);
			result = PR_STARTED;
			break;
		case APM_ENABLED_SYNC:
			Audio_Ucode();
			result = PR_COMPLETED;
			break;
	}

	return result;
}

//*****************************************************************************
//
//*****************************************************************************
CAudioPlugin *		CreateAudioPlugin()
{
	return CAudioPluginVita::Create();
}
