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

#ifndef SYSPSP_HLEGRAPHICS_COMBINER_RENDERSETTINGS_H_
#define SYSPSP_HLEGRAPHICS_COMBINER_RENDERSETTINGS_H_

#include <stdio.h>

#include <string>
#include <vector>

#include "Graphics/ColourValue.h"

class CBlendConstantExpression;

struct DaedalusVtx;

struct SRenderState
{
	u32 *					Vertices;
	u32						NumVertices;
	c32						PrimitiveColour;
	c32						EnvironmentColour;
};

enum EPspBlendMode
{
	PBM_MODULATE,
	PBM_REPLACE,
	PBM_BLEND,
};

enum EPspBlendAlphaMode
{
	PBAM_RGBA,
	PBAM_RGB,
};


struct SRenderStateOut
{
	const CBlendConstantExpression *		VertexExpressionRGB;
	const CBlendConstantExpression *		VertexExpressionA;
	EPspBlendMode							BlendMode;
	EPspBlendAlphaMode						BlendAlphaMode;
	c32										TextureFactor;		// For PBM_BLEND
	bool									MakeTextureWhite;
};

class CAlphaRenderSettings
{
public:
	CAlphaRenderSettings( const char * description );
	~CAlphaRenderSettings();

	void			AddTermTexel0();
	void			AddTermTexel1();
	void			AddTermConstant( const CBlendConstantExpression * constant_expression );

	void			Finalise();

	void			Apply( bool texture_installed, const SRenderState & state, SRenderStateOut & out ) const;
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	void			Print( bool texture_installed ) const;
#endif
	bool			UsesTexture0() const		{ return mUsesTexel0; }
	bool			UsesTexture1() const		{ return mUsesTexel1; }

	void			SetInexact()				{ mInexact = true; }
	bool			IsInexact() const			{ return mInexact; }

	const char *	GetDescription() const							{ return mDescription.c_str(); }

private:
	std::string		mDescription;
	bool			mInexact;
	bool			mUsesTexel0;
	bool			mUsesTexel1;
	const CBlendConstantExpression *	mConstantExpression;
};




class CRenderSettings
{
public:
	CRenderSettings( const char * description ) : mDescription( description ) {}
	virtual ~CRenderSettings() {}
	virtual bool			IsInexact() const = 0;
	virtual bool			UsesTexture0() const = 0;
	virtual bool			UsesTexture1() const = 0;
	virtual void			Apply( bool texture_installed, const SRenderState & state, SRenderStateOut & out ) const = 0;
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	virtual void			Print( bool texture_installed ) const = 0;
#endif
	const char *			GetDescription() const							{ return mDescription.c_str(); }

private:
	std::string				mDescription;
};


class CRenderSettingsInvalid : public CRenderSettings
{
public:
	CRenderSettingsInvalid( const char * description ) : CRenderSettings( description ) {}
	virtual bool			IsInexact() const								{ return true; }
	virtual bool			UsesTexture0() const							{ return false; }
	virtual bool			UsesTexture1() const							{ return false; }
	virtual void			Apply( bool texture_installed, const SRenderState & state, SRenderStateOut & out ) const	{ }
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	virtual void			Print( bool texture_installed ) const			{ printf( "Invalid\n" ); }
#endif
};

class CRenderSettingsModulate : public CRenderSettings
{
public:
	CRenderSettingsModulate( const char * description );
	~CRenderSettingsModulate();

	void					AddTermTexel0();
	void					AddTermTexel1();
	void					AddTermConstant( const CBlendConstantExpression * constant_expression );

	void					Finalise();

	void					SetInexact()					{ mInexact = true; }
	virtual bool			IsInexact() const				{ return mInexact; }

	virtual bool			UsesTexture0() const			{ return mUsesTexel0; }
	virtual bool			UsesTexture1() const			{ return mUsesTexel1; }

	virtual void			Apply( bool texture_installed, const SRenderState & state, SRenderStateOut & out ) const;
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	virtual void			Print( bool texture_installed ) const;
#endif

private:
	const CBlendConstantExpression *	mConstantExpression;
	bool					mUsesTexel0;
	bool					mUsesTexel1;
	bool					mInexact;
};

class CRenderSettingsBlend : public CRenderSettings
{
public:
	CRenderSettingsBlend(  const char * description, const CBlendConstantExpression * a, const CBlendConstantExpression * b );
	~CRenderSettingsBlend();

	void					SetInexact()					{ mInexact = true; }
	virtual bool			IsInexact() const				{ return mInexact; }

	virtual bool			UsesTexture0() const			{ return true; }
	virtual bool			UsesTexture1() const			{ return false; }

	virtual void			Apply( bool texture_installed, const SRenderState & state, SRenderStateOut & out ) const;
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	virtual void			Print( bool texture_installed ) const;
#endif

private:
	const CBlendConstantExpression *	mConstantExpressionA;
	const CBlendConstantExpression *	mConstantExpressionB;
	bool					mInexact;
};



class CBlendStates
{
public:
	CBlendStates();
	~CBlendStates();

	bool					IsInexact() const;
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	void					Print() const;
#endif
#ifdef DAEDALUS_ENABLE_ASSERTS
	void					SetAlphaSettings( const CAlphaRenderSettings * alpha_settings )				{ DAEDALUS_ASSERT( mAlphaSettings == NULL, "Overwriting settings" ); mAlphaSettings = alpha_settings; }
#else
void					SetAlphaSettings( const CAlphaRenderSettings * alpha_settings )				{ mAlphaSettings = alpha_settings; }
#endif
	void					AddColourSettings( const CRenderSettings * colour_settings )				{ mColourSettings.push_back( colour_settings ); }

	u32								GetNumStates() const												{ return mColourSettings.size(); }
	const CAlphaRenderSettings *	GetAlphaSettings() const											{ return mAlphaSettings; }
	#ifdef DAEDALUS_ENABLE_ASSERTS
	const CRenderSettings *			GetColourSettings( u32 i ) const									{ DAEDALUS_ASSERT( i < mColourSettings.size(), "Invalid idx" ); return mColourSettings[ i ]; }
#else
	const CRenderSettings *			GetColourSettings( u32 i ) const									{ return mColourSettings[ i ]; }
	#endif
private:
	const CAlphaRenderSettings *			mAlphaSettings;
	std::vector<const CRenderSettings *>	mColourSettings;

};

#endif // SYSPSP_HLEGRAPHICS_COMBINER_RENDERSETTINGS_H_
