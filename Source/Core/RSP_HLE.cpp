/*
Copyright (C) 2001 StrmnNrmn

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


#include "Base/Types.h"
#include <cstring> 
#include <fstream>
#if defined(GCC_LEGACY)
#include <fmt/core.h> 
#else 
#include <format>
#endif
#include "Core/RSP_HLE.h"

#include "Core/Interrupt.h"
#include "Core/Memory.h"
#include "Debug/DBGConsole.h"
#include "Debug/DebugLog.h"
#include "Debug/Dump.h"			// For Dump_GetDumpDirectory()
#include "Utility/MathUtil.h"
#include "Ultra/ultra_mbi.h"
#include "Ultra/ultra_rcp.h"
#include "Ultra/ultra_sptask.h"
#include "HLEAudio/AudioPlugin.h"
#include "HLEGraphics/GraphicsPlugin.h"
#include "Utility/BatchTest.h"

#include "Debug/PrintOpCode.h"
#include "Utility/Profiler.h"

static const bool	gGraphicsEnabled = true;
static const bool	gAudioEnabled	 = true;

extern void jpeg_decode_PS(OSTask *task);
extern void jpeg_decode_PS0(OSTask *task);
extern void jpeg_decode_OB(OSTask *task);


/* RE2Task.cpp + HqvmTask.cpp */
extern "C" {
	//extern void resize_bilinear_task(OSTask *task);
	//extern void decode_video_frame_task(OSTask *task);
	//extern void fill_video_double_buffer_task(OSTask *task);
	extern void hvqm2_decode_sp1_task(OSTask *task);
};

#ifdef DAEDALUS_DEBUG_CONSOLE
#if 0
static void RDP_DumpRSPCode(char * name, u32 crc, u32 * mem_base, u32 pc_base, u32 len)
{
	#if defined(GCC_LEGACY)
	std::string filename = fmt::format("task_dump_{}_crc_0x{}.txt", name, crc);
	#else 
	std::string filename = std::format("task_dump_{}_crc_0x{}.txt", name, crc);
	#endif
	// snprintf(filename, sizeof(filename), "task_dump_%s_crc_0x%08x.txt", name, crc);

	std::filesystem::path filepath = setBasePath("rsp_dumps");
	std::filesystem::create_directory(filepath);
	filepath /= filename;

	std::fstream fp(filepath, std::ios::in);

	if (fp.is_open())
	{
	for (u32 i = 0; i < len; i+=4)
	{
		OpCode op;
		u32 pc = i & 0x0FFF;
		op._u32 = mem_base[i/4];

		char opinfo[400];
		SprintRSPOpCodeInfo( opinfo, pc + pc_base, op );
		#if defined(GCC_LEGACY)
		fp << fmt::format("0x{:08x}: <0x{:08x}> {}\n", pc + pc_base, op._u32, opinfo);
		#else 
		fp << std::format("0x{:08x}: <0x{:08x}> {}\n", pc + pc_base, op._u32, opinfo);
		#endif
	}
	}
}
#endif


#if 0
static void	RSP_HLE_DumpTaskInfo( const OSTask * pTask )
{
	DBGConsole_Msg(0, "DP: Task:%08x Flags:%08x BootCode:%08x BootCodeSize:%08x",
		pTask->t.type, pTask->t.flags, pTask->t.ucode_boot, pTask->t.ucode_boot_size);

	DBGConsole_Msg(0, "DP: uCode:%08x uCodeSize:%08x uCodeData:%08x uCodeDataSize:%08x",
		pTask->t.ucode, pTask->t.ucode_size, pTask->t.ucode_data, pTask->t.ucode_data_size);

	DBGConsole_Msg(0, "DP: Stack:%08x StackS:%08x Output:%08x OutputS:%08x",
		pTask->t.dram_stack, pTask->t.dram_stack_size, pTask->t.output_buff, pTask->t.output_buff_size);

	DBGConsole_Msg(0, "DP: Data(PC):%08x DataSize:%08x YieldData:%08x YieldDataSize:%08x",
		pTask->t.data_ptr, pTask->t.data_size, pTask->t.yield_data_ptr, pTask->t.yield_data_size);
}
#endif

#endif
//

void RSP_HLE_Finished(u32 setbits)
{
	// Need to point to last instr?
	//Memory_DPC_SetRegister(DPC_CURRENT_REG, (u32)pTask->t.data_ptr);

	//
	// Set the SP flags appropriately. The RSP is not running anyway, no need to stop it
	//
	u32 status = Memory_SP_SetRegisterBits(SP_STATUS_REG, setbits);

	//
	// We've set the SP_STATUS_BROKE flag - better check if it causes an interrupt
	//
	if( status & SP_STATUS_INTR_BREAK )
	{
		Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_SP);
		R4300_Interrupt_UpdateCause3();
	}
}


//

static EProcessResult RSP_HLE_Graphics()
{
	DAEDALUS_PROFILE( "HLE: Graphics" );

	if (gGraphicsEnabled && gGraphicsPlugin != nullptr)
	{
		gGraphicsPlugin->ProcessDList();
	}
	else
	{
		// Skip the entire dlist if graphics are disabled
		Memory_MI_SetRegisterBits(MI_INTR_REG, MI_INTR_DP);
		R4300_Interrupt_UpdateCause3();
	}


#ifdef DAEDALUS_BATCH_TEST_ENABLED
	if (CBatchTestEventHandler * handler = BatchTest_GetHandler())
	{
		handler->OnDisplayListComplete();
	}
#endif

	return PR_COMPLETED;
}


//

static EProcessResult RSP_HLE_Audio()
{
	DAEDALUS_PROFILE( "HLE: Audio" );

	if (gAudioEnabled && gAudioPlugin != nullptr)
	{
		return gAudioPlugin->ProcessAList();
	}
	return PR_COMPLETED;
}

// RSP_HLE_Jpeg and RSP_HLE_CICX105 were borrowed from Mupen64plus
static u32 sum_bytes(const u8 *bytes, u32 size)
{
    u32 sum = 0;
    const u8 * const bytes_end = bytes + size;

    while (bytes != bytes_end)
        sum += *bytes++;

    return sum;
}

EProcessResult RSP_HLE_Jpeg(OSTask * task)
{
	// most ucode_boot procedure copy 0xf80 bytes of ucode whatever the ucode_size is.
	// For practical purpose we use a ucode_size = min(0xf80, task->ucode_size)
	u32 sum = sum_bytes(g_pu8RamBase + (u32)task->t.ucode , std::min<u32>(task->t.ucode_size, 0xf80) >> 1);

	//DBGConsole_Msg(0, "JPEG Task: Sum=0x%08x", sum);
	switch(sum)
	{
	case 0x2c85a: // Pokemon Stadium Jap Exclusive jpg decompression
		jpeg_decode_PS0(task);
		break;
	case 0x2caa6: // Zelda OOT, Pokemon Stadium {1,2} jpg decompression
		jpeg_decode_PS(task);
		break;
	case 0x130de: // Ogre Battle & Buttom of the 9th background decompression
	case 0x278b0:
        jpeg_decode_OB(task);
		break;
	}

	return PR_COMPLETED;
}

EProcessResult RSP_HLE_CICX105(OSTask * task [[maybe_unused]])
{
    const u32 sum {sum_bytes(g_pu8SpImemBase, 0x1000 >> 1)};

    switch(sum)
    {
        /* CIC x105 ucode (used during boot of CIC x105 games) */
        case 0x9e2: /* CIC 6105 */
        case 0x9f2: /* CIC 7105 */
			{
				u32 i;
				u8 * dst {g_pu8RamBase + 0x2fb1f0};
				u8 * src {g_pu8SpImemBase + 0x120};

				/* dma_read(0x1120, 0x1e8, 0x1e8) */
				memcpy(g_pu8SpImemBase + 0x120, g_pu8RamBase + 0x1e8, 0x1f0);

				/* dma_write(0x1120, 0x2fb1f0, 0xfe817000) */
				for (i = 0; i < 24; ++i)
				{
					memcpy(dst, src, 8);
					dst += 0xff0;
					src += 0x8;

				}
			}
			break;

    }

	return PR_COMPLETED;
}

// Pokemon Puzzle League uses this
EProcessResult RSP_HLE_Hvqm(OSTask * task)
{
	hvqm2_decode_sp1_task(task);
	return PR_COMPLETED;
}

void RSP_HLE_Reset()
{}

void RSP_HLE_ProcessTask()
{
	OSTask * pTask = (OSTask *)(g_pu8SpMemBase + 0x0FC0);
	EProcessResult	result( PR_NOT_STARTED );

	// non task
	if(pTask->t.ucode_boot_size > 0x1000)
	{
		RSP_HLE_CICX105(pTask);
		RSP_HLE_Finished(SP_STATUS_BROKE|SP_STATUS_HALT);
		return;
	}

	switch ( pTask->t.type )
	{
		case M_GFXTASK:
			// frozen task
			if(Memory_DPC_GetRegister(DPC_STATUS_REG) & DPC_STATUS_FREEZE)
			{
				return;
			}
			result = RSP_HLE_Graphics();
			break;

		case M_AUDTASK:
			result = RSP_HLE_Audio();
			break;

		case M_VIDTASK:
			// Can't handle
			break;

		case M_FBTASK:
			result = RSP_HLE_Hvqm(pTask);
			break;

		case M_JPGTASK:
			result = RSP_HLE_Jpeg(pTask);
			break;
		default:
			// Can't handle
			DBGConsole_Msg(0, "Unknown task: %08x", pTask->t.type );
			//	RSP_HLE_DumpTaskInfo( pTask );
			//	RDP_DumpRSPCode("boot",    0xDEAFF00D, (u32*)(g_pu8RamBase + (((u32)pTask->t.ucode_boot)&0x00FFFFFF)), 0x04001000, pTask->t.ucode_boot_size);
			//	RDP_DumpRSPCode("unkcode", 0xDEAFF00D, (u32*)(g_pu8RamBase + (((u32)pTask->t.ucode)&0x00FFFFFF)),      0x04001080, 0x1000 - 0x80);//pTask->t.ucode_size);

			break;
	}

	// Started and completed. No need to change cores. [synchronously]
	if( result == PR_COMPLETED )
		RSP_HLE_Finished(SP_STATUS_TASKDONE|SP_STATUS_BROKE|SP_STATUS_HALT);
}
