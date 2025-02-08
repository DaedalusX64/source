/*
Copyright (C) 2001,2005 StrmnNrmn

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

#pragma once

#ifndef DYNAREC_CODEGENERATOR_H_
#define DYNAREC_CODEGENERATOR_H_

struct OpCode;
struct SBranchDetails;
class CIndirectExitMap;

#include "Core/R4300Instruction.h"
#include "DynaRec/AssemblyUtils.h"
#include "DynaRec/RegisterSpan.h"
#include "DynaRec/TraceRecorder.h"
#include "DynaRec/N64RegisterCache.h"

#include "Core/CPU.h"

#include "Config/ConfigOptions.h"

#include <algorithm>
#include <vector>

#define URO_HI_SIGN_EXTEND 0 // Sign extend from src
#define URO_HI_CLEAR 1		 // Clear hi bits

// Return true if this register dont need sign extension //Corn
inline bool N64Reg_DontNeedSign(EN64Reg n64_reg) { return (0x30000001 >> n64_reg) & 1; }

//
//	Allow an architecure-independent way of passing around register sets
//
struct RegisterSnapshotHandle
{
	explicit RegisterSnapshotHandle(u32 handle) : Handle(handle) {}

	u32 Handle;
};

class CCodeGenerator
{
public:
	using ExceptionHandlerFn = void (*)();

	virtual ~CCodeGenerator() {}

	virtual void Initialise(u32 entry_address, u32 exit_address, u32 *hit_counter, const void *p_base, const SRegisterUsageInfo &register_usage) = 0;
	virtual void Finalise(ExceptionHandlerFn p_exception_handler_fn, const std::vector<CJumpLocation> &exception_handler_jumps, const std::vector<RegisterSnapshotHandle> &exception_handler_snapshots) = 0;

	virtual void UpdateRegisterCaching(u32 instruction_idx) = 0;

	virtual RegisterSnapshotHandle GetRegisterSnapshot() = 0;

	virtual CCodeLabel GetEntryPoint() const = 0;
	virtual CCodeLabel GetCurrentLocation() const = 0;
	virtual u32	GetCompiledCodeSize() const = 0;

	virtual CJumpLocation GenerateExitCode(u32 exit_address, u32 jump_address, u32 num_instructions, CCodeLabel next_fragment) = 0;
	virtual void GenerateEretExitCode(u32 num_instructions, CIndirectExitMap *p_map) = 0;
	virtual void GenerateIndirectExitCode(u32 num_instructions, CIndirectExitMap *p_map) = 0;

	virtual void GenerateBranchHandler(CJumpLocation branch_handler_jump, RegisterSnapshotHandle snapshot) = 0;

	virtual CJumpLocation GenerateOpCode(const STraceEntry &ti, bool branch_delay_slot, const SBranchDetails *p_branch, CJumpLocation *p_branch_jump) = 0;
	virtual CJumpLocation ExecuteNativeFunction(CCodeLabel speed_hack, bool check_return = false) = 0;
};

template <typename NativeReg>
class CCodeGeneratorImpl : public CCodeGenerator
{
public:
	CCodeGeneratorImpl(const NativeReg *cacheRegs)
		: mUseFixedRegisterAllocation(false), mLoopTop(nullptr), RegistersToUseForCaching(cacheRegs)
	{
	}

protected:
	const NativeReg *RegistersToUseForCaching;
	RegisterSpanList mRegisterSpanList;
	CCodeLabel mLoopTop;

	bool mUseFixedRegisterAllocation;
	std::vector<CN64RegisterCache<NativeReg>> mRegisterSnapshots;
	CN64RegisterCache<NativeReg> mRegisterCache;

	// For register allocation
	RegisterSpanList mActiveIntervals;
	std::vector<NativeReg> mAvailableRegisters;

	virtual void SetVar(u32 *p_var, u32 value) = 0;
	virtual void SetVar(u32 *p_var, NativeReg reg_src) = 0;
	virtual void GetVar(NativeReg dst_reg, const u32 *p_var) = 0;
	virtual void LoadConstant(NativeReg reg, s32 value) = 0;
	virtual void Copy(NativeReg src_reg, NativeReg dest_reg) = 0;
	virtual void SignedExtend(NativeReg src_reg, NativeReg dest_reg) = 0;

	/*
		Register allocation helper functions
	*/
	void FlushAllGenericRegisters(CN64RegisterCache<NativeReg> &cache, bool invalidate)
	{
		for (u32 i = 1; i < NUM_N64_REGS; i++)
		{
			EN64Reg n64_reg = EN64Reg(i);

			FlushRegister(cache, n64_reg, 0, invalidate);
			FlushRegister(cache, n64_reg, 1, invalidate);
		}
	}

	void UpdateRegister(EN64Reg n64_reg, NativeReg native_reg, bool options)
	{
		if (n64_reg == N64Reg_R0)
			return; // Try to modify R0!!!

		StoreRegisterLo(n64_reg, native_reg);

		// Skip storing sign extension on some regs //Corn
		if (N64Reg_DontNeedSign(n64_reg))
			return;

#ifdef SCRATCH_REG
		if (options == URO_HI_SIGN_EXTEND)
		{
			NativeReg scratch_reg = SCRATCH_REG;
			if (mRegisterCache.IsCached(n64_reg, 1))
			{
				scratch_reg = mRegisterCache.GetCachedReg(n64_reg, 1);
			}
			SignExtend(scratch_reg, native_reg); // Sign extend
			StoreRegisterHi(n64_reg, scratch_reg);
		}
		else // == URO_HI_CLEAR
#endif
		{
			SetRegister(n64_reg, 1, 0);
		}
	}

	void SetRegister(EN64Reg n64_reg, u32 lo_hi_idx, u32 value)
	{
		mRegisterCache.SetKnownValue(n64_reg, lo_hi_idx, value);
		mRegisterCache.MarkAsDirty(n64_reg, lo_hi_idx, true);
		if (mRegisterCache.IsCached(n64_reg, lo_hi_idx))
		{
			mRegisterCache.MarkAsValid(n64_reg, lo_hi_idx, false); // The actual cache is invalid though!
		}
	}

	//
	void SetRegister64(EN64Reg n64_reg, s32 lo_value, s32 hi_value)
	{
		SetRegister(n64_reg, 0, lo_value);
		SetRegister(n64_reg, 1, hi_value);
	}

	//	Set the low 32 bits of a register to a known value (and hence the upper
	//	32 bits are also known though sign extension)

	void SetRegister32s(EN64Reg n64_reg, s32 value)
	{
		// SetRegister64( n64_reg, value, value >= 0 ? 0 : 0xffffffff );
		SetRegister64(n64_reg, value, value >> 31);
	}

	void StoreRegisterLo(EN64Reg n64_reg, NativeReg native_reg) { StoreRegister(n64_reg, 0, native_reg); }
	void StoreRegisterHi(EN64Reg n64_reg, NativeReg native_reg) { StoreRegister(n64_reg, 1, native_reg); }

	//
	void StoreRegister(EN64Reg n64_reg, u32 lo_hi_idx, NativeReg native_reg)
	{
		mRegisterCache.ClearKnownValue(n64_reg, lo_hi_idx);

		if (mRegisterCache.IsCached(n64_reg, lo_hi_idx))
		{
			auto cached_reg(mRegisterCache.GetCachedReg(n64_reg, lo_hi_idx));

			//		gTotalRegistersCached++;

			// Update our copy as necessary
			if (native_reg != cached_reg)
			{
				Copy(native_reg, cached_reg);
			}
			mRegisterCache.MarkAsDirty(n64_reg, lo_hi_idx, true);
			mRegisterCache.MarkAsValid(n64_reg, lo_hi_idx, true);
		}
		else
		{
			//		gTotalRegistersUncached++;

			SetVar(lo_hi_idx ? &gGPR[n64_reg]._u32_1 : &gGPR[n64_reg]._u32_0, native_reg);

			mRegisterCache.MarkAsDirty(n64_reg, lo_hi_idx, false);
		}
	}

	void LoadRegisterLo(NativeReg native_reg, EN64Reg n64_reg) { LoadRegister(native_reg, n64_reg, 0); }
	void LoadRegisterHi(NativeReg native_reg, EN64Reg n64_reg) { LoadRegister(native_reg, n64_reg, 1); }

	//	Similar to GetRegisterAndLoad, but ALWAYS loads into the specified psp register
	void LoadRegister(NativeReg native_reg, EN64Reg n64_reg, u32 lo_hi_idx)
	{
		if (mRegisterCache.IsCached(n64_reg, lo_hi_idx))
		{
			auto cached_reg(mRegisterCache.GetCachedReg(n64_reg, lo_hi_idx));

			//		gTotalRegistersCached++;

			// Load the register if it's currently invalid
			if (!mRegisterCache.IsValid(n64_reg, lo_hi_idx))
			{
				GetRegisterValue(cached_reg, n64_reg, lo_hi_idx);
				mRegisterCache.MarkAsValid(n64_reg, lo_hi_idx, true);
			}

			// Copy the register if necessary
			if (native_reg != cached_reg)
			{
				Copy(cached_reg, native_reg);
			}
		}
		else if (n64_reg == N64Reg_R0)
		{
#ifdef ALWAYS_ZERO_REG
			Copy(ALWAYS_ZERO_REG, native_reg);
#else
			LoadConstant(native_reg, 0);
#endif
		}
		else
		{
			//		gTotalRegistersUncached++;
			GetRegisterValue(native_reg, n64_reg, lo_hi_idx);
		}
	}

	NativeReg GetRegisterNoLoadLo(EN64Reg n64_reg, NativeReg scratch_reg) { return GetRegisterNoLoad(n64_reg, 0, scratch_reg); }
	NativeReg GetRegisterNoLoadHi(EN64Reg n64_reg, NativeReg scratch_reg) { return GetRegisterNoLoad(n64_reg, 1, scratch_reg); }

	// Get a (cached) N64 register mapped to a PSP register(usefull for dst register)
	NativeReg GetRegisterNoLoad(EN64Reg n64_reg, u32 lo_hi_idx, NativeReg scratch_reg)
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

	// Load value from an emulated N64 register or known value to a PSP register
	void GetRegisterValue(NativeReg native_reg, EN64Reg n64_reg, u32 lo_hi_idx)
	{
		if (mRegisterCache.IsKnownValue(n64_reg, lo_hi_idx))
		{
			// printf( "Loading %s[%d] <- %08x\n", RegNames[ n64_reg ], lo_hi_idx, mRegisterCache.GetKnownValue( n64_reg, lo_hi_idx ) );
			LoadConstant(native_reg, mRegisterCache.GetKnownValue(n64_reg, lo_hi_idx)._s32);
			if (mRegisterCache.IsCached(n64_reg, lo_hi_idx))
			{
				mRegisterCache.MarkAsValid(n64_reg, lo_hi_idx, true);
				mRegisterCache.MarkAsDirty(n64_reg, lo_hi_idx, true);
				mRegisterCache.ClearKnownValue(n64_reg, lo_hi_idx);
			}
		}
		else
		{
			GetVar(native_reg, lo_hi_idx ? &gGPR[n64_reg]._u32_1 : &gGPR[n64_reg]._u32_0);
		}
	}

	void PrepareCachedRegisterLo(EN64Reg n64_reg) { PrepareCachedRegister(n64_reg, 0); }
	void PrepareCachedRegisterHi(EN64Reg n64_reg) { PrepareCachedRegister(n64_reg, 1); }

	//	This function pulls in a cached register so that it can be used at a later point.
	//	This is usally done when we have a branching instruction - it guarantees that
	//	the register is valid regardless of whether or not the branch is taken.
	void PrepareCachedRegister(EN64Reg n64_reg, u32 lo_hi_idx)
	{
		if (mRegisterCache.IsCached(n64_reg, lo_hi_idx))
		{
			auto cached_reg(mRegisterCache.GetCachedReg(n64_reg, lo_hi_idx));

			// Load the register if it's currently invalid
			if (!mRegisterCache.IsValid(n64_reg, lo_hi_idx))
			{
				GetRegisterValue(cached_reg, n64_reg, lo_hi_idx);
				mRegisterCache.MarkAsValid(n64_reg, lo_hi_idx, true);
			}
		}
	}

	//	Flush a specific register back to memory if dirty.
	//	Clears the dirty flag and invalidates the contents if specified

	void FlushRegister(CN64RegisterCache<NativeReg> &cache, EN64Reg n64_reg, u32 lo_hi_idx, bool invalidate)
	{
		if (cache.IsDirty(n64_reg, lo_hi_idx))
		{
			if (cache.IsKnownValue(n64_reg, lo_hi_idx))
			{
				s32 known_value(cache.GetKnownValue(n64_reg, lo_hi_idx)._s32);

				SetVar(lo_hi_idx ? &gGPR[n64_reg]._u32_1 : &gGPR[n64_reg]._u32_0, known_value);
			}
			else if (cache.IsCached(n64_reg, lo_hi_idx))
			{
#ifdef DAEDALUS_ENABLE_ASSERTS
				DAEDALUS_ASSERT(cache.IsValid(n64_reg, lo_hi_idx), "Register is dirty but not valid?");
#endif
				auto cached_reg(cache.GetCachedReg(n64_reg, lo_hi_idx));

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

	NativeReg GetRegisterAndLoadLo(EN64Reg n64_reg, NativeReg scratch_reg) { return GetRegisterAndLoad(n64_reg, 0, scratch_reg); }
	NativeReg GetRegisterAndLoadHi(EN64Reg n64_reg, NativeReg scratch_reg) { return GetRegisterAndLoad(n64_reg, 1, scratch_reg); }

	// Get (cached) N64 register value mapped to a PSP register (or scratch reg)
	// and also load the value if not loaded yet(usefull for src register)
	NativeReg GetRegisterAndLoad(EN64Reg n64_reg, u32 lo_hi_idx, NativeReg scratch_reg)
	{
		NativeReg reg;
		bool need_load(false);

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
#ifdef ALWAYS_ZERO_REG
		else if (n64_reg == N64Reg_R0)
		{
			reg = ALWAYS_ZERO_REG;
		}
#endif
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

	//
	void SetRegisterSpanList(const SRegisterUsageInfo &register_usage, bool loops_to_self)
	{
		mRegisterSpanList = register_usage.SpanList;

		// Sort in order of increasing start point
		std::sort(mRegisterSpanList.begin(), mRegisterSpanList.end(), SAscendingSpanStartSort());

		// Push all the available registers in reverse order (i.e. use temporaries later)
		// Use temporaries first so we can avoid flushing them in case of a funcion call //Corn
#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT(mAvailableRegisters.empty(), "Why isn't the available register list empty?");
#endif
		for (u32 i{0}; RegistersToUseForCaching[i] != (NativeReg)-1; i++)
		{
			mAvailableRegisters.push_back(RegistersToUseForCaching[i]);
		}

		// Optimization for self looping code
		if (gDynarecLoopOptimisation & loops_to_self)
		{
			mUseFixedRegisterAllocation = true;
			u32 cache_reg_idx(0);
			u32 HiLo{0};
			while (HiLo < 2) // If there are still unused registers, assign to high part of reg
			{
				RegisterSpanList::const_iterator span_it = mRegisterSpanList.begin();
				while (span_it < mRegisterSpanList.end())
				{
					const SRegisterSpan &span(*span_it);
					if (cache_reg_idx < mAvailableRegisters.size())
					{
						NativeReg cachable_reg(mAvailableRegisters[cache_reg_idx]);
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
			u32 i{1};
			while (i < NUM_N64_REGS)
			{
				EN64Reg n64_reg = EN64Reg(i);
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
		} // End of Loop optimization code
	}

	//

	void ExpireOldIntervals(u32 instruction_idx)
	{
		// mActiveIntervals is held in order of increasing end point
		for (RegisterSpanList::iterator span_it = mActiveIntervals.begin(); span_it < mActiveIntervals.end(); ++span_it)
		{
			const SRegisterSpan &span(*span_it);

			if (span.SpanEnd >= instruction_idx)
			{
				break;
			}

			// This interval is no longer active - flush the register and return it to the list of available regs
			auto native_reg(mRegisterCache.GetCachedReg(span.Register, 0));

			FlushRegister(mRegisterCache, span.Register, 0, true);

			mRegisterCache.ClearCachedReg(span.Register, 0);

			mAvailableRegisters.push_back(native_reg);

			span_it = mActiveIntervals.erase(span_it);
		}
	}

	//
	void SpillAtInterval(const SRegisterSpan &live_span)
	{
#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT(!mActiveIntervals.empty(), "There are no active intervals");
#endif
		const SRegisterSpan &last_span(mActiveIntervals.back()); // Spill the last active interval (it has the greatest end point)

		if (last_span.SpanEnd > live_span.SpanEnd)
		{
			// Uncache the old span
			auto native_reg(mRegisterCache.GetCachedReg(last_span.Register, 0));
			FlushRegister(mRegisterCache, last_span.Register, 0, true);
			mRegisterCache.ClearCachedReg(last_span.Register, 0);

			// Cache the new span
			mRegisterCache.SetCachedReg(live_span.Register, 0, native_reg);

			mActiveIntervals.pop_back();		   // Remove the last span
			mActiveIntervals.push_back(live_span); // Insert in order of increasing end point

			std::sort(mActiveIntervals.begin(), mActiveIntervals.end(), SAscendingSpanEndSort()); // XXXX - will be quicker to insert in the correct place rather than sorting each time
		}
		else
		{
			// There is no space for this register - we just don't update the register cache info, so we save/restore it from memory as needed
		}
	}

	//
	const CN64RegisterCache<NativeReg> &GetRegisterCacheFromHandle(RegisterSnapshotHandle snapshot) const
	{
		DAEDALUS_ASSERT(snapshot.Handle < mRegisterSnapshots.size(), "Invalid snapshot handle");

		return mRegisterSnapshots[snapshot.Handle];
	}

public:
	//
	void UpdateRegisterCaching(u32 instruction_idx) override
	{
		if (!mUseFixedRegisterAllocation)
		{
			ExpireOldIntervals(instruction_idx);

			for (RegisterSpanList::const_iterator span_it = mRegisterSpanList.begin(); span_it < mRegisterSpanList.end(); ++span_it)
			{
				const SRegisterSpan &span(*span_it);

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
							mRegisterCache.SetCachedReg(span.Register, 0, mAvailableRegisters.back());

							// Pop this register from the available list
							mAvailableRegisters.pop_back();
							mActiveIntervals.push_back(span); // Insert in order of increasing end point

							std::sort(mActiveIntervals.begin(), mActiveIntervals.end(), SAscendingSpanEndSort()); // XXXX - will be quicker to insert in the correct place rather than sorting each time
						}
					}
				}
			}
		}
	}

	//

	RegisterSnapshotHandle GetRegisterSnapshot() override
	{
		RegisterSnapshotHandle handle(mRegisterSnapshots.size());

		mRegisterSnapshots.push_back(mRegisterCache);

		return handle;
	}
};

extern "C"
{
	void _EnterDynaRec(const void *p_function, const void *p_base_pointer, const void *p_rebased_mem, u32 mem_limit);
}

#endif // DYNAREC_CODEGENERATOR_H_
