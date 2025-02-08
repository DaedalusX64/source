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

#ifndef SYSW32_DYNAREC_X86_CODEGENERATORX86_H_
#define SYSW32_DYNAREC_X86_CODEGENERATORX86_H_

#include "DynaRec/CodeGenerator.h"
#include "DynaRec/TraceRecorder.h"
#include "AssemblyWriterX86.h"
#include "DynarecTargetX86.h"

// XXXX For GenerateCompare_S/D
#define FLAG_SWAP			0x100
#define FLAG_C_LT		(0x41)					// jne-   le
#define FLAG_C_LE		(FLAG_SWAP|0x41)		// je -   ! gt
#define FLAG_C_EQ		(FLAG_SWAP|0x40)		// je -   ! eq

class CCodeGeneratorX86 : public CCodeGeneratorImpl<EIntelReg>, public CAssemblyWriterX86
{
	public:
		CCodeGeneratorX86( CAssemblyBuffer * p_primary, CAssemblyBuffer * p_secondary );

		virtual void				Initialise( u32 entry_address, u32 exit_address, u32 * hit_counter, const void * p_base, const SRegisterUsageInfo & register_usage );
		virtual void				Finalise( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps, const std::vector<RegisterSnapshotHandle>& exception_handler_snapshots );

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

		virtual CJumpLocation		ExecuteNativeFunction( CCodeLabel speed_hack, bool check_return );

	private:
				void				SetVar( u32 * p_var, u32 value );
				void				SetVar8( u32 * p_var, u8 value );

				CJumpLocation		GenerateBranchAlways( CCodeLabel target );
				CJumpLocation		GenerateBranchIfSet( const u32 * p_var, CCodeLabel target );
				CJumpLocation		GenerateBranchIfNotSet( const u32 * p_var, CCodeLabel target );
				CJumpLocation		GenerateBranchIfEqual32( const u32 * p_var, u32 value, CCodeLabel target );
				CJumpLocation		GenerateBranchIfEqual8( const u32 * p_var, u8 value, CCodeLabel target );
				CJumpLocation		GenerateBranchIfNotEqual32( const u32 * p_var, u32 value, CCodeLabel target );
				CJumpLocation		GenerateBranchIfNotEqual8( const u32 * p_var, u8 value, CCodeLabel target );

				void				GenerateGenericR4300( OpCode op_code, CPU_Instruction p_instruction );

				void				GenerateExceptionHander( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps, const std::vector<RegisterSnapshotHandle>& exception_handler_snapshots );
	private:
				bool				mSpCachedInESI;		// Is sp cached in ESI?
				u32					mSetSpPostUpdate;	// Set Sp base counter after this update

				CAssemblyBuffer *	mpPrimary;
				CAssemblyBuffer *	mpSecondary;

	private:
				void	GenerateLoad(u32 memBase, EN64Reg base, s16 offset, u8 twiddle, u8 bits);
				void	GenerateCACHE( EN64Reg base, s16 offset, u32 cache_op );
				bool	GenerateLW(EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateSW(EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateSWC1( u32 ft, EN64Reg base, s16 offset );
				bool	GenerateLB(EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLBU(EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLH(EN64Reg rt, EN64Reg base, s16 offset );
				bool	GenerateLWC1(u32 ft, EN64Reg base, s16 offset );

				void	GenerateADDIU( EN64Reg rt, EN64Reg rs, s16 immediate );

				void	GenerateJAL( u32 address );
				void	GenerateJR( EN64Reg rs);

				void	GenerateSLL( EN64Reg rd, EN64Reg rt, u32 sa );
				void	GenerateSRL( EN64Reg rd, EN64Reg rt, u32 sa );
				void	GenerateSRA( EN64Reg rd, EN64Reg rt, u32 sa );
};

#endif // SYSW32_DYNAREC_X86_CODEGENERATORX86_H_
