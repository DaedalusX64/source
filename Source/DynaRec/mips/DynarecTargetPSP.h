/*
Copyright (C) 2005 StrmnNrmn

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

#ifndef SYSPSP_DYNAREC_DYNARECTARGETPSP_H_
#define SYSPSP_DYNAREC_DYNARECTARGETPSP_H_

#include "Core/R4300OpCode.h"

enum EPspReg
{
	PspReg_R0 = 0, PspReg_AT, PspReg_V0, PspReg_V1,
	PspReg_A0, PspReg_A1, PspReg_A2, PspReg_A3,
	PspReg_T0, PspReg_T1, PspReg_T2, PspReg_T3,
	PspReg_T4, PspReg_T5, PspReg_T6, PspReg_T7,
	PspReg_S0, PspReg_S1, PspReg_S2, PspReg_S3,
	PspReg_S4, PspReg_S5, PspReg_S6, PspReg_S7,
	PspReg_T8, PspReg_T9, PspReg_K0, PspReg_K1,
	PspReg_GP, PspReg_SP, PspReg_S8, PspReg_RA,

	PspReg_Invalid = -1
};

enum EPspFloatReg
{
	PspFloatReg_F00 = 0,PspFloatReg_F01, PspFloatReg_F02, PspFloatReg_F03, PspFloatReg_F04, PspFloatReg_F05, PspFloatReg_F06, PspFloatReg_F07,
	PspFloatReg_F08,	PspFloatReg_F09, PspFloatReg_F10, PspFloatReg_F11, PspFloatReg_F12, PspFloatReg_F13, PspFloatReg_F14, PspFloatReg_F15,
	PspFloatReg_F16,	PspFloatReg_F17, PspFloatReg_F18, PspFloatReg_F19, PspFloatReg_F20, PspFloatReg_F21, PspFloatReg_F22, PspFloatReg_F23,
	PspFloatReg_F24,	PspFloatReg_F25, PspFloatReg_F26, PspFloatReg_F27, PspFloatReg_F28, PspFloatReg_F29, PspFloatReg_F30, PspFloatReg_F31,

};

// always zero register
#define ALWAYS_ZERO_REG PspReg_R0
#define SCRATCH_REG PspReg_V0

// Return true if this register is temporary (i.e. not saved across function calls)
inline bool	Reg_IsTemporary( EPspReg psp_reg )	{ return (0xB300FFFF >> psp_reg) & 1;}

struct PspOpCode
{
	union
	{
		u32 _u32;

		struct
		{
			unsigned offset : 16;
			unsigned rt : 5;
			unsigned rs : 5;
			unsigned op : 6;
		};

		struct
		{
			unsigned immediate : 16;
			unsigned : 5;
			unsigned base : 5;
			unsigned : 6;
		};

		struct
		{
			unsigned target : 26;
			unsigned : 6;
		};

		// SPECIAL
		struct
		{
			unsigned spec_op : 6;
			unsigned sa : 5;
			unsigned rd : 5;
			unsigned : 5;
			unsigned : 5;
			unsigned : 6;
		};

		// REGIMM
		struct
		{
			unsigned : 16;
			unsigned regimm_op : 5;
			unsigned : 11;
		};

		// COP0 op
		struct
		{
			unsigned : 21;
			unsigned cop0_op : 5;
			unsigned : 6;
		};

		// COP1 op
		struct
		{
			unsigned cop1_funct : 6;
			unsigned fd : 5;		// sa
			unsigned fs : 5;		// rd
			unsigned ft : 5;		// rt
			unsigned cop1_op : 5;
			unsigned : 6;
		};

		struct
		{
			unsigned : 16;
			unsigned cop1_bc : 2;
			unsigned : 14;
		};
	};
};


#endif // SYSPSP_DYNAREC_DYNARECTARGETPSP_H_
