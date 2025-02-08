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



#include "Base/Types.h"


#include "Interface/ConfigOptions.h"
#include "Core/CPU.h"
#include "Core/R4300.h"
#include "Debug/Registers.h"
#include "Debug/DBGConsole.h"
#include "Debug/DebugLog.h"
#include "DynaRec/AssemblyUtils.h"
#include "DynaRec/IndirectExitMap.h"
#include "DynaRec/StaticAnalysis.h"
#include "DynaRec/Trace.h"
#include "Ultra/ultra_R4300.h"

#include "CodeGeneratorX64.h"

using namespace AssemblyUtils;

static const u32		NUM_MIPS_REGISTERS( 32 );
static const u32		INTEL_REG_UNUSED( ~0 );
// XX this optimisation works very well on the PSP, option to disable it was removed
static const bool		gDynarecStackOptimisation = true;
//*****************************************************************************
//	XXXX
//*****************************************************************************
void Dynarec_ClearedCPUStuffToDo()
{
}
void Dynarec_SetCPUStuffToDo()
{
}

static u32				gIntelRegUsageMap[NUM_X64_REGISTERS];
static u32				gWriteCheck[NUM_MIPS_REGISTERS];

static const EIntelReg	gRegistersToUseForCaching[] = {
//	RCX_CODE,
//	RDX_CODE,
	RSI_CODE,
	RDI_CODE,
	RBP_CODE,
};

//*****************************************************************************
//
//*****************************************************************************
CCodeGeneratorX64::CCodeGeneratorX64( CAssemblyBuffer * p_primary, CAssemblyBuffer * p_secondary )
:	CCodeGeneratorImpl<EIntelReg>( gRegistersToUseForCaching )
,	CAssemblyWriterX64( p_primary )
,	mSpCachedInESI( false )
,	mSetSpPostUpdate( 0 )
,	mpPrimary( p_primary )
,	mpSecondary( p_secondary )
{
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeGeneratorX64::Finalise( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps, const std::vector< RegisterSnapshotHandle >& exception_handler_snapshots )
{
	if( !exception_handler_jumps.empty() )
	{
		GenerateExceptionHander( p_exception_handler_fn, exception_handler_jumps, exception_handler_snapshots );
	}

	SetAssemblyBuffer( NULL );
	mpPrimary = NULL;
	mpSecondary = NULL;
}

//*****************************************************************************
//
//*****************************************************************************
#if DAEDALUS_DEBUG_DYNAREC
u32 gNumFragmentsExecuted = 0;
extern "C"
{

void LogFragmentEntry( u32 entry_address )
{
	gNumFragmentsExecuted++;
	if(gNumFragmentsExecuted >= 0x99990)
	{
		printf("Address %08x\n", entry_address );
	}
}

}
#endif

//*****************************************************************************
//
//*****************************************************************************
void	CCodeGeneratorX64::Initialise( u32 entry_address, u32 exit_address, u32 * hit_counter, const void * p_base, const SRegisterUsageInfo & register_usage )
{
#if DAEDALUS_DEBUG_DYNAREC
	MOVI(FIRST_PARAM_REG_CODE, entry_address);
	CALL( CCodeLabel( (void*)LogFragmentEntry ) );
#endif
	// if( hit_counter != NULL )
	// {
	// 	MOV_REG_MEM( RAX_CODE, hit_counter );
	// 	ADDI( RAX_CODE, 1 );
	// 	MOV_MEM_REG( hit_counter, RAX_CODE );
	// }

	// p_base/span_list ignored for now
}

//*****************************************************************************
//
//*****************************************************************************
CCodeLabel	CCodeGeneratorX64::GetEntryPoint() const
{
	return mpPrimary->GetStartAddress();
}

//*****************************************************************************
//
//*****************************************************************************
CCodeLabel	CCodeGeneratorX64::GetCurrentLocation() const
{
	return mpPrimary->GetLabel();
}

//*****************************************************************************
//
//*****************************************************************************
u32	CCodeGeneratorX64::GetCompiledCodeSize() const
{
	return mpPrimary->GetSize() + mpSecondary->GetSize();
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation CCodeGeneratorX64::GenerateExitCode( u32 exit_address, u32 jump_address, u32 num_instructions, CCodeLabel next_fragment )
{
	//DAEDALUS_ASSERT( exit_address != u32( ~0 ), "Invalid exit address" );
	DAEDALUS_ASSERT( !next_fragment.IsSet() || jump_address == 0, "Shouldn't be specifying a jump address if we have a next fragment?" );

#ifdef _DEBUG
	if(exit_address == u32(~0))
	{
		INT3();
	}
#endif

	MOVI(FIRST_PARAM_REG_CODE, num_instructions);
	CALL( CCodeLabel( (void*)CPU_UpdateCounter ) );

	// This jump may be NULL, in which case we patch it below
	// This gets patched with a jump to the next fragment if the target is later found
	CJumpLocation jump_to_next_fragment( GenerateBranchIfNotSet( const_cast< u32 * >( &gCPUState.StuffToDo ), next_fragment ) );

	// If the flag was set, we need in initialise the pc/delay to exit with
	CCodeLabel interpret_next_fragment( GetAssemblyBuffer()->GetLabel() );

	u8		exit_delay;

	if( jump_address != 0 )
	{
		SetVar( &gCPUState.TargetPC, jump_address );
		exit_delay = EXEC_DELAY;
	}
	else
	{
		exit_delay = NO_DELAY;
	}

	SetVar8( &gCPUState.Delay, exit_delay );
	SetVar( &gCPUState.CurrentPC, exit_address );

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
void CCodeGeneratorX64::GenerateEretExitCode( u32 num_instructions, CIndirectExitMap * p_map )
{
	MOVI(FIRST_PARAM_REG_CODE, num_instructions);
	CALL( CCodeLabel( (void*)CPU_UpdateCounter ) );

	// We always exit to the interpreter, regardless of the state of gCPUState.StuffToDo

	// Eret is a bit bodged so we exit at PC + 4
	MOV_REG_MEM( RAX_CODE, &gCPUState.CurrentPC );
	ADDI( RAX_CODE, 4 );
	MOV_MEM_REG( &gCPUState.CurrentPC, RAX_CODE );
	SetVar8( &gCPUState.Delay, NO_DELAY );

	// No need to call CPU_SetPC(), as this is handled by CFragment when we exit

	RET();
}

//*****************************************************************************
// Handle branching back to the interpreter after an indirect jump
//*****************************************************************************
void CCodeGeneratorX64::GenerateIndirectExitCode( u32 num_instructions, CIndirectExitMap * p_map )
{
	MOVI(FIRST_PARAM_REG_CODE, num_instructions);
	CALL( CCodeLabel( (void*) CPU_UpdateCounter ) );

	CCodeLabel		no_target( NULL );
	CJumpLocation	jump_to_next_fragment( GenerateBranchIfNotSet( const_cast< u32 * >( &gCPUState.StuffToDo ), no_target ) );

	CCodeLabel		exit_dynarec( GetAssemblyBuffer()->GetLabel() );
	// New return address is in gCPUState.TargetPC
	MOV_REG_MEM( RAX_CODE, &gCPUState.TargetPC );
	MOV_MEM_REG( &gCPUState.CurrentPC, RAX_CODE );
	SetVar8( &gCPUState.Delay, NO_DELAY );

	// No need to call CPU_SetPC(), as this is handled by CFragment when we exit

	RET();

	// gCPUState.StuffToDo == 0, try to jump to the indirect target
	PatchJumpLong( jump_to_next_fragment, GetAssemblyBuffer()->GetLabel() );

	MOVI_64(FIRST_PARAM_REG_CODE, (uintptr_t)p_map);
	MOV_REG_MEM( SECOND_PARAM_REG_CODE, &gCPUState.TargetPC );
	CALL( CCodeLabel( (void*)IndirectExitMap_Lookup ) );

	// If the target was not found, exit
	TEST( RAX_CODE, RAX_CODE );
	JELong( exit_dynarec );

	JMP_REG( RAX_CODE );
}

//*****************************************************************************
//
//*****************************************************************************
void CCodeGeneratorX64::GenerateExceptionHander( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps, const std::vector< RegisterSnapshotHandle>& exception_handler_snapshots )
{
	CCodeLabel exception_handler( GetAssemblyBuffer()->GetLabel() );

	CALL( CCodeLabel( (void*)p_exception_handler_fn ) );
	RET();

	for( std::vector< CJumpLocation >::const_iterator it = exception_handler_jumps.begin(); it != exception_handler_jumps.end(); ++it )
	{
		CJumpLocation	jump( *it );
		PatchJumpLong( jump, exception_handler );
	}
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeGeneratorX64::SetVar( u32 * p_var, u32 value )
{
	MOVI_MEM( p_var, value );
}

void	CCodeGeneratorX64::SetVar( u32 * p_var, EIntelReg reg )
{
	MOV_MEM_REG(p_var, reg);
}

void CCodeGeneratorX64::GetVar(EIntelReg dst_reg, const u32 *p_var) {
	MOV_REG_MEM(dst_reg, p_var);
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeGeneratorX64::SetVar8( u32 * p_var, u8 value )
{
	MOVI_MEM8( p_var, value );
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeGeneratorX64::GenerateBranchHandler( CJumpLocation branch_handler_jump, RegisterSnapshotHandle snapshot )
{
	PatchJumpLong( branch_handler_jump, GetAssemblyBuffer()->GetLabel() );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX64::GenerateBranchAlways( CCodeLabel target )
{
	return JMPLong( target );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX64::GenerateBranchIfSet( const u32 * p_var, CCodeLabel target )
{
	MOV_REG_MEM( RAX_CODE, p_var );
	TEST( RAX_CODE, RAX_CODE );

	return JNELong( target );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX64::GenerateBranchIfNotSet( const u32 * p_var, CCodeLabel target )
{
	MOV_REG_MEM( RAX_CODE, p_var );
	TEST( RAX_CODE, RAX_CODE );

	return JELong( target );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX64::GenerateBranchIfEqual32( const u32 * p_var, u32 value, CCodeLabel target )
{
	CMP_MEM32_I32( p_var, value );

	return JELong( target );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX64::GenerateBranchIfEqual8( const u32 * p_var, u8 value, CCodeLabel target )
{
	CMP_MEM32_I8( p_var, value );

	return JELong( target );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX64::GenerateBranchIfNotEqual32( const u32 * p_var, u32 value, CCodeLabel target )
{
	CMP_MEM32_I32( p_var, value );

	return JNELong( target );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX64::GenerateBranchIfNotEqual8( const u32 * p_var, u8 value, CCodeLabel target )
{
	CMP_MEM32_I8( p_var, value );

	return JNELong( target );
}

//*****************************************************************************
//	Generates instruction handler for the specified op code.
//	Returns a jump location if an exception handler is required
//*****************************************************************************
CJumpLocation	CCodeGeneratorX64::GenerateOpCode( const STraceEntry& ti, bool branch_delay_slot, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump)
{
	u32 address = ti.Address;
	bool exception = false;
	OpCode op_code = ti.OpCode;

	if (op_code._u32 == 0)
	{
		if( branch_delay_slot )
		{
			SetVar8( &gCPUState.Delay, NO_DELAY );
		}
		return CJumpLocation();
	}

	if( branch_delay_slot )
	{
		SetVar8( &gCPUState.Delay, EXEC_DELAY );
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
		case OP_J:			handled = true; break;
		case OP_JAL:		GenerateJAL( address ); handled = true; break;
		case OP_CACHE:		GenerateCACHE( base, op_code.immediate, rt ); handled = true; break;

		case OP_DADDI:		GenerateDADDIU( rt, rs, s16( op_code.immediate ) );	handled = true; break;
		case OP_DADDIU:		GenerateDADDIU( rt, rs, s16( op_code.immediate ) );	handled = true; break;

		// For LW, SW, SWC1, LB etc, only generate an exception handler if access wasn't done through the stack (handle = false)
		// This will have to be reworked once we handle accesses other than the stack!
		case OP_SW:
			handled = GenerateSW(rt, base, s16(op_code.immediate));
			exception = !handled;
			break;
		case OP_SWC1:
			handled = GenerateSWC1(ft, base, s16(op_code.immediate));
			exception = !handled;
			break;
		case OP_LW:
			handled = GenerateLW(rt, base, s16(op_code.immediate));
			exception = !handled;
			break;
		case OP_LB:
			handled = GenerateLB(rt, base, s16(op_code.immediate));
			exception = !handled;
			break;
		case OP_LBU:
			 handled = GenerateLBU(rt, base, s16(op_code.immediate));
			 exception = !handled;
			break;
		case OP_LH:
			handled = GenerateLH(rt, base, s16(op_code.immediate));
			exception = !handled;
			break;
		case OP_LWC1:
			handled = GenerateLWC1(ft, base, s16(op_code.immediate));
			exception = !handled;
			break;
		case OP_LUI:
			GenerateLUI(rt, s16(op_code.immediate));
			exception = true;
			break;

		case OP_ADDIU:
		case OP_ADDI:
			GenerateADDIU(rt, rs, s16(op_code.immediate)); handled = true; break;
			break;
		case OP_ANDI:
			GenerateANDI(rt, rs, op_code.immediate);
			handled = true;
			break;
		case OP_ORI:
			GenerateORI(rt, rs, op_code.immediate);
			handled = true;
			break;
		case OP_XORI:
			GenerateXORI(rt, rs, op_code.immediate);
			handled = true;
			break;
		case OP_SPECOP:
			{
				switch(op_code.spec_op)
				{
				// case SpecOp_JR:		handled = GenerateJR(rs); break;

				case SpecOp_SLL:	GenerateSLL( rd, rt, sa );	handled = true; break;
				case SpecOp_SRA:	GenerateSRA( rd, rt, sa );	handled = true; break;
				case SpecOp_SRL:	GenerateSRL( rd, rt, sa );	handled = true; break;
				
				case SpecOp_OR:		GenerateOR( rd, rs, rt ); 	handled = true; break;
				case SpecOp_AND:	GenerateAND( rd, rs, rt ); handled = true; break;
				case SpecOp_XOR:	GenerateXOR( rd, rs, rt ); handled = true; break;
				case SpecOp_NOR:	GenerateNOR( rd, rs, rt );	handled = true; break;

				case SpecOp_ADD:
				case SpecOp_ADDU:	GenerateADDU( rd, rs, rt );	handled = true; break;

				case SpecOp_DADD:
				case SpecOp_DADDU:	GenerateDADDU( rd, rs, rt ); handled = true; break;

				case SpecOp_SUB:
				case SpecOp_SUBU:	GenerateSUBU( rd, rs, rt );	handled = true; break;

				case SpecOp_DSUB:
				case SpecOp_DSUBU:	GenerateDSUBU( rd, rs, rt );	handled = true; break;
				}
			}
			break;
	}

	if (!handled)
	{
		if( R4300_InstructionHandlerNeedsPC( op_code ) )
		{
			SetVar( &gCPUState.CurrentPC, address );
			exception = true;
		}
		GenerateGenericR4300( op_code, R4300_GetInstructionHandler( op_code ) );
	}
	CJumpLocation	exception_handler;
	CCodeLabel		no_target( NULL );

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
				*p_branch_jump = GenerateBranchIfNotEqual8( &gCPUState.Delay, DO_DELAY, no_target );
			}
			else
			{
				*p_branch_jump = GenerateBranchIfEqual8( &gCPUState.Delay, DO_DELAY, no_target );
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
				*p_branch_jump = GenerateBranchIfNotEqual32( &gCPUState.TargetPC, p_branch->TargetAddress, no_target );
			}
		}
	}
	else
	{
		if( branch_delay_slot )
		{
			SetVar8( &gCPUState.Delay, NO_DELAY );
		}
	}


	return exception_handler;
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeGeneratorX64::GenerateGenericR4300( OpCode op_code, CPU_Instruction p_instruction )
{
	// XXXX Flush all fp registers before a generic call

	// Call function - __fastcall
	MOVI(FIRST_PARAM_REG_CODE, op_code._u32);
	CALL( CCodeLabel( (void*)p_instruction ) );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation CCodeGeneratorX64::ExecuteNativeFunction( CCodeLabel speed_hack, bool check_return )
{
	CALL( speed_hack );
	if( check_return )
	{
		TEST( RAX_CODE, RAX_CODE );

		return JELong( CCodeLabel(NULL) );
	}
	else
	{
		return CJumpLocation(NULL);
	}
}


void	CCodeGeneratorX64::GenerateCACHE( EN64Reg base, s16 offset, u32 cache_op )
{
	u32 dwCache = cache_op & 0x3;
	u32 dwAction = (cache_op >> 2) & 0x7;

	// For instruction cache invalidation, make sure we let the CPU know so the whole
	// dynarec system can be invalidated
	if(dwCache == 0 && (dwAction == 0 || dwAction == 4))
	{
		MOV_REG_MEM(FIRST_PARAM_REG_CODE, &gCPUState.CPU[base]._u32_0);
		MOVI(SECOND_PARAM_REG_CODE, 0x20);
		ADDI(FIRST_PARAM_REG_CODE, offset);
		CALL( CCodeLabel( reinterpret_cast< const void * >( CPU_InvalidateICacheRange ) ));
	}
	else
	{
		// We don't care about data cache etc
	}
}

void CCodeGeneratorX64::GenerateLoad(EN64Reg base, s16 offset, u8 twiddle, u8 bits)
{
	MOV_REG_MEM(RCX_CODE, &gCPUState.CPU[base]._u32_0);

	if (twiddle == 0)
	{
		DAEDALUS_ASSERT_Q(bits == 32);
		ADD(RCX_CODE, R15_CODE, true);
		MOV_REG_MEM_BASE_OFFSET(RAX_CODE, RCX_CODE, offset);
	}
	else
	{
		ADDI(RCX_CODE, offset, true);
		XORI(RCX_CODE, twiddle, true);
		ADD(RCX_CODE, R15_CODE, true);
		switch(bits)
		{
		case 32:
			MOV_REG_MEM_BASE(RAX_CODE, RCX_CODE);
			break;
		case 16:
			MOV16_REG_MEM_BASE(RAX_CODE, RCX_CODE);
			break;
		case 8:
			MOV8_REG_MEM_BASE(RAX_CODE, RCX_CODE);
			break;
		}
	}
}

bool CCodeGeneratorX64::GenerateLW( EN64Reg rt, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		GenerateLoad(base, offset, 0, 32);

		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_0, RAX_CODE);
		CDQ();
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_1, RDX_CODE);

		return true;
	}
	return false;
}


bool CCodeGeneratorX64::GenerateSWC1( u32 ft, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		MOV_REG_MEM(RCX_CODE, &gCPUState.CPU[base]._u32_0);
		ADD(RCX_CODE, R15_CODE, true);

		MOV_REG_MEM(RAX_CODE, &gCPUState.FPU[ft]._u32);
		MOV_MEM_BASE_OFFSET_REG(RCX_CODE, offset, RAX_CODE);
		return true;
	}

	return false;
}

//u32 address = (u32)( gGPR[op_code.base]._s32_0 + (s32)(s16)op_code.immediate );
//	Write32Bits(address, gGPR[op_code.rt]._u32_0);
bool CCodeGeneratorX64::GenerateSW( EN64Reg rt, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		MOV_REG_MEM(RCX_CODE, &gCPUState.CPU[base]._u32_0);
		ADD(RCX_CODE, R15_CODE, true);
		MOV_REG_MEM(RAX_CODE, &gCPUState.CPU[rt]._u32_0);
		MOV_MEM_BASE_OFFSET_REG(RCX_CODE, offset, RAX_CODE);
		return true;
	}

	return false;
}

bool CCodeGeneratorX64::GenerateLB( EN64Reg rt, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		GenerateLoad(base, offset, U8_TWIDDLE, 8);
		MOVSX(RAX_CODE, RAX_CODE, true);
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_0, RAX_CODE);
		CDQ();
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_1, RDX_CODE);

		return true;
	}

	return false;
}

bool CCodeGeneratorX64::GenerateLBU( EN64Reg rt, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		GenerateLoad(base, offset, U8_TWIDDLE, 8);
		MOVZX(RAX_CODE, RAX_CODE, true);
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_0, RAX_CODE);
		XOR(RDX_CODE, RDX_CODE);
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_1, RDX_CODE);
		return true;
	}

	return false;
}


bool CCodeGeneratorX64::GenerateLH( EN64Reg rt, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		GenerateLoad(base, offset, U16_TWIDDLE, 16);

		MOVSX(RAX_CODE, RAX_CODE, false);
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_0, RAX_CODE);
		CDQ();
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_1, RDX_CODE);
		return true;
	}

	return false;
}

//gGPR[op_code.rt]._s64 = (s64)(s32)((s32)(s16)op_code.immediate<<16);
void CCodeGeneratorX64::GenerateLUI( EN64Reg rt, s16 immediate )
{
	if (rt == 0) return;

	MOVI(RAX_CODE, s32(immediate) << 16);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[rt]._u32_0, RAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[rt]._u32_1, RDX_CODE);
}

//gGPR[op_code.rt]._s64 = gGPR[op_code.rs]._s64 + (s32)(s16)op_code.immediate;
void CCodeGeneratorX64::GenerateDADDIU( EN64Reg rt, EN64Reg rs, s16 immediate )
{
	if (rt == 0) return;

	MOV64_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u64);
	ADDI(RAX_CODE, immediate, true);
	MOV64_MEM_REG(&gCPUState.CPU[rt]._u64, RAX_CODE);
}

// gGPR[op_code.rt]._s64 = (s64)(s32)(gGPR[op_code.rs]._s32_0 + (s32)(s16)op_code.immediate);
void CCodeGeneratorX64::GenerateADDIU( EN64Reg rt, EN64Reg rs, s16 immediate )
{
	if (rt == 0) return;

	MOV_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u32_0);
	ADDI(RAX_CODE, immediate);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[rt]._u32_0, RAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[rt]._u32_1, RDX_CODE);
}

//gGPR[op_code.rt]._u64 = gGPR[op_code.rs]._u64 & (u64)(u16)op_code.immediate;
void CCodeGeneratorX64::GenerateANDI( EN64Reg rt, EN64Reg rs, u16 immediate )
{
	if (rt == 0) return;

	MOV64_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u64);
	ANDI(RAX_CODE, immediate, true);
	MOV64_MEM_REG(&gCPUState.CPU[rt]._u64, RAX_CODE);
}

//gGPR[op_code.rt]._u64 = gGPR[op_code.rs]._u64 | (u64)(u16)op_code.immediate;
void CCodeGeneratorX64::GenerateORI( EN64Reg rt, EN64Reg rs, u16 immediate )
{
	if (rt == 0) return;

	MOV64_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u64);
	ORI(RAX_CODE, immediate, true);
	MOV64_MEM_REG(&gCPUState.CPU[rt]._u64, RAX_CODE);
}

// gGPR[op_code.rt]._u64 = gGPR[op_code.rs]._u64 ^ (u64)(u16)op_code.immediate;
void CCodeGeneratorX64::GenerateXORI( EN64Reg rt, EN64Reg rs, u16 immediate )
{
	if (rt == 0) return;

	MOV64_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u64);
	XORI(RAX_CODE, immediate, true);
	MOV64_MEM_REG(&gCPUState.CPU[rt]._u64, RAX_CODE);
}

// gGPR[ op_code.rd ]._s64 = (s64)(s32)( (gGPR[ op_code.rt ]._u32_0 << op_code.sa) & 0xFFFFFFFF );
void CCodeGeneratorX64::GenerateSLL( EN64Reg rd, EN64Reg rt, u32 sa )
{
	// NOP
	if (rd == 0) return;

	MOV_REG_MEM(RAX_CODE, &gCPUState.CPU[rt]._u32_0);
	SHLI(RAX_CODE, sa);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_0, RAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_1, RDX_CODE);
}

// gGPR[ op_code.rd ]._s64 = (s64)(s32)( gGPR[ op_code.rt ]._u32_0 >> op_code.sa );
void CCodeGeneratorX64::GenerateSRL( EN64Reg rd, EN64Reg rt, u32 sa )
{
	if (rd == 0) return;

	MOV_REG_MEM(RAX_CODE, &gCPUState.CPU[rt]._u32_0);
	SHRI(RAX_CODE, sa);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_0, RAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_1, RDX_CODE);
}

//gGPR[ op_code.rd ]._s64 = (s64)(s32)( gGPR[ op_code.rt ]._s32_0 >> op_code.sa );
void CCodeGeneratorX64::GenerateSRA( EN64Reg rd, EN64Reg rt, u32 sa )
{
	if (rd == 0) return;

	MOV_REG_MEM(RAX_CODE, &gCPUState.CPU[rt]._u32_0);
	SARI(RAX_CODE, sa);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_0, RAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_1, RDX_CODE);
}

//gGPR[ op_code.rd ]._u64 = gGPR[ op_code.rs ]._u64 | gGPR[ op_code.rt ]._u64;
void CCodeGeneratorX64::GenerateOR( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	if (rd == 0) return;

	MOV64_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u64);
	MOV64_REG_MEM(RCX_CODE, &gCPUState.CPU[rt]._u64);
	OR(RAX_CODE, RCX_CODE, true);
	MOV64_MEM_REG(&gCPUState.CPU[rd]._u64, RAX_CODE);
}

//gGPR[ op_code.rd ]._u64 = gGPR[ op_code.rs ]._u64 & gGPR[ op_code.rt ]._u64;
void CCodeGeneratorX64::GenerateAND( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	if (rd == 0) return;

	MOV64_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u64);
	MOV64_REG_MEM(RCX_CODE, &gCPUState.CPU[rt]._u64);
	AND(RAX_CODE, RCX_CODE, true);
	MOV64_MEM_REG(&gCPUState.CPU[rd]._u64, RAX_CODE);
}

//gGPR[ op_code.rd ]._u64 = gGPR[ op_code.rs ]._u64 ^ gGPR[ op_code.rt ]._u64;
void CCodeGeneratorX64::GenerateXOR( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	if (rd == 0) return;

	MOV64_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u64);
	MOV64_REG_MEM(RCX_CODE, &gCPUState.CPU[rt]._u64);
	XOR(RAX_CODE, RCX_CODE, true);
	MOV64_MEM_REG(&gCPUState.CPU[rd]._u64, RAX_CODE);
}

//gGPR[ op_code.rd ]._u64 = ~(gGPR[ op_code.rs ]._u64 | gGPR[ op_code.rt ]._u64);
void CCodeGeneratorX64::GenerateNOR( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	if (rd == 0) return;

	MOV64_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u64);
	MOV64_REG_MEM(RCX_CODE, &gCPUState.CPU[rt]._u64);
	OR(RAX_CODE, RCX_CODE, true);
	NOT(RAX_CODE, true);
	MOV64_MEM_REG(&gCPUState.CPU[rd]._u64, RAX_CODE);
}

// gGPR[ op_code.rd ]._s64 = (s64)(s32)( gGPR[ op_code.rs ]._s32_0 + gGPR[ op_code.rt ]._s32_0 );
void CCodeGeneratorX64::GenerateADDU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	if (rd == 0) return;

	MOV_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u32_0);
	MOV_REG_MEM(RCX_CODE, &gCPUState.CPU[rt]._u32_0);
	ADD(RAX_CODE, RCX_CODE);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_0, RAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_1, RDX_CODE);
}

// 	gGPR[ op_code.rd ]._u64 = gGPR[ op_code.rs ]._u64 + gGPR[ op_code.rt ]._u64;
void CCodeGeneratorX64::GenerateDADDU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	if (rd == 0) return;

	MOV64_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u64);
	MOV64_REG_MEM(RCX_CODE, &gCPUState.CPU[rt]._u64);
	ADD(RAX_CODE, RCX_CODE, true);
	MOV64_MEM_REG(&gCPUState.CPU[rd]._u64, RAX_CODE);
}

//	gGPR[ op_code.rd ]._s64 = (s64)(s32)( gGPR[ op_code.rs ]._s32_0 - gGPR[ op_code.rt ]._s32_0 );
void CCodeGeneratorX64::GenerateSUBU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	if (rd == 0) return;

	MOV_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u32_0);
	MOV_REG_MEM(RCX_CODE, &gCPUState.CPU[rt]._u32_0);
	SUB(RAX_CODE, RCX_CODE);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_0, RAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_1, RDX_CODE);
}

//gGPR[ op_code.rd ]._u64 = gGPR[ op_code.rs ]._u64 - gGPR[ op_code.rt ]._u64;
void CCodeGeneratorX64::GenerateDSUBU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	if (rd == 0) return;

	MOV64_REG_MEM(RAX_CODE, &gCPUState.CPU[rs]._u64);
	MOV64_REG_MEM(RCX_CODE, &gCPUState.CPU[rt]._u64);
	SUB(RAX_CODE, RCX_CODE, true);
	MOV64_MEM_REG(&gCPUState.CPU[rd]._u64, RAX_CODE);
}

bool CCodeGeneratorX64::GenerateLWC1( u32 ft, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		GenerateLoad(base, offset, 0, 32);

		MOV_MEM_REG(&gCPUState.FPU[ft]._u32, RAX_CODE);
		return true;
	}

	return false;
}

void	CCodeGeneratorX64::GenerateJAL( u32 address )
{
	MOVI(RAX_CODE, address + 8);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[N64Reg_RA]._u32_0, RAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[N64Reg_RA]._u32_1, RDX_CODE);
}

void	CCodeGeneratorX64::GenerateJR( EN64Reg rs)
{
	//TODO
}
