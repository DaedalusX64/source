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

#include "Core/R4300OpCode.h"

// ARM register codes.
enum EArmReg
{
	ArmReg_R0 = 0, ArmReg_R1,  ArmReg_R2,  ArmReg_R3,
	ArmReg_R4,     ArmReg_R5,  ArmReg_R6,  ArmReg_R7,
	ArmReg_R8,     ArmReg_R9,  ArmReg_R10, ArmReg_R11,
	ArmReg_R12,    ArmReg_R13, ArmReg_R14, ArmReg_R15,

	NUM_ARM_REGISTERS = 16,

	//Aliases
};

enum EArmVfpReg
{
	ArmVfpReg_S0 = 0, ArmVfpReg_S1,  ArmVfpReg_S2,  ArmVfpReg_S3,
	ArmVfpReg_S4,     ArmVfpReg_S5,  ArmVfpReg_S6,  ArmVfpReg_S7,
	ArmVfpReg_S8,     ArmVfpReg_S9,  ArmVfpReg_S10, ArmVfpReg_S11,
	ArmVfpReg_S12,    ArmVfpReg_S13, ArmVfpReg_S14, ArmVfpReg_S15,
	ArmVfpReg_S16,    ArmVfpReg_S17, ArmVfpReg_S18, ArmVfpReg_S19,
	ArmVfpReg_S20,    ArmVfpReg_S21, ArmVfpReg_S22, ArmVfpReg_S23,
	ArmVfpReg_S24,    ArmVfpReg_S25, ArmVfpReg_S26, ArmVfpReg_S27,
	ArmVfpReg_S28,    ArmVfpReg_S29, ArmVfpReg_S30, ArmVfpReg_S31,

	NUM_VFP_REGISTERS = 32,

	//Aliases
};

// ARM conditions. Do NOT reorder these
enum EArmCond
{
   EQ, NE, CS, CC, MI, PL, VS, VC,
   HI, LS, GE, LT, GT, LE, AL, NV
};

// Return true if this register dont need sign extension //Corn
inline bool	N64Reg_DontNeedSign( EN64Reg n64_reg )	{ return (0x30000001 >> n64_reg) & 1;}