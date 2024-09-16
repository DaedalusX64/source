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

#include "stdafx.h"
#include "RenderSettings.h"
#include "BlendConstant.h"


/**
  * Set how textures are applied
  *
  * Key for the apply-modes:
  *   - Cv - Color value result
  *   - Ct - Texture color
  *   - Cf - Fragment color
  *   - Cc - Constant color (specified by sceGuTexEnvColor())
  *
  * Available apply-modes are: (TFX)
  *   - GU_TFX_MODULATE -		Cv=Ct*Cf				TCC_RGB: Av=Af				TCC_RGBA: Av=At*Af
  *   - GU_TFX_DECAL -									TCC_RGB: Cv=Ct,Av=Af		TCC_RGBA: Cv=Cf*(1-At)+Ct*At Av=Af
  *   - GU_TFX_BLEND -			Cv=(Cf*(1-Ct))+(Cc*Ct)	TCC_RGB: Av=Af				TCC_RGBA: Av=At*Af
  *   - GU_TFX_REPLACE -		Cv=Ct					TCC_RGB: Av=Af				TCC_RGBA: Av=At
  *   - GU_TFX_ADD -			Cv=Cf+Ct				TCC_RGB: Av=Af				TCC_RGBA: Av=At*Af
  *
  * The fields TCC_RGB and TCC_RGBA specify components that differ between
  * the two different component modes.
  *
  *   - GU_TFX_MODULATE - The texture is multiplied with the current diffuse fragment
  *   - GU_TFX_REPLACE - The texture replaces the fragment
  *   - GU_TFX_ADD - The texture is added on-top of the diffuse fragment
  *
  * Available component-modes are: (TCC)
  *   - GU_TCC_RGB - The texture alpha does not have any effect
  *   - GU_TCC_RGBA - The texture alpha is taken into account
  *
  * @param tfx - Which apply-mode to use
  * @param tcc - Which component-mode to use
**/

///* Texture Effect */
//#define GU_TFX_MODULATE		(0)
//#define GU_TFX_DECAL		(1)
//#define GU_TFX_BLEND		(2)
//#define GU_TFX_REPLACE		(3)
//#define GU_TFX_ADD		(4)
//
///* Texture Color Component */
//#define GU_TCC_RGB		(0)
//#define GU_TCC_RGBA		(1)
//
//
///* Blending Op */
//#define GU_ADD			(0)
//#define GU_SUBTRACT		(1)
//#define GU_REVERSE_SUBTRACT	(2)
//#define GU_MIN			(3)
//#define GU_MAX			(4)
//#define GU_ABS			(5)


//*****************************************************************************
//
//*****************************************************************************
CAlphaRenderSettings::CAlphaRenderSettings( const char * description )
:	mDescription( description )
,	mInexact( false )
,	mUsesTexel0( false )
,	mUsesTexel1( false )
,	mConstantExpression( NULL )
{
}

//*****************************************************************************
//
//*****************************************************************************
CAlphaRenderSettings::~CAlphaRenderSettings()
{
	delete mConstantExpression;
}

//*****************************************************************************
//
//*****************************************************************************
void	CAlphaRenderSettings::AddTermTexel0()
{
	if( mUsesTexel0 || mUsesTexel1 )
	{
		mInexact = true;
	}
	mUsesTexel0 = true;
}

//*****************************************************************************
//
//*****************************************************************************
void	CAlphaRenderSettings::AddTermTexel1()
{
	if( mUsesTexel0 || mUsesTexel1 )
	{
		mInexact = true;
	}
	mUsesTexel1 = true;
}

//*****************************************************************************
//
//*****************************************************************************
void	CAlphaRenderSettings::AddTermConstant( const CBlendConstantExpression * constant_expression )
{
	if( mConstantExpression != NULL )
	{
		mConstantExpression = new CBlendConstantExpressionMul( mConstantExpression, constant_expression );
	}
	else
	{
		mConstantExpression = constant_expression;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void	CAlphaRenderSettings::Finalise()
{
	if( mConstantExpression == NULL )
	{
		mConstantExpression = new CBlendConstantExpressionValue( BC_1 );
	}
}

//*****************************************************************************
//
//*****************************************************************************
void		CAlphaRenderSettings::Apply( bool texture_installed, const SRenderState & state, SRenderStateOut & out ) const
{
	out.VertexExpressionA = mConstantExpression;

	if( mUsesTexel0 || mUsesTexel1 )
	{
		#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( texture_installed, "We have a texture, but it's not installed?" );
		#endif
		out.BlendAlphaMode = PBAM_RGBA;
	}
	else
	{
		out.BlendAlphaMode = PBAM_RGB;
	}
}
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
//*****************************************************************************
//
//*****************************************************************************
void		CAlphaRenderSettings::Print( bool texture_installed ) const
{
	DAEDALUS_ASSERT( mConstantExpression != NULL, "Should always have settings" );

	if( mUsesTexel0 || mUsesTexel1 )
	{
		// Replace vertex shade colour with modulate_colour
		printf( "    GU_TCC_RGBA, diffuse_a := %s\n", mConstantExpression->ToString().c_str() );
	}
	else
	{
		// Texel alpha ignored
		// Replace vertex shade colour with modulate_colour
		printf( "    GU_TCC_RGB, diffuse_a := %s\n", mConstantExpression->ToString().c_str() );
	}
}

#endif
//*****************************************************************************
//
//*****************************************************************************
CRenderSettingsModulate::CRenderSettingsModulate( const char * description )
	:	CRenderSettings( description )
	,	mConstantExpression( NULL )
	,	mUsesTexel0( false )
	,	mUsesTexel1( false )
	,	mInexact( false )
{
}

//*****************************************************************************
//
//*****************************************************************************
CRenderSettingsModulate::~CRenderSettingsModulate()
{
	delete mConstantExpression;
}

//*****************************************************************************
//
//*****************************************************************************
void	CRenderSettingsModulate::AddTermTexel0()
{
	if( mUsesTexel0 )
	{
		mInexact = true;
	}
	mUsesTexel0 = true;
}

//*****************************************************************************
//
//*****************************************************************************
void	CRenderSettingsModulate::AddTermTexel1()
{
	if( mUsesTexel0 || mUsesTexel1 )
	{
		mInexact = true;
	}
	mUsesTexel1 = true;
}

//*****************************************************************************
//
//*****************************************************************************
void	CRenderSettingsModulate::AddTermConstant( const CBlendConstantExpression * constant_expression )
{
	if( mConstantExpression != NULL )
	{
		mConstantExpression = new CBlendConstantExpressionMul( mConstantExpression, constant_expression );
	}
	else
	{
		mConstantExpression = constant_expression;
	}
}

//*****************************************************************************
//
//*****************************************************************************
void	CRenderSettingsModulate::Finalise()
{
}

//*****************************************************************************
//
//*****************************************************************************
void		CRenderSettingsModulate::Apply( bool texture_installed, const SRenderState & state, SRenderStateOut & out ) const
{
	out.VertexExpressionRGB = mConstantExpression;

	if( texture_installed )
	{
		//
		// Enable texturing
		//

		if( !mUsesTexel0 && !mUsesTexel1 )
		{
			// Make a white texture with the existing alpha channel
			out.MakeTextureWhite = true;
		}

		if( mConstantExpression != NULL )
		{
			out.BlendMode = PBM_MODULATE;
		}
		else
		{
			out.BlendMode = PBM_REPLACE;
		}
	}
	else
	{
		//
		// Disable texturing
		//
		#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( mConstantExpression != NULL, "No texture or diffuse" );
		#endif
	}
}
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
//*****************************************************************************
//
//*****************************************************************************
void		CRenderSettingsModulate::Print( bool texture_installed ) const
{
	if( texture_installed )
	{
		// Enable texturing
		if( !mUsesTexel0 && !mUsesTexel1 )
		{
			printf( "    Need white texture\n" );
		}

		if( mConstantExpression != NULL )
		{
			// Replace vertex shade colour with modulate_colour
			printf( "    GU_TFX_MODULATE, diffuse_rgb := %s\n", mConstantExpression->ToString().c_str() );
		}
		else
		{
			printf( "    GU_TFX_REPLACE, diffuse_rgb ignored\n" );
		}
	}
	else
	{
		// Disable texturing
		if( mConstantExpression != NULL )
		{
			// Replace vertex shade colour with modulate_colour
			printf( "    No tex, diffuse_rgb := %s\n", mConstantExpression->ToString().c_str() );
		}
		else
		{
			// What are we using
			printf( "    No texture or diffuse?\n" );
		}
	}
}

#endif
//*****************************************************************************
//
//*****************************************************************************
CRenderSettingsBlend::CRenderSettingsBlend( const char * description, const CBlendConstantExpression * a, const CBlendConstantExpression * b )
	:	CRenderSettings( description )
	,	mConstantExpressionA( a )
	,	mConstantExpressionB( b )
	,	mInexact( false )
{
}

//*****************************************************************************
//
//*****************************************************************************
CRenderSettingsBlend::~CRenderSettingsBlend()
{
	delete mConstantExpressionA;
	delete mConstantExpressionB;
}


//*****************************************************************************
//
//*****************************************************************************
void		CRenderSettingsBlend::Apply( bool texture_installed, const SRenderState & state, SRenderStateOut & out ) const
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( texture_installed, "No texture for blend?" );
#endif
	//
	//	Apply expression A as a function of shade.
	//	Evaluate expression A as a constant
	//
	out.VertexExpressionRGB = mConstantExpressionA;
	out.TextureFactor = mConstantExpressionB->EvaluateConstant( state.PrimitiveColour, state.EnvironmentColour );
	out.BlendMode = PBM_BLEND;
}
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
//*****************************************************************************
//
//*****************************************************************************
void		CRenderSettingsBlend::Print( bool texture_installed ) const
{
	std::string		mod_str_a( mConstantExpressionA->ToString() );
	std::string		mod_str_b( mConstantExpressionB->ToString() );

	// Replace vertex shade colour with modulate_colour
	printf( "    GU_TFX_BLEND, shade(a) := %s, factor(b) := %s\n", mod_str_a.c_str(), mod_str_b.c_str() );
}

#endif
//*****************************************************************************
//
//*****************************************************************************
CBlendStates::CBlendStates()
:	mAlphaSettings( NULL )
{

}

//*****************************************************************************
//
//*****************************************************************************
CBlendStates::~CBlendStates()
{
	delete mAlphaSettings;
	for( u32 i = 0; i < mColourSettings.size(); ++i )
	{
		delete mColourSettings[ i ];
	}
}

//*****************************************************************************
//
//*****************************************************************************
bool	CBlendStates::IsInexact() const
{
	if(mAlphaSettings->IsInexact())
	{
		return true;
	}

	for( u32 i = 0; i < mColourSettings.size(); ++i )
	{
		const CRenderSettings *		settings( mColourSettings[ i ] );

		if( settings->IsInexact() )
		{
			return true;
		}
	}

	return false;
}

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
//*****************************************************************************
//
//*****************************************************************************
void	CBlendStates::Print() const
{
	if(mAlphaSettings->IsInexact())
	{
		printf( "Alpha: INEXACT: %s\n", mAlphaSettings->GetDescription() );
	}
	else
	{
		printf( "Alpha: %s\n", mAlphaSettings->GetDescription() );
	}

	for( u32 i = 0; i < mColourSettings.size(); ++i )
	{
		const CRenderSettings *		settings( mColourSettings[ i ] );

		bool		install_texture0( settings->UsesTexture0() || mAlphaSettings->UsesTexture0() );
		bool		install_texture1( settings->UsesTexture1() || mAlphaSettings->UsesTexture1() );

		if( settings->IsInexact() || (install_texture0 && install_texture1) )
		{
			printf( "Stage %d: INEXACT = %s", i, settings->GetDescription() );
			if(install_texture0 && install_texture1)
			{
				printf( " Uses T0 and T1" );
			}
			printf( "\n" );
		}
		else
		{
			printf( "Stage %d: %s\n", i, settings->GetDescription() );
		}

		settings->Print( install_texture0 || install_texture1 );
		mAlphaSettings->Print( install_texture0 || install_texture1 );
	}

}
#endif
