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

#ifndef SYSW32_DYNAREC_X64_DYNARECTARGETX64_H_
#define SYSW32_DYNAREC_X64_DYNARECTARGETX64_H_

// Intel register codes. Odd ordering is for intel bytecode
enum EIntelReg {
    INVALID_CODE = 0xFFFFFFFF,
    RAX_CODE = 0,
    RCX_CODE = 1,
    RDX_CODE = 2,
    RBX_CODE = 3,
    RSP_CODE = 4,
    RBP_CODE = 5,
    RSI_CODE = 6,
    RDI_CODE = 7,

    R8_CODE = 8,
    R9_CODE = 9,
    R10_CODE = 10,
    R11_CODE = 11,
    R12_CODE = 12,
    R13_CODE = 13,
    R14_CODE = 14,
    R15_CODE = 15,

    NUM_X64_REGISTERS = 16,
};

#undef ALWAYS_ZERO_REG
#define SCRATCH_REG RAX_CODE

#endif // SYSW32_DYNAREC_X64_DYNARECTARGETX64_H_
