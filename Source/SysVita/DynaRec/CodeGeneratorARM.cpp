/*
Copyright (C) 2020 MasterFeizz

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
#include "CodeGeneratorARM.h"

#include <cstdio>
#include <algorithm>
#include "Config/ConfigOptions.h"
#include "Core/CPU.h"
#include "Core/R4300.h"
#include "Core/Registers.h"
#include "Core/ROM.h"
#include "Debug/DBGConsole.h"
#include "Debug/DebugLog.h"
#include "DynaRec/AssemblyUtils.h"
#include "DynaRec/IndirectExitMap.h"
#include "DynaRec/StaticAnalysis.h"
#include "DynaRec/Trace.h"
#include "OSHLE/ultra_R4300.h"

using namespace AssemblyUtils;

static const u32		NUM_MIPS_REGISTERS( 32 );
static const EArmReg	gMemoryBaseReg = ArmReg_R10;
static const EArmReg	gMemUpperBoundReg = ArmReg_R9;

static const EArmReg gRegistersToUseForCaching[] = {
//	ArmReg_R0, 
//	ArmReg_R1,  
	//ArmReg_R2,  
	//ArmReg_R3,
//	ArmReg_R4,
	ArmReg_R5,
	ArmReg_R6,
	ArmReg_R7,
	ArmReg_R8,
	// ArmReg_R9,  this is the memory base register
	// ArmReg_R10, this is the memory upper bound register
	ArmReg_R11,
	// ArmReg_R12, this holds a pointer to gCpuState
	// ArmReg_R13, this is the stack pointer
	 ArmReg_R14,// this is the link register
	// ArmReg_R15, this is the PC
};

// XX this optimisation works very well on the PSP, option to disable it was removed
static const bool		gDynarecStackOptimisation = true;

// function stubs from assembly
extern "C" { void _DirectExitCheckNoDelay( u32 instructions_executed, u32 exit_pc ); }
extern "C" { void _DirectExitCheckDelay( u32 instructions_executed, u32 exit_pc, u32 target_pc ); }
extern "C" { void _IndirectExitCheck( u32 instructions_executed, CIndirectExitMap* map, u32 exit_pc ); }
extern "C" { const void * g_MemoryLookupTableReadForDynarec = g_MemoryLookupTableRead; }
extern "C" { 	
	void HandleException_extern()
	{
		switch (gCPUState.Delay)
		{
			case DO_DELAY:
				gCPUState.CurrentPC += 4;
				gCPUState.Delay = EXEC_DELAY;
				break;
			case EXEC_DELAY:
				gCPUState.CurrentPC = gCPUState.TargetPC;
				gCPUState.Delay = NO_DELAY;
				break;
			case NO_DELAY:
				gCPUState.CurrentPC += 4;
				break;
			default:
				NODEFAULT;
		}
	}
}

extern "C"
{
	u32 _ReadBitsDirect_u8( u32 address, u32 current_pc );
	u32 _ReadBitsDirect_s8( u32 address, u32 current_pc );
	u32 _ReadBitsDirect_u16( u32 address, u32 current_pc );
	u32 _ReadBitsDirect_s16( u32 address, u32 current_pc );
	u32 _ReadBitsDirect_u32( u32 address, u32 current_pc );

	u32 _ReadBitsDirectBD_u8( u32 address, u32 current_pc );
	u32 _ReadBitsDirectBD_s8( u32 address, u32 current_pc );
	u32 _ReadBitsDirectBD_u16( u32 address, u32 current_pc );
	u32 _ReadBitsDirectBD_s16( u32 address, u32 current_pc );
	u32 _ReadBitsDirectBD_u32( u32 address, u32 current_pc );

	// Dynarec calls this for simplicity
	void Write32BitsForDynaRec( u32 address, u32 value )
	{
		Write32Bits_NoSwizzle( address, value );
	}
	void Write16BitsForDynaRec( u32 address, u16 value )
	{
		Write16Bits_NoSwizzle( address, value );
	}
	void Write8BitsForDynaRec( u32 address, u8 value )
	{
		Write8Bits_NoSwizzle( address, value );
	}

	void _WriteBitsDirect_u32( u32 address, u32 value, u32 current_pc );
	void _WriteBitsDirect_u16( u32 address, u32 value, u32 current_pc );		// Value in low 16 bits
	void _WriteBitsDirect_u8( u32 address, u32 value, u32 current_pc );			// Value in low 8 bits

	void _WriteBitsDirectBD_u32( u32 address, u32 value, u32 current_pc );
	void _WriteBitsDirectBD_u16( u32 address, u32 value, u32 current_pc );		// Value in low 16 bits
	void _WriteBitsDirectBD_u8( u32 address, u32 value, u32 current_pc );			// Value in low 8 bits
}

#define ReadBitsDirect_u8 _ReadBitsDirect_u8
#define ReadBitsDirect_s8 _ReadBitsDirect_s8
#define ReadBitsDirect_u16 _ReadBitsDirect_u16
#define ReadBitsDirect_s16 _ReadBitsDirect_s16
#define ReadBitsDirect_u32 _ReadBitsDirect_u32

#define ReadBitsDirectBD_u8 _ReadBitsDirectBD_u8
#define ReadBitsDirectBD_s8 _ReadBitsDirectBD_s8
#define ReadBitsDirectBD_u16 _ReadBitsDirectBD_u16
#define ReadBitsDirectBD_s16 _ReadBitsDirectBD_s16
#define ReadBitsDirectBD_u32 _ReadBitsDirectBD_u32


#define WriteBitsDirect_u32 _WriteBitsDirect_u32
#define WriteBitsDirect_u16 _WriteBitsDirect_u16
#define WriteBitsDirect_u8 _WriteBitsDirect_u8

#define WriteBitsDirectBD_u32 _WriteBitsDirectBD_u32
#define WriteBitsDirectBD_u16 _WriteBitsDirectBD_u16
#define WriteBitsDirectBD_u8 _WriteBitsDirectBD_u8


//Helper functions used for slow loads
s32 Read8Bits_Signed ( u32 address ) { return (s8) Read8Bits(address); };
s32 Read16Bits_Signed( u32 address ) { return (s16)Read16Bits(address); };

#define URO_HI_SIGN_EXTEND 0	// Sign extend from src
#define URO_HI_CLEAR	   1	// Clear hi bits

//*****************************************************************************
//	XXXX
//*****************************************************************************
void Dynarec_ClearedCPUStuffToDo(){}
void Dynarec_SetCPUStuffToDo(){}
//*****************************************************************************
//
//*****************************************************************************
CCodeGeneratorARM::CCodeGeneratorARM( CAssemblyBuffer * p_primary, CAssemblyBuffer * p_secondary )
:	CCodeGenerator( )
,	CAssemblyWriterARM( p_primary, p_secondary )
,	mSpCachedInESI( false )
,	mSetSpPostUpdate( 0 )
,	mpPrimary( p_primary )
,	mpSecondary( p_secondary )
,	mLoopTop( nullptr )
,	mUseFixedRegisterAllocation( false )
{
}

void	CCodeGeneratorARM::Finalise( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps, const std::vector< RegisterSnapshotHandle >& exception_handler_snapshots )
{
	if( !exception_handler_jumps.empty() )
	{
		GenerateExceptionHander( p_exception_handler_fn, exception_handler_jumps, exception_handler_snapshots );
	}

	SetBufferA();
	InsertLiteralPool(false);
	SetBufferB();
	InsertLiteralPool(false);
	SetAssemblyBuffer( NULL );
	mpPrimary = NULL;
	mpSecondary = NULL;
}

#if 0
u32 gNumFragmentsExecuted = 0;
extern "C"
{

void LogFragmentEntry( u32 entry_address )
{
	gNumFragmentsExecuted++;
	if(gNumFragmentsExecuted >= 0x99990)
	{
		char buffer[ 128 ]
		sprintf( buffer, "Address %08x\n", entry_address );
		OutputDebugString( buffer );
	}
}

}
#endif

void CCodeGeneratorARM::Initialise( u32 entry_address, u32 exit_address, u32 * hit_counter, const void * p_base, const SRegisterUsageInfo & register_usage )
{
	//MOVI(ECX_CODE, entry_address);
	// CALL( CCodeLabel( LogFragmentEntry ) );

	//PUSH(1 << ArmReg_R14);
	mEntryAddress = entry_address;
	if( hit_counter != NULL )
	{
		MOV32(ArmReg_R1, (u32)hit_counter);
		LDR(ArmReg_R0, ArmReg_R1, 0);
		ADD_IMM(ArmReg_R0, ArmReg_R0, 1);
		STR(ArmReg_R0, ArmReg_R1, 0);
	}

	// p_base/span_list ignored for now
	SetRegisterSpanList(register_usage, entry_address == exit_address);
}

void	CCodeGeneratorARM::SetRegisterSpanList(const SRegisterUsageInfo& register_usage, bool loops_to_self)
{
	mRegisterSpanList = register_usage.SpanList;

	// Sort in order of increasing start point
	std::sort(mRegisterSpanList.begin(), mRegisterSpanList.end(), SAscendingSpanStartSort());

	const u32 NUM_CACHE_REGS(sizeof(gRegistersToUseForCaching) / sizeof(gRegistersToUseForCaching[0]));

	// Push all the available registers in reverse order (i.e. use temporaries later)
	// Use temporaries first so we can avoid flushing them in case of a funcion call //Corn
#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT(mAvailableRegisters.empty(), "Why isn't the available register list empty?");
#endif
	for (u32 i{ 0 }; i < NUM_CACHE_REGS; i++)
	{
		mAvailableRegisters.push(gRegistersToUseForCaching[i]);
	}

	// Optimization for self looping code
	if (gDynarecLoopsOptimisation && loops_to_self)
	{
		mUseFixedRegisterAllocation = true;
		u32		cache_reg_idx(0);
		u32		HiLo = 0;
		while (HiLo < 2)		// If there are still unused registers, assign to high part of reg
		{
			RegisterSpanList::const_iterator span_it = mRegisterSpanList.begin();
			while (span_it < mRegisterSpanList.end())
			{
				const SRegisterSpan& span(*span_it);
				if (cache_reg_idx < NUM_CACHE_REGS)
				{
					EArmReg		cachable_reg(gRegistersToUseForCaching[cache_reg_idx]);
					mRegisterCache.SetCachedReg(span.Register, HiLo, cachable_reg);
					cache_reg_idx++;
				}
				++span_it;
			}
			++HiLo;
		}
		//
		//	Pull all the cached registers into memory
		//
		// Skip r0
		u32 i = 1;
		while (i < NUM_N64_REGS)
		{
			EN64Reg	n64_reg = EN64Reg(i);
			u32 lo_hi_idx{};
			while (lo_hi_idx < 2)
			{
				if (mRegisterCache.IsCached(n64_reg, lo_hi_idx))
				{
					PrepareCachedRegister(n64_reg, lo_hi_idx);

					//
					//	If the register is modified anywhere in the fragment, we need
					//	to mark it as dirty so it's flushed correctly on exit.
					//
					if (register_usage.IsModified(n64_reg))
					{
						mRegisterCache.MarkAsDirty(n64_reg, lo_hi_idx, true);
					}
				}
				++lo_hi_idx;
			}
			++i;
		}
		mLoopTop = GetAssemblyBuffer()->GetLabel();
	} //End of Loop optimization code
}

void	CCodeGeneratorARM::ExpireOldIntervals(u32 instruction_idx)
{
	// mActiveIntervals is held in order of increasing end point
	for (RegisterSpanList::iterator span_it = mActiveIntervals.begin(); span_it < mActiveIntervals.end(); ++span_it)
	{
		const SRegisterSpan& span(*span_it);

		if (span.SpanEnd >= instruction_idx)
		{
			break;
		}

		// This interval is no longer active - flush the register and return it to the list of available regs
		EArmReg		arm_reg(mRegisterCache.GetCachedReg(span.Register, 0));

		FlushRegister(mRegisterCache, span.Register, 0, true);

		mRegisterCache.ClearCachedReg(span.Register, 0);

		mAvailableRegisters.push(arm_reg);

		span_it = mActiveIntervals.erase(span_it);
	}
}


//

void	CCodeGeneratorARM::SpillAtInterval(const SRegisterSpan& live_span)
{
#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT(!mActiveIntervals.empty(), "There are no active intervals");
#endif
	const SRegisterSpan& last_span(mActiveIntervals.back());		// Spill the last active interval (it has the greatest end point)

	if (last_span.SpanEnd > live_span.SpanEnd)
	{
		// Uncache the old span
		EArmReg		arm_reg(mRegisterCache.GetCachedReg(last_span.Register, 0));
		FlushRegister(mRegisterCache, last_span.Register, 0, true);
		mRegisterCache.ClearCachedReg(last_span.Register, 0);

		// Cache the new span
		mRegisterCache.SetCachedReg(live_span.Register, 0, arm_reg);

		mActiveIntervals.pop_back();				// Remove the last span
		mActiveIntervals.push_back(live_span);	// Insert in order of increasing end point

		std::sort(mActiveIntervals.begin(), mActiveIntervals.end(), SAscendingSpanEndSort());		// XXXX - will be quicker to insert in the correct place rather than sorting each time
	}
	else
	{
		// There is no space for this register - we just don't update the register cache info, so we save/restore it from memory as needed
	}
}


//

void	CCodeGeneratorARM::UpdateRegisterCaching(u32 instruction_idx)
{
	if (!mUseFixedRegisterAllocation)
	{
		ExpireOldIntervals(instruction_idx);

		for (RegisterSpanList::const_iterator span_it = mRegisterSpanList.begin(); span_it < mRegisterSpanList.end(); ++span_it)
		{
			const SRegisterSpan& span(*span_it);

			// As we keep the intervals sorted in order of SpanStart, we can exit as soon as we encounter a SpanStart in the future
			if (instruction_idx < span.SpanStart)
			{
				break;
			}

			// Only process live intervals
			if ((instruction_idx >= span.SpanStart) & (instruction_idx <= span.SpanEnd))
			{
				if (!mRegisterCache.IsCached(span.Register, 0))
				{
					if (mAvailableRegisters.empty())
					{
						SpillAtInterval(span);
					}
					else
					{
						// Use this register for caching
						mRegisterCache.SetCachedReg(span.Register, 0, mAvailableRegisters.top());

						// Pop this register from the available list
						mAvailableRegisters.pop();
						mActiveIntervals.push_back(span);		// Insert in order of increasing end point

						std::sort(mActiveIntervals.begin(), mActiveIntervals.end(), SAscendingSpanEndSort());		// XXXX - will be quicker to insert in the correct place rather than sorting each time
					}
				}
			}
		}
	}
}

// Loads a variable from memory (must be within offset range of &gCPUState)
void CCodeGeneratorARM::GetVar(EArmReg arm_reg, const u32* p_var)
{
	uint16_t offset = (u32)p_var - (u32)& gCPUState;
	LDR(arm_reg, ArmReg_R12, offset);
}

// Stores a variable into memory (must be within offset range of &gCPUState)
void CCodeGeneratorARM::SetVar( const u32 * p_var, u32 value )
{
	uint16_t offset = (u32)p_var - (u32)&gCPUState;
	MOV32(ArmReg_R4, (u32)value);
	STR(ArmReg_R4, ArmReg_R12, offset);
}

// Stores a register into memory (must be within offset range of &gCPUState)
void CCodeGeneratorARM::SetVar(const u32* p_var, EArmReg reg)
{
	uint16_t offset = (u32)p_var - (u32)& gCPUState;
	STR(reg, ArmReg_R12, offset);
}

void CCodeGeneratorARM::SetFloatVar(const f32* p_var, EArmVfpReg reg)
{
	uint16_t offset = (u32)p_var - (u32)&gCPUState;
	
	VSTR(reg, ArmReg_R12, offset);
}

void CCodeGeneratorARM::GetFloatVar( EArmVfpReg dst_reg, const f32 * p_var )
{
	uint16_t offset = (u32)p_var - (u32)&gCPUState;
	
	VLDR(dst_reg, ArmReg_R12, offset);
}

void CCodeGeneratorARM::SetDoubleVar(const f64* p_var, EArmVfpReg reg)
{
	uint16_t offset = (u32)p_var - (u32)&gCPUState;
	
	VSTR_D(reg, ArmReg_R12, offset);
}

void CCodeGeneratorARM::GetDoubleVar( EArmVfpReg dst_reg, const f64 * p_var )
{
	uint16_t offset = (u32)p_var - (u32)&gCPUState;
	
	VLDR_D(dst_reg, ArmReg_R12, offset);
}

//*****************************************************************************
//
//*****************************************************************************
RegisterSnapshotHandle	CCodeGeneratorARM::GetRegisterSnapshot()
{
	RegisterSnapshotHandle	handle(mRegisterSnapshots.size());

	mRegisterSnapshots.push_back(mRegisterCache);

	return handle;
}

//*****************************************************************************
//
//*****************************************************************************
CCodeLabel	CCodeGeneratorARM::GetEntryPoint() const
{
	return mpPrimary->GetStartAddress();
}

//*****************************************************************************
//
//*****************************************************************************
CCodeLabel	CCodeGeneratorARM::GetCurrentLocation() const
{
	return mpPrimary->GetLabel();
}

void	CCodeGeneratorARM::GetRegisterValue(EArmReg arm_reg, EN64Reg n64_reg, u32 lo_hi_idx)
{
	if (mRegisterCache.IsKnownValue(n64_reg, lo_hi_idx))
	{
		//printf( "Loading %s[%d] <- %08x\n", RegNames[ n64_reg ], lo_hi_idx, mRegisterCache.GetKnownValue( n64_reg, lo_hi_idx ) );
		MOV32(arm_reg, mRegisterCache.GetKnownValue(n64_reg, lo_hi_idx)._s32);
		if (mRegisterCache.IsCached(n64_reg, lo_hi_idx))
		{
			mRegisterCache.MarkAsValid(n64_reg, lo_hi_idx, true);
			mRegisterCache.MarkAsDirty(n64_reg, lo_hi_idx, true);
			mRegisterCache.ClearKnownValue(n64_reg, lo_hi_idx);
		}
	}
	else
	{
		GetVar(arm_reg, lo_hi_idx ? &gGPR[n64_reg]._u32_1 : &gGPR[n64_reg]._u32_0);
	}
}

//	Similar to GetRegisterAndLoad, but ALWAYS loads into the specified psp register

void CCodeGeneratorARM::LoadRegister( EArmReg arm_reg, EN64Reg n64_reg, u32 lo_hi_idx )
{
	if( mRegisterCache.IsCached( n64_reg, lo_hi_idx ) )
	{
		EArmReg	cached_reg( mRegisterCache.GetCachedReg( n64_reg, lo_hi_idx ) );


		// Load the register if it's currently invalid
		if( !mRegisterCache.IsValid( n64_reg,lo_hi_idx ) )
		{
			GetRegisterValue( cached_reg, n64_reg, lo_hi_idx );
			mRegisterCache.MarkAsValid( n64_reg, lo_hi_idx, true );
		}

		// Copy the register if necessary
		if( arm_reg != cached_reg )
		{
			MOV( arm_reg, cached_reg);
		}
	}
	else if( n64_reg == N64Reg_R0 )
	{
		MOV32(arm_reg, 0);
	}
	else
	{
		GetRegisterValue( arm_reg, n64_reg, lo_hi_idx );
	}
}

//	This function pulls in a cached register so that it can be used at a later point.
//	This is usally done when we have a branching instruction - it guarantees that
//	the register is valid regardless of whether or not the branch is taken.
void	CCodeGeneratorARM::PrepareCachedRegister(EN64Reg n64_reg, u32 lo_hi_idx)
{
	if (mRegisterCache.IsCached(n64_reg, lo_hi_idx))
	{
		EArmReg	cached_reg(mRegisterCache.GetCachedReg(n64_reg, lo_hi_idx));

		// Load the register if it's currently invalid
		if (!mRegisterCache.IsValid(n64_reg, lo_hi_idx))
		{
			GetRegisterValue(cached_reg, n64_reg, lo_hi_idx);
			mRegisterCache.MarkAsValid(n64_reg, lo_hi_idx, true);
		}
	}
}

const CN64RegisterCacheARM& CCodeGeneratorARM::GetRegisterCacheFromHandle(RegisterSnapshotHandle snapshot) const
{
#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT(snapshot.Handle < mRegisterSnapshots.size(), "Invalid snapshot handle");
#endif
	return mRegisterSnapshots[snapshot.Handle];
}


//	Flush a specific register back to memory if dirty.
//	Clears the dirty flag and invalidates the contents if specified

void CCodeGeneratorARM::FlushRegister(CN64RegisterCacheARM& cache, EN64Reg n64_reg, u32 lo_hi_idx, bool invalidate)
{
	if (cache.IsDirty(n64_reg, lo_hi_idx))
	{
		if (cache.IsKnownValue(n64_reg, lo_hi_idx))
		{
			s32		known_value(cache.GetKnownValue(n64_reg, lo_hi_idx)._s32);

			SetVar(lo_hi_idx ? &gGPR[n64_reg]._u32_1 : &gGPR[n64_reg]._u32_0, known_value);
		}
		else if (cache.IsCached(n64_reg, lo_hi_idx))
		{
#ifdef DAEDALUS_ENABLE_ASSERTS
			DAEDALUS_ASSERT(cache.IsValid(n64_reg, lo_hi_idx), "Register is dirty but not valid?");
#endif
			EArmReg	cached_reg(cache.GetCachedReg(n64_reg, lo_hi_idx));

			SetVar(lo_hi_idx ? &gGPR[n64_reg]._u32_1 : &gGPR[n64_reg]._u32_0, cached_reg);
		}
#ifdef DAEDALUS_DEBUG_CONSOLE
		else
		{
			DAEDALUS_ERROR("Register is dirty, but not known or cached");
		}
#endif
		// We're no longer dirty
		cache.MarkAsDirty(n64_reg, lo_hi_idx, false);
	}

	// Invalidate the register, so we pick up any values the function might have changed
	if (invalidate)
	{
		cache.ClearKnownValue(n64_reg, lo_hi_idx);
		if (cache.IsCached(n64_reg, lo_hi_idx))
		{
			cache.MarkAsValid(n64_reg, lo_hi_idx, false);
		}
	}
}

//	This function flushes all dirty registers back to memory
//	If the invalidate flag is set this also invalidates the known value/cached
//	register. This is primarily to ensure that we keep the register set
//	in a consistent set across calls to generic functions. Ideally we need
//	to reimplement generic functions with specialised code to avoid the flush.

void	CCodeGeneratorARM::FlushAllRegisters(CN64RegisterCacheARM& cache, bool invalidate)
{
	mFloatCMPIsValid = false;	//invalidate float compare register
	mMultIsValid = false;	//Mult hi/lo are invalid

	// Skip r0
	for (u32 i = 1; i < NUM_N64_REGS; i++)
	{
		EN64Reg	n64_reg = EN64Reg(i);

		FlushRegister(cache, n64_reg, 0, invalidate);
		FlushRegister(cache, n64_reg, 1, invalidate);
	}

	FlushAllFloatingPointRegisters(cache, invalidate);
}

void	CCodeGeneratorARM::FlushAllFloatingPointRegisters( CN64RegisterCacheARM & cache, bool invalidate )
{
	
	for( u32 i {0}; i < NUM_N64_FP_REGS; i++ )
	{
		EN64FloatReg	n64_reg = EN64FloatReg( i );
		if( cache.IsFPDirty( n64_reg ) )
		{
			#ifdef DAEDALUS_ENABLE_ASSERTS
			DAEDALUS_ASSERT( cache.IsFPValid( n64_reg ), "Register is dirty but not valid?" );
			#endif
			EArmVfpReg	arm_reg = EArmVfpReg( n64_reg );

			SetFloatVar( &gCPUState.FPU[n64_reg]._f32, arm_reg );

			cache.MarkFPAsDirty( n64_reg, false );
		}

		if( invalidate )
		{
			// Invalidate the register, so we pick up any values the function might have changed
			cache.MarkFPAsValid( n64_reg, false );
			cache.MarkFPAsSim( n64_reg, false );
		}
	}
}

void	CCodeGeneratorARM::RestoreAllRegisters(CN64RegisterCacheARM& current_cache, CN64RegisterCacheARM& new_cache)
{
	// Skip r0
	for (u32 i{ 1 }; i < NUM_N64_REGS; i++)
	{
		EN64Reg	n64_reg = EN64Reg(i);

		if (new_cache.IsValid(n64_reg, 0) && !current_cache.IsValid(n64_reg, 0))
		{
			GetVar(new_cache.GetCachedReg(n64_reg, 0), &gGPR[n64_reg]._u32_0);
		}
		if (new_cache.IsValid(n64_reg, 1) && !current_cache.IsValid(n64_reg, 1))
		{
			GetVar(new_cache.GetCachedReg(n64_reg, 1), &gGPR[n64_reg]._u32_1);
		}
	}

	// XXXX some fp regs are preserved across function calls?
	for (u32 i{ 0 }; i < NUM_N64_FP_REGS; ++i)
	{
		EN64FloatReg	n64_reg = EN64FloatReg(i);
		if (new_cache.IsFPValid(n64_reg) && !current_cache.IsFPValid(n64_reg))
		{
			EArmVfpReg	arm_reg = EArmVfpReg(n64_reg);

			GetFloatVar(arm_reg, &gCPUState.FPU[n64_reg]._f32);
		}
	}
}
//

void CCodeGeneratorARM::StoreRegister(EN64Reg n64_reg, u32 lo_hi_idx, EArmReg arm_reg)
{
	mRegisterCache.ClearKnownValue(n64_reg, lo_hi_idx);

	if (mRegisterCache.IsCached(n64_reg, lo_hi_idx))
	{
		EArmReg	cached_reg(mRegisterCache.GetCachedReg(n64_reg, lo_hi_idx));

		//		gTotalRegistersCached++;

				// Update our copy as necessary
		if (arm_reg != cached_reg)
		{
			MOV(cached_reg, arm_reg);
		}
		mRegisterCache.MarkAsDirty(n64_reg, lo_hi_idx, true);
		mRegisterCache.MarkAsValid(n64_reg, lo_hi_idx, true);
	}
	else
	{
		//		gTotalRegistersUncached++;

		SetVar(lo_hi_idx ? &gGPR[n64_reg]._u32_1 : &gGPR[n64_reg]._u32_0, arm_reg);

		mRegisterCache.MarkAsDirty(n64_reg, lo_hi_idx, false);
	}
}


//

void CCodeGeneratorARM::SetRegister64(EN64Reg n64_reg, s32 lo_value, s32 hi_value)
{
	SetRegister(n64_reg, 0, lo_value);
	SetRegister(n64_reg, 1, hi_value);
}


//	Set the low 32 bits of a register to a known value (and hence the upper
//	32 bits are also known though sign extension)

inline void CCodeGeneratorARM::SetRegister32s(EN64Reg n64_reg, s32 value)
{
	//SetRegister64( n64_reg, value, value >= 0 ? 0 : 0xffffffff );
	SetRegister64(n64_reg, value, value >> 31);
}


//

inline void CCodeGeneratorARM::SetRegister(EN64Reg n64_reg, u32 lo_hi_idx, u32 value)
{
	mRegisterCache.SetKnownValue(n64_reg, lo_hi_idx, value);
	mRegisterCache.MarkAsDirty(n64_reg, lo_hi_idx, true);
	if (mRegisterCache.IsCached(n64_reg, lo_hi_idx))
	{
		mRegisterCache.MarkAsValid(n64_reg, lo_hi_idx, false);		// The actual cache is invalid though!
	}
}

//

void CCodeGeneratorARM::UpdateRegister(EN64Reg n64_reg, EArmReg  arm_reg, bool options)
{
	//if(n64_reg == N64Reg_R0) return;	//Try to modify R0!!!

	StoreRegisterLo(n64_reg, arm_reg);

	//Skip storing sign extension on some regs //Corn
	if (N64Reg_DontNeedSign(n64_reg)) return;

	if (options == URO_HI_SIGN_EXTEND)
	{
		EArmReg scratch_reg = ArmReg_R4;
		if (mRegisterCache.IsCached(n64_reg, 1))
		{
			scratch_reg = mRegisterCache.GetCachedReg(n64_reg, 1);
		}
		MOV_ASR_IMM(scratch_reg, arm_reg, 0x1F);		// Sign extend
		StoreRegisterHi(n64_reg, scratch_reg);
	}
	else	// == URO_HI_CLEAR
	{
		SetRegister(n64_reg, 1, 0);
	}
}

EArmVfpReg	CCodeGeneratorARM::GetFloatRegisterAndLoad( EN64FloatReg n64_reg )
{
	EArmVfpReg	arm_reg = EArmVfpReg( n64_reg );	// 1:1 mapping
	if( !mRegisterCache.IsFPValid( n64_reg ) )
	{
		GetFloatVar( arm_reg, &gCPUState.FPU[n64_reg]._f32 );
		mRegisterCache.MarkFPAsValid( n64_reg, true );
	}

	mRegisterCache.MarkFPAsSim( n64_reg, false );

	return arm_reg;
}

EArmVfpReg	CCodeGeneratorARM::GetDoubleRegisterAndLoad( EN64FloatReg n64_reg )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( n64_reg % 2 == 0, "n64_reg not a multiple of 2?" );
	#endif
	EArmVfpReg arm_reg = EArmVfpReg(n64_reg); // 1:1 mapping
	if (!mRegisterCache.IsFPValid(n64_reg) && !mRegisterCache.IsFPValid(EN64FloatReg(n64_reg + 1)) )
	{
		GetDoubleVar(EArmVfpReg(arm_reg/2), (f64*)&gCPUState.FPU[n64_reg]._f32);
		mRegisterCache.MarkFPAsValid( n64_reg, true );
		mRegisterCache.MarkFPAsValid( EN64FloatReg(n64_reg+1), true );
	}
	else if (!mRegisterCache.IsFPValid(n64_reg))
	{
		GetFloatVar( arm_reg, &gCPUState.FPU[n64_reg]._f32 );
		mRegisterCache.MarkFPAsValid( n64_reg, true );
	}
	else if (!mRegisterCache.IsFPValid(EN64FloatReg(n64_reg+1)))
	{
		GetFloatVar( EArmVfpReg(arm_reg+1), &gCPUState.FPU[n64_reg+1]._f32 );
		mRegisterCache.MarkFPAsValid( EN64FloatReg(n64_reg+1), true );
	}

	mRegisterCache.MarkFPAsSim( n64_reg, false );

	return EArmVfpReg(arm_reg/2); // ARM double precision instructions index by pairs (0-16), so divide the return register by 2
}

inline void CCodeGeneratorARM::UpdateFloatRegister( EN64FloatReg n64_reg )
{
	mRegisterCache.MarkFPAsDirty( n64_reg, true );
	mRegisterCache.MarkFPAsValid( n64_reg, true );
	mRegisterCache.MarkFPAsSim( n64_reg, false );
}

inline void CCodeGeneratorARM::UpdateDoubleRegister( EN64FloatReg n64_reg )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( n64_reg % 2 == 0, "n64_reg not a multiple of 2?" );
	#endif
	mRegisterCache.MarkFPAsDirty( n64_reg, true );
	mRegisterCache.MarkFPAsValid( n64_reg, true );
	mRegisterCache.MarkFPAsSim( n64_reg, false );
	mRegisterCache.MarkFPAsDirty( EN64FloatReg(n64_reg+1), true );
	mRegisterCache.MarkFPAsValid( EN64FloatReg(n64_reg+1), true );
	mRegisterCache.MarkFPAsSim( EN64FloatReg(n64_reg+1), false );
}
//*****************************************************************************
//
//*****************************************************************************
u32	CCodeGeneratorARM::GetCompiledCodeSize() const
{
	return mpPrimary->GetSize() + mpSecondary->GetSize();
}

//Get a (cached) N64 register mapped to an ARM register(usefull for dst register)
EArmReg	CCodeGeneratorARM::GetRegisterNoLoad( EN64Reg n64_reg, u32 lo_hi_idx, EArmReg scratch_reg )
{
	if (mRegisterCache.IsCached(n64_reg, lo_hi_idx))
	{
		return mRegisterCache.GetCachedReg(n64_reg, lo_hi_idx);
	}
	else
	{
		return scratch_reg;
	}
}

//Get (cached) N64 register value mapped to a ARM register (or scratch reg)
//and also load the value if not loaded yet(usefull for src register)

EArmReg	CCodeGeneratorARM::GetRegisterAndLoad(EN64Reg n64_reg, u32 lo_hi_idx, EArmReg scratch_reg)
{
	EArmReg		reg;
	bool		need_load(false);

	if (mRegisterCache.IsCached(n64_reg, lo_hi_idx))
	{
		//		gTotalRegistersCached++;
		reg = mRegisterCache.GetCachedReg(n64_reg, lo_hi_idx);

		// We're loading it below, so set the valid flag
		if (!mRegisterCache.IsValid(n64_reg, lo_hi_idx))
		{
			need_load = true;
			mRegisterCache.MarkAsValid(n64_reg, lo_hi_idx, true);
		}
	}
	else if (n64_reg == N64Reg_R0)
	{
		reg = scratch_reg;

		MOV32(scratch_reg, 0);
	}
	else
	{
		//		gTotalRegistersUncached++;
		reg = scratch_reg;
		need_load = true;
	}

	if (need_load)
	{
		GetRegisterValue(reg, n64_reg, lo_hi_idx);
	}

	return reg;
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation CCodeGeneratorARM::GenerateExitCode( u32 exit_address, u32 jump_address, u32 num_instructions, CCodeLabel next_fragment )
{
	//DAEDALUS_ASSERT( exit_address != u32( ~0 ), "Invalid exit address" );
	DAEDALUS_ASSERT( !next_fragment.IsSet() || jump_address == 0, "Shouldn't be specifying a jump address if we have a next fragment?" );

#ifdef _DEBUG
	if(exit_address == u32(~0))
	{
		INT3();
	}
#endif

	if( (exit_address == mEntryAddress) & mLoopTop.IsSet() )
	{
		#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( mUseFixedRegisterAllocation, "Have mLoopTop but unfixed register allocation?" );
		#endif
		FlushAllFloatingPointRegisters( mRegisterCache, false );

		// Check if we're ok to continue, without flushing any registers
		GetVar( ArmReg_R0, &gCPUState.CPUControl[C0_COUNT]._u32 );
		GetVar( ArmReg_R1, (const u32*)&gCPUState.Events[0].mCount );

		//
		//	Pull in any registers which may have been flushed for whatever reason.
		//
		// Skip r0
		for( u32 i {1}; i < NUM_N64_REGS; i++ )
		{
			EN64Reg	n64_reg = EN64Reg( i );

			//if( mRegisterCache.IsDirty( n64_reg, 0 ) & mRegisterCache.IsKnownValue( n64_reg, 0 ) )
			//{
			//	FlushRegister( mRegisterCache, n64_reg, 0, false );
			//}
			//if( mRegisterCache.IsDirty( n64_reg, 1 ) & mRegisterCache.IsKnownValue( n64_reg, 1 ) )
			//{
			//	FlushRegister( mRegisterCache, n64_reg, 1, false );
			//}

			PrepareCachedRegister( n64_reg, 0 );
			PrepareCachedRegister( n64_reg, 1 );
		}

		// Assuming we don't need to set CurrentPC/Delay flags before we branch to the top..
		//
		ADD_IMM( ArmReg_R0, ArmReg_R0, num_instructions, ArmReg_R2 );
		SetVar( &gCPUState.CPUControl[C0_COUNT]._u32, ArmReg_R0 );

		//
		//	If the event counter is still positive, just jump directly to the top of our loop
		//
		SUB_IMM( ArmReg_R1, ArmReg_R1, s16(num_instructions), ArmReg_R2 );
		SetVar( (u32*)&gCPUState.Events[0].mCount, ArmReg_R1 );
		CMP_IMM(ArmReg_R1, 0);
		BX_IMM( mLoopTop, GT );

		FlushAllRegisters( mRegisterCache, true );

		SetVar( &gCPUState.CurrentPC, exit_address );
		SetVar( &gCPUState.Delay, NO_DELAY );	// ASSUMES store is done in just a single op.
		
		CALL( CCodeLabel( reinterpret_cast< const void * >( CPU_HANDLE_COUNT_INTERRUPT ) ));

		RET();
		//
		//	Return an invalid jump location to indicate we've handled our own linking.
		//
		return CJumpLocation( nullptr );
	}
	
	
	FlushAllRegisters(mRegisterCache, true);
	
	MOV32(ArmReg_R0, num_instructions);
	MOV32(ArmReg_R1, exit_address);
	if (jump_address != 0)
	{
		MOV32(ArmReg_R2, jump_address);
		MOV32(ArmReg_R3, (u32)&_DirectExitCheckDelay);
	}
	else
	{
		MOV32(ArmReg_R3, (u32)&_DirectExitCheckNoDelay);
	}
	
	BLX(ArmReg_R3);
	
// If the flag was set, we need in initialise the pc/delay to exit with
	CJumpLocation jump_to_next_fragment = BX_IMM( CCodeLabel { nullptr } );
	
	CCodeLabel interpret_next_fragment( GetAssemblyBuffer()->GetLabel() );
	// No need to call CPU_SetPC(), as this is handled by CFragment when we exit
	RET();

	// Patch up the exit jump
	if( !next_fragment.IsSet() )
	{
		PatchJumpLong( jump_to_next_fragment, interpret_next_fragment );
	}

	return jump_to_next_fragment;
}

//*****************************************************************************
// Handle branching back to the interpreter after an ERET
//*****************************************************************************
void CCodeGeneratorARM::GenerateEretExitCode( u32 num_instructions, CIndirectExitMap * p_map )
{
	FlushAllRegisters(mRegisterCache, true);
	
	MOV32(ArmReg_R0, num_instructions);
	MOV32(ArmReg_R1, reinterpret_cast<u32>(p_map));
	LDR(ArmReg_R2, ArmReg_R12, offsetof(SCPUState, CurrentPC));
	// Eret is a bit bodged so we exit at PC + 4
	ADD_IMM(ArmReg_R2, ArmReg_R2, 4);
	
	MOV32(ArmReg_R4, (u32)&_IndirectExitCheck);
	BLX(ArmReg_R4);
}

//*****************************************************************************
// Handle branching back to the interpreter after an indirect jump
//*****************************************************************************
void CCodeGeneratorARM::GenerateIndirectExitCode( u32 num_instructions, CIndirectExitMap * p_map )
{
	FlushAllRegisters(mRegisterCache, true);

	MOV32(ArmReg_R0, num_instructions);
	MOV32(ArmReg_R1, reinterpret_cast<u32>(p_map));
	LDR(ArmReg_R2, ArmReg_R12, offsetof(SCPUState, TargetPC));
	
	MOV32(ArmReg_R4, (u32)&_IndirectExitCheck);
	BLX(ArmReg_R4);
}

//*****************************************************************************
//
//*****************************************************************************
void CCodeGeneratorARM::GenerateExceptionHander( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps, const std::vector< RegisterSnapshotHandle>& exception_handler_snapshots )
{
	CCodeLabel exception_handler( GetAssemblyBuffer()->GetLabel() );

	SetBufferB();
	MOV32(ArmReg_R0, (u32)p_exception_handler_fn );
	BLX(ArmReg_R0);

	RET();

	for (int i = 0; i < exception_handler_jumps.size(); i++)
	{
		CJumpLocation	jump(exception_handler_jumps[i]);
		InsertLiteralPool(false);
		PatchJumpLong(jump, GetAssemblyBuffer()->GetLabel());

		CN64RegisterCacheARM cache = GetRegisterCacheFromHandle(exception_handler_snapshots[i]);
		FlushAllRegisters(cache, true);

		// jump to the handler
		GenerateBranchAlways(exception_handler);
	}
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorARM::GenerateBranchAlways( CCodeLabel target )
{
	CJumpLocation jumpLocation = BX_IMM(target);
	return jumpLocation;
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorARM::GenerateBranchIfSet( const u32 * p_var, CCodeLabel target )
{
	s32 diff = (u8*)p_var - (u8*)&gCPUState;
	
	// if this is in range of CPUState, just load it
	if (diff <= 0xFFF && diff >= -0xFFF)
	{
		LDR(ArmReg_R0, ArmReg_R12, diff);
	}
	else
	{
		MOV32(ArmReg_R1, (u32)p_var);
		LDR(ArmReg_R0, ArmReg_R1, 0);
	}
	
	CMP_IMM(ArmReg_R0, 0);

	return BX_IMM(target, NE);
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorARM::GenerateBranchIfNotSet( const u32 * p_var, CCodeLabel target )
{
	s32 diff = (u8*)p_var - (u8*)&gCPUState;
	
	// if this is in range of CPUState, just load it
	if (diff <= 0xFFF && diff >= -0xFFF)
	{
		LDR(ArmReg_R0, ArmReg_R12, diff);
	}
	else
	{
		MOV32(ArmReg_R1, (u32)p_var);
		LDR(ArmReg_R0, ArmReg_R1, 0);
	}
	CMP_IMM(ArmReg_R0, 0);

	return BX_IMM(target, EQ);
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorARM::GenerateBranchIfEqual( const u32 * p_var, u32 value, CCodeLabel target )
{
	s32 diff = (u8*)p_var - (u8*)&gCPUState;
	
	// if this is in range of CPUState, just load it
	if (diff <= 0xFFF && diff >= -0xFFF)
	{
		LDR(ArmReg_R0, ArmReg_R12, diff);
	}
	else
	{
		MOV32(ArmReg_R1, (u32)p_var);
		LDR(ArmReg_R0, ArmReg_R1, 0);
	}
	MOV32(ArmReg_R1, value);

	CMP(ArmReg_R0, ArmReg_R1);

	return BX_IMM(target, EQ);
}

CJumpLocation	CCodeGeneratorARM::GenerateBranchIfNotEqual( const u32 * p_var, u32 value, CCodeLabel target )
{
	s32 diff = (u8*)p_var - (u8*)&gCPUState;
	
	// if this is in range of CPUState, just load it
	if (diff <= 0xFFF && diff >= -0xFFF)
	{
		LDR(ArmReg_R0, ArmReg_R12, diff);
	}
	else
	{
		MOV32(ArmReg_R1, (u32)p_var);
		LDR(ArmReg_R0, ArmReg_R1, 0);
	}
	MOV32(ArmReg_R1, value);

	CMP(ArmReg_R0, ArmReg_R1);

	return BX_IMM(target, NE);
}

//*****************************************************************************
//	Cached Interpreter GenerateOpCode variant
//*****************************************************************************
CJumpLocation	CCodeGeneratorARM::GenerateInterpOpCode( const STraceEntry& ti, bool branch_delay_slot, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump)
{
	u32 address = ti.Address;
	bool exception = false;
	OpCode op_code = ti.OpCode;

	if (op_code._u32 == 0)
	{
		if( branch_delay_slot )
		{
			SetVar( &gCPUState.Delay, NO_DELAY );
		}
		return CJumpLocation( NULL);
	}

	if( branch_delay_slot )
	{
		SetVar( &gCPUState.Delay, EXEC_DELAY );
	}

	const EN64Reg	rs = EN64Reg( op_code.rs );
	const EN64Reg	rt = EN64Reg( op_code.rt );
	const EN64Reg	rd = EN64Reg( op_code.rd );
	const u32		sa = op_code.sa;
	const EN64Reg	base = EN64Reg( op_code.base );
	//const u32		jump_target( (address&0xF0000000) | (op_code.target<<2) );
	//const u32		branch_target( address + ( ((s32)(s16)op_code.immediate)<<2 ) + 4);
	const u32		ft = op_code.ft;


	bool handled = false;

	switch(op_code.op)
	{
		case OP_J:		/* nothing to do */ handled = true; break;
		default: break;
	}

	CJumpLocation	exception_handler(NULL);

	if (!handled)
	{
		CCodeLabel	no_target( NULL );

		if( R4300_InstructionHandlerNeedsPC( op_code ) )
		{
			SetVar( &gCPUState.CurrentPC, address );
			exception = true;
		}

		GenerateGenericR4300( op_code, R4300_GetInstructionHandler( op_code ) );

		if( exception )
		{
			exception_handler = GenerateBranchIfSet( const_cast< u32 * >( &gCPUState.StuffToDo ), no_target );
		}

		// Check whether we want to invert the status of this branch
		if( p_branch != NULL )
		{
			//
			// Check if the branch has been taken
			//
			if( p_branch->Direct )
			{
				if( p_branch->ConditionalBranchTaken )
				{
					*p_branch_jump = GenerateBranchIfNotEqual( &gCPUState.Delay, DO_DELAY, no_target );
				}
				else
				{
					*p_branch_jump = GenerateBranchIfEqual( &gCPUState.Delay, DO_DELAY, no_target );
				}
			}
			else
			{
				// XXXX eventually just exit here, and skip default exit code below
				if( p_branch->Eret )
				{
					*p_branch_jump = GenerateBranchAlways( no_target );
				}
				else
				{
					*p_branch_jump = GenerateBranchIfNotEqual( &gCPUState.TargetPC, p_branch->TargetAddress, no_target );
				}
			}
		}
		else
		{
			if( branch_delay_slot )
			{
				SetVar( &gCPUState.Delay, NO_DELAY );
			}
		}
	}
	else
	{
		if(exception)
		{
			exception_handler = GenerateBranchIfSet( const_cast< u32 * >( &gCPUState.StuffToDo ), CCodeLabel(NULL) );
		}

		if( p_branch && branch_delay_slot )
		{
			SetVar( &gCPUState.Delay, NO_DELAY );
		}
	}


	return exception_handler;
}

//*****************************************************************************
//	Generates instruction handler for the specified op code.
//	Returns a jump location if an exception handler is required
//*****************************************************************************

CJumpLocation	CCodeGeneratorARM::GenerateOpCode( const STraceEntry& ti, bool branch_delay_slot, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump)
{
	u32 address = ti.Address;
	bool exception = false;
	OpCode op_code = ti.OpCode;

	CJumpLocation	exception_handler(NULL);

	if (op_code._u32 == 0)
	{
		if( branch_delay_slot )
		{
			SetVar(&gCPUState.Delay, NO_DELAY);
		}
		return CJumpLocation( NULL);
	}

	if( branch_delay_slot )
	{
		SetVar(&gCPUState.Delay, EXEC_DELAY);
	}

	mQuickLoad = ti.Usage.Access8000;

	const EN64Reg	rs = EN64Reg( op_code.rs );
	const EN64Reg	rt = EN64Reg( op_code.rt );
	const EN64Reg	rd = EN64Reg( op_code.rd );
	const u32		sa = op_code.sa;
	const EN64Reg	base = EN64Reg( op_code.base );
	//const u32		jump_target( (address&0xF0000000) | (op_code.target<<2) );
	//const u32		branch_target( address + ( ((s32)(s16)op_code.immediate)<<2 ) + 4);
	const u32		ft = op_code.ft;


	bool handled = false;

	switch(op_code.op)
	{
		case OP_J:			/* nothing to do */		handled = true; break;

		case OP_JAL: 	GenerateJAL( address ); handled = true; break;

		case OP_BEQ:	GenerateBEQ( rs, rt, p_branch, p_branch_jump ); handled = true; break;
		case OP_BEQL:	GenerateBEQ( rs, rt, p_branch, p_branch_jump ); handled = true; break;
		case OP_BNE:	GenerateBNE( rs, rt, p_branch, p_branch_jump );	handled = true; break;
		case OP_BNEL:	GenerateBNE( rs, rt, p_branch, p_branch_jump );	handled = true; break;
		case OP_BLEZ:	GenerateBLEZ( rs, p_branch, p_branch_jump ); handled = true; break;
		case OP_BLEZL:	GenerateBLEZ( rs, p_branch, p_branch_jump ); handled = true; break;
		case OP_BGTZ:	GenerateBGTZ( rs, p_branch, p_branch_jump ); handled = true; break;
		case OP_BGTZL:	GenerateBGTZ( rs, p_branch, p_branch_jump ); handled = true; break;

		case OP_ADDI:	GenerateADDIU(rt, rs, s16(op_code.immediate)); handled = true; break;
		case OP_ADDIU:	GenerateADDIU(rt, rs, s16(op_code.immediate)); handled = true; break;
		case OP_ANDI:	GenerateANDI( rt, rs, op_code.immediate ); handled = true; break;
		case OP_ORI:	GenerateORI( rt, rs, op_code.immediate ); handled = true; break;
		case OP_XORI:	GenerateXORI( rt, rs, op_code.immediate );handled = true; break;

		case OP_DADDI:	GenerateDADDIU( rt, rs, s16( op_code.immediate ) );	handled = true; break;
		case OP_DADDIU:	GenerateDADDIU( rt, rs, s16( op_code.immediate ) );	handled = true; break;

		case OP_SW:		handled = GenerateSW(address, branch_delay_slot, rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_SH:		handled = GenerateSH(address, branch_delay_slot, rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_SB:		handled = GenerateSB(address, branch_delay_slot,rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_SD:		handled = GenerateSD(address, branch_delay_slot,rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_SWC1:	if (gDynarecWordsOptimisation) { handled = GenerateSWC1(address, branch_delay_slot,ft, base, s16(op_code.immediate)); exception = !handled; } break;
		case OP_SDC1:	if (gDynarecWordsOptimisation) { handled = GenerateSDC1(address, branch_delay_slot,ft, base, s16(op_code.immediate)); exception = !handled; } break;

		case OP_SLTIU: 	GenerateSLTI( rt, rs, s16( op_code.immediate ), true );  handled = true; break;
		case OP_SLTI:	GenerateSLTI( rt, rs, s16( op_code.immediate ), false ); handled = true; break;

		case OP_LW:		handled = GenerateLW(address, branch_delay_slot,rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_LH:		handled = GenerateLH(address, branch_delay_slot,rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_LHU: 	handled = GenerateLHU(address, branch_delay_slot,rt, base, s16(op_code.immediate));  exception = !handled; break;
		case OP_LB: 	handled = GenerateLB(address, branch_delay_slot,rt, base, s16(op_code.immediate));   exception = !handled; break;
		case OP_LBU:	handled = GenerateLBU(address, branch_delay_slot,rt, base, s16(op_code.immediate));  exception = !handled; break;
		case OP_LD:		handled = GenerateLD(address, branch_delay_slot, rt, base, s16(op_code.immediate));  exception = !handled; break;
		case OP_LWC1:	if (gDynarecWordsOptimisation) { handled = GenerateLWC1(address, branch_delay_slot,ft, base, s16(op_code.immediate)); exception = !handled; } break;
		case OP_LDC1:	if (gDynarecWordsOptimisation) { handled = GenerateLDC1(address, branch_delay_slot,ft, base, s16(op_code.immediate)); exception = !handled; } break;

		case OP_LUI:	GenerateLUI( rt, s16( op_code.immediate ) ); handled = true; break;

		case OP_CACHE:	handled = GenerateCACHE( base, op_code.immediate, rt ); exception = !handled; break;

		case OP_REGIMM:
			switch( op_code.regimm_op )
			{
				// These can be handled by the same Generate function, as the 'likely' bit is handled elsewhere
				case RegImmOp_BLTZ:
				case RegImmOp_BLTZL: GenerateBLTZ( rs, p_branch, p_branch_jump ); handled = true; break;

				case RegImmOp_BGEZ:
				case RegImmOp_BGEZL: GenerateBGEZ( rs, p_branch, p_branch_jump ); handled = true; break;
			}
			break;

		case OP_SPECOP:
			switch( op_code.spec_op )
			{
				case SpecOp_SLL: 	GenerateSLL( rd, rt, sa ); handled = true; break;
				case SpecOp_SRA: 	GenerateSRA( rd, rt, sa ); handled = true; break;
				//Causes Rayman 2's menu to stop working
				//case SpecOp_SRL: 	GenerateSRL( rd, rt, sa ); handled = true; break;
				case SpecOp_SLLV:	GenerateSLLV( rd, rs, rt );	handled = true; break;
				case SpecOp_SRLV:	GenerateSRLV( rd, rs, rt );	handled = true; break;
				case SpecOp_SRAV:	GenerateSRAV( rd, rs, rt );	handled = true; break;

				case SpecOp_OR:		GenerateOR( rd, rs, rt ); handled = true; break;
				case SpecOp_AND:	GenerateAND( rd, rs, rt ); handled = true; break;
				case SpecOp_XOR:	GenerateXOR( rd, rs, rt ); handled = true; break;
				case SpecOp_NOR:	GenerateNOR( rd, rs, rt );	handled = true; break;

				case SpecOp_ADD:	GenerateADDU( rd, rs, rt );	handled = true; break;
				case SpecOp_ADDU:	GenerateADDU( rd, rs, rt );	handled = true; break;

				case SpecOp_SUB:	GenerateSUBU( rd, rs, rt );	handled = true; break;
				case SpecOp_SUBU:	GenerateSUBU( rd, rs, rt );	handled = true; break;

				case SpecOp_MULTU:	GenerateMULT( rs, rt, true );	handled = true; break;
				case SpecOp_MULT:	GenerateMULT( rs, rt, false );	handled = true; break;

				case SpecOp_MFLO:	GenerateMFLO( rd );			handled = true; break;
				case SpecOp_MFHI:	GenerateMFHI( rd );			handled = true; break;
				case SpecOp_MTLO:	GenerateMTLO( rd );			handled = true; break;
				case SpecOp_MTHI:	GenerateMTHI( rd );			handled = true; break;

				case SpecOp_DADD:	GenerateDADDU( rd, rs, rt );	handled = true; break;
				case SpecOp_DADDU:	GenerateDADDU( rd, rs, rt );	handled = true; break;

				case SpecOp_DSUB:	GenerateDSUBU( rd, rs, rt );	handled = true; break;
				case SpecOp_DSUBU:	GenerateDSUBU( rd, rs, rt );	handled = true; break;

				case SpecOp_SLT:	GenerateSLT( rd, rs, rt, false );	handled = true; break;
				case SpecOp_SLTU:	GenerateSLT( rd, rs, rt, true );	handled = true; break;
				case SpecOp_JR:		GenerateJR( rs, p_branch, p_branch_jump );	handled = true; break;
				case SpecOp_JALR:	GenerateJALR( rs, rd, address, p_branch, p_branch_jump );	handled = true; break;

				default: break;
			}
			break;
		case OP_COPRO0:
			switch( op_code.cop0_op )
			{
			case Cop0Op_MFC0:	GenerateMFC0( rt, op_code.fs ); handled = true; break;
			default:
				break;
			}
		break;
		case OP_COPRO1:
			switch( op_code.cop1_op )
			{
				case Cop1Op_MFC1:	GenerateMFC1( rt, op_code.fs ); handled = true; break;
				case Cop1Op_MTC1:	GenerateMTC1( op_code.fs, rt ); handled = true; break;

				case Cop1Op_CFC1:	GenerateCFC1( rt, op_code.fs ); handled = true; break;
				case Cop1Op_CTC1:	GenerateCTC1( op_code.fs, rt ); handled = true; break;

				case Cop1Op_DInstr:
					switch( op_code.cop1_funct )
					{
						case Cop1OpFunc_ADD:	GenerateADD_D( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_SUB:	GenerateSUB_D( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_MUL:	GenerateMUL_D( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_DIV:	GenerateDIV_D( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						
						case Cop1OpFunc_SQRT:		GenerateSQRT_D( op_code.fd, op_code.fs ); handled = true; break;
						case Cop1OpFunc_ABS:		GenerateABS_D( op_code.fd, op_code.fs ); handled = true; break;
						case Cop1OpFunc_MOV:		GenerateMOV_D( op_code.fd, op_code.fs ); handled = true; break;
						case Cop1OpFunc_NEG:		GenerateNEG_D( op_code.fd, op_code.fs ); handled = true; break;
						
						case Cop1OpFunc_TRUNC_W:	GenerateTRUNC_W_D( op_code.fd, op_code.fs ); handled = true; break;
						case Cop1OpFunc_CVT_S:		GenerateCVT_S_D( op_code.fd, op_code.fs ); handled = true; break;
						
						case Cop1OpFunc_CMP_F:		GenerateCMP_D( op_code.fs, op_code.ft, NV, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_UN:		GenerateCMP_D( op_code.fs, op_code.ft, NV, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_EQ:		GenerateCMP_D( op_code.fs, op_code.ft, EQ, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_UEQ:	GenerateCMP_D( op_code.fs, op_code.ft, EQ, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_OLT:	GenerateCMP_D( op_code.fs, op_code.ft, MI, 1 ); handled = true; break;
						case Cop1OpFunc_CMP_ULT:	GenerateCMP_D( op_code.fs, op_code.ft, MI, 1 ); handled = true; break;
						case Cop1OpFunc_CMP_OLE:	GenerateCMP_D( op_code.fs, op_code.ft, LS, 1 ); handled = true; break;
						case Cop1OpFunc_CMP_ULE:	GenerateCMP_D( op_code.fs, op_code.ft, LS, 1 ); handled = true; break;

						case Cop1OpFunc_CMP_SF:		GenerateCMP_D( op_code.fs, op_code.ft, NV, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_NGLE:	GenerateCMP_D( op_code.fs, op_code.ft, NV, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_SEQ:	GenerateCMP_D( op_code.fs, op_code.ft, EQ, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_NGL:	GenerateCMP_D( op_code.fs, op_code.ft, EQ, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_LT:		GenerateCMP_D( op_code.fs, op_code.ft, MI, 1 ); handled = true; break;
						case Cop1OpFunc_CMP_NGE:	GenerateCMP_D( op_code.fs, op_code.ft, MI, 1 ); handled = true; break;
						case Cop1OpFunc_CMP_LE:		GenerateCMP_D( op_code.fs, op_code.ft, LS, 1 ); handled = true; break;
						case Cop1OpFunc_CMP_NGT:	GenerateCMP_D( op_code.fs, op_code.ft, LS, 1 ); handled = true; break;
					}
					break;

				case Cop1Op_SInstr:
					switch( op_code.cop1_funct )
					{
						case Cop1OpFunc_ADD:	GenerateADD_S( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_SUB:	GenerateSUB_S( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_MUL:	GenerateMUL_S( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_DIV:	GenerateDIV_S( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
						case Cop1OpFunc_SQRT:	GenerateSQRT_S( op_code.fd, op_code.fs ); handled = true; break;
						case Cop1OpFunc_ABS:		GenerateABS_S( op_code.fd, op_code.fs ); handled = true; break;
						case Cop1OpFunc_MOV:		GenerateMOV_S( op_code.fd, op_code.fs ); handled = true; break;
						case Cop1OpFunc_NEG:		GenerateNEG_S( op_code.fd, op_code.fs ); handled = true; break;
						
						case Cop1OpFunc_TRUNC_W:	GenerateTRUNC_W_S( op_code.fd, op_code.fs ); handled = true; break;
						case Cop1OpFunc_CVT_D:		GenerateCVT_D_S( op_code.fd, op_code.fs ); handled = true; break;
						
						case Cop1OpFunc_CMP_F:		GenerateCMP_S( op_code.fs, op_code.ft, NV, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_UN:		GenerateCMP_S( op_code.fs, op_code.ft, NV, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_EQ:		if (!g_ROM.DISABLE_DYNA_CMP) { GenerateCMP_S( op_code.fs, op_code.ft, EQ, 0 ); handled = true; } break;
						case Cop1OpFunc_CMP_UEQ:	GenerateCMP_S( op_code.fs, op_code.ft, EQ, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_OLT:	GenerateCMP_S( op_code.fs, op_code.ft, MI, 1 ); handled = true; break;
						case Cop1OpFunc_CMP_ULT:	GenerateCMP_S( op_code.fs, op_code.ft, MI, 1 ); handled = true; break;
						case Cop1OpFunc_CMP_OLE:	GenerateCMP_S( op_code.fs, op_code.ft, LS, 1 ); handled = true; break;
						case Cop1OpFunc_CMP_ULE:	GenerateCMP_S( op_code.fs, op_code.ft, LS, 1 ); handled = true; break;

						case Cop1OpFunc_CMP_SF:		GenerateCMP_S( op_code.fs, op_code.ft, NV, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_NGLE:	GenerateCMP_S( op_code.fs, op_code.ft, NV, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_SEQ:	GenerateCMP_S( op_code.fs, op_code.ft, EQ, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_NGL:	GenerateCMP_S( op_code.fs, op_code.ft, EQ, 0 ); handled = true; break;
						case Cop1OpFunc_CMP_LT:		GenerateCMP_S( op_code.fs, op_code.ft, MI, 1 ); handled = true; break;
						case Cop1OpFunc_CMP_NGE:	GenerateCMP_S( op_code.fs, op_code.ft, MI, 1 ); handled = true; break;
						case Cop1OpFunc_CMP_LE:		GenerateCMP_S( op_code.fs, op_code.ft, LS, 1 ); handled = true; break;
						case Cop1OpFunc_CMP_NGT:	GenerateCMP_S( op_code.fs, op_code.ft, LS, 1 ); handled = true; break;
					}
					break;

				default: break;
			}
			break;

		default: break;
	}

	if (!handled)
	{
		CCodeLabel	no_target( NULL );

		if( R4300_InstructionHandlerNeedsPC( op_code ) )
		{
			SetVar(&gCPUState.CurrentPC, address);
			exception = true;
		}

		GenerateGenericR4300( op_code, R4300_GetInstructionHandler( op_code ) );

		if( exception )
		{
			exception_handler = GenerateBranchIfSet( const_cast< u32 * >( &gCPUState.StuffToDo ), no_target );
		}

		// Check whether we want to invert the status of this branch
		if( p_branch != NULL )
		{
			//
			// Check if the branch has been taken
			//
			if( p_branch->Direct )
			{
				if( p_branch->ConditionalBranchTaken )
				{
					*p_branch_jump = GenerateBranchIfNotEqual( &gCPUState.Delay, DO_DELAY, no_target );
				}
				else
				{
					*p_branch_jump = GenerateBranchIfEqual( &gCPUState.Delay, DO_DELAY, no_target );
				}
			}
			else
			{
				// XXXX eventually just exit here, and skip default exit code below
				if( p_branch->Eret )
				{
					*p_branch_jump = GenerateBranchAlways( no_target );
				}
				else
				{
					*p_branch_jump = GenerateBranchIfNotEqual( &gCPUState.TargetPC, p_branch->TargetAddress, no_target );
				}
			}
		}
		else
		{
			if( branch_delay_slot )
			{
				SetVar( &gCPUState.Delay, NO_DELAY );
			}
		}
	}
	else
	{
		if(exception)
		{
			exception_handler = GenerateBranchIfSet( const_cast< u32 * >( &gCPUState.StuffToDo ), CCodeLabel(NULL) );
		}

		if( p_branch && branch_delay_slot )
		{
			SetVar( &gCPUState.Delay, NO_DELAY );
		}
	}


	return exception_handler;
}

void	CCodeGeneratorARM::GenerateBranchHandler( CJumpLocation branch_handler_jump, RegisterSnapshotHandle snapshot )
{
	InsertLiteralPool(false);
	PatchJumpLong( branch_handler_jump, GetAssemblyBuffer()->GetLabel() );
	mRegisterCache = GetRegisterCacheFromHandle(snapshot);
}

void	CCodeGeneratorARM::GenerateGenericR4300( OpCode op_code, CPU_Instruction p_instruction )
{
	// XXXX Flush all fp registers before a generic call
	FlushAllRegisters(mRegisterCache, true);
	// Call function - __fastcall
	MOV32(ArmReg_R0, op_code._u32);
	CALL( CCodeLabel( (void*)p_instruction ) );
}

CJumpLocation CCodeGeneratorARM::ExecuteNativeFunction( CCodeLabel speed_hack, bool check_return )
{
	FlushAllRegisters(mRegisterCache, true);
	CALL( speed_hack );
	 
	if( check_return )
	{
		TST( ArmReg_R0, ArmReg_R0 );
		return BX_IMM( CCodeLabel(NULL), EQ );
	}
	else
	{
		return CJumpLocation(NULL);
	}
}

//*****************************************************************************
//
//	Load Instructions
//
//*****************************************************************************

//Helper function, loads into given register
inline void CCodeGeneratorARM::GenerateLoad( u32 current_pc, EArmReg arm_dest, EN64Reg base, s16 offset, u8 twiddle, u8 bits, bool is_signed, ReadMemoryFunction p_read_memory )
{
	if ((gDynarecStackOptimisation && base == N64Reg_SP) || (gMemoryAccessOptimisation && mQuickLoad))
	{
		EArmReg reg_base = GetRegisterAndLoadLo(base, ArmReg_R0);
		EArmReg load_reg = reg_base;
		if (offset != 0)
		{
			ADD_IMM(ArmReg_R0, reg_base, offset, ArmReg_R1);
			load_reg = ArmReg_R0;
		}
		
		if (twiddle != 0 )
		{
			XOR_IMM(ArmReg_R0, load_reg, twiddle);
			load_reg = ArmReg_R0;
		}

		switch(bits)
		{
			case 32:	LDR_REG(arm_dest, load_reg, gMemoryBaseReg); break;

			case 16:	if(is_signed)	{ LDRSH_REG(arm_dest, load_reg, gMemoryBaseReg); }
						else			{ LDRH_REG(arm_dest, load_reg, gMemoryBaseReg); } break;

			case 8:		if(is_signed)	{ LDRSB_REG(arm_dest, load_reg, gMemoryBaseReg); }
						else			{ LDRB_REG(arm_dest, load_reg, gMemoryBaseReg); } break;
		}
	}
	else
	{	

		EArmReg reg_base = GetRegisterAndLoadLo(base, ArmReg_R0);
		EArmReg load_reg = reg_base;
		if (offset != 0)
		{
			ADD_IMM(ArmReg_R0, reg_base, offset, ArmReg_R1);
			load_reg = ArmReg_R0;
		}
		
		if (twiddle != 0 )
		{
			XOR_IMM(ArmReg_R0, load_reg, twiddle);
			load_reg = ArmReg_R0;
		}
		CMP(load_reg, gMemUpperBoundReg);
		CJumpLocation loc = BX_IMM(CCodeLabel { NULL }, GE );
		switch(bits)
		{
			case 32:	LDR_REG(arm_dest, load_reg, gMemoryBaseReg); break;

			case 16:	if(is_signed)	{ LDRSH_REG(arm_dest, load_reg, gMemoryBaseReg); }
						else			{ LDRH_REG(arm_dest, load_reg, gMemoryBaseReg); } break;

			case 8:		if(is_signed)	{ LDRSB_REG(arm_dest, load_reg, gMemoryBaseReg); }
						else			{ LDRB_REG(arm_dest, load_reg, gMemoryBaseReg); } break;
		}

		CJumpLocation skip;
		CCodeLabel current; 
		
		if (IsBufferB())
		{
			skip = GenerateBranchAlways( CCodeLabel { NULL });
		}
		else
		{
			current = GetAssemblyBuffer()->GetLabel();
			SetBufferB();
		}
		

		PatchJumpLong( loc, GetAssemblyBuffer()->GetLabel() );

		CN64RegisterCacheARM current_regs(mRegisterCache);
		FlushAllRegisters(mRegisterCache, true);

		if (load_reg != ArmReg_R0)
		{
			MOV(ArmReg_R0, load_reg);
		}
		MOV32(ArmReg_R1, current_pc);
		MOV32(ArmReg_R4, (u32)p_read_memory);
		BLX( ArmReg_R4 );

		// Restore all registers BEFORE copying back final value
		RestoreAllRegisters(mRegisterCache, current_regs);
		if (arm_dest != ArmReg_R0)
		{
			MOV(arm_dest, ArmReg_R0);
		}
		mRegisterCache = current_regs;

		
		if (current.IsSet())
		{
			// return back to our original buffer
			GenerateBranchAlways(current);
			SetBufferA();
		}
		else
		{	
			 // patch the unconditional branch to skip the slow path
			PatchJumpLong(skip, GetAssemblyBuffer()->GetLabel());
		}
	}
}

//Load Word
bool CCodeGeneratorARM::GenerateLW( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	EArmReg arm_dest = GetRegisterNoLoadLo(rt, ArmReg_R0);
	GenerateLoad( address, arm_dest, base, offset, 0, 32, false, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );
	UpdateRegister(rt, arm_dest, URO_HI_SIGN_EXTEND);
	return true;
}

//Load Double Word
bool CCodeGeneratorARM::GenerateLD( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	EArmReg regt_hi= GetRegisterNoLoadHi(rt, ArmReg_R0);
	GenerateLoad( address, regt_hi, base, offset, 0, 32, false, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );
	StoreRegisterHi(rt, regt_hi);

	EArmReg regt_lo = GetRegisterNoLoadLo(rt, ArmReg_R0);
	GenerateLoad( address, regt_lo, base, offset + 4, 0, 32, false, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );
	StoreRegisterLo(rt, regt_lo);

	return true;
}

//Load half word signed
bool CCodeGeneratorARM::GenerateLH( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	EArmReg arm_dest = GetRegisterNoLoadLo(rt, ArmReg_R0);
	GenerateLoad( address, arm_dest, base, offset, U16_TWIDDLE, 16, true, set_branch_delay ? ReadBitsDirectBD_s16 : ReadBitsDirect_s16 );
	UpdateRegister(rt, arm_dest, URO_HI_SIGN_EXTEND);

	return true;
}

//Load half word unsigned
bool CCodeGeneratorARM::GenerateLHU( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	EArmReg arm_dest = GetRegisterNoLoadLo(rt, ArmReg_R0);
	GenerateLoad(address, arm_dest, base, offset, U16_TWIDDLE, 16, false, set_branch_delay ? ReadBitsDirectBD_u16 : ReadBitsDirect_u16 );
	UpdateRegister(rt, arm_dest, URO_HI_CLEAR);

	return true;
}

//Load byte signed
bool CCodeGeneratorARM::GenerateLB( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	EArmReg arm_dest = GetRegisterNoLoadLo(rt, ArmReg_R0);
	GenerateLoad( address, arm_dest, base, offset, U8_TWIDDLE, 8, true, set_branch_delay ? ReadBitsDirectBD_s8 : ReadBitsDirect_s8 );
	UpdateRegister(rt, arm_dest, URO_HI_SIGN_EXTEND);

	return true;
}

//Load byte unsigned
bool CCodeGeneratorARM::GenerateLBU( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	EArmReg arm_dest = GetRegisterNoLoadLo(rt, ArmReg_R0);
	GenerateLoad( address, arm_dest, base, offset, U8_TWIDDLE, 8, false, set_branch_delay ? ReadBitsDirectBD_u8 : ReadBitsDirect_u8 );
	UpdateRegister(rt, arm_dest, URO_HI_CLEAR);

	return true;
}

bool CCodeGeneratorARM::GenerateLWC1( u32 address, bool set_branch_delay, u32 ft, EN64Reg base, s16 offset )
{
	EArmVfpReg arm_ft = EArmVfpReg(ft);
	GenerateLoad( address, ArmReg_R0, base, offset, 0, 32, false, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );
	
	VMOV_S(arm_ft, ArmReg_R0);
	
	UpdateFloatRegister(EN64FloatReg(ft));
	return true;
}

bool CCodeGeneratorARM::GenerateLDC1( u32 address, bool set_branch_delay, u32 ft, EN64Reg base, s16 offset )
{
	EArmVfpReg arm_ft = EArmVfpReg(ft+1);
	GenerateLoad(address, ArmReg_R0, base, offset, 0, 32, false, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );
	VMOV_S(arm_ft, ArmReg_R0);
	UpdateFloatRegister(EN64FloatReg(ft+1));

	arm_ft = EArmVfpReg(ft);
	GenerateLoad( address, ArmReg_R0, base, offset + 4, 0, 32, false, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );
	VMOV_S(arm_ft, ArmReg_R0);
	UpdateFloatRegister(EN64FloatReg(ft));
	return true;
}

//Load Upper Immediate
void CCodeGeneratorARM::GenerateLUI( EN64Reg rt, s16 immediate )
{
	SetRegister32s(rt, s32(immediate) << 16);
}

//*****************************************************************************
//
//	Store Instructions
//
//*****************************************************************************

//Helper function, stores register R1 into memory
inline void CCodeGeneratorARM::GenerateStore(u32 address, EArmReg arm_src, EN64Reg base, s16 offset, u8 twiddle, u8 bits, WriteMemoryFunction p_write_memory )
{
	if ((gDynarecStackOptimisation && base == N64Reg_SP) || (gMemoryAccessOptimisation && mQuickLoad))
	{
		EArmReg reg_base = GetRegisterAndLoadLo(base, ArmReg_R0);
		EArmReg store_reg = reg_base;

		if (offset != 0)
		{
			ADD_IMM(ArmReg_R0, store_reg, offset, ArmReg_R2);
			store_reg = ArmReg_R0;
		}
		
		if (twiddle != 0)
		{
			XOR_IMM(ArmReg_R0, store_reg, twiddle);
			store_reg = ArmReg_R0;
		}
		
		switch(bits)
		{
			case 32:	STR_REG(arm_src, store_reg, gMemoryBaseReg); break;
			case 16:	STRH_REG(arm_src, store_reg, gMemoryBaseReg); break; 
			case 8:		STRB_REG(arm_src, store_reg, gMemoryBaseReg); break; 
		}
	}
	else
	{	
		//Slow Store
		EArmReg reg_base = GetRegisterAndLoadLo(base, ArmReg_R0);
		EArmReg store_reg = reg_base;

		if (offset != 0)
		{
			ADD_IMM(ArmReg_R0, store_reg, offset, ArmReg_R2);
			store_reg = ArmReg_R0;
		}
		
		if (twiddle != 0)
		{
			XOR_IMM(ArmReg_R0, store_reg, twiddle);
			store_reg = ArmReg_R0;
		}
		
		CMP(store_reg, gMemUpperBoundReg);
		CJumpLocation loc = BX_IMM(CCodeLabel { NULL }, GE );
		switch(bits)
		{
			case 32:	STR_REG(arm_src, store_reg, gMemoryBaseReg); break;
			case 16:	STRH_REG(arm_src, store_reg, gMemoryBaseReg); break; 
			case 8:		STRB_REG(arm_src, store_reg, gMemoryBaseReg); break; 
		}

		CJumpLocation skip;
		CCodeLabel current; 
		
		if (IsBufferB())
		{
			skip = GenerateBranchAlways( CCodeLabel { NULL });
		}
		else
		{
			current = GetAssemblyBuffer()->GetLabel();
			SetBufferB();
		}
		PatchJumpLong( loc, GetAssemblyBuffer()->GetLabel() );
		
		
		if (store_reg != ArmReg_R0)
		{
			MOV(ArmReg_R0, store_reg);
		}

		if (arm_src != ArmReg_R1)
		{
			MOV(ArmReg_R1, arm_src);
		}

		MOV32(ArmReg_R2, address);
		CN64RegisterCacheARM current_regs(mRegisterCache);
		FlushAllRegisters(mRegisterCache, true);
		MOV32(ArmReg_R3, (u32)p_write_memory);
		BLX( ArmReg_R3  );
		// Restore all registers BEFORE copying back final value
		RestoreAllRegisters(mRegisterCache, current_regs);
		mRegisterCache = current_regs;
		if (current.IsSet())
		{
			// return back to our original buffer
			GenerateBranchAlways(current);
			SetBufferA();
		}
		else
		{	
			 // patch the unconditional branch to skip the slow path
			PatchJumpLong(skip, GetAssemblyBuffer()->GetLabel());
		}
	}
}

// Store Word From Copro 1
bool CCodeGeneratorARM::GenerateSWC1( u32 address, bool set_branch_delay, u32 ft, EN64Reg base, s16 offset )
{
	EArmVfpReg arm_ft = GetFloatRegisterAndLoad(EN64FloatReg(ft));
	VMOV_S(ArmReg_R1, arm_ft);
	GenerateStore( address, ArmReg_R1, base, offset, 0, 32, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );
	return true;
}

bool CCodeGeneratorARM::GenerateSDC1( u32 address, bool set_branch_delay, u32 ft, EN64Reg base, s16 offset )
{
	EArmVfpReg arm_ft = GetDoubleRegisterAndLoad(EN64FloatReg(ft));
	
	VMOV_H(ArmReg_R1, arm_ft);
	GenerateStore( address, ArmReg_R1, base, offset, 0, 32, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32);

	VMOV_L(ArmReg_R1, arm_ft);
	GenerateStore( address, ArmReg_R1, base, offset + 4, 0, 32, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );
	return true;
}

bool CCodeGeneratorARM::GenerateSW( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	EArmReg reg = GetRegisterAndLoadLo(rt, ArmReg_R1);
	GenerateStore( address, reg, base, offset, 0, 32, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );

	return true;
}

bool CCodeGeneratorARM::GenerateSD( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	EArmReg reg = GetRegisterAndLoadHi(rt, ArmReg_R1);
	GenerateStore( address, reg, base, offset, 0, 32, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );

	reg = GetRegisterAndLoadLo(rt, ArmReg_R1);
	GenerateStore( address, reg, base, offset + 4, 0, 32, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );

	return true;
}

bool CCodeGeneratorARM::GenerateSH( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	EArmReg reg = GetRegisterAndLoadLo(rt, ArmReg_R1);
	GenerateStore( address, reg, base, offset, U16_TWIDDLE, 16, set_branch_delay ? WriteBitsDirectBD_u16 : WriteBitsDirect_u16 );
	return true;
}

bool CCodeGeneratorARM::GenerateSB( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	EArmReg reg = GetRegisterAndLoadLo(rt, ArmReg_R1);
	GenerateStore( address, reg, base, offset, U8_TWIDDLE, 8, set_branch_delay ? WriteBitsDirectBD_u8 : WriteBitsDirect_u8 );
	return true;
}

//*****************************************************************************
//*****************************************************************************
//*****************************************************************************

bool CCodeGeneratorARM::GenerateCACHE( EN64Reg base, s16 offset, u32 cache_op )
{
	u32 dwCache = cache_op & 0x3;
	u32 dwAction = (cache_op >> 2) & 0x7;

	// For instruction cache invalidation, make sure we let the CPU know so the whole
	// dynarec system can be invalidated
	if(dwCache == 0 && (dwAction == 0 || dwAction == 4))
	{
		FlushAllRegisters(mRegisterCache, true);
		LDR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, CPU[0]._u32_0) + sizeof(SCPUState::CPU[0]) * base);
		MOV32(ArmReg_R1, offset);
		ADD(ArmReg_R0, ArmReg_R0, ArmReg_R1);
		MOV_IMM(ArmReg_R1, 0x20);
		CALL(CCodeLabel( (void*)CPU_InvalidateICacheRange ));

		return true;
	}

	return false;
}



void CCodeGeneratorARM::GenerateJAL( u32 address )
{
	SetRegister32s(N64Reg_RA, address + 8);
}

void CCodeGeneratorARM::GenerateJR( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	EArmReg reg = GetRegisterAndLoadLo(rs, ArmReg_R0);
	MOV32(ArmReg_R1, p_branch->TargetAddress);
	SetVar(&gCPUState.TargetPC, reg);
	CMP(reg, ArmReg_R1);
	*p_branch_jump = BX_IMM(CCodeLabel(nullptr), NE);
}

void CCodeGeneratorARM::GenerateJALR( EN64Reg rs, EN64Reg rd, u32 address, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	SetRegister32s(rd, address + 8);
	
	EArmReg reg = GetRegisterAndLoadLo(rs, ArmReg_R0);
	MOV32(ArmReg_R1, p_branch->TargetAddress);
	SetVar(&gCPUState.TargetPC, reg);
	CMP(reg, ArmReg_R1);
	*p_branch_jump = BX_IMM(CCodeLabel(nullptr), NE);
}

void CCodeGeneratorARM::GenerateADDIU( EN64Reg rt, EN64Reg rs, s16 immediate )
{
	if( rs == N64Reg_R0 )
	{
		SetRegister32s( rt, immediate );
	}
	else if(mRegisterCache.IsKnownValue( rs, 0 ))
	{
		s32		known_value( mRegisterCache.GetKnownValue( rs, 0 )._s32 + (s32)immediate );
		SetRegister32s( rt, known_value );
	}
	else
	{
		EArmReg reg = GetRegisterAndLoadLo(rs, ArmReg_R0);
		EArmReg dst = GetRegisterNoLoadLo(rt, ArmReg_R1);

		ADD_IMM(dst, reg, immediate, ArmReg_R1);

		UpdateRegister(rt, dst, URO_HI_SIGN_EXTEND);
	}
}

void CCodeGeneratorARM::GenerateDADDIU( EN64Reg rt, EN64Reg rs, s16 immediate )
{
	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);
	EArmReg regt = GetRegisterNoLoadLo(rt, ArmReg_R0);
	MOV32(ArmReg_R1, (u32)immediate);

	ADD(regt, regs, ArmReg_R1, AL, 1);
	StoreRegisterLo(rt, regt);
	
	EArmReg regs_hi = GetRegisterAndLoadHi(rs, ArmReg_R0);
	EArmReg regt_hi = GetRegisterNoLoadHi(rt, ArmReg_R0);
	ADC_IMM(regt_hi, regs_hi, (s64)immediate >> 32, ArmReg_R1);
	StoreRegisterHi(rt, ArmReg_R1);
}

void CCodeGeneratorARM::GenerateDADDU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	EArmReg reg_lo_d = GetRegisterNoLoadLo(rd, ArmReg_R0);
	EArmReg reg_lo_s = GetRegisterAndLoadLo(rs, ArmReg_R1);
	EArmReg reg_lo_t = GetRegisterAndLoadLo(rt, ArmReg_R0);

	ADD(reg_lo_d, reg_lo_s, reg_lo_t, AL, 1);

	StoreRegisterLo(rd, reg_lo_d);

	EArmReg reg_hi_d = GetRegisterNoLoadHi(rd, ArmReg_R0);
	EArmReg reg_hi_s = GetRegisterAndLoadHi(rs, ArmReg_R1);
	EArmReg reg_hi_t = GetRegisterAndLoadHi(rt, ArmReg_R0);
	ADC(reg_hi_d, reg_hi_s, reg_hi_t);

	StoreRegisterHi(rd, reg_hi_d);
}

void CCodeGeneratorARM::GenerateDSUBU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	EArmReg reg_lo_d = GetRegisterNoLoadLo(rd, ArmReg_R0);
	EArmReg reg_lo_s = GetRegisterAndLoadLo(rs, ArmReg_R1);
	EArmReg reg_lo_t = GetRegisterAndLoadLo(rt, ArmReg_R0);

	SUB(reg_lo_d, reg_lo_s, reg_lo_t, AL, 1);

	StoreRegisterLo(rd, reg_lo_d);

	EArmReg reg_hi_d = GetRegisterNoLoadHi(rd, ArmReg_R0);
	EArmReg reg_hi_s = GetRegisterAndLoadHi(rs, ArmReg_R1);
	EArmReg reg_hi_t = GetRegisterAndLoadHi(rt, ArmReg_R0);
	SBC(reg_hi_d, reg_hi_s, reg_hi_t);

	StoreRegisterHi(rd, reg_hi_d);
}

void CCodeGeneratorARM::GenerateANDI( EN64Reg rt, EN64Reg rs, u16 immediate )
{
	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);
	EArmReg regt = GetRegisterNoLoadLo(rt, ArmReg_R0);

	AND_IMM(regt, regs, immediate, ArmReg_R1);

	UpdateRegister(rt, regt, URO_HI_CLEAR);
}

void CCodeGeneratorARM::GenerateORI( EN64Reg rt, EN64Reg rs, u16 immediate )
{
	if(rs == N64Reg_R0)
	{
		// If we're oring again 0, then we're just setting a constant value
		SetRegister64( rt, immediate, 0 );
	}
	else if(mRegisterCache.IsKnownValue( rs, 0 ))
	{
		s32		known_value_lo( mRegisterCache.GetKnownValue( rs, 0 )._u32 | (u32)immediate );
		s32		known_value_hi( mRegisterCache.GetKnownValue( rs, 1 )._u32 );

		SetRegister64( rt, known_value_lo, known_value_hi );
	}
	else
	{
		EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);
		EArmReg regt = GetRegisterNoLoadLo(rt, ArmReg_R0);
		ORR_IMM(regt, regs, immediate, ArmReg_R1);
		StoreRegisterLo( rt, regt );
	}
}

void CCodeGeneratorARM::GenerateXORI( EN64Reg rt, EN64Reg rs, u16 immediate )
{
	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);
	EArmReg regt = GetRegisterNoLoadLo(rt, ArmReg_R0);

	XOR_IMM(regt, regs, immediate, ArmReg_R1);
	StoreRegisterLo( rt, regt);
}

// Set on Less Than Immediate
void CCodeGeneratorARM::GenerateSLTI( EN64Reg rt, EN64Reg rs, s16 immediate, bool is_unsigned )
{
	EArmReg regt = GetRegisterNoLoadLo(rt, ArmReg_R0);
	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);

	MOV32(ArmReg_R1, immediate);
	CMP(regs, ArmReg_R1);
	MOV_IMM(regt, 0);
	MOV_IMM(regt, 1, 0, is_unsigned ? CC : LT);

	UpdateRegister(rt, regt, URO_HI_CLEAR);
}

//*****************************************************************************
//
//	Special OPs
//
//*****************************************************************************

//Shift left logical
void CCodeGeneratorARM::GenerateSLL( EN64Reg rd, EN64Reg rt, u32 sa )
{
	EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
	EArmReg regd = GetRegisterNoLoadLo(rd, ArmReg_R0);
	MOV_LSL_IMM(regd, regt, sa);

	UpdateRegister(rd, regd, URO_HI_SIGN_EXTEND);
}

//Shift right logical
void CCodeGeneratorARM::GenerateSRL( EN64Reg rd, EN64Reg rt, u32 sa )
{
	EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
	EArmReg regd = GetRegisterNoLoadLo(rd, ArmReg_R0);
	MOV_LSR_IMM(regd, regt, sa);
	UpdateRegister(rd, regd, URO_HI_SIGN_EXTEND);
}

//Shift right arithmetic 
void CCodeGeneratorARM::GenerateSRA( EN64Reg rd, EN64Reg rt, u32 sa )
{
	EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
	EArmReg regd = GetRegisterNoLoadLo(rd, ArmReg_R0);

	MOV_ASR_IMM(regd, regt, sa);
	UpdateRegister(rd, regd, URO_HI_SIGN_EXTEND);
}

//Shift left logical variable
void CCodeGeneratorARM::GenerateSLLV( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R1);
	EArmReg regd = GetRegisterNoLoadLo(rd, ArmReg_R0);
	AND_IMM(ArmReg_R1, regs, 0x1F);

	MOV_LSL(regd, regt, ArmReg_R1);

	UpdateRegister(rd, regd, URO_HI_SIGN_EXTEND);
}

//Shift right logical variable
void CCodeGeneratorARM::GenerateSRLV( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R1);
	EArmReg regd = GetRegisterNoLoadLo(rd, ArmReg_R0);
	AND_IMM(ArmReg_R1, regs, 0x1F);

	MOV_LSR(regd, regt, ArmReg_R1);

	UpdateRegister(rd, regd, URO_HI_SIGN_EXTEND);
}

//Shift right arithmetic variable
void CCodeGeneratorARM::GenerateSRAV( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R1);
	EArmReg regd = GetRegisterNoLoadLo(rd, ArmReg_R0);
	AND_IMM(ArmReg_R1, regs, 0x1F);

	MOV_ASR(regd, regt, ArmReg_R1);

	UpdateRegister(rd, regd, URO_HI_SIGN_EXTEND);
}

void CCodeGeneratorARM::GenerateOR( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	
	bool HiIsDone = false;

	if (mRegisterCache.IsKnownValue(rs, 1) && mRegisterCache.IsKnownValue(rt, 1))
	{
		SetRegister(rd, 1, mRegisterCache.GetKnownValue(rs, 1)._u32 | mRegisterCache.GetKnownValue(rt, 1)._u32 );
		HiIsDone = true;
	}
	else if ((mRegisterCache.IsKnownValue(rs, 1) && (mRegisterCache.GetKnownValue(rs, 1)._s32 == -1)) |
		     (mRegisterCache.IsKnownValue(rt, 1) && (mRegisterCache.GetKnownValue(rt, 1)._s32 == -1)) )
	{
		SetRegister(rd, 1, ~0 );
		HiIsDone = true;
	}

	if (mRegisterCache.IsKnownValue(rs, 0) && mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister(rd, 0, mRegisterCache.GetKnownValue(rs, 0)._u32 | mRegisterCache.GetKnownValue(rt, 0)._u32);
		return;
	}

	if( rs == N64Reg_R0 )
	{
		// This doesn't seem to happen
		/*if (mRegisterCache.IsKnownValue(rt, 1))
		{
			SetRegister(rd, 1, mRegisterCache.GetKnownValue(rt, 1)._u32 );
			HiIsDone = true;
		}
		*/
		if(mRegisterCache.IsKnownValue(rt, 0))
		{
			SetRegister64(rd,
				mRegisterCache.GetKnownValue(rt, 0)._u32, mRegisterCache.GetKnownValue(rt, 1)._u32);
			return;
		}

		// This case rarely seems to happen...
		// As RS is zero, the OR is just a copy of RT to RD.
		// Try to avoid loading into a temp register if the dest is cached
		EArmReg reg_lo_d( GetRegisterNoLoadLo( rd, ArmReg_R0 ) );
		LoadRegisterLo( reg_lo_d, rt );
		StoreRegisterLo( rd, reg_lo_d );
		if(!HiIsDone)
		{
			EArmReg reg_hi_d( GetRegisterNoLoadHi( rd, ArmReg_R0 ) );
			LoadRegisterHi( reg_hi_d, rt );
			StoreRegisterHi( rd, reg_hi_d );
		}
	}
	else if( rt == N64Reg_R0 )
	{
		if (mRegisterCache.IsKnownValue(rs, 1))
		{
			SetRegister(rd, 1, mRegisterCache.GetKnownValue(rs, 1)._u32 );
			HiIsDone = true;
		}

		if(mRegisterCache.IsKnownValue(rs, 0))
		{
			SetRegister64(rd, mRegisterCache.GetKnownValue(rs, 0)._u32,
				mRegisterCache.GetKnownValue(rs, 1)._u32);
			return;
		}

		// As RT is zero, the OR is just a copy of RS to RD.
		// Try to avoid loading into a temp register if the dest is cached
		EArmReg reg_lo_d( GetRegisterNoLoadLo( rd, ArmReg_R0 ) );
		LoadRegisterLo( reg_lo_d, rs );
		StoreRegisterLo( rd, reg_lo_d );
		if(!HiIsDone)
		{
			EArmReg reg_hi_d( GetRegisterNoLoadHi( rd, ArmReg_R0 ) );
			LoadRegisterHi( reg_hi_d, rs );
			StoreRegisterHi( rd, reg_hi_d );
		}
	}
	else
	{
		EArmReg regt_lo = GetRegisterAndLoadLo(rt, ArmReg_R0);
		EArmReg regs_lo = GetRegisterAndLoadLo(rs, ArmReg_R1);
		EArmReg regd_lo = GetRegisterNoLoadLo(rd, ArmReg_R0);

		ORR(regd_lo, regs_lo, regt_lo);

		StoreRegisterLo(rd, regd_lo);

		if(!HiIsDone)
		{
			if( mRegisterCache.IsKnownValue(rs, 1) & (mRegisterCache.GetKnownValue(rs, 1)._u32 == 0) )
			{
				EArmReg reg_hi_d( GetRegisterNoLoadHi( rd, ArmReg_R0 ) );
				LoadRegisterHi( reg_hi_d, rt );
				StoreRegisterHi( rd, reg_hi_d );
			}
			else if( mRegisterCache.IsKnownValue(rt, 1) & (mRegisterCache.GetKnownValue(rt, 1)._u32 == 0) )
			{
				EArmReg reg_hi_d( GetRegisterNoLoadHi( rd, ArmReg_R0 ) );
				LoadRegisterHi( reg_hi_d, rs );
				StoreRegisterHi( rd, reg_hi_d );
			}
			else
			{
				EArmReg regt_hi = GetRegisterAndLoadHi(rt, ArmReg_R0);
				EArmReg regs_hi = GetRegisterAndLoadHi(rs, ArmReg_R1);
				EArmReg regd_hi = GetRegisterNoLoadHi(rd, ArmReg_R0);
				ORR(regd_hi, regs_hi, regt_hi);

				StoreRegisterHi(rd, regd_hi);
			}
		}
	}
}

void CCodeGeneratorARM::GenerateXOR( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	EArmReg regt_lo = GetRegisterAndLoadLo(rt, ArmReg_R0);
	EArmReg regs_lo = GetRegisterAndLoadLo(rs, ArmReg_R1);
	EArmReg regd_lo = GetRegisterNoLoadLo(rd, ArmReg_R0);

	XOR(regd_lo, regs_lo, regt_lo);

	StoreRegisterLo(rd, regd_lo);
	EArmReg regt_hi = GetRegisterAndLoadHi(rt, ArmReg_R0);
	EArmReg regs_hi = GetRegisterAndLoadHi(rs, ArmReg_R1);
	EArmReg regd_hi = GetRegisterNoLoadHi(rd, ArmReg_R0);
	XOR(regd_hi, regs_hi, regt_hi);

	StoreRegisterHi(rd, regd_hi);
}

void CCodeGeneratorARM::GenerateNOR( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	
	if (rs == N64Reg_R0 || rt == N64Reg_R0)
	{
		EN64Reg fromReg;
		if (rs == N64Reg_R0)
		{
			fromReg = rt;
		}
		else
		{
			fromReg = rs;
		}
		
		EArmReg regf_lo = GetRegisterAndLoadLo(fromReg, ArmReg_R0);
		EArmReg regd_lo = GetRegisterNoLoadLo(rd, ArmReg_R0);
		MVN(regd_lo, regf_lo);
		StoreRegisterLo(rd, regd_lo);
		
		EArmReg regf_hi = GetRegisterAndLoadHi(fromReg, ArmReg_R0);
		EArmReg regd_hi = GetRegisterNoLoadHi(rd, ArmReg_R0);
		MVN(regd_hi, regf_hi);
		StoreRegisterHi(rd, regd_hi);
	}
	else
	{
		EArmReg regt_lo = GetRegisterAndLoadLo(rt, ArmReg_R0);
		EArmReg regs_lo = GetRegisterAndLoadLo(rs, ArmReg_R1);
		EArmReg regd_lo = GetRegisterNoLoadLo(rd, ArmReg_R0);

		ORR(regd_lo, regs_lo, regt_lo);
		MVN(regd_lo, regd_lo);

		StoreRegisterLo(rd, regd_lo);
		EArmReg regt_hi = GetRegisterAndLoadHi(rt, ArmReg_R0);
		EArmReg regs_hi = GetRegisterAndLoadHi(rs, ArmReg_R1);
		EArmReg regd_hi = GetRegisterNoLoadHi(rd, ArmReg_R0);
		ORR(regd_hi, regs_hi, regt_hi);
		MVN(regd_hi, regd_hi);
		StoreRegisterHi(rd, regd_hi);
	}
}

void CCodeGeneratorARM::GenerateAND( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	EArmReg regt_lo = GetRegisterAndLoadLo(rt, ArmReg_R0);
	EArmReg regs_lo = GetRegisterAndLoadLo(rs, ArmReg_R1);
	EArmReg regd_lo = GetRegisterNoLoadLo(rd, ArmReg_R0);

	AND(regd_lo, regs_lo, regt_lo);

	StoreRegisterLo(rd, regd_lo);
	EArmReg regt_hi = GetRegisterAndLoadHi(rt, ArmReg_R0);
	EArmReg regs_hi = GetRegisterAndLoadHi(rs, ArmReg_R1);
	EArmReg regd_hi = GetRegisterNoLoadHi(rd, ArmReg_R0);
	AND(regd_hi, regs_hi, regt_hi);

	StoreRegisterHi(rd, regd_hi);
}


void CCodeGeneratorARM::GenerateADDU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	if (mRegisterCache.IsKnownValue(rs, 0) && mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister32s(rd, mRegisterCache.GetKnownValue(rs, 0)._s32
			+ mRegisterCache.GetKnownValue(rt, 0)._s32);
		return;
	}

	if( rs == N64Reg_R0 )
	{
		if(mRegisterCache.IsKnownValue(rt, 0))
		{
			SetRegister32s(rd, mRegisterCache.GetKnownValue(rt, 0)._s32);
			return;
		}

		// As RS is zero, the ADD is just a copy of RT to RD.
		// Try to avoid loading into a temp register if the dest is cached
		EArmReg reg_lo_d( GetRegisterNoLoadLo( rd, ArmReg_R0 ) );
		LoadRegisterLo( reg_lo_d, rt );
		UpdateRegister( rd, reg_lo_d, URO_HI_SIGN_EXTEND );
	}
	else if( rt == N64Reg_R0 )
	{
		if(mRegisterCache.IsKnownValue(rs, 0))
		{
			SetRegister32s(rd, mRegisterCache.GetKnownValue(rs, 0)._s32);
			return;
		}

		// As RT is zero, the ADD is just a copy of RS to RD.
		// Try to avoid loading into a temp register if the dest is cached
		EArmReg reg_lo_d( GetRegisterNoLoadLo( rd, ArmReg_R0 ) );
		LoadRegisterLo( reg_lo_d, rs );
		UpdateRegister( rd, reg_lo_d, URO_HI_SIGN_EXTEND );
	}
	else
	{
		EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
		EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R1);
		EArmReg regd = GetRegisterNoLoadLo(rd, ArmReg_R0);

		ADD(regd, regs, regt);

		UpdateRegister(rd, regd, URO_HI_SIGN_EXTEND);
	}
}

void CCodeGeneratorARM::GenerateSUBU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R1);
	EArmReg regd = GetRegisterNoLoadLo(rd, ArmReg_R0);

	SUB(regd, regs, regt);

	UpdateRegister(rd, regd, URO_HI_SIGN_EXTEND);
}

void CCodeGeneratorARM::GenerateMULT( EN64Reg rs, EN64Reg rt, bool is_unsigned )
{
	EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R1);

	if(is_unsigned)
		UMULL( ArmReg_R0, ArmReg_R4, regt, regs );
	else
		SMULL( ArmReg_R0, ArmReg_R4, regt, regs );


	MOV_ASR_IMM(ArmReg_R1, ArmReg_R0, 0x1F);
	SetVar(&gCPUState.MultLo._u32_0, ArmReg_R0);
	SetVar(&gCPUState.MultLo._u32_1, ArmReg_R1);

	MOV_ASR_IMM(ArmReg_R0, ArmReg_R4, 0x1F);
	SetVar(&gCPUState.MultHi._u32_0, ArmReg_R4);
	SetVar(&gCPUState.MultHi._u32_1, ArmReg_R0);
}

void CCodeGeneratorARM::GenerateMFLO( EN64Reg rd )
{
	//gGPR[ op_code.rd ]._u64 = gCPUState.MultLo._u64;

	EArmReg regd_lo = GetRegisterNoLoadLo(rd, ArmReg_R0);
	LDR(regd_lo, ArmReg_R12, offsetof(SCPUState, MultLo._u32_0));

	StoreRegisterLo(rd, regd_lo);

	EArmReg regd_hi = GetRegisterNoLoadHi(rd, ArmReg_R0);
	LDR(regd_hi, ArmReg_R12, offsetof(SCPUState, MultLo._u32_1));

	StoreRegisterHi(rd, regd_hi);
}

void CCodeGeneratorARM::GenerateMFHI( EN64Reg rd )
{
	EArmReg regd_lo = GetRegisterNoLoadLo(rd, ArmReg_R0);
	LDR(regd_lo, ArmReg_R12, offsetof(SCPUState, MultHi._u32_0));

	StoreRegisterLo(rd, regd_lo);

	EArmReg regd_hi = GetRegisterNoLoadHi(rd, ArmReg_R0);
	LDR(regd_hi, ArmReg_R12, offsetof(SCPUState, MultHi._u32_1));

	StoreRegisterHi(rd, regd_hi);
}

void CCodeGeneratorARM::GenerateMTLO( EN64Reg rs )
{
	//gCPUState.MultLo._u64 = gGPR[ op_code.rs ]._u64;
	EArmReg regs_lo = GetRegisterAndLoadLo(rs, ArmReg_R0);
	STR(regs_lo, ArmReg_R12, offsetof(SCPUState, MultLo._u32_0));

	EArmReg regs_hi = GetRegisterAndLoadHi(rs, ArmReg_R0);
	STR(regs_hi, ArmReg_R12, offsetof(SCPUState, MultLo._u32_1));
}

void CCodeGeneratorARM::GenerateMTHI( EN64Reg rs )
{
	//gCPUState.MultHi._u64 = gGPR[ op_code.rs ]._u64;
	EArmReg regs_lo = GetRegisterAndLoadLo(rs, ArmReg_R0);
	STR(regs_lo, ArmReg_R12, offsetof(SCPUState, MultHi._u32_0));

	EArmReg regs_hi = GetRegisterAndLoadHi(rs, ArmReg_R0);
	STR(regs_hi, ArmReg_R12, offsetof(SCPUState, MultHi._u32_1));
}

void CCodeGeneratorARM::GenerateSLT( EN64Reg rd, EN64Reg rs, EN64Reg rt, bool is_unsigned )
{
	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);
	EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R1);
	EArmReg regd = GetRegisterNoLoadLo(rd, ArmReg_R0);
	

	CMP(regs, regt);
	MOV_IMM(regd, 0);
	MOV_IMM(regd, 1, 0, is_unsigned ? CC : LT);

	UpdateRegister(rd, regd, URO_HI_CLEAR);
}

//*****************************************************************************
//
//	Branch instructions
//
//*****************************************************************************

void CCodeGeneratorARM::GenerateBEQ( EN64Reg rs, EN64Reg rt, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BEQ?" );
	#endif

	if (rt == N64Reg_R0)
	{
		EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);
		CMP_IMM(regs, 0);
	}
	else if (rs == N64Reg_R0)
	{
		EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
		CMP_IMM(regt, 0);
	}
	else
	{
		EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);
		EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R1);

		// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does
		CMP(regs, regt);
	}

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), NE);
	}
	else
	{
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), EQ);
	}
}

void CCodeGeneratorARM::GenerateBNE( EN64Reg rs, EN64Reg rt, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BEQ?" );
	#endif

	if (rt == N64Reg_R0)
	{
		EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);
		CMP_IMM(regs, 0);
	}
	else if (rs == N64Reg_R0)
	{
		EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
		CMP_IMM(regt, 0);
	}
	else
	{
		EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);
		EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R1);

		// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does
		CMP(regs, regt);
	}

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), EQ);
	}
	else
	{
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), NE);
	}
}

void CCodeGeneratorARM::GenerateBLEZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BLEZ?" );
	#endif

	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);

	// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does
	CMP_IMM(regs, 0);

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), GT);
	}
	else
	{
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), LE);
	}
}

void CCodeGeneratorARM::GenerateBGEZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BLTZ?" );
	#endif

	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);

	// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does
	CMP_IMM(regs, 0);

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), LT);
	}
	else
	{
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), GE);
	}
}

void CCodeGeneratorARM::GenerateBLTZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BLTZ?" );
	#endif

	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);

	// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does
	CMP_IMM(regs, 0);

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), GE);
	}
	else
	{
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), LT);
	}
}

void CCodeGeneratorARM::GenerateBGTZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BGTZ?" );
	#endif

	EArmReg regs = GetRegisterAndLoadLo(rs, ArmReg_R0);

	// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does
	CMP_IMM(regs, 0);

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), LE);
	}
	else
	{
		*p_branch_jump = BX_IMM(CCodeLabel(nullptr), GT);
	}
}

//*****************************************************************************
//
//	CoPro1 instructions
//
//*****************************************************************************

void CCodeGeneratorARM::GenerateADD_S( u32 fd, u32 fs, u32 ft )
{
	EArmVfpReg arm_fs = GetFloatRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_ft = GetFloatRegisterAndLoad(EN64FloatReg(ft));
	EArmVfpReg arm_fd = EArmVfpReg(fd);
	VADD(arm_fd, arm_fs, arm_ft);
	UpdateFloatRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateSUB_S( u32 fd, u32 fs, u32 ft )
{
	EArmVfpReg arm_fs = GetFloatRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_ft = GetFloatRegisterAndLoad(EN64FloatReg(ft));
	EArmVfpReg arm_fd = EArmVfpReg(fd);
	VSUB(arm_fd, arm_fs, arm_ft);
	UpdateFloatRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateMUL_S( u32 fd, u32 fs, u32 ft )
{
	EArmVfpReg arm_fs = GetFloatRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_ft = GetFloatRegisterAndLoad(EN64FloatReg(ft));
	EArmVfpReg arm_fd = EArmVfpReg(fd);
	VMUL(arm_fd, arm_fs, arm_ft);
	UpdateFloatRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateDIV_S( u32 fd, u32 fs, u32 ft )
{
	EArmVfpReg arm_fs = GetFloatRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_ft = GetFloatRegisterAndLoad(EN64FloatReg(ft));
	EArmVfpReg arm_fd = EArmVfpReg(fd);
	VDIV(arm_fd, arm_fs, arm_ft);
	UpdateFloatRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateSQRT_S( u32 fd, u32 fs )
{
	EArmVfpReg arm_fs = GetFloatRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_fd = EArmVfpReg(fd);
	VSQRT(arm_fd, arm_fs);
	UpdateFloatRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateABS_S( u32 fd, u32 fs )
{
	EArmVfpReg arm_fs = GetFloatRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_fd = EArmVfpReg(fd);
	VABS(arm_fd, arm_fs);
	UpdateFloatRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateMOV_S( u32 fd, u32 fs )
{
	EArmVfpReg arm_fs = GetFloatRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_fd = EArmVfpReg(fd);
	VMOV_S(arm_fd, arm_fs);
	UpdateFloatRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateNEG_S( u32 fd, u32 fs )
{
	EArmVfpReg arm_fs = GetFloatRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_fd = EArmVfpReg(fd);
	VNEG(arm_fd, arm_fs);
	UpdateFloatRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateTRUNC_W_S( u32 fd, u32 fs )
{
	EArmVfpReg arm_fs = GetFloatRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_fd = EArmVfpReg(fd);
	VCVT_S32_F32(arm_fd, arm_fs);
	UpdateFloatRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateCVT_D_S( u32 fd, u32 fs )
{
	EArmVfpReg arm_fs = GetFloatRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_fd = EArmVfpReg(fd/2);
	VCVT_F64_F32(arm_fd, arm_fs);
	UpdateDoubleRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateCMP_S( u32 fs, u32 ft, EArmCond cond, u8 E )
{
	if(cond == NV)
	{
		MOV32(ArmReg_R0, ~FPCSR_C);

		LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
		AND(ArmReg_R0, ArmReg_R1, ArmReg_R0);

		STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
	}
	else
	{
		EArmVfpReg arm_fs = GetFloatRegisterAndLoad(EN64FloatReg(fs));
		EArmVfpReg arm_ft = GetFloatRegisterAndLoad(EN64FloatReg(ft));

		LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
		AND_IMM(ArmReg_R0, ArmReg_R1, ~FPCSR_C, ArmReg_R0);

		VCMP(arm_fs, arm_ft, E);

		MOV_IMM(ArmReg_R1, 0x02, 0x5);
		ADD(ArmReg_R0, ArmReg_R0, ArmReg_R1, cond);

		STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
	}
}

void CCodeGeneratorARM::GenerateADD_D( u32 fd, u32 fs, u32 ft )
{
	EArmVfpReg arm_fs = GetDoubleRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_ft = GetDoubleRegisterAndLoad(EN64FloatReg(ft));
	EArmVfpReg arm_fd = EArmVfpReg(fd/2);

	VADD_D(arm_fd, arm_fs, arm_ft);

	UpdateDoubleRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateSUB_D( u32 fd, u32 fs, u32 ft )
{
	EArmVfpReg arm_fs = GetDoubleRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_ft = GetDoubleRegisterAndLoad(EN64FloatReg(ft));
	EArmVfpReg arm_fd = EArmVfpReg(fd/2);

	VSUB_D(arm_fd, arm_fs, arm_ft);

	UpdateDoubleRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateDIV_D( u32 fd, u32 fs, u32 ft )
{
	EArmVfpReg arm_fs = GetDoubleRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_ft = GetDoubleRegisterAndLoad(EN64FloatReg(ft));
	EArmVfpReg arm_fd = EArmVfpReg(fd/2);

	VDIV_D(arm_fd, arm_fs, arm_ft);

	UpdateDoubleRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateMUL_D( u32 fd, u32 fs, u32 ft )
{
	EArmVfpReg arm_fs = GetDoubleRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_ft = GetDoubleRegisterAndLoad(EN64FloatReg(ft));
	EArmVfpReg arm_fd = EArmVfpReg(fd/2);

	VMUL_D(arm_fd, arm_fs, arm_ft);

	UpdateDoubleRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateSQRT_D( u32 fd, u32 fs )
{
	EArmVfpReg arm_fs = GetDoubleRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_fd = EArmVfpReg(fd/2);

	VSQRT_D(arm_fd, arm_fs);

	UpdateDoubleRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateABS_D( u32 fd, u32 fs )
{
	EArmVfpReg arm_fs = GetDoubleRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_fd = EArmVfpReg(fd/2);

	VABS_D(arm_fd, arm_fs);

	UpdateDoubleRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateMOV_D( u32 fd, u32 fs )
{
	EArmVfpReg arm_fs = GetDoubleRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_fd = EArmVfpReg(fd/2);

	VMOV(arm_fd, arm_fs);

	UpdateDoubleRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateNEG_D( u32 fd, u32 fs )
{
	EArmVfpReg arm_fs = GetDoubleRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_fd = EArmVfpReg(fd/2);

	VNEG_D(arm_fd, arm_fs);

	UpdateDoubleRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateTRUNC_W_D( u32 fd, u32 fs )
{
	EArmVfpReg arm_fs = GetDoubleRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_fd = EArmVfpReg(fd);

	VCVT_S32_F64(arm_fd, arm_fs);

	UpdateFloatRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateCVT_S_D( u32 fd, u32 fs )
{
	EArmVfpReg arm_fs = GetDoubleRegisterAndLoad(EN64FloatReg(fs));
	EArmVfpReg arm_fd = EArmVfpReg(fd);

	VCVT_F32_F64(arm_fd, arm_fs);

	UpdateFloatRegister(EN64FloatReg(fd));
}

void CCodeGeneratorARM::GenerateCMP_D( u32 fs, u32 ft, EArmCond cond, u8 E )
{
	if( cond == NV )
	{
		LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
		AND_IMM(ArmReg_R0, ArmReg_R1, ~FPCSR_C, ArmReg_R0);
		STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
	}
	else
	{
		EArmVfpReg arm_fs = GetDoubleRegisterAndLoad(EN64FloatReg(fs));
		EArmVfpReg arm_ft = GetDoubleRegisterAndLoad(EN64FloatReg(ft));

		LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));

		LDR(ArmReg_R1, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
		AND_IMM(ArmReg_R0, ArmReg_R1, ~FPCSR_C, ArmReg_R0);

		VCMP_D(arm_fs, arm_ft, E);

		MOV_IMM(ArmReg_R1, 0x02, 0x5);
		ADD(ArmReg_R0, ArmReg_R0, ArmReg_R1, cond);

		STR(ArmReg_R0, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
	}
}

inline void	CCodeGeneratorARM::GenerateMFC0( EN64Reg rt, u32 fs )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	// Never seen this to happen, no reason to bother to handle it
	DAEDALUS_ASSERT( fs != C0_RAND, "Reading MFC0 random register is unhandled");
	#endif
	EArmReg reg_dst( GetRegisterNoLoadLo( rt, ArmReg_R0 ) );

	GetVar( reg_dst, &gCPUState.CPUControl[ fs ]._u32 );
	UpdateRegister( rt, reg_dst, URO_HI_SIGN_EXTEND );
}

void CCodeGeneratorARM::GenerateMFC1( EN64Reg rt, u32 fs )
{
	EArmReg regt = GetRegisterNoLoadLo(rt, ArmReg_R0);
	EArmVfpReg arm_fs = GetFloatRegisterAndLoad(EN64FloatReg(fs));
	VMOV_S(regt, arm_fs);

	UpdateRegister(rt, regt, URO_HI_SIGN_EXTEND);
}

void CCodeGeneratorARM::GenerateMTC1( u32 fs, EN64Reg rt )
{
	EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
	
	EArmVfpReg arm_fs = EArmVfpReg(fs);
	
	VMOV_S(arm_fs, regt);
	
	UpdateFloatRegister(EN64FloatReg(fs));
}

void CCodeGeneratorARM::GenerateCFC1( EN64Reg rt, u32 fs )
{
	if ( fs == 0 || fs == 31 )
	{
		EArmReg regt = GetRegisterNoLoadLo(rt, ArmReg_R0);
		LDR(regt, ArmReg_R12, offsetof(SCPUState, FPUControl[0]._s32) + sizeof(SCPUState::FPUControl[0]) * fs);

		UpdateRegister(rt, regt, URO_HI_SIGN_EXTEND);
	}
}

void CCodeGeneratorARM::GenerateCTC1( u32 fs, EN64Reg rt )
{
	if ( fs == 31 )
	{
		EArmReg regt = GetRegisterAndLoadLo(rt, ArmReg_R0);
		STR(regt, ArmReg_R12, offsetof(SCPUState, FPUControl[31]._u32));
	}
}