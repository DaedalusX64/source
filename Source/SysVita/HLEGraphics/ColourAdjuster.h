/*
Copyright (C) 2001 StrmnNrmn

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

#ifndef SYSVITA_HLEGRAPHICS_COLOURADJUSTER_H_
#define SYSVITA_HLEGRAPHICS_COLOURADJUSTER_H_

#include "Graphics/ColourValue.h"

struct DaedalusVtx;

class CColourAdjuster
{
public:

	void		SetRGBA( c32 colour )			{ Set( COL32_MASK_RGBA, colour ); }
	void		SetRGB( c32 colour )			{ Set( COL32_MASK_RGB, colour ); }
	void		SetA( c32 colour )				{ Set( COL32_MASK_A, colour ); }

	void		SetAOpaque()					{ Set( COL32_MASK_A, c32( 0,0,0, 255 ) ); }

	void		ModulateRGBA( c32 colour )		{ Modulate( COL32_MASK_RGBA, colour ); }
	void		ModulateRGB( c32 colour )		{ Modulate( COL32_MASK_RGB, colour ); }
	void		ModulateA( c32 colour )			{ Modulate( COL32_MASK_A, colour ); }

	void		SubtractRGBA( c32 colour )		{ Subtract( COL32_MASK_RGBA, colour ); }
	void		SubtractRGB( c32 colour )		{ Subtract( COL32_MASK_RGB, colour ); }
	void		SubtractA( c32 colour )			{ Subtract( COL32_MASK_A, colour ); }

	void		Reset();

	void		Process( u32 * p_vertices, u32 num_verts ) const;

private:
	void		Set( u32 mask, c32 colour );
	void		Modulate( u32 mask, c32 colour );
	void		Subtract( u32 mask, c32 colour );

	u32			GetMask() const		{ return mModulateMask | mSetMask | mSubtractMask; }

private:
	static const u32 COL32_MASK_A = 0xff000000;
	static const u32 COL32_MASK_RGB = 0x00ffffff;
	static const u32 COL32_MASK_RGBA = 0xffffffff;

private:
	u32			mModulateMask;
	c32			mModulateColour;
	u32			mSetMask;
	c32			mSetColour;
	u32			mSubtractMask;
	c32			mSubtractColour;
};

#endif // SYSVITA_HLEGRAPHICS_COLOURADJUSTER_H_
