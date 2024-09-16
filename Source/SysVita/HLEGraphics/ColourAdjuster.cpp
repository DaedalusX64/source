/*
Copyright (C) 2006 StrmnNrmn

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
#include "ColourAdjuster.h"

#include <stdlib.h>

#include "HLEGraphics/DaedalusVtx.h"

//*****************************************************************************
//
//*****************************************************************************
void	CColourAdjuster::Reset()
{
	mModulateMask = 0;
	mSetMask = 0;
	mSubtractMask = 0;
}

//*****************************************************************************
//
//*****************************************************************************
void CColourAdjuster::Process( uint32_t * p_vertices, u32 num_verts ) const
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( (mSetMask & mModulateMask & mSubtractMask) == 0, "Setting and modulating the same component" );
	#endif
	switch( mSetMask )
	{
		case 0:
		break;

		case COL32_MASK_RGBA:
			{
				for(u32 v = 0 ; v < num_verts; v++)
				{
					p_vertices[v] = mSetColour.GetColour();
				}
			}
		break;

		default:
			{
				u32		clear_bits {~mSetMask};
				u32		set_bits {mSetColour.GetColour() & mSetMask};

				for(u32 v = 0; v < num_verts; v++)
				{
					p_vertices[v] = (p_vertices[v] & clear_bits) | set_bits;
				}
			}
		break;
	}

	switch( mSubtractMask )
	{
		case COL32_MASK_RGB:
			{
				for(u32 v = 0; v < num_verts; v++)
				{
					p_vertices[v] = c32(p_vertices[v]).SubRGB( mSubtractColour ).GetColour();
				}
			}
		break;

		case COL32_MASK_A:
			{
				for(u32 v = 0; v < num_verts; v++)
				{
					p_vertices[v] = c32(p_vertices[v]).SubA( mSubtractColour ).GetColour();
				}
			}
		break;

		case COL32_MASK_RGBA:
			{
				for(u32 v = 0; v < num_verts; v++)
				{
					p_vertices[v] = c32(p_vertices[v]).Sub( mSubtractColour ).GetColour();
				}
			}
		break;
	}

	switch( mModulateMask )
	{
		case COL32_MASK_RGB:
			{
				for(u32 v = 0; v < num_verts; v++)
				{
					p_vertices[v] = c32(p_vertices[v]).ModulateRGB( mModulateColour ).GetColour();
				}
			}
		break;

		case COL32_MASK_A:
			{
				for(u32 v = 0; v < num_verts; v++)
				{
					p_vertices[v] = c32(p_vertices[v]).ModulateA( mModulateColour ).GetColour();
				}
			}
		break;

		case COL32_MASK_RGBA:
			{
				for(u32 v = 0; v < num_verts; v++)
				{
					p_vertices[v] = c32(p_vertices[v]).Modulate( mModulateColour ).GetColour();
				}
			}
		break;
	}

}

//*****************************************************************************
//
//*****************************************************************************
void	CColourAdjuster::Set( u32 mask, c32 colour )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( (GetMask() & mask) == 0, "These bits have already been set" );
	#endif
	mSetMask |= mask;

	u32		current {mSetColour.GetColour()};
	u32		col {colour.GetColour()};

	mSetColour = c32( ( current & ~mask) | (col & mask) );
}

//*****************************************************************************
//
//*****************************************************************************
void	CColourAdjuster::Modulate( u32 mask, c32 colour )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( (GetMask() & mask) == 0, "These bits have already been set" );
	#endif
	mModulateMask |= mask;

	u32		current {mModulateColour.GetColour()};
	u32		col {colour.GetColour()};

	mModulateColour = c32( ( current & ~mask) | (col & mask) );
}

//*****************************************************************************
//
//*****************************************************************************
void	CColourAdjuster::Subtract( u32 mask, c32 colour )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( (GetMask() & mask) == 0, "These bits have already been set" );
	#endif
	mSubtractMask |= mask;

	u32		current {mSubtractColour.GetColour()};
	u32		col {colour.GetColour() };

	mSubtractColour = c32( ( current & ~mask) | (col & mask) );
}
