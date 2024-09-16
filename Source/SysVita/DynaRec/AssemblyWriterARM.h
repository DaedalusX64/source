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

#include <vector>

#include "DynaRec/AssemblyBuffer.h"
#include "DynarecTargetARM.h"

class CAssemblyWriterARM
{
	struct Literal
	{
		CCodeLabel 	Target;
		uint32_t	Value;
	};

	public:
		CAssemblyWriterARM(CAssemblyBuffer* p_buffer_a, CAssemblyBuffer* p_buffer_b) : mpAssemblyBuffer(p_buffer_a), mpAssemblyBufferA(p_buffer_a), mpAssemblyBufferB(p_buffer_b) { literals = &literalsA; }

		CAssemblyBuffer*	GetAssemblyBuffer() const									{ return mpAssemblyBuffer; }
		void				SetAssemblyBuffer( CAssemblyBuffer * p_buffer )				{ mpAssemblyBuffer = p_buffer; }

		void                SetBufferA() { mpAssemblyBuffer = mpAssemblyBufferA; literals = &literalsA; }
		void                SetBufferB() { mpAssemblyBuffer = mpAssemblyBufferB; literals = &literalsB; }
		bool                IsBufferB() { return mpAssemblyBuffer == mpAssemblyBufferB; }
		bool                IsBufferA() { return mpAssemblyBuffer == mpAssemblyBufferA; }
		void				InsertLiteralPool(bool branch);
		uint32_t			GetLiteralPoolDistance();

		inline void NOP()	{	EmitDWORD(0xe1a00000);	}

		void				ADD    (EArmReg rd, EArmReg rn, EArmReg rm, EArmCond = AL, u8 S = 0);
		void				ADD_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp);
		void				ADD_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4 = 0);
		void				ADC    (EArmReg rd, EArmReg rn, EArmReg rm);
		void				ADC_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp);
		
		void				SBC(EArmReg rd, EArmReg rn, EArmReg rm);
		void				SBC_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp);
		
		void				SUB(EArmReg rd, EArmReg rn, EArmReg rm, EArmCond = AL, u8 S = 0);
		void				SUB_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp);
		void				SUB_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror);

		void				MUL  (EArmReg rd, EArmReg rn, EArmReg rm);
		void				UMULL(EArmReg rdLo, EArmReg rdHi, EArmReg rn, EArmReg rm);
		void				SMULL(EArmReg rdLo, EArmReg rdHi, EArmReg rn, EArmReg rm);

		void				NEG(EArmReg rd, EArmReg rm);
		void				BIC(EArmReg rd, EArmReg rn, EArmReg rm);
		void				BIC_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp);
		
		void				AND    (EArmReg rd, EArmReg rn, EArmReg rm, EArmCond = AL);
		void				AND_IMM(EArmReg rd, EArmReg rn, u8 imm);
		void				AND_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp);

		void				ORR(EArmReg rd, EArmReg rn, EArmReg rm);
		void				ORR_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp);
		
		void				XOR(EArmReg rd, EArmReg rn, EArmReg rm);
		void                XOR_IMM(EArmReg rd, EArmReg rn, u8 imm);
		void                XOR_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp);

		void				TST    (EArmReg rn, EArmReg rm);
		void				CMP    (EArmReg rn, EArmReg rm);
		void				CMP_IMM(EArmReg rn, u8 imm);

		void				B  (s32 offset, EArmCond cond = AL);
		void				BL (s32 offset, EArmCond cond = AL);
		void				BX (EArmReg rm, EArmCond cond = AL);
		void				BLX(EArmReg rm, EArmCond cond = AL);
		
		void				PUSH(u16 regs);
		void				POP (u16 regs);

		void				LDR  (EArmReg rt, EArmReg rn, s16 offset);
		void				LDRB (EArmReg rt, EArmReg rn, s16 offset);
		void				LDRSB(EArmReg rt, EArmReg rn, s16 offset);
		void				LDRH (EArmReg rt, EArmReg rn, s16 offset);
		void				LDRSH(EArmReg rt, EArmReg rn, s16 offset);
		void				LDRD(EArmReg rt, EArmReg rn, s16 offset);

		void				LDR_REG(EArmReg rt, EArmReg rn, EArmReg rm);
		void				LDRB_REG(EArmReg rt, EArmReg rn, EArmReg rm);
		void				LDRSB_REG(EArmReg rt, EArmReg rn, EArmReg rm);
		void				LDRH_REG(EArmReg rt, EArmReg rn, EArmReg rm);
		void				LDRSH_REG(EArmReg rt, EArmReg rn, EArmReg rm);

		void				STR (EArmReg rt, EArmReg rn, s16 offset);
		void				STRH(EArmReg rt, EArmReg rn, s16 offset);
		void				STRB(EArmReg rt, EArmReg rn, s16 offset);
		void				STR_REG(EArmReg rt, EArmReg rn, EArmReg rm);
		void				STRH_REG(EArmReg rt, EArmReg rn, EArmReg rm);
		void				STRB_REG(EArmReg rt, EArmReg rn, EArmReg rm);
		void				STRD(EArmReg rt, EArmReg rn, s16 offset);

		void				MVN(EArmReg rd, EArmReg rm);
		void				MOV    (EArmReg rd, EArmReg rm);
		void				MOV_LSL(EArmReg rd, EArmReg rn, EArmReg rm);
		void				MOV_LSR(EArmReg rd, EArmReg rn, EArmReg rm);
		void				MOV_ASR(EArmReg rd, EArmReg rn, EArmReg rm);
		void				MOV_LSL_IMM(EArmReg rd, EArmReg rm, u8 imm5);
		void				MOV_LSR_IMM(EArmReg rd, EArmReg rm, u8 imm5);
		void				MOV_ASR_IMM(EArmReg rd, EArmReg rm, u8 imm5);
		void				MOV_IMM(EArmReg rd, u8 imm, u8 ror4 = 0, EArmCond = AL);

		void				MOVW(EArmReg reg, u16 imm);
		void				MOVT(EArmReg reg, u16 imm);

		/* Vfp instructions */
		void				VLDR (EArmVfpReg fd, EArmReg rn, s16 offset12);
		void				VSTR (EArmVfpReg fd, EArmReg rn, s16 offset12);
		void				VADD (EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm);
		void				VSUB (EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm);
		void				VMUL (EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm);
		void				VDIV (EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm);
		void				VSQRT(EArmVfpReg Sd, EArmVfpReg Sm);
		void				VABS (EArmVfpReg Sd, EArmVfpReg Sm);
		void				VNEG (EArmVfpReg Sd, EArmVfpReg Sm);
		void				VCMP (EArmVfpReg Sd, EArmVfpReg Sm, u8 E = 0);
		void				VCVT_S32_F32(EArmVfpReg Sd, EArmVfpReg Sm);
		void				VCVT_F64_F32(EArmVfpReg Dd, EArmVfpReg Sm);
		
		void				VMOV_S(EArmReg Rt, EArmVfpReg Dm);
		void				VMOV_S(EArmVfpReg Dm, EArmReg Rt);
		void				VMOV_S(EArmVfpReg Dm, EArmVfpReg Rt);
		void				VMOV_L(EArmReg Rt, EArmVfpReg Dm);
		void				VMOV_L(EArmVfpReg Dm, EArmReg Rt);
		void				VMOV_H(EArmReg Rt, EArmVfpReg Dm);
		void				VMOV_H(EArmVfpReg Dm, EArmReg Rt);
		void				VMOV (EArmVfpReg dm, EArmReg rt, EArmReg rt2);
		void				VMOV (EArmReg rt, EArmReg rt2, EArmVfpReg dm);
		void				VMOV (EArmVfpReg Dm, EArmVfpReg Rt);

		void				VADD_D (EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm);
		void				VSUB_D (EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm);
		void				VMUL_D (EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm);
		void				VDIV_D (EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm);
		void				VSQRT_D(EArmVfpReg Dd, EArmVfpReg Dm);
		void				VABS_D (EArmVfpReg Dd, EArmVfpReg Dm);
		void				VNEG_D (EArmVfpReg Dd, EArmVfpReg Dm);
		void				VCMP_D (EArmVfpReg Sd, EArmVfpReg Sm, u8 E = 0);
		void				VCVT_S32_F64(EArmVfpReg Sd, EArmVfpReg Dm);
		void				VCVT_F32_F64(EArmVfpReg Sd, EArmVfpReg Dm);
		
		void				VLDR_D (EArmVfpReg dd, EArmReg rn, s16 offset12);
		void				VSTR_D (EArmVfpReg dd, EArmReg rn, s16 offset12);

		/* Pseudo instructions for convinience */
		void				MOV32(EArmReg reg, u32 imm);
		void				CALL(CCodeLabel target);
		void				RET();
		CJumpLocation 		BX_IMM(CCodeLabel target, EArmCond cond = AL);

	private:
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
			int dist = GetLiteralPoolDistance(); 
			mpAssemblyBuffer->EmitDWORD( dword );
			
			if (dist == 4084)
			{
				// make sure our PC relative loads arn't too far away from the pool
				InsertLiteralPool(true);
			}
		}

		inline void EmitConstant(u32 c)
		{
			mpAssemblyBuffer->EmitDWORD( c );
		}
		
		CAssemblyBuffer*	mpAssemblyBuffer;
		CAssemblyBuffer*    mpAssemblyBufferA;
		CAssemblyBuffer*    mpAssemblyBufferB;
		std::vector<Literal>* literals;
		std::vector<Literal> literalsA;
		std::vector<Literal> literalsB;
};