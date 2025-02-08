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

#ifndef SYSW32_DYNAREC_X64_ASSEMBLYWRITERX64_H_
#define SYSW32_DYNAREC_X64_ASSEMBLYWRITERX64_H_

#include "Core/CPU.h"
#include "DynaRec/AssemblyBuffer.h"
#include "DynarecTargetX64.h"

class CAssemblyWriterX64
{
	public:
		CAssemblyWriterX64( CAssemblyBuffer * p_buffer )
			:	mpAssemblyBuffer( p_buffer )
		{
		}

	public:
		CAssemblyBuffer *	GetAssemblyBuffer() const									{ return mpAssemblyBuffer; }
		void				SetAssemblyBuffer( CAssemblyBuffer * p_buffer )				{ mpAssemblyBuffer = p_buffer; }

	// XXXX
	private:
	public:
				// I sometimes use this to force an exceptions so I can analyse the generated code
				inline void CRASH(u32 x)
				{
					EmitWORD(0x05c7);
					EmitDWORD(0x00000000);
					EmitDWORD(x); //c7 05 xx+0 xx xx xx ef be ad de
				}

				inline void NOP()
				{
					EmitBYTE(0x90);
				}

				inline void INT3()
				{
					EmitBYTE(0xcc);
				}

				void				PUSH(EIntelReg reg);
				void				POP(EIntelReg reg);
				void				PUSHI( u32 value );

				void				ADD(EIntelReg reg1, EIntelReg reg2, bool is64 = false);				// add	reg1, reg2
				void				SUB(EIntelReg reg1, EIntelReg reg2, bool is64 = false);
				void				MUL_EAX_MEM(u32 * mem);							// mul	eax, dword ptr[ mem ]
				void				MUL(EIntelReg reg);									// mul	eax, reg

				void				ADD_REG_MEM_IDXx4( EIntelReg destreg, u32 * mem, EIntelReg idxreg );	// add reg, dword ptr[ mem + idx*4 ]

				void				AND(EIntelReg reg1, EIntelReg reg2, bool is64 = false);
				void				AND_EAX( u32 mask );								// and		eax, 0x00FF	- Mask off top bits!

				void				OR(EIntelReg reg1, EIntelReg reg2, bool is64 = false);
				void				XOR(EIntelReg reg1, EIntelReg reg2, bool is64 = false);
				void				NOT(EIntelReg reg1, bool is64 = false);

				void				ADDI(EIntelReg reg, s32 data, bool is64 = false);
				void				SUBI(EIntelReg reg, s32 data, bool is64 = false);
				void				ADCI(EIntelReg reg, s32 data);
				void				ANDI(EIntelReg reg, u32 data, bool is64 = false);
				void				ORI(EIntelReg reg, u32 data, bool is64 = false);
				void				XORI(EIntelReg reg, u32 data, bool is64 = false);

				void				SHLI(EIntelReg reg, u8 sa);
				void				SHRI(EIntelReg reg, u8 sa);
				void				SARI(EIntelReg reg, u8 sa);

				void				CMP(EIntelReg reg1, EIntelReg reg2);
				void				TEST(EIntelReg reg1, EIntelReg reg2);
				void				TEST_AH( u8 flags );
				void				CMPI(EIntelReg reg, u32 data);
				void				CMP_MEM32_I32(const u32 *p_mem, u32 data);			// cmp		dword ptr p_mem, data
				void				CMP_MEM32_I8(const u32 *p_mem, u8 data);			// cmp		dword ptr p_mem, data

				void				SETL(EIntelReg reg);
				void				SETB(EIntelReg reg);

				void				JS(s32 off);
				void				JA(u8 off);
				void				JB(u8 off);
				void				JAE(u8 off);
				void				JE(u8 off);
				void				JNE(u8 off);
				void				JL(u8 off);
				void				JGE(u8 off);
				void				JLE(u8 off);
				void				JG(u8 off);
				void				JMP_SHORT(u8 off);

				CJumpLocation		JMPLong( CCodeLabel target );
				CJumpLocation		JNELong( CCodeLabel target );
				CJumpLocation		JELong( CCodeLabel target );

				void				JMP_REG( EIntelReg reg );
				void				CALL_REG( EIntelReg reg );
				CJumpLocation		CALL( CCodeLabel target );

				void				CALL_MEM_PLUS_REGx4( void * mem, EIntelReg reg );	// call dword ptr [mem + reg*4]
				void				RET();

				static inline bool IsValidMov8Reg( EIntelReg isrc )
				{
					return isrc <= RBX_CODE;
				}

				void				CDQ();

				void				MOV(EIntelReg reg1, EIntelReg reg2, bool is64 = false);				// mov  reg1, reg2
				
				void				MOVSX(EIntelReg reg1, EIntelReg reg2, bool _8bit);	// movsx reg1, reg2
				void				MOVZX(EIntelReg reg1, EIntelReg reg2, bool _8bit);	// movzx reg1, reg2
				void				MOV_MEM_REG(u32 * mem, EIntelReg isrc);			// mov dword ptr[ mem ], reg
				void				MOV_REG_MEM(EIntelReg reg, const u32 * mem);		// mov reg, dword ptr[ mem ]

				void				MOV8_MEM_BASE_REG( EIntelReg ibase, EIntelReg isrc );		// mov byte ptr [base], src
				void				MOV8_REG_MEM_BASE( EIntelReg idst, EIntelReg ibase );		// mov dst, byte ptr [base]

				void				MOV16_MEM_BASE_REG( EIntelReg ibase, EIntelReg isrc );		// mov word ptr [base], src
				void				MOV16_REG_MEM_BASE( EIntelReg idst, EIntelReg ibase );		// mov dst, word ptr[base] e.g. mov ax, word ptr [eax]

				void				MOV_MEM_BASE_OFFSET32_REG( EIntelReg ibase, s32 offset, EIntelReg isrc );		// mov dword ptr [base + nnnnnnnn], src
				void				MOV_MEM_BASE_OFFSET8_REG( EIntelReg ibase, s8 offset, EIntelReg isrc );		// mov dword ptr [base + nn], src
				void				MOV_MEM_BASE_OFFSET_REG( EIntelReg ibase, s32 offset, EIntelReg isrc );
				void				MOV_MEM_BASE_REG( EIntelReg ibase, EIntelReg isrc );							// mov dword ptr [base], src

				void				MOV_REG_MEM_BASE_OFFSET32( EIntelReg idst, EIntelReg ibase, s32 offset );		// mov dst, dword ptr [base + nnnnnnnn]
				void				MOV_REG_MEM_BASE_OFFSET8( EIntelReg idst, EIntelReg ibase, s8 offset );		// mov dst, dword ptr [base + nn]
				void				MOV_REG_MEM_BASE_OFFSET( EIntelReg idst, EIntelReg ibase, s32 offset );
				void				MOV_REG_MEM_BASE( EIntelReg idst, EIntelReg ibase );							// mov dst, dword ptr [base]

				void				MOVI(EIntelReg reg, u32 data);						// mov reg, data
				void				MOVI_MEM(u32 * mem, u32 data);						// mov dword ptr[ mem ], data
				void				MOVI_MEM8(u32 * mem, u8 data);						// mov byte ptr[ mem ], data

				void				MOVSX8( EIntelReg idst, EIntelReg isrc );			// movsx	dst, src		(e.g. movsx eax, al)
				void				MOVSX16( EIntelReg idst, EIntelReg isrc );			// movsx	dst, src		(e.g. movsx eax, ax)

				void				FSQRT();
				void				FCHS();

				void				FILD_MEM( u32 * pmem );
				void				FLD_MEMp32( u32 * pmem );
				void				FSTP_MEMp32( u32 * pmem );
				void				FLD_MEMp64( u64 * pmem );
				void				FSTP_MEMp64( u64 * pmem );
				void				FISTP_MEMp( u32 * pmem );

				void				FLD( u32 i );
				void				FXCH( u32 i );
				void				FCOMP( u32 i );

				void				FNSTSW();
				void				FFREE( u32 i );
				void				FINCSTP();
				void				FSQRTp();

				void				FCHSp();
				void				FADD( u32 i );
				void				FSUB( u32 i );
				void				FMUL( u32 i );
				void				FDIV( u32 i );

				// 64bit functions
				void				LEA(EIntelReg reg, const void* mem);

				void				MOVI_64(EIntelReg reg, u64 data);						// mov reg, data
				void				MOV64_REG_MEM(EIntelReg reg, const u64 * mem);
				void				MOV64_MEM_REG(u64 * mem, EIntelReg isrc);
	private:
				CJumpLocation		JumpConditionalLong( CCodeLabel target, u8 jump_type );


		void X64_EXT(EIntelReg &reg1, EIntelReg &reg2, bool is64reg, bool is64addr);

		inline void EmitBYTE(u8 byte)
		{
			mpAssemblyBuffer->EmitBYTE( byte );
		}

		inline void EmitWORD(u16 word)
		{
			mpAssemblyBuffer->EmitWORD( word );
		}

		inline void EmitDWORD(u32 dword)
		{
			mpAssemblyBuffer->EmitDWORD( dword );
		}

		inline void EmitQWORD(u64 qword)
		{
			mpAssemblyBuffer->EmitQWORD( qword );
		}

		inline void EmitADDR(const void* ptr)
		{
			s64 diff = (intptr_t)ptr - (intptr_t)&gCPUState;
			DAEDALUS_ASSERT(diff < INT32_MAX && diff > INT32_MIN, "address offset range is too big to fit");
			mpAssemblyBuffer->EmitDWORD((s32)diff);
		}

	private:
		CAssemblyBuffer *				mpAssemblyBuffer;
};

#endif // SYSW32_DYNAREC_X64_ASSEMBLYWRITERX64_H_
