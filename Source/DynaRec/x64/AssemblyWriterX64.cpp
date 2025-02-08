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
#include "AssemblyWriterX64.h"

#define PREFIX 0x40
#define PREFIX_W64 0x08
#define PREFIX_REG 0x04
#define PREFIX_REGB 0x01
#define PREFIX_SIB  0x02

static EIntelReg dummy = RAX_CODE;

inline void CAssemblyWriterX64::X64_EXT(EIntelReg &reg1, EIntelReg &reg2, bool is64reg, bool is64addr)
{
	u8 first_byte = 0;
	if (reg1 > R8_CODE) {first_byte |= PREFIX_REG; reg1 = EIntelReg(reg1 & 7);}
	if (reg2 > R8_CODE) {first_byte |= PREFIX_REGB; reg2 = EIntelReg(reg2 & 7);}
	if (is64reg) first_byte |= PREFIX_W64;
	if (is64addr) first_byte |= PREFIX_SIB;

	if (first_byte) {
		EmitBYTE(first_byte | PREFIX);
	}
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::PUSH(EIntelReg reg)
{
	X64_EXT(dummy, reg, true, false);
	EmitBYTE(0x50 | reg);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::POP(EIntelReg reg)
{
	X64_EXT(dummy, reg, true, false);
	EmitBYTE(0x58 | reg);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::PUSHI( u32 value )
{
	EmitBYTE( 0x68 );
	EmitDWORD( value );
}

//*****************************************************************************
//	add	reg1, reg2
//*****************************************************************************
void	CAssemblyWriterX64::ADD(EIntelReg reg1, EIntelReg reg2, bool is64)
{
	X64_EXT(reg1, reg2, is64, false);

	EmitBYTE(0x03);
	EmitBYTE(0xc0 | (reg1<<3) | reg2);
}

//*****************************************************************************
//	sub	reg1, reg2
//*****************************************************************************
void	CAssemblyWriterX64::SUB(EIntelReg reg1, EIntelReg reg2, bool is64)
{
	X64_EXT(reg1, reg2, is64, false);

	EmitBYTE(0x2b);
	EmitBYTE(0xc0 | (reg1<<3) | reg2);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::MUL_EAX_MEM(u32 * mem)
{
	EmitBYTE(0xf7);
	EmitBYTE(0xa3);
	EmitADDR(mem); // mul    DWORD PTR [rbx+0x78563412]
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::MUL(EIntelReg reg)
{
	EmitBYTE(0xf7);
	EmitBYTE(0xe0 | reg);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::ADD_REG_MEM_IDXx4( EIntelReg destreg, u32 * mem, EIntelReg idxreg )
{
	EmitBYTE(0x03);
	EmitBYTE(0x84 | (destreg << 3));
	EmitBYTE(0x83 | (idxreg<<3));
	EmitADDR(mem); //  03 84 83 12 34 56 79    add    eax,DWORD PTR [rbx+rax*4+0x79563412]
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::AND(EIntelReg reg1, EIntelReg reg2, bool is64)
{
	if (reg1 != reg2)
	{
		X64_EXT(reg1, reg2, is64, false);

		EmitBYTE(0x23);
		EmitBYTE(0xc0 | (reg1<<3) | reg2);
	}
}

//*****************************************************************************
// and		eax, 0x00FF	- Mask off top bits!
// This is slightly more efficient than ANDI( EAX_CODE, data )
//*****************************************************************************
void	CAssemblyWriterX64::AND_EAX( u32 mask )
{
	EmitBYTE(0x25);
	EmitDWORD(mask);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::OR(EIntelReg reg1, EIntelReg reg2, bool is64)
{
	if (reg1 != reg2)
	{
		X64_EXT(reg1, reg2, is64, false);

		EmitBYTE(0x0B);
		EmitBYTE(0xc0 | (reg1<<3) | reg2);
	}
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::XOR(EIntelReg reg1, EIntelReg reg2, bool is64)
{
	X64_EXT(reg1, reg2, is64, false);
	
	EmitBYTE(0x33);
	EmitBYTE(0xc0 | (reg1<<3) | reg2);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::NOT(EIntelReg reg1, bool is64)
{
	X64_EXT(reg1, dummy, is64, false);
	
	EmitBYTE(0xf7);
	EmitBYTE(0xd0 | reg1);
}

//*****************************************************************************
// Use short form (0x83c0) if data is just one byte!
//*****************************************************************************
void CAssemblyWriterX64::ADDI(EIntelReg reg, s32 data, bool is64)
{
	if (data == 0)
		return;

	X64_EXT(reg, dummy, is64, false);

	if (data <= 127 && data > -127)
	{
		EmitBYTE(0x83);
		EmitBYTE(0xc0 | reg);
		EmitBYTE((u8)data);
	}
	else
	{
		EmitBYTE(0x81);
		EmitBYTE(0xc0 | reg);
		EmitDWORD(data);
	}

}

//*****************************************************************************
// Use short form (0x83c0) if data is just one byte!
//*****************************************************************************
void CAssemblyWriterX64::SUBI(EIntelReg reg, s32 data, bool is64)
{
	if (data == 0)
		return;

	X64_EXT(reg, dummy, is64, false);

	if (data <= 127 && data > -127)
	{
		EmitBYTE(0x83);
		EmitBYTE(0xe8 | reg);
		EmitBYTE((u8)data);
	}
	else
	{
		EmitBYTE(0x81);
		EmitBYTE(0xe8 | reg);
		EmitDWORD(data);
	}
}


//*****************************************************************************
// Use short form (0x83d0) if data is just one byte!
//*****************************************************************************
void CAssemblyWriterX64::ADCI(EIntelReg reg, s32 data)
{
	/*if (reg == EAX_CODE)
	{
		EmitBYTE(0x15);
		EmitDWORD(data)
	}
	else*/
	if (data <= 127 && data > -127)
	{
		EmitBYTE(0x83);
		EmitBYTE(0xd0 | reg);
		EmitBYTE((u8)(data));
	}
	else
	{
		EmitBYTE(0x81);
		EmitBYTE(0xd0 | reg);
		EmitDWORD(data);
	}
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::ANDI(EIntelReg reg1, u32 data, bool is64)
{
	X64_EXT(reg1, dummy, is64, false);

	/*if (reg == EAX_CODE)
		EmitBYTE(0x25);
	else */
	{
		EmitBYTE(0x81);
		EmitBYTE(0xe0 | reg1);
	}
	EmitDWORD(data);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::ORI(EIntelReg reg1, u32 data, bool is64)
{
	X64_EXT(reg1, dummy, is64, false);

	/*if (reg == EAX_CODE)
		EmitBYTE(0x0D);
	else*/
	{
		EmitBYTE(0x81);
		EmitBYTE(0xc8 | reg1);
	}
	EmitDWORD(data);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::XORI(EIntelReg reg, u32 data, bool is64)
{
	X64_EXT(reg, dummy, is64, false);

	if (data <= 255)
	{
		EmitBYTE(0x83);
		EmitBYTE(0xf0 | reg);
		EmitBYTE((u8)data);
	}
	else
	{
		EmitBYTE(0x81);
		EmitBYTE(0xf0 | reg);
		EmitDWORD(data);
	}
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::SHLI(EIntelReg reg, u8 sa)
{
	X64_EXT(reg, dummy, false, false);

	EmitBYTE(0xc1);
	EmitBYTE(0xe0 | reg);
	EmitBYTE(sa);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::SHRI(EIntelReg reg, u8 sa)
{
	X64_EXT(reg, dummy, false, false);

	EmitBYTE(0xc1);
	EmitBYTE(0xe8 | reg);
	EmitBYTE(sa);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::SARI(EIntelReg reg, u8 sa)
{
	X64_EXT(reg, dummy, false, false);

	EmitBYTE(0xc1);
	EmitBYTE(0xf8 | reg);
	EmitBYTE(sa);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::CMP(EIntelReg reg1, EIntelReg reg2)
{
	X64_EXT(reg1, reg2, false, false);

	EmitBYTE(0x3b);
	EmitBYTE(0xc0 | (reg1<<3) | reg2);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::TEST(EIntelReg reg1, EIntelReg reg2)
{
	X64_EXT(reg1, reg2, false, false);
	
	EmitBYTE(0x85);
	EmitBYTE(0xc0 | (reg1<<3) | reg2);
}

//*****************************************************************************
// test		ah, 41
//*****************************************************************************
void CAssemblyWriterX64::TEST_AH( u8 flags )
{
	EmitBYTE(0xf6);
	EmitBYTE(0xc4);
	EmitBYTE(flags);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::CMPI(EIntelReg reg, u32 data)
{
	X64_EXT(reg, dummy, false, false);

	EmitBYTE(0x81);
	EmitBYTE(0xf8 | reg);
	EmitDWORD(data);
}

//*****************************************************************************
//	cmp		dword ptr p_mem, data
//*****************************************************************************
void CAssemblyWriterX64::CMP_MEM32_I32(const u32 *p_mem, u32 data)
{
	EmitBYTE(0x81);
	EmitBYTE(0xbb);
	EmitADDR(p_mem); // 0:  81 bb 12 34 56 78 12    cmp    DWORD PTR [rbx+0x78563412],0x78563412
					 // 7:  34 56 78
	EmitDWORD(data);
}

//*****************************************************************************
//	cmp		dword ptr p_mem, data
//*****************************************************************************
void CAssemblyWriterX64::CMP_MEM32_I8(const u32 *p_mem, u8 data)
{
	EmitBYTE(0x83);
	EmitBYTE(0xbb);
	EmitADDR(p_mem); // 83 bb 12 34 56 78 12    cmp    DWORD PTR [rbx+0x78563412],0x12
	EmitBYTE(data);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::SETL(EIntelReg reg)
{
	EmitWORD(0x9c0f);
	EmitBYTE(0xc0 | reg);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::SETB(EIntelReg reg)
{
	EmitWORD(0x920f);
	EmitBYTE(0xc0 | reg);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::JS(s32 off)
{
	EmitBYTE(0x0f);
	EmitBYTE(0x88);
	EmitDWORD( u32(off) );
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::JA(u8 off)
{
	EmitWORD((off<<8) | 0x77);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::JB(u8 off)
{
	EmitWORD((off<<8) | 0x72);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::JAE(u8 off)
{
	EmitWORD((off<<8) | 0x73);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::JE(u8 off)
{
	EmitWORD((off<<8) | 0x74);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::JNE(u8 off)
{
	EmitWORD((off<<8) | 0x75);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::JL(u8 off)
{
	EmitWORD((off<<8) | 0x7C);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::JGE(u8 off)
{
	EmitWORD((off<<8) | 0x7D);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::JLE(u8 off)
{
	EmitWORD((off<<8) | 0x7E);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::JG(u8 off)
{
	EmitWORD((off<<8) | 0x7F);
}

//*****************************************************************************
//
//*****************************************************************************
void CAssemblyWriterX64::JMP_SHORT(u8 off)
{
	EmitWORD((off<<8) | 0xeb);
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation CAssemblyWriterX64::JumpConditionalLong( CCodeLabel target, u8 jump_type )
{
	const u32	JUMP_LONG_LENGTH = 6;

	CJumpLocation	jump_location( mpAssemblyBuffer->GetJumpLocation() );
	s32				offset( jump_location.GetOffset( target ) - JUMP_LONG_LENGTH );

	EmitBYTE( 0x0f );
	EmitBYTE( jump_type );		//
	EmitDWORD( offset );

	return jump_location;
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation CAssemblyWriterX64::JELong( CCodeLabel target )
{
	return JumpConditionalLong( target, 0x84 );
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation CAssemblyWriterX64::JNELong( CCodeLabel target )
{
	return JumpConditionalLong( target, 0x85 );
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::JMP_REG( EIntelReg reg )
{
	EmitWORD( ((0xe0|reg)<<8) | 0xff );
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::CALL_REG( EIntelReg reg )
{
	EmitWORD(((0xd0|reg)<<8) | 0xff);
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CAssemblyWriterX64::CALL( CCodeLabel target )
{
	const u32	CALL_LONG_LENGTH = 5;

#ifdef DAEDALUS_W32
	SUBI(RSP_CODE, 32, true);
#endif

	CJumpLocation	jump_location( mpAssemblyBuffer->GetJumpLocation() );
	CJumpLocation   rbx_location(&gCPUState);

	if (jump_location.IsIn32BitRange(target))
	{
		s32				offset( jump_location.GetOffset( target ) - CALL_LONG_LENGTH );

		EmitBYTE( 0xe8 );
		EmitDWORD( offset );
	}
	else if (rbx_location.IsIn32BitRange(target))
	{
		LEA(RAX_CODE, target.GetTargetU8P());
		EmitWORD(0xd0ff);
	}

#ifdef DAEDALUS_W32
	ADDI(RSP_CODE, 32, true);
#endif
	return jump_location;
}

//*****************************************************************************
//
//*****************************************************************************
CJumpLocation	CAssemblyWriterX64::JMPLong( CCodeLabel target )
{
	const u32	JUMP_DIRECT_LONG_LENGTH = 5;

	CJumpLocation	jump_location( mpAssemblyBuffer->GetJumpLocation() );
	s32				offset( jump_location.GetOffset( target ) - JUMP_DIRECT_LONG_LENGTH );

	EmitBYTE(0xe9);
	EmitDWORD( static_cast< u32 >( offset ) );

	return jump_location;
}

//*****************************************************************************
// call dword ptr [mem + reg*4]
//*****************************************************************************
void	CAssemblyWriterX64::CALL_MEM_PLUS_REGx4( void * mem, EIntelReg reg )
{
	EmitBYTE(0xFF);
	EmitBYTE(0x94);
	EmitBYTE(0x83 | (reg<<3));
	EmitADDR(mem); // ff 94 83 12 34 56 78    call   QWORD PTR [rbx+rax*4+0x78563412]
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::RET()
{
	EmitBYTE(0xC3);
}

//*****************************************************************************
// mov reg1, reg2
//*****************************************************************************
void	CAssemblyWriterX64::MOV(EIntelReg reg1, EIntelReg reg2, bool is64)
{
	if (reg1 != reg2)
	{
		X64_EXT(reg1, reg2, is64, false);

		EmitBYTE(0x8b);
		EmitBYTE(0xc0 | (reg1<<3) | reg2);
	}
}

//*****************************************************************************
// movsx reg1, reg2
//*****************************************************************************
void	CAssemblyWriterX64::MOVSX(EIntelReg reg1, EIntelReg reg2, bool _8bit)
{
	EmitBYTE(0x0f);
	EmitBYTE(_8bit ? 0xBE : 0xBF);
	EmitBYTE(0xc0 | (reg1<<3) | reg2);
}

//*****************************************************************************
// movzx reg1, reg2
//*****************************************************************************
void	CAssemblyWriterX64::MOVZX(EIntelReg reg1, EIntelReg reg2, bool _8bit)
{
	EmitBYTE(0x0f);
	EmitBYTE(_8bit ? 0xB6 : 0xB7);
	EmitBYTE(0xc0 | (reg1<<3) | reg2);
}

//*****************************************************************************
// mov dword ptr[ mem ], reg
//*****************************************************************************
void	CAssemblyWriterX64::MOV_MEM_REG(u32 * mem, EIntelReg isrc)
{
	/*if (reg == EAX_CODE)
		EmitBYTE(0xa3);
	else*/
	{
		EmitBYTE(0x89);
		EmitBYTE((isrc<<3) | 0x83);
	}
	EmitADDR(mem); //  89 83 12 34 56 78       mov    DWORD PTR [rbx+0x78563412],eax
}

//*****************************************************************************
// mov qword ptr[ mem ], reg
//*****************************************************************************
void	CAssemblyWriterX64::MOV64_MEM_REG(u64 * mem, EIntelReg isrc)
{
	X64_EXT(dummy, isrc, true, false);

	/*if (reg == EAX_CODE)
		EmitBYTE(0xa3);
	else*/
	{
		EmitBYTE(0x89);
		EmitBYTE((isrc<<3) | 0x83);
	}
	EmitADDR(mem); // 48 89 83 12 34 56 78    mov    QWORD PTR [rbx+0x78563412],rax
}


//*****************************************************************************
// mov reg, qword ptr[ mem ]
//*****************************************************************************
void	CAssemblyWriterX64::MOV64_REG_MEM(EIntelReg reg, const u64 * mem)
{
	X64_EXT(dummy, reg, true, false);

	/*if (reg == EAX_CODE)
		EmitBYTE(0xa1);
	else*/
	{
		EmitBYTE(0x8b);
		EmitBYTE((reg<<3) | 0x83); 
	}

	EmitADDR(mem); // 48 8b 83 12 34 56 78    mov    rax,QWORD PTR [rbx+0x78563412]
}

//*****************************************************************************
// mov reg, dword ptr[ mem ]
//*****************************************************************************
void	CAssemblyWriterX64::MOV_REG_MEM(EIntelReg reg, const u32 * mem)
{
	/*if (reg == EAX_CODE)
		EmitBYTE(0xa1);
	else*/
	{
		EmitBYTE(0x8b);
		EmitBYTE((reg<<3) | 0x83); 
	}

	EmitADDR(mem); // 8b 83 12 34 56 78       mov    eax,DWORD PTR [rbx+0x78563412]
}

//*****************************************************************************
// mov byte ptr [base], src
//*****************************************************************************
void	CAssemblyWriterX64::MOV8_MEM_BASE_REG( EIntelReg ibase, EIntelReg isrc )
{
	DAEDALUS_ASSERT( IsValidMov8Reg( isrc ), "Invalid source register for 8 bit move" );
	EmitBYTE(0x88);
	EmitBYTE(0x00 | (isrc<<3) | ibase);
}

//*****************************************************************************
// mov dst, byte ptr [base]
//*****************************************************************************
void	CAssemblyWriterX64::MOV8_REG_MEM_BASE( EIntelReg idst, EIntelReg ibase )
{
	DAEDALUS_ASSERT( IsValidMov8Reg( idst ), "Invalid destination register for 8 bit move" );

	EmitBYTE(0x8A);
	EmitBYTE(0x00 | (idst<<3) | ibase);
}

//*****************************************************************************
// mov word ptr [base], src
//*****************************************************************************
void	CAssemblyWriterX64::MOV16_MEM_BASE_REG( EIntelReg ibase, EIntelReg isrc )
{
	EmitBYTE(0x66);
	EmitBYTE(0x89);
	EmitBYTE(0x00 | (isrc<<3) | ibase);
}

//*****************************************************************************
// mov dst, word ptr[base] e.g. mov ax, word ptr [eax]
//*****************************************************************************
void	CAssemblyWriterX64::MOV16_REG_MEM_BASE( EIntelReg idst, EIntelReg ibase )
{
	EmitBYTE(0x66);
	EmitBYTE(0x8B);
	EmitBYTE(0x00 | (idst<<3) | ibase);
}

//*****************************************************************************
// mov dword ptr [base + nnnnnnnn], src
//*****************************************************************************
void	CAssemblyWriterX64::MOV_MEM_BASE_OFFSET32_REG( EIntelReg ibase, s32 offset, EIntelReg isrc )
{
	EmitBYTE(0x89);
	EmitBYTE(0x80 | (isrc<<3) | ibase);
	EmitDWORD((u32)offset);
}

//*****************************************************************************
// mov dword ptr [base + nn], src
//*****************************************************************************
void	CAssemblyWriterX64::MOV_MEM_BASE_OFFSET8_REG( EIntelReg ibase, s8 offset, EIntelReg isrc )
{
	EmitBYTE(0x89);
	EmitBYTE(0x40 | (isrc<<3) | ibase);
	EmitBYTE((u8)offset);
}

//*****************************************************************************
// mov dword ptr [base], src
//*****************************************************************************
void	CAssemblyWriterX64::MOV_MEM_BASE_REG( EIntelReg ibase, EIntelReg isrc )
{
	EmitBYTE(0x89);
	EmitBYTE(0x00 | (isrc<<3) | ibase);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::MOV_MEM_BASE_OFFSET_REG( EIntelReg ibase, s32 offset, EIntelReg isrc )
{
	if(offset == 0)
	{
		MOV_MEM_BASE_REG( ibase, isrc );
	}
	else if (offset < 128 && offset > -128)
	{
		MOV_MEM_BASE_OFFSET8_REG( ibase, (s8)offset, isrc );
	}
	else
	{
		MOV_MEM_BASE_OFFSET32_REG( ibase, offset, isrc );
	}
}

//*****************************************************************************
// mov dst, dword ptr [base + nnnnnnnn]
//*****************************************************************************
void	CAssemblyWriterX64::MOV_REG_MEM_BASE_OFFSET32( EIntelReg idst, EIntelReg ibase, s32 offset )
{
	EmitBYTE(0x8B);
	EmitBYTE(0x80 | (idst<<3) | ibase);
	EmitDWORD((u32)offset);
}

//*****************************************************************************
// mov dst, dword ptr [base + nn]
//*****************************************************************************
void	CAssemblyWriterX64::MOV_REG_MEM_BASE_OFFSET8( EIntelReg idst, EIntelReg ibase, s8 offset )
{
	EmitBYTE(0x8B);
	EmitBYTE(0x40 | (idst<<3) | ibase);
	EmitBYTE((u8)offset);
}

//*****************************************************************************
// mov dst, dword ptr [base]
//*****************************************************************************
void	CAssemblyWriterX64::MOV_REG_MEM_BASE( EIntelReg idst, EIntelReg ibase )
{
	EmitBYTE(0x8B);
	EmitBYTE(0x00 | (idst<<3) | ibase);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::MOV_REG_MEM_BASE_OFFSET( EIntelReg idst, EIntelReg ibase, s32 offset )
{
	if(offset == 0)
	{
		MOV_REG_MEM_BASE( idst, ibase );
	}
	else if(offset < 128 && offset > -128)
	{
		MOV_REG_MEM_BASE_OFFSET8( idst, ibase, (s8)offset );
	}
	else
	{
		MOV_REG_MEM_BASE_OFFSET32( idst, ibase, offset );
	}
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::MOVI(EIntelReg reg, u32 data)
{
	EmitBYTE(0xB8 | reg);
	EmitDWORD(data);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::MOVI_64(EIntelReg reg, u64 data)
{
	// 0:  48 b8 78 56 34 12 78    movabs rax,0x1234567812345678
	// 7:  56 34 12
	X64_EXT(dummy, reg, true, false);

	EmitBYTE(0xB8 | reg);
	EmitQWORD(data);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::MOVI_MEM(u32 * mem, u32 data)
{
	EmitBYTE(0xc7);
	EmitBYTE(0x83);
	EmitADDR(mem); 	// 0:  c7 83 12 34 56 78 12    mov    DWORD PTR [rbx+0x78563412],0x78563412
					// 7:  34 56 78
	EmitDWORD(data);
}

//*****************************************************************************
//	mov		byte ptr mem, data
//*****************************************************************************
void	CAssemblyWriterX64::MOVI_MEM8(u32 * mem, u8 data)
{
	EmitBYTE(0xc6);
	EmitBYTE(0x83);
	EmitADDR(mem);	//0:  c6 83 12 34 56 78 12    mov    BYTE PTR [rbx+0x78563412],0x12
	EmitBYTE(data);
}

//*****************************************************************************
// movsx	dst, src		(e.g. movsx eax, al)
//*****************************************************************************
void	CAssemblyWriterX64::MOVSX8( EIntelReg idst, EIntelReg isrc )
{
	DAEDALUS_ASSERT( IsValidMov8Reg( isrc ), "Invalid source register for 8 bit move" );
	EmitBYTE(0x0F);
	EmitBYTE(0xBE);
	EmitBYTE(0xC0 | (idst<<3) | isrc);
}

//*****************************************************************************
// movsx	dst, src		(e.g. movsx eax, ax)
//*****************************************************************************
void	CAssemblyWriterX64::MOVSX16( EIntelReg idst, EIntelReg isrc )
{
	EmitBYTE(0x0F);
	EmitBYTE(0xBF);
	EmitBYTE(0xC0 | (idst<<3) | isrc);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FSQRT()
{
	EmitWORD(0xfad9);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FCHS()
{
	EmitWORD(0xe0d9);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FILD_MEM( u32 * pmem )
{
	EmitWORD(0x83db);
	EmitADDR(pmem); // 0:  db 83 12 34 56 78       fild   DWORD PTR [rbx+0x78563412]
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FLD_MEMp32( u32 * pmem )
{
	EmitWORD(0x83d9);
	EmitADDR(pmem); //  d9 83 12 34 56 78       fld    DWORD PTR [rbx+0x78563412]
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FSTP_MEMp32( u32 * pmem )
{
	EmitWORD( 0x9bd9);
	EmitADDR(pmem); // 0:  d9 9b 12 34 56 78       fstp   DWORD PTR [rbx+0x78563412]
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FLD_MEMp64( u64 * mem)
{
	EmitWORD(0xabdf);
	EmitADDR(mem); //  df ab 78 56 34 12       fild   QWORD PTR [rbx+0x12345678]
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FSTP_MEMp64( u64 * mem)
{
	EmitWORD(0x9bdd);
	EmitADDR(mem); // dd 9b 78 56 34 12       fstp   QWORD PTR [rbx+0x12345678]
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FISTP_MEMp( u32 * pmem )
{
	EmitWORD( 0x9bdb );
	EmitADDR(pmem); //  db 9b 12 34 56 78       fistp  DWORD PTR [rbx+0x78563412]
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FLD( u32 i )
{
	EmitWORD((u16)((0xc0 + (i))<<8) |(0xd9));
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FXCH( u32 i )
{
	EmitWORD((u16)((0xc8 + (i))<<8) |(0xd9));
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FCOMP( u32 i )
{
	EmitWORD((u16)((0xd8 + (i))<<8) |(0xd8));
}

//*****************************************************************************
//fnstsw      ax
//*****************************************************************************
void	CAssemblyWriterX64::FNSTSW()
{
	EmitWORD(0xe0df);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FFREE( u32 i )
{
	EmitWORD((u16)((0xc0 + (i))<<8) |(0xdd));
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FINCSTP()
{
	EmitWORD(0xf7d9);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FSQRTp()
{
	EmitWORD(0xfad9);
}

//*****************************************************************************
//
//*****************************************************************************
void	CAssemblyWriterX64::FCHSp()
{
	EmitWORD(0xe0d9);
}

//*****************************************************************************
// fadd st(0),st(i)
//*****************************************************************************
void	CAssemblyWriterX64::FADD( u32 i )
{
	EmitWORD((u16)((0xc0 + (i))<<8) |(0xd8));
}

//*****************************************************************************
// fsub st(0),st(i)
//*****************************************************************************
void	CAssemblyWriterX64::FSUB( u32 i )
{
	EmitWORD((u16)((0xe0 + (i))<<8) |(0xd8));
}

//*****************************************************************************
// fmul st(0),st(i)
//*****************************************************************************
void	CAssemblyWriterX64::FMUL( u32 i )
{
	EmitWORD((u16)((0xc8 + (i))<<8) |(0xd8));
}

//*****************************************************************************
// fdiv st(0),st(i)
//*****************************************************************************
void	CAssemblyWriterX64::FDIV( u32 i )
{
	EmitWORD((u16)((0xf0 + (i))<<8) |(0xd8));
}

void	CAssemblyWriterX64::CDQ()
{
	EmitBYTE(0x99);
}

void	CAssemblyWriterX64::LEA(EIntelReg reg, const void* mem)
{
	X64_EXT(dummy, reg, true, false);

	EmitBYTE(0x8d);
	EmitBYTE(0x83 | (reg << 3));
	EmitADDR(mem); // 0:  48 8d 83 12 34 56 78    lea    rax,[rbx+0x78563412]
}