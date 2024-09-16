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

#ifndef SYSPSP_HLEGRAPHICS_COMBINER_COMBINERTREE_H_
#define SYSPSP_HLEGRAPHICS_COMBINER_COMBINERTREE_H_

#include "Utility/DaedalusTypes.h"
#include "CombinerInput.h"

class CAlphaRenderSettings;
class CBlendStates;
class CCombinerOperand;
class COutputStream;

//*****************************************************************************
//
//*****************************************************************************
class CCombinerTree
{
public:
	CCombinerTree( u64 mux, bool two_cycles );
	~CCombinerTree();

	COutputStream &				Stream( COutputStream & stream ) const;

	const CBlendStates *		GetBlendStates() const				{ return mBlendStates; }

private:
	CBlendStates *				GenerateBlendStates( const CCombinerOperand * colour_operand, const CCombinerOperand * alpha_operand ) const;

	CAlphaRenderSettings *		GenerateAlphaRenderSettings(  const CCombinerOperand * operand ) const;
	void						GenerateRenderSettings( CBlendStates * states, const CCombinerOperand * operand ) const;


	static CCombinerOperand *	Simplify( CCombinerOperand * operand );


	static CCombinerOperand *	BuildCycle1( ECombinerInput a, ECombinerInput b, ECombinerInput c, ECombinerInput d );
	static CCombinerOperand *	BuildCycle2( ECombinerInput a, ECombinerInput b, ECombinerInput c, ECombinerInput d, const CCombinerOperand * cycle_1_output );

	static CCombinerOperand *	Build( CCombinerOperand * a, CCombinerOperand * b, CCombinerOperand * c, CCombinerOperand * d );

private:
	u64							mMux;
	CCombinerOperand *			mCycle1;
	CCombinerOperand *			mCycle1A;
	CCombinerOperand *			mCycle2;
	CCombinerOperand *			mCycle2A;

	CBlendStates *				mBlendStates;
};

#endif // SYSPSP_HLEGRAPHICS_COMBINER_COMBINERTREE_H_
