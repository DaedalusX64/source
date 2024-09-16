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

#include <stdlib.h>

#include "DynarecTargetARM.h"

//*************************************************************************************
//
//*************************************************************************************
class CN64RegisterCacheARM
{
public:
		CN64RegisterCacheARM();

		void Reset();

		inline void	SetCachedReg( EN64Reg n64_reg, u32 lo_hi_idx, EArmReg arm_reg )
		{
			mRegisterCacheInfo[ n64_reg ][ lo_hi_idx ].ArmRegister = arm_reg;
		}

		inline bool	IsCached( EN64Reg reg, u32 lo_hi_idx ) const
		{
			return mRegisterCacheInfo[ reg ][ lo_hi_idx ].ArmRegister != NUM_ARM_REGISTERS;
		}

		inline bool	IsValid( EN64Reg reg, u32 lo_hi_idx ) const
		{
			#ifdef DAEDALUS_ENABLE_ASSERTS
			DAEDALUS_ASSERT( !mRegisterCacheInfo[ reg ][ lo_hi_idx ].Valid || IsCached( reg, lo_hi_idx ), "Checking register is valid but uncached?" );
			#endif
			return mRegisterCacheInfo[ reg ][ lo_hi_idx ].Valid;
		}

		inline bool	IsDirty( EN64Reg reg, u32 lo_hi_idx ) const
		{
#ifdef DAEDALUS_ENABLE_ASSERTS
			bool	is_dirty( mRegisterCacheInfo[ reg ][ lo_hi_idx ].Dirty );

			if( is_dirty )
			{
				DAEDALUS_ASSERT( IsKnownValue( reg, lo_hi_idx ) || IsCached( reg, lo_hi_idx ), "Checking dirty flag on unknown/uncached register?" );
			}

			return is_dirty;
#else
			return mRegisterCacheInfo[ reg ][ lo_hi_idx ].Dirty;
#endif
		}

		inline EArmReg	GetCachedReg( EN64Reg reg, u32 lo_hi_idx ) const
		{
			#ifdef DAEDALUS_ENABLE_ASSERTS
			DAEDALUS_ASSERT( IsCached( reg, lo_hi_idx ), "Trying to retreive an uncached register" );
			#endif
			return mRegisterCacheInfo[ reg ][ lo_hi_idx ].ArmRegister;
		}

		inline void	MarkAsValid( EN64Reg reg, u32 lo_hi_idx, bool valid )
		{
			#ifdef DAEDALUS_ENABLE_ASSERTS
			DAEDALUS_ASSERT( IsCached( reg, lo_hi_idx ), "Changing valid flag on uncached register?" );
			#endif
			mRegisterCacheInfo[ reg ][ lo_hi_idx ].Valid = valid;
		}

		inline void	MarkAsDirty( EN64Reg reg, u32 lo_hi_idx, bool dirty )
		{
#ifdef DAEDALUS_ENABLE_ASSERTS
			if( dirty )
			{
				 DAEDALUS_ASSERT( IsKnownValue( reg, lo_hi_idx ) || IsCached( reg, lo_hi_idx ), "Setting dirty flag on unknown/uncached register?" );
			}
#endif
			mRegisterCacheInfo[ reg ][ lo_hi_idx ].Dirty = dirty;
		}


		inline bool	IsKnownValue( EN64Reg reg, u32 lo_hi_idx ) const
		{
			return mRegisterCacheInfo[ reg ][ lo_hi_idx ].Known;
		}

		inline void	SetKnownValue( EN64Reg reg, u32 lo_hi_idx, s32 value )
		{
			mRegisterCacheInfo[ reg ][ lo_hi_idx ].Known = true;
			mRegisterCacheInfo[ reg ][ lo_hi_idx ].KnownValue._u32 = value;
		}

		inline void	ClearKnownValue( EN64Reg reg, u32 lo_hi_idx )
		{
			mRegisterCacheInfo[ reg ][ lo_hi_idx ].Known = false;
		}

		inline REG32	GetKnownValue( EN64Reg reg, u32 lo_hi_idx ) const
		{
			return mRegisterCacheInfo[ reg ][ lo_hi_idx ].KnownValue;
		}

		inline bool	IsFPValid( EN64FloatReg reg ) const
		{
			return mFPRegisterCacheInfo[ reg ].Valid;
		}

		inline bool	IsFPDirty( EN64FloatReg reg ) const
		{
			return mFPRegisterCacheInfo[ reg ].Dirty;
		}

		inline bool	IsFPSim( EN64FloatReg reg ) const
		{
			return mFPRegisterCacheInfo[ reg ].Sim;
		}

		inline void	MarkFPAsValid( EN64FloatReg reg, bool valid )
		{
			mFPRegisterCacheInfo[ reg ].Valid = valid;
		}

		inline void	MarkFPAsDirty( EN64FloatReg reg, bool dirty )
		{
			mFPRegisterCacheInfo[ reg ].Dirty = dirty;
		}

		inline void	MarkFPAsSim( EN64FloatReg reg, bool Sim )
		{
			mFPRegisterCacheInfo[ reg ].Sim = Sim;
		}

		void		ClearCachedReg( EN64Reg n64_reg, u32 lo_hi_idx );

private:

		struct RegisterCacheInfoARM
		{
			REG32			KnownValue;			// The contents (if known)
			EArmReg			ArmRegister;		// If cached, this is the psp register we're using
			bool			Valid;				// Is the contents of the register valid?
			bool			Dirty;				// Is the contents of the register modified?
			bool			Known;				// Is the contents of the known?
			//bool			SignExtended;		// Is this (high) register just sign extension of low reg?
		};

		// ARM fp registers are stored in a 1:1 mapping with the n64 counterparts
		struct FPRegisterCacheInfoARM
		{
			bool			Valid;
			bool			Dirty;
			bool			Sim;
		};
		
		RegisterCacheInfoARM	mRegisterCacheInfo[ NUM_N64_REGS ][ 2 ];
		FPRegisterCacheInfoARM	mFPRegisterCacheInfo[ NUM_N64_FP_REGS ];
};