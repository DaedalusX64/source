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

#pragma once

#ifndef SYSPSP_HLEGRAPHICS_COMBINER_COMBINERINPUT_H_
#define SYSPSP_HLEGRAPHICS_COMBINER_COMBINERINPUT_H_

//*****************************************************************************
//
//*****************************************************************************
enum ECombinerInput
{
	CI_COMBINED,
	CI_TEXEL0,
	CI_TEXEL1,
	CI_PRIMITIVE,
	CI_SHADE,
	CI_ENV,
	CI_COMBINED_ALPHA,
	CI_TEXEL0_ALPHA,
	CI_TEXEL1_ALPHA,
	CI_PRIMITIVE_ALPHA,
	CI_SHADE_ALPHA,
	CI_ENV_ALPHA,
	CI_LOD_FRACTION,
	CI_PRIM_LOD_FRACTION,
	CI_K5,
	CI_1,
	CI_0,
	CI_UNKNOWN,
};

const char * GetCombinerInputName( ECombinerInput input );

#endif // SYSPSP_HLEGRAPHICS_COMBINER_COMBINERINPUT_H_
