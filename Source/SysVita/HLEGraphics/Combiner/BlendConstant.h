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

#ifndef SYSPSP_HLEGRAPHICS_COMBINER_BLENDCONSTANT_H_
#define SYSPSP_HLEGRAPHICS_COMBINER_BLENDCONSTANT_H_

#include <string>

#include "Graphics/ColourValue.h"

struct SRenderState;

enum EBlendConstant
{
	BC_SHADE,
	BC_PRIMITIVE,
	BC_ENVIRONMENT,
	BC_PRIMITIVE_ALPHA,
	BC_ENVIRONMENT_ALPHA,
	BC_1,
	BC_0,
};

class CBlendConstantExpression
{
public:
	virtual ~CBlendConstantExpression();

	virtual c32					Evaluate( c32 shade, c32 primitive, c32 environment ) const = 0;
	virtual c32					EvaluateConstant( c32 primitive, c32 environment ) const = 0;
	virtual bool				TryEvaluateConstant( const SRenderState & state, c32 * out ) const = 0;

	virtual bool				IsShade() const { return false; }

	virtual void				ApplyExpressionRGB( const SRenderState & state ) const = 0;
	virtual void				ApplyExpressionAlpha( const SRenderState & state ) const = 0;

	virtual std::string			ToString() const = 0;
};

class CBlendConstantExpressionValue : public CBlendConstantExpression
{
public:
	CBlendConstantExpressionValue( EBlendConstant constant )
	:	mConstant( constant )
	{
	}

	virtual ~CBlendConstantExpressionValue();

	virtual c32					Evaluate( c32 shade, c32 primitive, c32 environment ) const;
	virtual c32 				EvaluateConstant( c32 primitive, c32 environment ) const;
	virtual bool 				TryEvaluateConstant( const SRenderState & state, c32 * out ) const;

	virtual bool 				IsShade() const;

	virtual void 				ApplyExpressionRGB( const SRenderState & state ) const;
	virtual void 				ApplyExpressionAlpha( const SRenderState & state ) const;

	virtual std::string 		ToString() const;

private:
	EBlendConstant				mConstant;
};

template< typename ColOp >
class CBlendConstantExpression2 : public CBlendConstantExpression
{
public:
	CBlendConstantExpression2( const CBlendConstantExpression * a, const CBlendConstantExpression * b )
		:	mA( a )
		,	mB( b )
	{
	}

	virtual ~CBlendConstantExpression2();

	virtual c32 				Evaluate( c32 shade, c32 primitive, c32 environment ) const;
	virtual c32					EvaluateConstant( c32 primitive, c32 environment ) const;
	virtual bool 				TryEvaluateConstant( const SRenderState & state, c32 * out ) const;

	virtual void 				ApplyExpressionRGB( const SRenderState & state ) const;
	virtual void 				ApplyExpressionAlpha( const SRenderState & state ) const;

	virtual std::string 		ToString() const;

private:
	const CBlendConstantExpression *	mA;
	const CBlendConstantExpression *	mB;
};

struct AddOp
{
	static inline c32 Process( c32 a, c32 b )	{ return a.Add( b ); }
	static inline const char * OpString()		{ return " + "; }
};
struct SubOp
{
	static inline c32 Process( c32 a, c32 b )	{ return a.Sub( b ); }
	static inline const char * OpString()		{ return " - "; }
};
struct MulOp
{
	static inline c32 Process( c32 a, c32 b )	{ return a.Modulate( b ); }
	static inline const char * OpString()		{ return " * "; }
};

typedef CBlendConstantExpression2< AddOp >	CBlendConstantExpressionAdd;
typedef CBlendConstantExpression2< SubOp >	CBlendConstantExpressionSub;
typedef CBlendConstantExpression2< MulOp >	CBlendConstantExpressionMul;



class CBlendConstantExpressionBlend : public CBlendConstantExpression
{
public:
	CBlendConstantExpressionBlend( const CBlendConstantExpression * a, const CBlendConstantExpression * b, const CBlendConstantExpression * f )
		:	mA( a )
		,	mB( b )
		,	mF( f )
	{
	}

	virtual ~CBlendConstantExpressionBlend();

	virtual c32 				Evaluate( c32 shade, c32 primitive, c32 environment ) const;
	virtual c32 				EvaluateConstant( c32 primitive, c32 environment ) const;
	virtual bool 				TryEvaluateConstant( const SRenderState & state, c32 * out ) const;

	virtual void 				ApplyExpressionRGB( const SRenderState & state ) const;
	virtual void 				ApplyExpressionAlpha( const SRenderState & state ) const;

	virtual std::string 		ToString() const;

private:
	const CBlendConstantExpression *			mA;
	const CBlendConstantExpression *			mB;
	const CBlendConstantExpression *			mF;
};

#endif // SYSPSP_HLEGRAPHICS_COMBINER_BLENDCONSTANT_H_
