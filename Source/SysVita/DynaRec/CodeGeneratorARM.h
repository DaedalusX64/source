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

#pragma once

#include "DynaRec/CodeGenerator.h"
#include "AssemblyWriterARM.h"
#include "DynarecTargetARM.h"
#include "DynaRec/TraceRecorder.h"
#include "N64RegisterCacheARM.h"
#include <stack>

// XXXX For GenerateCompare_S/D
#define FLAG_SWAP			0x100
#define FLAG_C_LT		(0x41)					// jne-   le
#define FLAG_C_LE		(FLAG_SWAP|0x41)		// je -   ! gt
#define FLAG_C_EQ		(FLAG_SWAP|0x40)		// je -   ! eq

typedef u32 (*ReadMemoryFunction)( u32 address );

class CCodeGeneratorARM : public CCodeGenerator, public CAssemblyWriterARM
{
	public:
		CCodeGeneratorARM( CAssemblyBuffer * p_primary, CAssemblyBuffer * p_secondary );

		virtual void				Initialise( u32 entry_address, u32 exit_address, u32 * hit_counter, const void * p_base, const SRegisterUsageInfo & register_usage );
		void                        SetRegisterSpanList(const SRegisterUsageInfo& register_usage, bool loops_to_self);
		virtual void				Finalise( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps, const std::vector< RegisterSnapshotHandle>& exception_handler_snapshots );

		virtual void				UpdateRegisterCaching( u32 instruction_idx );

		virtual RegisterSnapshotHandle	GetRegisterSnapshot();

		virtual CCodeLabel			GetEntryPoint() const;
		virtual CCodeLabel			GetCurrentLocation() const;
		virtual u32					GetCompiledCodeSize() const;

		virtual	CJumpLocation		GenerateExitCode( u32 exit_address, u32 jump_address, u32 num_instructions, CCodeLabel next_fragment );
		virtual void				GenerateEretExitCode( u32 num_instructions, CIndirectExitMap * p_map );
		virtual void				GenerateIndirectExitCode( u32 num_instructions, CIndirectExitMap * p_map );

		virtual void				GenerateBranchHandler( CJumpLocation branch_handler_jump, RegisterSnapshotHandle snapshot );

		virtual CJumpLocation		GenerateOpCode( const STraceEntry& ti, bool branch_delay_slot, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump);
		virtual CJumpLocation		GenerateInterpOpCode( const STraceEntry& ti, bool branch_delay_slot, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump);
		
		virtual CJumpLocation		ExecuteNativeFunction( CCodeLabel speed_hack, bool check_return );

	private:
				void                ExpireOldIntervals(u32 instruction_idx);
				void                SpillAtInterval(const SRegisterSpan& live_span);
				void                GetVar(EArmReg arm_reg, const u32* p_var);
				void				SetVar( const u32 * p_var, u32 value );
				void				SetVar(const u32* p_var, EArmReg reg);
				void				SetFloatVar(const f32* p_var, EArmVfpReg reg);
				void				GetFloatVar( EArmVfpReg dst_reg, const f32 * p_var );
				void				SetDoubleVar(const f64* p_var, EArmVfpReg reg);
				void				GetDoubleVar( EArmVfpReg dst_reg, const f64 * p_var );
				
				EArmReg				GetRegisterNoLoad( EN64Reg n64_reg, u32 lo_hi_idx, EArmReg scratch_reg );
				EArmReg				GetRegisterNoLoadLo( EN64Reg n64_reg, EArmReg scratch_reg )		{ return GetRegisterNoLoad( n64_reg, 0, scratch_reg ); }
				EArmReg				GetRegisterNoLoadHi( EN64Reg n64_reg, EArmReg scratch_reg )		{ return GetRegisterNoLoad( n64_reg, 1, scratch_reg ); }

				EArmReg				GetRegisterAndLoad( EN64Reg n64_reg, u32 lo_hi_idx, EArmReg scratch_reg );
				EArmReg				GetRegisterAndLoadLo( EN64Reg n64_reg, EArmReg scratch_reg )	{ return GetRegisterAndLoad( n64_reg, 0, scratch_reg ); }
				EArmReg				GetRegisterAndLoadHi( EN64Reg n64_reg, EArmReg scratch_reg )	{ return GetRegisterAndLoad( n64_reg, 1, scratch_reg ); }

				void                GetRegisterValue(EArmReg arm_reg, EN64Reg n64_reg, u32 lo_hi_idx);
				void                LoadRegister( EArmReg arm_reg, EN64Reg n64_reg, u32 lo_hi_idx );
				void                LoadRegisterLo( EArmReg arm_reg, EN64Reg n64_reg ) { LoadRegister(arm_reg, n64_reg, 0); }
				void                LoadRegisterHi( EArmReg arm_reg, EN64Reg n64_reg ) { LoadRegister(arm_reg, n64_reg, 1); }
				void                PrepareCachedRegister(EN64Reg n64_reg, u32 lo_hi_idx);
				void				PrepareCachedRegisterLo(EN64Reg n64_reg) { PrepareCachedRegister(n64_reg, 0); }
				void				PrepareCachedRegisterHi(EN64Reg n64_reg) { PrepareCachedRegister(n64_reg, 1); }
				void                FlushRegister(CN64RegisterCacheARM& cache, EN64Reg n64_reg, u32 lo_hi_idx, bool invalidate);
				void                FlushAllRegisters(CN64RegisterCacheARM& cache, bool invalidate);
				void                FlushAllFloatingPointRegisters( CN64RegisterCacheARM & cache, bool invalidate );
				void                RestoreAllRegisters(CN64RegisterCacheARM& current_cache, CN64RegisterCacheARM& new_cache);
				void                UpdateRegister(EN64Reg n64_reg, EArmReg  arm_reg, bool options);
				EArmVfpReg          GetFloatRegisterAndLoad( EN64FloatReg n64_reg );
				EArmVfpReg          GetDoubleRegisterAndLoad( EN64FloatReg n64_reg );
				void                UpdateFloatRegister( EN64FloatReg n64_reg );
				void                UpdateDoubleRegister( EN64FloatReg n64_reg );
				
				const CN64RegisterCacheARM& GetRegisterCacheFromHandle(RegisterSnapshotHandle snapshot) const;


				void				StoreRegister(EN64Reg n64_reg, u32 lo_hi_idx, EArmReg arm_reg);
				void				StoreRegisterLo(EN64Reg n64_reg, EArmReg arm_reg) { StoreRegister(n64_reg, 0, arm_reg); }
				void				StoreRegisterHi(EN64Reg n64_reg, EArmReg arm_reg) { StoreRegister(n64_reg, 1, arm_reg); }

				void				SetRegister64(EN64Reg n64_reg, s32 lo_value, s32 hi_value);

				void				SetRegister32s(EN64Reg n64_reg, s32 value);

				void				SetRegister(EN64Reg n64_reg, u32 lo_hi_idx, u32 value);
				CJumpLocation		GenerateBranchAlways( CCodeLabel target );
				CJumpLocation		GenerateBranchIfSet( const u32 * p_var, CCodeLabel target );
				CJumpLocation		GenerateBranchIfNotSet( const u32 * p_var, CCodeLabel target );
				CJumpLocation		GenerateBranchIfEqual( const u32 * p_var, u32 value, CCodeLabel target );
				CJumpLocation		GenerateBranchIfNotEqual( const u32 * p_var, u32 value, CCodeLabel target );
				CJumpLocation		GenerateBranchIfNotEqual( EArmReg reg_a, u32 value, CCodeLabel target );

				void				GenerateGenericR4300( OpCode op_code, CPU_Instruction p_instruction );

				void				GenerateExceptionHander( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps, const std::vector< RegisterSnapshotHandle>& exception_handler_snapshots );

				bool				mSpCachedInESI;		// Is sp cached in ESI?
				u32					mSetSpPostUpdate;	// Set Sp base counter after this update

				u32					mEntryAddress;
				CAssemblyBuffer *	mpPrimary;
				CAssemblyBuffer *	mpSecondary;
				RegisterSpanList	mRegisterSpanList;
				// For register allocation
				RegisterSpanList	mActiveIntervals;
				std::stack<EArmReg>	mAvailableRegisters;
				CCodeLabel			mLoopTop;
				bool				mUseFixedRegisterAllocation;
				std::vector< CN64RegisterCacheARM >	mRegisterSnapshots;
				CN64RegisterCacheARM	mRegisterCache;
				bool mQuickLoad;
				bool mFloatCMPIsValid;
				bool mMultIsValid;

	private:
				bool	GenerateCACHE( EN64Reg base, s16 offset, u32 cache_op );

				typedef u32 (*ReadMemoryFunction)( u32 address, u32 current_pc );
				typedef void (*WriteMemoryFunction)( u32 address, u32 value, u32 current_pc );
				
				void	GenerateStore( u32 address, EArmReg arm_src, EN64Reg base, s16 offset, u8 twiddle, u8 bits, WriteMemoryFunction p_write_memory );
				bool	GenerateSW(u32 address, bool branch_delay_slot, EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateSWC1( u32 address, bool branch_delay_slot, u32 ft, EN64Reg base, s16 offset );
				bool	GenerateSDC1( u32 address, bool branch_delay_slot, u32 ft, EN64Reg base, s16 offset );
				bool	GenerateSH( u32 address, bool branch_delay_slot, EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateSD( u32 address, bool branch_delay_slot, EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateSB( u32 address, bool branch_delay_slot, EN64Reg rt, EN64Reg base, s16 offset );

				void	GenerateLoad( u32 address, EArmReg arm_dest, EN64Reg base, s16 offset, u8 twiddle, u8 bits, bool is_signed, ReadMemoryFunction p_read_memory );
				bool	GenerateLW( u32 address, bool branch_delay_slot, EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLD( u32 address, bool branch_delay_slot, EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLB( u32 address, bool branch_delay_slot, EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLBU( u32 address, bool branch_delay_slot, EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLH( u32 address, bool branch_delay_slot, EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLHU( u32 address, bool branch_delay_slot, EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLWC1( u32 address, bool branch_delay_slot, u32 ft, EN64Reg base, s16 offset );
				bool	GenerateLDC1( u32 address, bool branch_delay_slot, u32 ft, EN64Reg base, s16 offset );
				void	GenerateLUI( EN64Reg rt, s16 immediate );

				void	GenerateADDIU( EN64Reg rt, EN64Reg rs, s16 immediate );
				void	GenerateANDI( EN64Reg rt, EN64Reg rs, u16 immediate );
				void	GenerateORI( EN64Reg rt, EN64Reg rs, u16 immediate );
				void	GenerateXORI( EN64Reg rt, EN64Reg rs, u16 immediate );
				void	GenerateSLTI( EN64Reg rt, EN64Reg rs, s16 immediate, bool is_unsigned );

				void	GenerateDADDIU( EN64Reg rt, EN64Reg rs, s16 immediate );
				void	GenerateDADDU( EN64Reg rd, EN64Reg rs, EN64Reg rt );
				void	GenerateDSUBU( EN64Reg rd, EN64Reg rs, EN64Reg rt );

				void	GenerateJAL( u32 address );
				void	GenerateJALR( EN64Reg rs, EN64Reg rd, u32 address, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );

				//Special Op
				void 	GenerateMFC0( EN64Reg rt, u32 fs );
				void	GenerateMFC1( EN64Reg rt, u32 fs );
				void	GenerateMTC1( u32 fs, EN64Reg rt );
				void	GenerateCFC1( EN64Reg rt, u32 fs );
				void	GenerateCTC1( u32 fs, EN64Reg rt );

				void	GenerateSLL( EN64Reg rd, EN64Reg rt, u32 sa );
				void	GenerateSRL( EN64Reg rd, EN64Reg rt, u32 sa );
				void	GenerateSRA( EN64Reg rd, EN64Reg rt, u32 sa );
				void	GenerateSLLV( EN64Reg rd, EN64Reg rs, EN64Reg rt );
				void	GenerateSRLV( EN64Reg rd, EN64Reg rs, EN64Reg rt );
				void	GenerateSRAV( EN64Reg rd, EN64Reg rs, EN64Reg rt );

				void	GenerateOR( EN64Reg rd, EN64Reg rs, EN64Reg rt );
				void	GenerateAND( EN64Reg rd, EN64Reg rs, EN64Reg rt );
				void	GenerateXOR( EN64Reg rd, EN64Reg rs, EN64Reg rt );
				void	GenerateNOR( EN64Reg rd, EN64Reg rs, EN64Reg rt );

				void	GenerateJR( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );

				void	GenerateADDU( EN64Reg rd, EN64Reg rs, EN64Reg rt );
				void	GenerateSUBU( EN64Reg rd, EN64Reg rs, EN64Reg rt );

				void	GenerateMULT( EN64Reg rs, EN64Reg rt, bool is_unsigned );

				void	GenerateDIV( EN64Reg rs, EN64Reg rt );
				void	GenerateDIVU( EN64Reg rs, EN64Reg rt );

				void	GenerateMFLO( EN64Reg rd );
				void	GenerateMFHI( EN64Reg rd );
				void	GenerateMTLO( EN64Reg rs );
				void	GenerateMTHI( EN64Reg rs );

				void	GenerateSLT( EN64Reg rd, EN64Reg rs, EN64Reg rt , bool is_unsigned );

				//Branch
				void	GenerateBEQ( EN64Reg rs, EN64Reg rt, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );
				void	GenerateBNE( EN64Reg rs, EN64Reg rt, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );
				void 	GenerateBLEZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );
				void 	GenerateBGEZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );
				void 	GenerateBLTZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );
				void 	GenerateBGTZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump );

				// CoPro1
				void	GenerateADD_S( u32 fd, u32 fs, u32 ft );
				void	GenerateSUB_S( u32 fd, u32 fs, u32 ft );
				void	GenerateMUL_S( u32 fd, u32 fs, u32 ft );
				void	GenerateDIV_S( u32 fd, u32 fs, u32 ft );
				void	GenerateSQRT_S( u32 fd, u32 fs );
				void	GenerateABS_S( u32 fd, u32 fs );
				void	GenerateMOV_S( u32 fd, u32 fs );
				void	GenerateNEG_S( u32 fd, u32 fs );
				void	GenerateTRUNC_W_S( u32 fd, u32 fs );
				void	GenerateCVT_D_S( u32 fd, u32 fs );
				void	GenerateCMP_S( u32 fs, u32 ft, EArmCond cond, u8 E );

				void	GenerateADD_D( u32 fd, u32 fs, u32 ft );
				void	GenerateSUB_D( u32 fd, u32 fs, u32 ft );
				void	GenerateMUL_D( u32 fd, u32 fs, u32 ft );
				void	GenerateDIV_D( u32 fd, u32 fs, u32 ft );
				void	GenerateSQRT_D( u32 fd, u32 fs );
				void	GenerateABS_D( u32 fd, u32 fs );
				void	GenerateMOV_D( u32 fd, u32 fs );
				void	GenerateNEG_D( u32 fd, u32 fs );
				void	GenerateTRUNC_W_D( u32 fd, u32 fs );
				void	GenerateCVT_S_D( u32 fd, u32 fs );
				void	GenerateCMP_D( u32 fs, u32 ft, EArmCond cond, u8 E );
};