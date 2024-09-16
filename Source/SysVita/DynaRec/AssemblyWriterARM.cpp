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
#include "AssemblyWriterARM.h"
#include <cstdio>

// Calculate the number of leading zeroes in data.
static inline uint32_t
CountLeadingZeroesSlow(uint32_t data)
{
    // Other platforms must fall back to a C routine. This won't be as
    // efficient as the CLZ instruction, but it is functional.
    uint32_t    try_shift;

    uint32_t    leading_zeroes = 0;

    // This loop does a bisection search rather than the obvious rotation loop.
    // This should be faster, though it will still be no match for CLZ.
    for (try_shift = 16; try_shift != 0; try_shift /= 2) {
        uint32_t    shift = leading_zeroes + try_shift;
        if (((data << shift) >> shift) == data) {
            leading_zeroes = shift;
        }
    }

    return leading_zeroes;
}

inline uint32_t CountLeadingZeroes(uint32_t data)
{
    uint32_t    leading_zeroes;

#if defined(__ARMCC__)
    // ARMCC can do this with an intrinsic.
    leading_zeroes = __clz(data);
#elif defined(__GNUC__)
    leading_zeroes = __builtin_clz(data);
#else
    leading_zeroes = CountLeadingZeroesSlow(data);
#endif

    return leading_zeroes;
}

// The ARM instruction set allows some flexibility to the second operand of
// most arithmetic operations. When operand 2 is an immediate value, it takes
// the form of an 8-bit value rotated by an even value in the range 0-30.
//
// Some values that can be encoded this scheme — such as 0xf000000f — are
// probably fairly rare in practice and require extra code to detect, so this
// function implements a fast CLZ-based heuristic to detect any value that can
// be encoded using just a shift, and not a full rotation. For example,
// 0xff000000 and 0x000000ff are both detected, but 0xf000000f is not.
//
// This function will return true to indicate that the encoding was successful,
// or false to indicate that the literal could not be encoded as an operand 2
// immediate. If successful, the encoded value will be written to *enc.
inline bool encOp2Imm(uint32_t literal, uint32_t * enc)
{
    // The number of leading zeroes in the literal. This is used to calculate
    // the rotation component of the encoding.
    uint32_t    leading_zeroes;

    // Components of the operand 2 encoding.
    int32_t    rot;
    uint32_t    imm8;

    // Check the literal to see if it is a simple 8-bit value. I suspect that
    // most literals are in fact small values, so doing this check early should
    // give a decent speed-up.
    if (literal < 256)
    {
        *enc = literal;
        return true;
    }

    // Determine the number of leading zeroes in the literal. This is used to
    // calculate the required rotation.
    leading_zeroes = CountLeadingZeroes(literal);

    // Assuming that we have a field of no more than 8 bits for a valid
    // literal, we can calculate the required rotation by subtracting
    // leading_zeroes from (32-8):
    //
    // Example:
    //      0: Known to be zero.
    //      1: Known to be one.
    //      X: Either zero or one.
    //      .: Zero in a valid operand 2 literal.
    //
    //  Literal:     [ 1XXXXXXX ........ ........ ........ ]
    //  leading_zeroes = 0
    //  Therefore rot (left) = 24.
    //  Encoded 8-bit literal:                  [ 1XXXXXXX ]
    //
    //  Literal:     [ ........ ..1XXXXX XX...... ........ ]
    //  leading_zeroes = 10
    //  Therefore rot (left) = 14.
    //  Encoded 8-bit literal:                  [ 1XXXXXXX ]
    //
    // Note, however, that we can only encode even shifts, and so
    // "rot=24-leading_zeroes" is not sufficient by itself. By ignoring
    // zero-bits in odd bit positions, we can ensure that we get a valid
    // encoding.
    //
    // Example:
    //  Literal:     [ 01XXXXXX ........ ........ ........ ]
    //  leading_zeroes = 1
    //  Therefore rot (left) = round_up(23) = 24.
    //  Encoded 8-bit literal:                  [ 01XXXXXX ]
    rot = 24 - (leading_zeroes & ~1);

    // The imm8 component of the operand 2 encoding can be calculated from the
    // rot value.
    imm8 = literal >> rot;

    // The validity of the literal can be checked by reversing the
    // calculation. It is much easier to decode the immediate than it is to
    // encode it!
    if (literal != (imm8 << rot)) {
        // The encoding is not valid, so report the failure. Calling code
        // should use some other method of loading the value (such as LDR).
        return false;
    }

    // The operand is valid, so encode it.
    // Note that the ARM encoding is actually described by a rotate to the
    // _right_, so rot must be negated here. Calculating a left shift (rather
    // than calculating a right rotation) simplifies the above code.
    *enc = ((-rot << 7) & 0xf00) | imm8;

    return true;
}

void	CAssemblyWriterARM::TST(EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe1100000 | rn << 16 | rm);
}

void	CAssemblyWriterARM::CMP_IMM(EArmReg rn, u8 imm)
{
	EmitDWORD(0xe3500000 | rn << 16 | imm);
}

void	CAssemblyWriterARM::CMP(EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe1500000 | rn << 16 | rm);
}

void	CAssemblyWriterARM::ADD(EArmReg rd, EArmReg rn, EArmReg rm, EArmCond cond, u8 S)
{
	EmitDWORD(0x00800000 | (cond << 28) | (S << 20) | ((rd) << 12) | ((rn) << 16) | rm);
}

void	CAssemblyWriterARM::ADD_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp)
{
	u32 enc;
	if(encOp2Imm(imm, &enc))
	{
		EmitDWORD(0xe2800000 | (rd << 12) | (rn << 16) | enc );
	}
	else if(encOp2Imm(-imm, &enc))
	{
		SUB_IMM(rd, rn, -imm, temp);
	}
	else
	{
		MOV32(temp, imm);
		ADD(rd,rn,temp);
	}
}

void	CAssemblyWriterARM::ADD_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4)
{
	EmitDWORD(0xe2800000 | (rd << 12) | (rn << 16) | ((ror4 & 0xf) << 8) | imm );
}

void	CAssemblyWriterARM::SUB(EArmReg rd, EArmReg rn, EArmReg rm, EArmCond cond, u8 S)
{
	EmitDWORD(0x00400000 | (cond << 28) | (S << 20) | ((rd) << 12) | ((rn) << 16) | rm);
}

void	CAssemblyWriterARM::SUB_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp)
{
	u32 enc;
	if(encOp2Imm(imm, &enc))
	{
		EmitDWORD(0xe2400000 | (rd << 12) | (rn << 16) | enc );
	}
	else if(encOp2Imm(-imm, &enc))
	{
		ADD_IMM(rd, rn, -imm, temp);
	}
	else
	{
		MOV32(temp, imm);
		SUB(rd,rn,temp);
	}
}

void	CAssemblyWriterARM::SUB_IMM(EArmReg rd, EArmReg rn, u8 imm, u8 ror4)
{
	EmitDWORD(0xe2400000 | (rd << 12) | (rn << 16) | ((ror4 & 0xf) << 8) | imm );
}

void	CAssemblyWriterARM::ADC(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe0a00000 | (rd << 12) | (rn << 16) | rm);
}

void	CAssemblyWriterARM::ADC_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp)
{
	u32 enc;
	if (encOp2Imm(imm, &enc))
	{
		EmitDWORD(0xe2a00000 | (rd << 12) | (rn << 16) | enc );
	}
	else if (encOp2Imm(-imm - 1, &enc))
	{
		SBC_IMM(rd, rn, -imm - 1, temp);
	}
	else
	{
		MOV32(temp, imm);
		ADC(rd, rn, temp);
	}
}

void	CAssemblyWriterARM::SBC(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe0c00000 | (rd << 12) | (rn << 16) | rm);
}

void	CAssemblyWriterARM::SBC_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp)
{
	u32 enc;
	if (encOp2Imm(imm,&enc))
	{
		EmitDWORD(0xe2c00000 | (rd << 12) | (rn << 16) | enc);
	}
	else if(encOp2Imm(-imm-1, &enc))
	{
		ADC_IMM(rd, rn, -imm - 1, temp);
	}
	else
	{
		MOV32(temp, imm);
		SBC(rd, rn, temp);
	}
}

void 	CAssemblyWriterARM::MUL(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe0000090 | (rm << 12) | (rd << 16) | rn);
}

void 	CAssemblyWriterARM::UMULL(EArmReg rdLo, EArmReg rdHi, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe0800090 | (rdHi << 16) | (rdLo << 12) | (rn << 8) | rm );
}

void 	CAssemblyWriterARM::SMULL(EArmReg rdLo, EArmReg rdHi, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe0c00090 | (rdHi << 16) | (rdLo << 12) | (rn << 8) | rm );
}

void	CAssemblyWriterARM::NEG(EArmReg rd, EArmReg rm)
{
	EmitDWORD(0xe2600000 | (rd << 12) | (rm << 16));
}

void	CAssemblyWriterARM::BIC(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe1c00000 | (rd << 12) | (rn << 16) | rm);
}

void	CAssemblyWriterARM::BIC_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp)
{
	u32 enc;
	if (encOp2Imm(imm, &enc))
	{
		EmitDWORD(0xe3c00000 | (rd << 12) | (rn << 16) | enc);
	}
	else if (encOp2Imm(~imm, &enc))
	{
		AND_IMM(rd, rn, ~imm, temp);
	}
	else
	{
		MOV32(temp, imm);
		BIC(rd, rn, temp);
	}
}

void	CAssemblyWriterARM::AND(EArmReg rd, EArmReg rn, EArmReg rm, EArmCond cond)
{
	EmitDWORD(0x00000000 | (cond << 28) | ((rd) << 12) | ((rn) << 16) | rm);
}

void	CAssemblyWriterARM::AND_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp)
{
	u32 enc;
	if(encOp2Imm(imm, &enc))
	{
		EmitDWORD(0xe2000000 | (rd << 12) | (rn << 16) | enc );
	}
	else if (encOp2Imm(~imm, &enc))
	{
		BIC_IMM(rd, rn, ~imm, temp);
	}
	else
	{
		MOV32(temp, imm);
		AND(rd, rn, temp);
	}
}

void	CAssemblyWriterARM::AND_IMM(EArmReg rd, EArmReg rn, u8 imm)
{
	EmitDWORD(0xe2000000 | (rd << 12) | (rn << 16) | imm );
}

void	CAssemblyWriterARM::ORR(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe1800000 | (rd << 12) | (rn << 16) | rm);
}

void	CAssemblyWriterARM::ORR_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp)
{
	u32 enc;
	
	if(encOp2Imm(imm,&enc))
	{
		EmitDWORD(0xe3800000 | (rd << 12) | (rn << 16) | enc);
	}
	else
	{
		MOV32(temp, imm);
		ORR(rd, rn, temp);
	}
}

void	CAssemblyWriterARM::XOR(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe0200000 | (rd << 12) | (rn << 16) | rm);
}

void	CAssemblyWriterARM::XOR_IMM(EArmReg rd, EArmReg rn, u8 imm)
{
	EmitDWORD(0xe2200000 | (rd << 12) | (rn << 16) | imm);
}

void	CAssemblyWriterARM::XOR_IMM(EArmReg rd, EArmReg rn, u32 imm, EArmReg temp)
{
	u32 enc;
	
	if(encOp2Imm(imm, &enc))
	{
		EmitDWORD(0xe2200000 | (rd << 12) | (rn << 16) | enc);
	}
	else
	{
		MOV32(temp, imm);
		XOR(rd, rn, temp);
	}
}

void	CAssemblyWriterARM::B(s32 offset, EArmCond cond)
{
	EmitDWORD(0x0a000000 | (cond << 28) | (offset >> 2));
}

void	CAssemblyWriterARM::BL(s32 offset, EArmCond cond)
{
	EmitDWORD(0x0b000000 | (cond << 28) | (offset >> 2));
}

void	CAssemblyWriterARM::BX(EArmReg rm, EArmCond cond)
{
	EmitDWORD(0x012fff10 | rm | (cond << 28));
	if(cond == AL)	InsertLiteralPool(false);
}

void	CAssemblyWriterARM::BLX(EArmReg rm, EArmCond cond)
{
	EmitDWORD(0x012fff30 | rm | (cond << 28));
}

void	CAssemblyWriterARM::PUSH(u16 regs)
{
	EmitDWORD(0xe92d0000 | regs);
}

void	CAssemblyWriterARM::POP(u16 regs)
{
	EmitDWORD(0xe8bd0000 | regs);
}

void	CAssemblyWriterARM::LDR(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe5100000 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | (offset & 0xfff));
}

void	CAssemblyWriterARM::LDRB(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe5500000 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | (offset & 0xfff));
}

void	CAssemblyWriterARM::LDRSB(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe15000d0 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | ((abs(offset) & 0xf0) << 4) | (abs(offset) & 0xf));
}

void	CAssemblyWriterARM::LDRSH(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe15000f0 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | ((abs(offset) & 0xf0) << 4) | (abs(offset) & 0xf));
}

void	CAssemblyWriterARM::LDRH(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe15000b0 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | ((abs(offset) & 0xf0) << 4) | (abs(offset) & 0xf));
}

void	CAssemblyWriterARM::LDRD(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe14000d0 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | ((abs(offset & 0xf0)) << 4) | (abs(offset) & 0xf));
}

void	CAssemblyWriterARM::LDR_REG(EArmReg rt, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe7100000 | (1 << 23) | (rn << 16) | (rt << 12) | (rm));
}

void	CAssemblyWriterARM::LDRB_REG(EArmReg rt, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe7500000 | (1 << 23) | (rn << 16) | (rt << 12) | (rm));
}

void	CAssemblyWriterARM::LDRSB_REG(EArmReg rt, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe11000d0 | (1 << 23) | (rn << 16) | (rt << 12) | rm);
}

void	CAssemblyWriterARM::LDRSH_REG(EArmReg rt, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe11000f0 | (1 << 23) | (rn << 16) | (rt << 12) | (rm));
}

void	CAssemblyWriterARM::LDRH_REG(EArmReg rt, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe11000b0 | (1 << 23) | (rn << 16) | (rt << 12) | (rm));
}

void	CAssemblyWriterARM::STR(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe5000000 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | (offset & 0xfff));
}

void	CAssemblyWriterARM::STRH(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe14000b0 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | ((abs(offset) & 0xf0) << 4) | (abs(offset) & 0xf));
}

void	CAssemblyWriterARM::STRB(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe5400000 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | (offset & 0xfff));
}

void	CAssemblyWriterARM::STRD(EArmReg rt, EArmReg rn, s16 offset)
{
	EmitDWORD(0xe14000f0 | ((offset >= 0) << 23) | ( rn << 16 ) | ( rt << 12 ) | ((abs(offset) & 0xf0) << 4) | (abs(offset) & 0xf));
}

void	CAssemblyWriterARM::STR_REG(EArmReg rt, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe7000000 | (1 << 23) | (rn << 16) | (rt << 12) | (rm));
}

void	CAssemblyWriterARM::STRH_REG(EArmReg rt, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe10000b0 | (1 << 23) | (rn << 16) | (rt << 12) | (rm));
}

void	CAssemblyWriterARM::STRB_REG(EArmReg rt, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe7400000 | (1 << 23) | (rn << 16) | (rt << 12) | (rm));
}

void	CAssemblyWriterARM::MVN(EArmReg rd, EArmReg rm)
{
	EmitDWORD(0xe1e00000 | (rd << 12) | rm);
}

void	CAssemblyWriterARM::MOV(EArmReg rd, EArmReg rm)
{
	EmitDWORD(0xe1a00000 | (rd << 12) | rm);
}

void	CAssemblyWriterARM::MOV_LSL(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe1a00010 | (rd << 12) | rn | (rm << 8));
}

void	CAssemblyWriterARM::MOV_LSR(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe1a00030 | (rd << 12) | rn | (rm << 8));
}

void	CAssemblyWriterARM::MOV_ASR(EArmReg rd, EArmReg rn, EArmReg rm)
{
	EmitDWORD(0xe1a00050 | (rd << 12) | rn | (rm << 8));
}

void	CAssemblyWriterARM::MOV_LSL_IMM(EArmReg rd, EArmReg rm, u8 imm5)
{
	EmitDWORD(0xe1a00000 | (rd << 12) | rm | (imm5 << 7));
}

void	CAssemblyWriterARM::MOV_LSR_IMM(EArmReg rd, EArmReg rm, u8 imm5)
{
	EmitDWORD(0xe1a00020 | (rd << 12) | rm | (imm5 << 7));
}

void	CAssemblyWriterARM::MOV_ASR_IMM(EArmReg rd, EArmReg rm, u8 imm5)
{
	EmitDWORD(0xe1a00040 | (rd << 12) | rm | (imm5 << 7));
}

void	CAssemblyWriterARM::MOV_IMM(EArmReg rd, u8 imm, u8 ror4, EArmCond cond)
{
	EmitDWORD(0x03a00000 | (cond << 28) | (rd << 12) | ((ror4 & 0xf) << 8) | imm);
}

void	CAssemblyWriterARM::VLDR(EArmVfpReg fd, EArmReg rn, s16 offset12)
{
	EmitDWORD(0xed100a00 | ((offset12 < 0) ? 0 : 1) << 23 | ((fd & 1) << 22) | (rn << 16) | (((fd >> 1) & 15) << 12) | ((abs(offset12) >> 2) & 255));
}

void	CAssemblyWriterARM::VSTR(EArmVfpReg fd, EArmReg rn, s16 offset12)
{
	EmitDWORD(0xed000a00 | ((offset12 < 0) ? 0 : 1) << 23 | ((fd & 1) << 22) | (rn << 16) | (((fd >> 1) & 15) << 12) | ((abs(offset12) >> 2) & 255));
}

void	CAssemblyWriterARM::VADD(EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm)
{
	EmitDWORD(0xee300a00 | ((Sd & 1) << 22) | (((Sn >> 1) & 15) << 16) | (((Sd >> 1) & 15) << 12) | ((Sn & 1) << 7) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VSUB(EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm)
{
	EmitDWORD(0xee300a40 | ((Sd & 1) << 22) | (((Sn >> 1) & 15) << 16) | (((Sd >> 1) & 15) << 12) | ((Sn & 1) << 7) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VMUL(EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm)
{
	EmitDWORD(0xee200a00 | ((Sd & 1) << 22) | (((Sn >> 1) & 15) << 16) | (((Sd >> 1) & 15) << 12) | ((Sn & 1) << 7) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VDIV(EArmVfpReg Sd, EArmVfpReg Sn, EArmVfpReg Sm)
{
	EmitDWORD(0xee800a00 | ((Sd & 1) << 22) | (((Sn >> 1) & 15) << 16) | (((Sd >> 1) & 15) << 12) | ((Sn & 1) << 7) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VSQRT(EArmVfpReg Sd, EArmVfpReg Sm)
{
	EmitDWORD(0xeeb10ac0 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VABS(EArmVfpReg Sd, EArmVfpReg Sm)
{
	EmitDWORD(0xeeb00ac0 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VNEG(EArmVfpReg Sd, EArmVfpReg Sm)
{
	EmitDWORD(0xeeb10a40 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VCMP(EArmVfpReg Sd, EArmVfpReg Sm, u8 E)
{
	EmitDWORD(0xeeb40a40 | ((Sd & 1) << 22) |  (((Sd >> 1) & 15) << 12) | ((Sm & 1) << 5) | ((Sm >> 1) & 15) | (E << 7));
	
	//vmrs    APSR_nzcv, FPSCR    @ Get the flags into APSR.
	EmitDWORD(0xeef1fa10);
}
// round to zero
void	CAssemblyWriterARM::VCVT_S32_F32(EArmVfpReg Sd, EArmVfpReg Sm)
{
	EmitDWORD(0xeebd0ac0 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | ((Sm & 1)<<5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VCVT_F64_F32(EArmVfpReg Dd, EArmVfpReg Sm)
{
	EmitDWORD(0xeeb70ac0 | (((Dd >> 4) & 1) << 22) | ((Dd & 15) << 12) | ((Sm & 1) << 5) | ((Sm >> 1) & 15));
}

void	CAssemblyWriterARM::VADD_D(EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm)
{
	EmitDWORD(0xee300b00 | (((Dd >> 4) & 1) << 22) | ((Dn & 15) << 16) | ((Dd & 15) << 12) | (((Dn >> 4) & 1) << 7) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VSUB_D(EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm)
{
	EmitDWORD(0xee300b40 | (((Dd >> 4) & 1) << 22) | ((Dn & 15) << 16) | ((Dd & 15) << 12) | (((Dn >> 4) & 1) << 7) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VMUL_D(EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm)
{
	EmitDWORD(0xee200b00 | (((Dd >> 4) & 1) << 22) | ((Dn & 15) << 16) | ((Dd & 15) << 12) | (((Dn >> 4) & 1) << 7) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VDIV_D(EArmVfpReg Dd, EArmVfpReg Dn, EArmVfpReg Dm)
{
	EmitDWORD(0xee800b00 | (((Dd >> 4) & 1) << 22) | ((Dn & 15) << 16) | ((Dd & 15) << 12) | (((Dn >> 4) & 1) << 7) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VMOV_S(EArmReg Rt, EArmVfpReg Dm)
{
	EmitDWORD(0xee100a10 | ((Dm >> 1) << 16) | (Rt << 12) | ((Dm & 0x1) << 7));
}

void	CAssemblyWriterARM::VMOV_S( EArmVfpReg Dm, EArmReg Rt)
{
	EmitDWORD(0xee000a10 | ((Dm >> 1) << 16) | (Rt << 12) | ((Dm & 0x1) << 7));
}

void	CAssemblyWriterARM::VMOV_S( EArmVfpReg Dm, EArmVfpReg Rt)
{
	EmitDWORD(0xeeb00a40 | ((Dm & 1) << 22) | (((Dm>>1) & 15) << 12) | ((Rt & 1) << 5) | ((Rt >> 1) & 15));
}

void	CAssemblyWriterARM::VMOV_L(EArmReg Rt, EArmVfpReg Dm)
{
	EmitDWORD(0xee100b10 | ((Dm) << 16) | (Rt << 12));
}

void	CAssemblyWriterARM::VMOV_L(EArmVfpReg Dm, EArmReg Rt)
{
	EmitDWORD(0xee000b10 | ((Dm) << 16) | (Rt << 12));
}

void	CAssemblyWriterARM::VMOV_H(EArmReg Rt, EArmVfpReg Dm)
{
	EmitDWORD(0xee300b10 | ((Dm) << 16) | (Rt << 12));
}

void	CAssemblyWriterARM::VMOV_H(EArmVfpReg Dm, EArmReg Rt)
{
	EmitDWORD(0xee200b10 | ((Dm) << 16) | (Rt << 12));
}

void	CAssemblyWriterARM::VSQRT_D(EArmVfpReg Dd, EArmVfpReg Dm)
{
	EmitDWORD(0xeeb10bc0 | (((Dd >> 4) & 1) << 22) | ((Dd & 15) << 12) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VABS_D(EArmVfpReg Dd, EArmVfpReg Dm)
{
	EmitDWORD(0xeeb00bc0 | (((Dd >> 4) & 1) << 22) | ((Dd & 15) << 12) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VNEG_D(EArmVfpReg Dd, EArmVfpReg Dm)
{
	EmitDWORD(0xeeb10b40 | (((Dd >> 4) & 1) << 22) | ((Dd & 15) << 12) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VMOV(EArmVfpReg Dm, EArmReg Rt, EArmReg Rt2)
{
	EmitDWORD(0xec400b10 | (Rt2 << 16) | (Rt << 12) | (Dm & 0b1111) | (((Dm >> 4) & 1) << 5));
}

void	CAssemblyWriterARM::VMOV(EArmReg Rt, EArmReg Rt2, EArmVfpReg Dm)
{
	EmitDWORD(0xec500b10 | (Rt2 << 16) | (Rt << 12) | (Dm & 0b1111) | (((Dm >> 4) & 1) << 5));
}

void	CAssemblyWriterARM::VMOV( EArmVfpReg Dm, EArmVfpReg Rt)
{
	EmitDWORD(0xeeb00b40 | (((Dm >> 4) & 1) << 22) | ((Dm & 15) << 12) | (((Rt >> 4) & 1) << 5) | (Rt & 15));
}

void	CAssemblyWriterARM::VLDR_D(EArmVfpReg Dd, EArmReg Rn, s16 offset12)
{
	EmitDWORD(0xed100b00 | ((offset12 < 0) ? 0 : 1) << 23 | (((Dd >> 4) & 1) << 22) | (Rn << 16) | ((Dd & 15) << 12) | ((abs(offset12) >> 2) & 255));
}

void	CAssemblyWriterARM::VSTR_D(EArmVfpReg Dd, EArmReg Rn, s16 offset12)
{
	EmitDWORD(0xed000b00 | ((offset12 < 0) ? 0 : 1) << 23 | (((Dd >> 4) & 1) << 22) | (Rn << 16) | ((Dd & 15) << 12) | ((abs(offset12) >> 2) & 255));
}

void	CAssemblyWriterARM::VCMP_D(EArmVfpReg Dd, EArmVfpReg Dm, u8 E)
{
	EmitDWORD(0xeeb40b40 | (((Dd >> 4) & 1) << 22) | ((Dd & 15) << 12) | (((Dm >> 4) & 1) << 5) | (Dm & 15) | (E << 7));

	//vmrs    APSR_nzcv, FPSCR    @ Get the flags into APSR.
	EmitDWORD(0xeef1fa10);
}

void	CAssemblyWriterARM::VCVT_S32_F64(EArmVfpReg Sd, EArmVfpReg Dm)
{
	EmitDWORD(0xeebd0bc0 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

void	CAssemblyWriterARM::VCVT_F32_F64(EArmVfpReg Sd, EArmVfpReg Dm)
{
	EmitDWORD(0xeeb70bc0 | ((Sd & 1) << 22) | (((Sd >> 1) & 15) << 12) | (((Dm >> 4) & 1) << 5) | (Dm & 15));
}

#ifdef DYNAREC_ARMV7
void	CAssemblyWriterARM::MOVW(EArmReg reg, u16 imm)
{
	EmitDWORD(0xe3000000 | (reg << 12) | ((imm & 0xf000) << 4) | (imm & 0x0fff));
}

void	CAssemblyWriterARM::MOVT(EArmReg reg, u16 imm)
{
	EmitDWORD(0xe3400000 | (reg << 12) | ((imm & 0xf000) << 4) | (imm & 0x0fff));
}
#endif

void	CAssemblyWriterARM::MOV32(EArmReg reg, u32 imm)
{
	if(!(imm >> 16))
	{
		#ifdef DYNAREC_ARMV7
		MOVW(reg, imm);
		#else
		MOV_IMM(reg, imm);
		if(imm & 0xff00) ADD_IMM(reg, reg, imm >> 8, 0xc);
		#endif
	}
	else
	{
		literals->push_back( Literal { mpAssemblyBuffer->GetLabel(), imm } );
		//This will be patched later to reflect the location of the literal pool
		LDR(reg, ArmReg_R15, 0x00);
	}
}

CJumpLocation CAssemblyWriterARM::BX_IMM( CCodeLabel target, EArmCond cond )
{
	u32 address( target.GetTargetU32() );

	CJumpLocation jump_location( mpAssemblyBuffer->GetJumpLocation() );

	#ifdef DYNAREC_ARMV7
	MOVW(ArmReg_R4, address);
	MOVT(ArmReg_R4, address >> 16);
	BX(ArmReg_R4, cond);
	#else
	s32 offset = (jump_location.GetOffset(target) - 8);
	B(offset & 0x3FFFFFF, cond);
	#endif
	if (cond == AL) InsertLiteralPool(false);

	return jump_location;
}

void CAssemblyWriterARM::CALL( CCodeLabel target )
{
	PUSH(0x5000); //R12, LR

	MOV32(ArmReg_R4, target.GetTargetU32());
	BLX(ArmReg_R4);

	POP(0x5000); //R12, LR
}

void CAssemblyWriterARM::RET()
{
	EmitDWORD(0xecbd0b20);
	POP(0x9ff0);
	InsertLiteralPool(false);
}

void CAssemblyWriterARM::InsertLiteralPool(bool branch)
{
	if( literals->empty() ) return;

	if(branch) B( (literals->size() - 1) * 4 );

	for (int i = 0; i < literals->size(); i++)
	{
		uint32_t *op =  (uint32_t*)(*literals)[i].Target.GetTarget();
		uint32_t offset = mpAssemblyBuffer->GetLabel().GetTargetU32() - (uint32_t)op;

		*op = *op | (offset - 8);

		EmitConstant((*literals)[i].Value);
	}

	literals->clear();
}

uint32_t CAssemblyWriterARM::GetLiteralPoolDistance()
{
	if(literals->empty())
		return 0;

	return mpAssemblyBuffer->GetLabel().GetTargetU32() - (*literals)[0].Target.GetTargetU32();
}