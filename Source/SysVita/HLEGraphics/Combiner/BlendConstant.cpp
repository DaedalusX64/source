#include "stdafx.h"
#include "BlendConstant.h"

#include "RenderSettings.h"

#include "HLEGraphics/DaedalusVtx.h"

CBlendConstantExpression::~CBlendConstantExpression()
{
}

CBlendConstantExpressionValue::~CBlendConstantExpressionValue()
{
}

bool CBlendConstantExpressionValue::IsShade() const
{
	return mConstant == BC_SHADE;
}

c32 CBlendConstantExpressionValue::Evaluate( c32 shade, c32 primitive, c32 environment ) const
{
	switch( mConstant )
	{
	case BC_SHADE:				return shade;
	case BC_PRIMITIVE:			return primitive;
	case BC_ENVIRONMENT:		return environment;
	case BC_PRIMITIVE_ALPHA:	return c32( primitive.GetA(), primitive.GetA(), primitive.GetA(), primitive.GetA() );
	case BC_ENVIRONMENT_ALPHA:	return c32( environment.GetA(), environment.GetA(), environment.GetA(), environment.GetA() );
	case BC_1:					return c32( 0xffffffff );
	case BC_0:					return c32( 0x00000000 );
	}
#ifdef DAEDALUS_DEBUG_CONSOLE
	DAEDALUS_ERROR( "Unhandled constant" );
	#endif
	return c32( 0xffffffff );
}

c32 CBlendConstantExpressionValue::EvaluateConstant( c32 primitive, c32 environment ) const
{
	switch( mConstant )
	{
		#ifdef DAEDALUS_DEBUG_CONSOLE
	case BC_SHADE:				DAEDALUS_ERROR( "Shouldn't be here" ); return c32( 0xffffffff );
	#else
	case BC_SHADE: return c32( 0xffffffff);
	#endif
	case BC_PRIMITIVE:			return primitive;
	case BC_ENVIRONMENT:		return environment;
	case BC_PRIMITIVE_ALPHA:	return c32( primitive.GetA(), primitive.GetA(), primitive.GetA(), primitive.GetA() );
	case BC_ENVIRONMENT_ALPHA:	return c32( environment.GetA(), environment.GetA(), environment.GetA(), environment.GetA() );
	case BC_1:					return c32( 0xffffffff );
	case BC_0:					return c32( 0x00000000 );
	}
	#ifdef DAEDALUS_DEBUG_CONSOLE
	DAEDALUS_ERROR( "Unhandled constant" );
	#endif
	return c32( 0xffffffff );
}

bool CBlendConstantExpressionValue::TryEvaluateConstant( const SRenderState & state, c32 * out ) const
{
	switch( mConstant )
	{
	case BC_SHADE:				return false;
	case BC_PRIMITIVE:			*out = state.PrimitiveColour; return true;
	case BC_ENVIRONMENT:		*out = state.EnvironmentColour; return true;
	case BC_PRIMITIVE_ALPHA:	*out = c32( state.PrimitiveColour.GetA(), state.PrimitiveColour.GetA(), state.PrimitiveColour.GetA(), state.PrimitiveColour.GetA() ); return true;
	case BC_ENVIRONMENT_ALPHA:	*out = c32( state.EnvironmentColour.GetA(), state.EnvironmentColour.GetA(), state.EnvironmentColour.GetA(), state.EnvironmentColour.GetA() ); return true;
	case BC_1:					*out = c32( 0xffffffff ); return true;
	case BC_0:					*out = c32( 0x00000000 ); return true;
	}
	#ifdef DAEDALUS_DEBUG_CONSOLE
	DAEDALUS_ERROR( "Unhandled constant" );
	#endif
	return false;
}

void CBlendConstantExpressionValue::ApplyExpressionRGB( const SRenderState & state ) const
{
	// Applying the shade colour leaves the vertex untouched, so bail out
	if( mConstant != BC_SHADE )
	{
		c32		new_colour( EvaluateConstant( state.PrimitiveColour, state.EnvironmentColour ) );
		for( u32 i = 0; i < state.NumVertices; ++i )
		{
			c32 tmp = c32(state.Vertices[ i ]);
			tmp.SetBits( new_colour, c32::MASK_RGB );
			state.Vertices[ i ] = tmp.GetColour();
		}
	}
}

void CBlendConstantExpressionValue::ApplyExpressionAlpha( const SRenderState & state ) const
{
	// Applying the shade colour leaves the vertex untouched, so bail out
	if( mConstant != BC_SHADE )
	{
		c32		new_colour( EvaluateConstant( state.PrimitiveColour, state.EnvironmentColour ) );
		for( u32 i = 0; i < state.NumVertices; ++i )
		{
			c32 tmp = c32(state.Vertices[ i ]);
			tmp.SetBits( new_colour, c32::MASK_A );
			state.Vertices[ i ] = tmp.GetColour();
		}
	}
}

std::string CBlendConstantExpressionValue::ToString() const
{
	switch( mConstant )
	{
	case BC_SHADE:				return "Shade";
	case BC_PRIMITIVE:			return "Primitive";
	case BC_ENVIRONMENT:		return "Environment";
	case BC_PRIMITIVE_ALPHA:	return "PrimitiveAlpha";
	case BC_ENVIRONMENT_ALPHA:	return "EnvironmentAlpha";
	case BC_1:					return "1";
	case BC_0:					return "0";
	}
		#ifdef DAEDALUS_DEBUG_CONSOLE
	DAEDALUS_ERROR( "Unhandled constant" );
	#endif
	return "?";
}


template< typename ColOp >
CBlendConstantExpression2<ColOp>::~CBlendConstantExpression2()
{
	delete mA;
	delete mB;
}

template< typename ColOp >
c32 CBlendConstantExpression2<ColOp>::Evaluate( c32 shade, c32 primitive, c32 environment ) const
{
	c32		a( mA->Evaluate( shade, primitive, environment ) );
	c32		b( mB->Evaluate( shade, primitive, environment ) );

	return ColOp::Process( a, b );
}

template< typename ColOp >
c32 CBlendConstantExpression2<ColOp>::EvaluateConstant( c32 primitive, c32 environment ) const
{
	c32		a( mA->EvaluateConstant( primitive, environment ) );
	c32		b( mB->EvaluateConstant( primitive, environment ) );

	return ColOp::Process( a, b );
}

template< typename ColOp >
bool CBlendConstantExpression2<ColOp>::TryEvaluateConstant( const SRenderState & state, c32 * out ) const
{
	return false;
}

template< typename ColOp >
void CBlendConstantExpression2<ColOp>::ApplyExpressionRGB( const SRenderState & state ) const
{
	c32	a; bool have_a( mA->TryEvaluateConstant( state, &a ) );
	c32 b; bool have_b( mB->TryEvaluateConstant( state, &b ) );
	if( have_a && have_b )
	{
		c32	col( ColOp::Process( a, b ) );
		for( u32 i = 0; i < state.NumVertices; ++i )
		{
			c32 tmp = c32(state.Vertices[ i ]);
			tmp.SetBits( col, c32::MASK_RGB );
			state.Vertices[ i ] = tmp.GetColour();
		}
	}
	else if( have_a )
	{
		if( mB->IsShade() )
		{
			for( u32 i = 0; i < state.NumVertices; ++i )
			{
				c32 tmp = c32(state.Vertices[ i ]);
				c32	col( ColOp::Process( a, tmp ) );

				tmp.SetBits( col, c32::MASK_RGB );
				state.Vertices[ i ] = tmp.GetColour();
			}
		}
		else
		{
			for( u32 i = 0; i < state.NumVertices; ++i )
			{
				c32 tmp = c32(state.Vertices[ i ]);
				b = mB->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour );

				tmp.SetBits( ColOp::Process( a, b ), c32::MASK_RGB );
				state.Vertices[ i ] = tmp.GetColour();
			}
		}
	}
	else if( have_b )
	{
		if( mA->IsShade() )
		{
			for( u32 i = 0; i < state.NumVertices; ++i )
			{
				c32 tmp = c32(state.Vertices[ i ]);
				c32	col( ColOp::Process( tmp, b ) );

				tmp.SetBits( col, c32::MASK_RGB );
				state.Vertices[ i ] = tmp.GetColour();
			}
		}
		else
		{
			for( u32 i = 0; i < state.NumVertices; ++i )
			{
				c32 tmp = c32(state.Vertices[ i ]);
				a = mA->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour );

				tmp.SetBits( ColOp::Process( a, b ), c32::MASK_RGB );
				state.Vertices[ i ] = tmp.GetColour();
			}
		}
	}
	else
	{
		for( u32 i = 0; i < state.NumVertices; ++i )
		{
			c32 tmp = c32(state.Vertices[ i ]);
			a = mA->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour );
			b = mB->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour );

			tmp.SetBits( ColOp::Process( a, b ), c32::MASK_RGB );
			state.Vertices[ i ] = tmp.GetColour();
		}
	}
}

template< typename ColOp >
void CBlendConstantExpression2<ColOp>::ApplyExpressionAlpha( const SRenderState & state ) const
{
	c32	a; bool have_a( mA->TryEvaluateConstant( state, &a ) );
	c32 b; bool have_b( mB->TryEvaluateConstant( state, &b ) );
	if( have_a && have_b )
	{
		c32	col( ColOp::Process( a, b ) );
		for( u32 i = 0; i < state.NumVertices; ++i )
		{
			c32 tmp = c32(state.Vertices[ i ]);
			tmp.SetBits( col, c32::MASK_A );
			state.Vertices[ i ] = tmp.GetColour();
		}
	}
	else if( have_a )
	{
		if( mB->IsShade() )
		{
			for( u32 i = 0; i < state.NumVertices; ++i )
			{
				c32 tmp = c32(state.Vertices[ i ]);
				c32	col( ColOp::Process( a, tmp ) );

				tmp.SetBits( col, c32::MASK_A );
				state.Vertices[ i ] = tmp.GetColour();
			}
		}
		else
		{
			for( u32 i = 0; i < state.NumVertices; ++i )
			{
				c32 tmp = c32(state.Vertices[ i ]);
				b = mB->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour );

				tmp.SetBits( ColOp::Process( a, b ), c32::MASK_A );
				state.Vertices[ i ] = tmp.GetColour();
			}
		}
	}
	else if( have_b )
	{
		if( mA->IsShade() )
		{
			for( u32 i = 0; i < state.NumVertices; ++i )
			{
				c32 tmp = c32(state.Vertices[ i ]);
				
				c32	col( ColOp::Process( tmp, b ) );

				tmp.SetBits( col, c32::MASK_A );
				state.Vertices[ i ] = tmp.GetColour();
			}
		}
		else
		{
			for( u32 i = 0; i < state.NumVertices; ++i )
			{
				c32 tmp = c32(state.Vertices[ i ]);
				
				a = mA->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour );

				tmp.SetBits( ColOp::Process( a, b ), c32::MASK_A );
				state.Vertices[ i ] = tmp.GetColour();
			}
		}
	}
	else
	{
		for( u32 i = 0; i < state.NumVertices; ++i )
		{
			c32 tmp = c32(state.Vertices[ i ]);
			
			a = mA->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour );
			b = mB->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour );
			
			tmp.SetBits( ColOp::Process( a, b ), c32::MASK_A );
			state.Vertices[ i ] = tmp.GetColour();
		}
	}
}

template< typename ColOp >
std::string CBlendConstantExpression2<ColOp>::ToString() const
{
	std::string	str;
	str += "(" + mA->ToString() + ColOp::OpString() + mB->ToString() + ")";
	return str;
}



CBlendConstantExpressionBlend::~CBlendConstantExpressionBlend()
{
	delete mA;
	delete mB;
	delete mF;
}

c32 CBlendConstantExpressionBlend::Evaluate( c32 shade, c32 primitive, c32 environment ) const
{
	c32		a( mA->Evaluate( shade, primitive, environment ) );
	c32		b( mB->Evaluate( shade, primitive, environment ) );
	c32		f( mF->Evaluate( shade, primitive, environment ) );

	return a.Interpolate( b, f );
}

c32 CBlendConstantExpressionBlend::EvaluateConstant( c32 primitive, c32 environment ) const
{
	c32		a( mA->EvaluateConstant( primitive, environment ) );
	c32		b( mB->EvaluateConstant( primitive, environment ) );
	c32		f( mF->EvaluateConstant( primitive, environment ) );

	return a.Interpolate( b, f );
}

bool CBlendConstantExpressionBlend::TryEvaluateConstant( const SRenderState & state, c32 * out ) const
{
	return false;
}

void CBlendConstantExpressionBlend::ApplyExpressionRGB( const SRenderState & state ) const
{
	for( u32 i = 0; i < state.NumVertices; ++i )
	{
		c32 tmp = c32(state.Vertices[ i ]);
		
		c32		a( mA->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour ) );
		c32		b( mB->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour ) );
		c32		f( mF->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour ) );
		
		tmp.SetBits( a.Interpolate( b, f ), c32::MASK_RGB );
		state.Vertices[ i ] = tmp.GetColour();
	}
}

void CBlendConstantExpressionBlend::ApplyExpressionAlpha( const SRenderState & state ) const
{
	for( u32 i = 0; i < state.NumVertices; ++i )
	{
		c32 tmp = c32(state.Vertices[ i ]);
		
		c32		a( mA->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour ) );
		c32		b( mB->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour ) );
		c32		f( mF->Evaluate( tmp, state.PrimitiveColour, state.EnvironmentColour ) );

		tmp.SetBits( a.Interpolate( b, f ), c32::MASK_A );
		state.Vertices[ i ] = tmp.GetColour();
	}
}

std::string CBlendConstantExpressionBlend::ToString() const
{
	std::string	str;
	str += "blend(" + mA->ToString() + ", " + mB->ToString() + ", " + mF->ToString() + ")";
	return str;
}


// Instantiate these typedefs.

template class CBlendConstantExpression2< AddOp >;
template class CBlendConstantExpression2< SubOp >;
template class CBlendConstantExpression2< MulOp >;
