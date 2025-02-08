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

#include "CodeGeneratorX86.h"

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

static u32				gIntelRegUsageMap[NUM_X86_REGISTERS];
static u32				gWriteCheck[NUM_MIPS_REGISTERS];

//*****************************************************************************
//
//*****************************************************************************
CCodeGeneratorX86::CCodeGeneratorX86( CAssemblyBuffer * p_primary, CAssemblyBuffer * p_secondary )
:	CCodeGeneratorImpl<EIntelReg>( )
,	CAssemblyWriterX86( p_primary )
,	mSpCachedInESI( false )
,	mSetSpPostUpdate( 0 )
,	mpPrimary( p_primary )
,	mpSecondary( p_secondary )
{
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeGeneratorX86::Finalise( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps )
{
	if( !exception_handler_jumps.empty() )
	{
		GenerateExceptionHander( p_exception_handler_fn, exception_handler_jumps );
	}

	SetAssemblyBuffer( NULL );
	mpPrimary = NULL;
	mpSecondary = NULL;
}

//*****************************************************************************
//
//*****************************************************************************
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
		snprintf( buffer, sizeof(buffer), "Address %08x\n", entry_address );
		OutputDebugString( buffer );
	}
}

}
#endif

//*****************************************************************************
//
//*****************************************************************************
void	CCodeGeneratorX86::Initialise( u32 entry_address, u32 exit_address, u32 * hit_counter, const void * p_base, const SRegisterUsageInfo & register_usage )
{
	//MOVI(ECX_CODE, entry_address);
	// CALL( CCodeLabel( LogFragmentEntry ) );

	if( hit_counter != NULL )
	{
		MOV_REG_MEM( EAX_CODE, hit_counter );
		ADDI( EAX_CODE, 1 );
		MOV_MEM_REG( hit_counter, EAX_CODE );
	}

	// p_base/span_list ignored for now
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeGeneratorX86::UpdateRegisterCaching( u32 instruction_idx )
{
	// This is ignored for now
}

//*****************************************************************************
//
//*****************************************************************************
RegisterSnapshotHandle	CCodeGeneratorX86::GetRegisterSnapshot()
{
	// This doesn't do anything useful yet.
	return RegisterSnapshotHandle( 0 );
}

//*****************************************************************************
//
//*****************************************************************************
CCodeLabel	CCodeGeneratorX86::GetEntryPoint() const
{
	return mpPrimary->GetStartAddress();
}

//*****************************************************************************
//
//*****************************************************************************
CCodeLabel	CCodeGeneratorX86::GetCurrentLocation() const
{
	return mpPrimary->GetLabel();
}

//*****************************************************************************
//
//*****************************************************************************
u32	CCodeGeneratorX86::GetCompiledCodeSize() const
{
	return mpPrimary->GetSize() + mpSecondary->GetSize();
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation CCodeGeneratorX86::GenerateExitCode( u32 exit_address, u32 jump_address, u32 num_instructions, CCodeLabel next_fragment )
{
	//DAEDALUS_ASSERT( exit_address != u32( ~0 ), "Invalid exit address" );
	DAEDALUS_ASSERT( !next_fragment.IsSet() || jump_address == 0, "Shouldn't be specifying a jump address if we have a next fragment?" );

#ifdef _DEBUG
	if(exit_address == u32(~0))
	{
		INT3();
	}
#endif

	MOVI(ECX_CODE, num_instructions);
	CALL( CCodeLabel( CPU_UpdateCounter ) );

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
void CCodeGeneratorX86::GenerateEretExitCode( u32 num_instructions, CIndirectExitMap * p_map )
{
	MOVI(ECX_CODE, num_instructions);
	CALL( CCodeLabel( CPU_UpdateCounter ) );

	// We always exit to the interpreter, regardless of the state of gCPUState.StuffToDo

	// Eret is a bit bodged so we exit at PC + 4
	MOV_REG_MEM( EAX_CODE, &gCPUState.CurrentPC );
	ADDI( EAX_CODE, 4 );
	MOV_MEM_REG( &gCPUState.CurrentPC, EAX_CODE );
	SetVar8( &gCPUState.Delay, NO_DELAY );

	// No need to call CPU_SetPC(), as this is handled by CFragment when we exit

	RET();
}

//*****************************************************************************
// Handle branching back to the interpreter after an indirect jump
//*****************************************************************************
void CCodeGeneratorX86::GenerateIndirectExitCode( u32 num_instructions, CIndirectExitMap * p_map )
{
	MOVI(ECX_CODE, num_instructions);
	CALL( CCodeLabel( CPU_UpdateCounter ) );

	CCodeLabel		no_target( NULL );
	CJumpLocation	jump_to_next_fragment( GenerateBranchIfNotSet( const_cast< u32 * >( &gCPUState.StuffToDo ), no_target ) );

	CCodeLabel		exit_dynarec( GetAssemblyBuffer()->GetLabel() );
	// New return address is in gCPUState.TargetPC
	MOV_REG_MEM( EAX_CODE, &gCPUState.TargetPC );
	MOV_MEM_REG( &gCPUState.CurrentPC, EAX_CODE );
	SetVar8( &gCPUState.Delay, NO_DELAY );

	// No need to call CPU_SetPC(), as this is handled by CFragment when we exit

	RET();

	// gCPUState.StuffToDo == 0, try to jump to the indirect target
	PatchJumpLong( jump_to_next_fragment, GetAssemblyBuffer()->GetLabel() );

	MOVI( ECX_CODE, reinterpret_cast< u32 >( p_map ) );
	MOV_REG_MEM( EDX_CODE, &gCPUState.TargetPC );
	CALL( CCodeLabel( IndirectExitMap_Lookup ) );

	// If the target was not found, exit
	TEST( EAX_CODE, EAX_CODE );
	JELong( exit_dynarec );

	JMP_REG( EAX_CODE );
}

//*****************************************************************************
//
//*****************************************************************************
void CCodeGeneratorX86::GenerateExceptionHander( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps )
{
	CCodeLabel exception_handler( GetAssemblyBuffer()->GetLabel() );

	CALL( CCodeLabel( p_exception_handler_fn ) );
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
void	CCodeGeneratorX86::SetVar( u32 * p_var, u32 value )
{
	MOVI_MEM( p_var, value );
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeGeneratorX86::SetVar8( u32 * p_var, u8 value )
{
	MOVI_MEM8( p_var, value );
}

//*****************************************************************************
//
//*****************************************************************************
void	CCodeGeneratorX86::GenerateBranchHandler( CJumpLocation branch_handler_jump, RegisterSnapshotHandle snapshot )
{
	PatchJumpLong( branch_handler_jump, GetAssemblyBuffer()->GetLabel() );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX86::GenerateBranchAlways( CCodeLabel target )
{
	return JMPLong( target );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX86::GenerateBranchIfSet( const u32 * p_var, CCodeLabel target )
{
	MOV_REG_MEM( EAX_CODE, p_var );
	TEST( EAX_CODE, EAX_CODE );

	return JNELong( target );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX86::GenerateBranchIfNotSet( const u32 * p_var, CCodeLabel target )
{
	MOV_REG_MEM( EAX_CODE, p_var );
	TEST( EAX_CODE, EAX_CODE );

	return JELong( target );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX86::GenerateBranchIfEqual32( const u32 * p_var, u32 value, CCodeLabel target )
{
	CMP_MEM32_I32( p_var, value );

	return JELong( target );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX86::GenerateBranchIfEqual8( const u32 * p_var, u8 value, CCodeLabel target )
{
	CMP_MEM32_I8( p_var, value );

	return JELong( target );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX86::GenerateBranchIfNotEqual32( const u32 * p_var, u32 value, CCodeLabel target )
{
	CMP_MEM32_I32( p_var, value );

	return JNELong( target );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CCodeGeneratorX86::GenerateBranchIfNotEqual8( const u32 * p_var, u8 value, CCodeLabel target )
{
	CMP_MEM32_I8( p_var, value );

	return JNELong( target );
}

//*****************************************************************************
//	Generates instruction handler for the specified op code.
//	Returns a jump location if an exception handler is required
//*****************************************************************************
CJumpLocation	CCodeGeneratorX86::GenerateOpCode( const STraceEntry& ti, bool branch_delay_slot, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump)
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

		// For LW, SW, SWC1, LB etc, only generate an exception handler if access wasn't done through the stack (handle = false)
		// This will have to be reworked once we handle accesses other than the stack!
		case OP_LW:
			handled = GenerateLW(rt, base, s16(op_code.immediate));
			exception = !handled;
			break;
		case OP_SW:
			handled = GenerateSW(rt, base, s16(op_code.immediate));
			exception = !handled;
			break;
		case OP_SWC1:
			handled = GenerateSWC1(ft, base, s16(op_code.immediate));
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
		case OP_ADDIU:
		case OP_ADDI:
			GenerateADDIU(rt, rs, s16(op_code.immediate)); handled = true; break;
			break;

		//case OP_SPECOP:
		//	{
		//		switch(op_code.spec_op)
		//		{
		//		//case SpecOp_JR:		handled = GenerateJR(rs); break;

		//		case SpecOp_SLL:	GenerateSLL( rd, rt, sa );	handled = true; break;
		//		case SpecOp_SRA:	GenerateSRA( rd, rt, sa );	handled = true; break;
		//		case SpecOp_SRL:	GenerateSRL( rd, rt, sa );	handled = true; break;
		//		}
		//	}
		//	break;
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
void	CCodeGeneratorX86::GenerateGenericR4300( OpCode op_code, CPU_Instruction p_instruction )
{
	// XXXX Flush all fp registers before a generic call

	// Call function - __fastcall
	MOVI(ECX_CODE, op_code._u32);
	CALL( CCodeLabel( p_instruction ) );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation CCodeGeneratorX86::ExecuteNativeFunction( CCodeLabel speed_hack, bool check_return )
{
	CALL( speed_hack );
	if( check_return )
	{
		TEST( EAX_CODE, EAX_CODE );

		return JELong( CCodeLabel(NULL) );
	}
	else
	{
		return CJumpLocation(NULL);
	}
}


void	CCodeGeneratorX86::GenerateCACHE( EN64Reg base, s16 offset, u32 cache_op )
{
	u32 dwCache = cache_op & 0x3;
	u32 dwAction = (cache_op >> 2) & 0x7;

	// For instruction cache invalidation, make sure we let the CPU know so the whole
	// dynarec system can be invalidated
	if(dwCache == 0 && (dwAction == 0 || dwAction == 4))
	{
		MOV_REG_MEM(ECX_CODE, &gCPUState.CPU[base]._u32_0);
		MOVI(EDX_CODE, 0x20);
		ADDI(ECX_CODE, offset);
		CALL( CCodeLabel( reinterpret_cast< const void * >( CPU_InvalidateICacheRange ) ));
	}
	else
	{
		// We don't care about data cache etc
	}
}

void CCodeGeneratorX86::GenerateLoad(u32 memBase, EN64Reg base, s16 offset, u8 twiddle, u8 bits)
{
	MOV_REG_MEM(ECX_CODE, &gCPUState.CPU[base]._u32_0);
	if (twiddle == 0)
	{
		DAEDALUS_ASSERT_Q(bits == 32);
		ADDI(ECX_CODE, memBase);
		MOV_REG_MEM_BASE_OFFSET(EAX_CODE, ECX_CODE, offset);
	}
	else
	{
		ADDI(ECX_CODE, offset);
		XOR_I8(ECX_CODE, twiddle);
		ADDI(ECX_CODE, memBase);
		switch(bits)
		{
		case 32:
			MOV_REG_MEM_BASE(EAX_CODE, ECX_CODE);
			break;
		case 16:
			MOV16_REG_MEM_BASE(EAX_CODE, ECX_CODE);
			break;
		case 8:
			MOV8_REG_MEM_BASE(EAX_CODE, ECX_CODE);
			break;
		}
	}
}

bool CCodeGeneratorX86::GenerateLW( EN64Reg rt, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		GenerateLoad((u32)g_pu8RamBase_8000, base, offset, 0, 32);

		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_0, EAX_CODE);
		CDQ();
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_1, EDX_CODE);

		return true;
	}
	return false;
}


bool CCodeGeneratorX86::GenerateSWC1( u32 ft, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		MOV_REG_MEM(ECX_CODE, &gCPUState.CPU[base]._u32_0);
		ADDI(ECX_CODE, (u32)g_pu8RamBase_8000);

		MOV_REG_MEM(EAX_CODE, &gCPUState.FPU[ft]._u32);
		MOV_MEM_BASE_OFFSET_REG(ECX_CODE, offset, EAX_CODE);
		return true;
	}

	return false;
}

bool CCodeGeneratorX86::GenerateSW( EN64Reg rt, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		MOV_REG_MEM(ECX_CODE, &gCPUState.CPU[base]._u32_0);
		ADDI(ECX_CODE, (u32)g_pu8RamBase_8000);
		MOV_REG_MEM(EAX_CODE, &gCPUState.CPU[rt]._u32_0);
		MOV_MEM_BASE_OFFSET_REG(ECX_CODE, offset, EAX_CODE);
		return true;
	}

	return false;
}

bool CCodeGeneratorX86::GenerateLB( EN64Reg rt, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		GenerateLoad((u32)g_pu8RamBase_8000, base, offset, U8_TWIDDLE, 8);
		MOVSX(EAX_CODE, EAX_CODE, true);
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_0, EAX_CODE);
		CDQ();
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_1, EDX_CODE);

		return true;
	}

	return false;
}

bool CCodeGeneratorX86::GenerateLBU( EN64Reg rt, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		GenerateLoad((u32)g_pu8RamBase_8000, base, offset, U8_TWIDDLE, 8);
		MOVZX(EAX_CODE, EAX_CODE, true);
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_0, EAX_CODE);
		XOR(EDX_CODE, EDX_CODE);
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_1, EDX_CODE);
		return true;
	}

	return false;
}


bool CCodeGeneratorX86::GenerateLH( EN64Reg rt, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		GenerateLoad((u32)g_pu8RamBase_8000, base, offset, U16_TWIDDLE, 16);

		MOVSX(EAX_CODE, EAX_CODE, false);
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_0, EAX_CODE);
		CDQ();
		MOV_MEM_REG(&gCPUState.CPU[rt]._u32_1, EDX_CODE);
		return true;
	}

	return false;
}

void CCodeGeneratorX86::GenerateADDIU( EN64Reg rt, EN64Reg rs, s16 immediate )
{
	MOV_REG_MEM(EAX_CODE, &gCPUState.CPU[rs]._u32_0);
	ADDI(EAX_CODE, immediate);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[rt]._u32_0, EAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[rt]._u32_1, EDX_CODE);
}

void CCodeGeneratorX86::GenerateSLL( EN64Reg rd, EN64Reg rt, u32 sa )
{
	MOV_REG_MEM(EAX_CODE, &gCPUState.CPU[rt]._u32_0);
	SHLI(EAX_CODE, sa);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_0, EAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_1, EDX_CODE);
}

void CCodeGeneratorX86::GenerateSRL( EN64Reg rd, EN64Reg rt, u32 sa )
{
	MOV_REG_MEM(EAX_CODE, &gCPUState.CPU[rt]._u32_0);
	SHRI(EAX_CODE, sa);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_0, EAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_1, EDX_CODE);
}

void CCodeGeneratorX86::GenerateSRA( EN64Reg rd, EN64Reg rt, u32 sa )
{
	MOV_REG_MEM(EAX_CODE, &gCPUState.CPU[rt]._u32_0);
	SARI(EAX_CODE, sa);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_0, EAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[rd]._u32_1, EDX_CODE);
}

bool CCodeGeneratorX86::GenerateLWC1( u32 ft, EN64Reg base, s16 offset )
{
	if (gDynarecStackOptimisation && base == N64Reg_SP)
	{
		GenerateLoad((u32)g_pu8RamBase_8000, base, offset, 0, 32);

		MOV_MEM_REG(&gCPUState.FPU[ft]._u32, EAX_CODE);
		return true;
	}

	return false;
}

void	CCodeGeneratorX86::GenerateJAL( u32 address )
{
	MOVI(EAX_CODE, address + 8);
	CDQ();
	MOV_MEM_REG(&gCPUState.CPU[N64Reg_RA]._u32_0, EAX_CODE);
	MOV_MEM_REG(&gCPUState.CPU[N64Reg_RA]._u32_1, EDX_CODE);
}

void	CCodeGeneratorX86::GenerateJR( EN64Reg rs)
{
	//TODO
}


void  _EnterDynaRec( const void * p_function, const void * p_base_pointer, const void * p_rebased_mem, u32 mem_limit )
{
	using FragmentFunction =  void (*)();


	reinterpret_cast< FragmentFunction >( p_function )();
}
