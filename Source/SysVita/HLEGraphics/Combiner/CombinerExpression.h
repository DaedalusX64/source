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

#ifndef SYSPSP_HLEGRAPHICS_COMBINER_COMBINEREXPRESSION_H_
#define SYSPSP_HLEGRAPHICS_COMBINER_COMBINEREXPRESSION_H_

#include <vector>
#include <algorithm>

#include "Utility/DaedalusTypes.h"
#include "CombinerInput.h"

class COutputStream;


//*****************************************************************************
//
//*****************************************************************************
class CCombinerOperand
{
public:
	enum ECombinerType
	{
		CT_INPUT = 0,
		CT_BLEND,
		CT_PRODUCT,
		CT_SUM,
	};

	CCombinerOperand( ECombinerType type ) : mType( type ) {}
	virtual ~CCombinerOperand() {}

	ECombinerType					GetType() const							{ return mType; }

	virtual CCombinerOperand *		Clone() const = 0;

	virtual bool					IsInput( ECombinerInput input ) const	{ return false; }
	virtual bool					IsInput() const							{ return false; }
	virtual bool					IsBlend() const							{ return false; }
	virtual bool					IsSum() const							{ return false; }
	virtual bool					IsProduct() const						{ return false; }

	virtual int						Compare( const CCombinerOperand & other ) const = 0;
	virtual bool					IsEqual( const CCombinerOperand & rhs ) const	{ return Compare( rhs ) == 0; }

	virtual CCombinerOperand *		SimplifyAndReduce() const = 0;
	virtual COutputStream &			Stream( COutputStream & stream ) const = 0;

private:
	ECombinerType					mType;
};

//*****************************************************************************
//
//*****************************************************************************
class CCombinerInput : public CCombinerOperand
{
public:
	CCombinerInput( ECombinerInput input )
		:	CCombinerOperand( CT_INPUT )
		,	mInput( input )
	{
	}

	ECombinerInput					GetInput() const						{ return mInput; }

	virtual CCombinerOperand *		Clone() const							{ return new CCombinerInput( mInput ); }
	virtual bool					IsInput( ECombinerInput input ) const	{ return input == mInput; }
	virtual bool					IsInput() const							{ return true; }

	virtual CCombinerOperand *		SimplifyAndReduce()	const				{ return Clone(); }

	virtual bool					IsEqual( const CCombinerOperand & rhs ) const	{ return rhs.IsInput( mInput ); }


	virtual int						Compare( const CCombinerOperand & other ) const;
	virtual COutputStream &			Stream( COutputStream & stream ) const;

private:
	ECombinerInput		mInput;
};


//*****************************************************************************
//
//*****************************************************************************
class CCombinerBlend : public CCombinerOperand
{
public:
	CCombinerBlend( CCombinerOperand * a, CCombinerOperand * b, CCombinerOperand * f )
		:	CCombinerOperand( CT_BLEND )
		,	mInputA( a )
		,	mInputB( b )
		,	mInputF( f )
	{
	}

	~CCombinerBlend()
	{
		delete mInputA;
		delete mInputB;
		delete mInputF;
	}

	CCombinerOperand *				GetInputA() const						{ return mInputA; }
	CCombinerOperand *				GetInputB() const						{ return mInputB; }
	CCombinerOperand *				GetInputF() const						{ return mInputF; }

	virtual CCombinerOperand *		Clone() const							{ return new CCombinerBlend( mInputA->Clone(), mInputB->Clone(), mInputF->Clone() ); }
	virtual bool					IsBlend() const							{ return true; }

	virtual CCombinerOperand *		SimplifyAndReduce()	const				{ return Clone(); }

	virtual bool					IsEqual( const CCombinerOperand & rhs ) const	{ return Compare( rhs ) == 0; }

	virtual int						Compare( const CCombinerOperand & other ) const;
	virtual COutputStream &			Stream( COutputStream & stream ) const;

private:
	CCombinerOperand *		mInputA;
	CCombinerOperand *		mInputB;
	CCombinerOperand *		mInputF;
};


//*****************************************************************************
//
//*****************************************************************************
class CCombinerSum : public CCombinerOperand
{
public:
	CCombinerSum();
	CCombinerSum( CCombinerOperand * operand );
	CCombinerSum( const CCombinerSum & rhs );
	~CCombinerSum();

	virtual int							Compare( const CCombinerOperand & other ) const;
	void								Add( CCombinerOperand * operand );

	void								Sub( CCombinerOperand * operand );

	// Try to reduce this operand to a blend. If it fails, returns NULL
	CCombinerOperand *					ReduceToBlend() const;

	virtual CCombinerOperand *			SimplifyAndReduce() const;

	void								SortOperands()					{ std::sort( mOperands.begin(), mOperands.end(), SortCombinerOperandPtr() ); }

	u32									GetNumOperands() const			{ return mOperands.size(); }
	const CCombinerOperand *			GetOperand( u32 i ) const		{ return mOperands[ i ].Operand; }
	bool								IsTermNegated( u32 i ) const	{ return mOperands[ i ].Negate; }

	virtual bool						IsSum() const					{ return true; }
	virtual CCombinerOperand *			Clone() const					{ return new CCombinerSum( *this ); }

	virtual COutputStream &				Stream( COutputStream & stream ) const;

private:

	struct Node
	{
		Node( CCombinerOperand * operand, bool negate )
			:	Operand( operand )
			,	Negate( negate )
		{
		}

		CCombinerOperand *	Operand;
		bool				Negate;
	};
	struct SortCombinerOperandPtr
	{
		bool operator()( const Node & a, const Node & b ) const
		{
			if( a.Negate && !b.Negate )
				return false;
			else if( !a.Negate && b.Negate )
				return true;

			return a.Operand->Compare( *b.Operand ) < 0;
		}
	};



	std::vector< Node >	mOperands;
};


//*****************************************************************************
//
//*****************************************************************************
class CCombinerProduct : public CCombinerOperand
{
public:
	CCombinerProduct();
	CCombinerProduct( CCombinerOperand * operand );
	CCombinerProduct( const CCombinerProduct & rhs );
	~CCombinerProduct();

	void						Clear();

	virtual int					Compare( const CCombinerOperand & other ) const;

	void						Mul( CCombinerOperand * operand );

	virtual CCombinerOperand *	SimplifyAndReduce() const;

	void						SortOperands()				{ std::sort( mOperands.begin(), mOperands.end(), SortCombinerOperandPtr() ); }

	u32							GetNumOperands() const		{ return mOperands.size(); }
	const CCombinerOperand *	GetOperand( u32 i ) const	{ return mOperands[ i ].Operand; }

	virtual bool				IsProduct() const			{ return true; }
	virtual CCombinerOperand *	Clone() const				{ return new CCombinerProduct( *this ); }

	virtual COutputStream &		Stream( COutputStream & stream ) const;

private:

	struct Node
	{
		Node( CCombinerOperand * operand )
			:	Operand( operand )
		{
		}
		CCombinerOperand *	Operand;
	};
	struct SortCombinerOperandPtr
	{
		bool operator()( const Node & a, const Node & b ) const
		{
			return a.Operand->Compare( *b.Operand ) < 0;
		}
	};


	std::vector< Node >	mOperands;
};

#endif // SYSPSP_HLEGRAPHICS_COMBINER_COMBINEREXPRESSION_H_
