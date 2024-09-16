/*
Copyright (C) 2007 StrmnNrmn

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

#ifndef SYSVITA_HLEGRAPHICS_BLENDMODES_H_
#define SYSVITA_HLEGRAPHICS_BLENDMODES_H_

#include "Graphics/ColourValue.h"
#include "SysVita/HLEGraphics/ColourAdjuster.h"

struct SBlendModeDetails
{
	c32					EnvColour;
	c32					PrimColour;

	bool				InstallTexture;
	CColourAdjuster		ColourAdjuster;
};

#define BLEND_MODE_ARGS		SBlendModeDetails & details

typedef void (*OverrideBlendModeFn)( BLEND_MODE_ARGS );

bool					IsInexactDefault( OverrideBlendModeFn Fn );
OverrideBlendModeFn		LookupOverrideBlendModeInexact( u64 mux );
OverrideBlendModeFn		LookupOverrideBlendModeForced( u64 mux );

#endif // SYSVITA_HLEGRAPHICS_BLENDMODES_H_
