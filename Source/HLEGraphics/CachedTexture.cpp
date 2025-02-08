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


#include "Base/Types.h"

#include <vector>
#include <random>
#if defined(GCC_LEGACY)
#include <fmt/core.h> 
#else 
#include <format>
#endif

#include "Interface/ConfigOptions.h"
#include "Core/ROM.h"
#include "Debug/DBGConsole.h"
#include "Debug/Dump.h"
#include "HLEGraphics/CachedTexture.h"
#include "HLEGraphics/ConvertImage.h"
#include "HLEGraphics/ConvertTile.h"
#include "HLEGraphics/TextureInfo.h"
#include "Graphics/ColourValue.h"
#include "Graphics/NativePixelFormat.h"
#include "Graphics/NativeTexture.h"
#include "Graphics/PngUtil.h"
#include "Graphics/TextureTransform.h"
#include "Math/Math.h"
#include "Utility/MathUtil.h"
#include "Ultra/ultra_gbi.h"


#include "Utility/Profiler.h"

#include <array>

static std::vector<u8>		gTexelBuffer;
// std::array<NativePf8888, 256> gPaletteBuffer;
static NativePf8888			gPaletteBuffer[ 256 ];

// NB: On the PSP we generate a lightweight hash of the texture data before
// updating the native texture. This avoids some expensive work where possible.
// On other platforms (e.g. OSX) updating textures is relatively inexpensive, so
// we just skip the hashing process entirely, and update textures every frame
// regardless of whether they've actually changed.
#if defined (DAEDALUS_PSP) || defined(DAEDALUS_CTR)
static const bool kUpdateTexturesEveryFrame = false;
#else
static const bool kUpdateTexturesEveryFrame = true;
#endif


#if defined(DAEDALUS_GL) || defined(DAEDALUS_ACCURATE_TMEM) || defined(DAEDALUS_CTR) || defined(DAEDALUS_GLES)
static ETextureFormat SelectNativeFormat(const TextureInfo & ti [[maybe_unused]])
{
	// On OSX, always use RGBA 8888 textures.
	return TexFmt_8888;
}

#else

#define DEFTEX	TexFmt_8888

// static const ETextureFormat TFmt[ 32 ] =
std::array<const ETextureFormat, 32> TFmt =
{
//	4bpp				8bpp				16bpp				32bpp
	DEFTEX,				DEFTEX,				TexFmt_5551,		TexFmt_8888,		// RGBA
	DEFTEX,				DEFTEX,				DEFTEX,				DEFTEX,				// YUV
	TexFmt_CI4_8888,	TexFmt_CI8_8888,	DEFTEX,				DEFTEX,				// CI
	TexFmt_4444,		TexFmt_4444,		TexFmt_8888,		DEFTEX,				// IA
	TexFmt_4444,		TexFmt_8888,		DEFTEX,				DEFTEX,				// I
	DEFTEX,				DEFTEX,				DEFTEX,				DEFTEX,				// ?
	DEFTEX,				DEFTEX,				DEFTEX,				DEFTEX,				// ?
	DEFTEX,				DEFTEX,				DEFTEX,				DEFTEX				// ?
};

// static const ETextureFormat TFmt_hack[ 32 ] =
std::array<const ETextureFormat, 32> TFmt_hack =
{
//	4bpp				8bpp				16bpp				32bpp
	DEFTEX,				DEFTEX,				TexFmt_4444,		TexFmt_8888,		// RGBA
	DEFTEX,				DEFTEX,				DEFTEX,				DEFTEX,				// YUV
	TexFmt_CI4_8888,	TexFmt_CI8_8888,	DEFTEX,				DEFTEX,				// CI
	TexFmt_4444,		TexFmt_4444,		TexFmt_8888,		DEFTEX,				// IA
	TexFmt_4444,		TexFmt_4444,		DEFTEX,				DEFTEX,				// I
	DEFTEX,				DEFTEX,				DEFTEX,				DEFTEX,				// ?
	DEFTEX,				DEFTEX,				DEFTEX,				DEFTEX,				// ?
	DEFTEX,				DEFTEX,				DEFTEX,				DEFTEX				// ?
};

static ETextureFormat SelectNativeFormat(const TextureInfo & ti)
{
	u32 idx = (ti.GetFormat() << 2) | ti.GetSize();
	return g_ROM.LOAD_T1_HACK ? TFmt_hack[idx] : TFmt[idx];
}
#endif

static bool GenerateTexels(void ** p_texels,
						   void ** p_palette,
						   const TextureInfo & ti,
						   ETextureFormat texture_format,
						   u32 pitch,
						   u32 buffer_size)
{
	if( gTexelBuffer.size() < buffer_size ) //|| gTexelBuffer.size() > (128 * 1024))//Cut off for downsizing may need to be adjusted to prevent some thrashing
	{
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
		printf( "Resizing texel buffer to %d bytes. Texture is %dx%d\n", buffer_size, ti.GetWidth(), ti.GetHeight() );
#endif
		gTexelBuffer.resize( buffer_size );
	}

	void *			texels  = &gTexelBuffer[0];
	NativePf8888 *	palette = IsTextureFormatPalettised( texture_format ) ? gPaletteBuffer : nullptr;

#ifdef DAEDALUS_ACCURATE_TMEM
	// NB: if line is 0, it implies this is a direct load from ram (e.g. S2DEX and Sprite2D ucodes)
	// Some games set ti.Line = 0 on LoadTile, ex SSV and Paper Mario
	if (ti.GetLine() > 0)
	{
		if (ConvertTile(ti, texels, palette, texture_format, pitch))
		{
			*p_texels  = texels;
			*p_palette = palette;
			return true;
		}
		else
		{
			return false;
		}
	}
#endif

	if (ConvertTexture(ti, texels, palette, texture_format, pitch))
	{
		*p_texels  = texels;
		*p_palette = palette;
		return true;
	}

	return false;
}

static void UpdateTexture( const TextureInfo & ti, std::shared_ptr<CNativeTexture> texture )
{
	#ifdef DAEDALUS_PROFILE
	DAEDALUS_PROFILE( "Texture Conversion" );
	#endif
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( texture != nullptr, "No texture" );
	#endif
	if ( texture != nullptr && texture->HasData() )
	{
		ETextureFormat	format = texture->GetFormat();
		u32 			stride = texture->GetStride();

		void *	texels;
		void *	palette;
		if( GenerateTexels( &texels, &palette, ti, format, stride, texture->GetBytesRequired() ) )
		{
			//
			//	Recolour the texels
			//
			if( ti.GetWhite() )
			{
				Recolour( texels, palette, ti.GetWidth(), ti.GetHeight(), stride, format, c32::White );
			}

			//
			//	Clamp edges. We do this so that non power-of-2 textures whose whose width/height
			//	is less than the mask value clamp correctly. It still doesn't fix those
			//	textures with a width which is greater than the power-of-2 size.
			//
			ClampTexels( texels, ti.GetWidth(), ti.GetHeight(), texture->GetCorrectedWidth(), texture->GetCorrectedHeight(), stride, format );

			//
			//	Mirror the texels if required (in-place)
			//
			bool mirror_s = ti.GetEmulateMirrorS();
			bool mirror_t = ti.GetEmulateMirrorT();
			if( mirror_s || mirror_t )
			{
				MirrorTexels( mirror_s, mirror_t, texels, stride, texels, stride, format, ti.GetWidth(), ti.GetHeight() );
			}

			texture->SetData( texels, palette );
		}
	}
}

CachedTexture * CachedTexture::Create( const TextureInfo & ti )
{
	if( ti.GetWidth() == 0 || ti.GetHeight() == 0 )
	{
		#ifdef DAEDALUS_DEBUG_CONSOLE
		DAEDALUS_ERROR( "Trying to create 0 width/height texture" );
		#endif
		return nullptr;
	}

	CachedTexture *	texture = new CachedTexture( ti );
	if (!texture->Initialise())
	{
		return nullptr;
	}

	return texture;
}

CachedTexture::CachedTexture( const TextureInfo & ti )
:	mTextureInfo( ti )
,	mpTexture(nullptr)
,	mTextureContentsHash( 0 )
,	mFrameLastUpToDate( gRDPFrame )
,	mFrameLastUsed( gRDPFrame )
{
}

CachedTexture::~CachedTexture()
{
}

bool CachedTexture::Initialise()
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT_Q(mpTexture == nullptr);
	#endif
	u32 width  = mTextureInfo.GetWidth();
	u32 height = mTextureInfo.GetHeight();
	std::default_random_engine FastRand;

	if (mTextureInfo.GetEmulateMirrorS()) width  *= 2;
	if (mTextureInfo.GetEmulateMirrorT()) height *= 2;

	mpTexture = CNativeTexture::Create( width, height, SelectNativeFormat(mTextureInfo) );
	if( mpTexture != nullptr )
	{
		// If this we're performing Texture updated checks, randomly offset the
		// 'FrameLastUpToDate' time. This ensures when lots of textures are
		// created on the same frame we update them over a nice distribution of frames.

		if(gCheckTextureHashFrequency > 0)
		{
			mFrameLastUpToDate = gRDPFrame + (FastRand() & (gCheckTextureHashFrequency - 1));
		}
		UpdateTextureHash();
		UpdateTexture( mTextureInfo, mpTexture );
	}

	return mpTexture != nullptr;
}

// Update the hash of the texture. Returns true if the texture should be updated.
bool CachedTexture::UpdateTextureHash()
{
	if (kUpdateTexturesEveryFrame)
	{
		// NB always assume we need updating.
		return true;
	}

	u32 new_hash_value = mTextureInfo.GenerateHashValue();
	bool changed       = new_hash_value != mTextureContentsHash;

	mTextureContentsHash = new_hash_value;
	return changed;
}

void CachedTexture::UpdateIfNecessary()
{
	if( !IsFresh() )
	{
		if (UpdateTextureHash())
		{
			UpdateTexture( mTextureInfo, mpTexture );
		}

		// FIXME(strmnrmn): should probably recreate mpWhiteTexture if it exists, else it may have stale data.

		mFrameLastUpToDate = gRDPFrame;
	}

	mFrameLastUsed = gRDPFrame;
}

// IsFresh - has this cached texture been updated recently?
bool CachedTexture::IsFresh() const
{
	if (gRDPFrame == mFrameLastUsed)
		return true;

	// If we're not updating textures every frame, check how long it's been
	// since we last updated it.
	if (!kUpdateTexturesEveryFrame)
	{
		return (gCheckTextureHashFrequency == 0 ||
				gRDPFrame < mFrameLastUpToDate + gCheckTextureHashFrequency);
	}

	return false;
}

bool CachedTexture::HasExpired() const
{
	std::default_random_engine FastRand;
	if (!kUpdateTexturesEveryFrame)
	{
		if (!IsFresh())
		{
			//Hack to make WONDER PROJECT J2 work (need to reload some textures every frame!) //Corn
			if( (g_ROM.GameHacks == WONDER_PROJECTJ2) && (mTextureInfo.GetTLutFormat() == kTT_RGBA16) && (mTextureInfo.GetSize() == G_IM_SIZ_8b) ) return true;

			//Hack for Worms Armageddon
			if( (g_ROM.GameHacks == WORMS_ARMAGEDDON) && (mTextureInfo.GetSize() == G_IM_SIZ_8b) && (mTextureContentsHash != mTextureInfo.GenerateHashValue()) ) return true;

			//Hack for Zelda OOT & MM text (only needed if there is not a general hash check) //Corn
			if( g_ROM.ZELDA_HACK && (mTextureInfo.GetSize() == G_IM_SIZ_4b) && mTextureContentsHash != mTextureInfo.GenerateHashValue() ) return true;

			//Check if texture has changed
			//if( mTextureContentsHash != mTextureInfo.GenerateHashValue() ) return true;
		}
	}

	//Otherwise we wait 20+random(0-3) frames before trashing the texture if unused
	//Spread trashing them over time so not all get killed at once (lower value uses less VRAM) //Corn
	return gRDPFrame - mFrameLastUsed > (20 + (FastRand() & 0x3));

}
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
void CachedTexture::DumpTexture( const TextureInfo & ti, const std::shared_ptr<CNativeTexture> texture  )
{
	DAEDALUS_ASSERT(texture != nullptr, "Should have a texture");

	if( texture != nullptr && texture->HasData() )
	{
		std::filesystem::path dumpdir = g_ROM.settings.GameName;
		#if defined(GCC_LEGACY)
		std::string filename = fmt::format("{}-{}_{}bpp-{}x{}-{}x{}.png", ti.GetLoadAddress(), ti.GetFormatName(), ti.GetSizeInBits(), 0, 0, ti.GetWidth(), ti.GetHeight() );
		#else 
		std::string filename = std::format("{}-{}_{}bpp-{}x{}-{}x{}.png", ti.GetLoadAddress(), ti.GetFormatName(), ti.GetSizeInBits(), 0, 0, ti.GetWidth(), ti.GetHeight() );
		#endif
		std::filesystem::path filepath;

		void *	texels;
		void *	palette;

		// Note that we re-convert the texels because those in the native texture may well already
		// be swizzle. Maybe we should just have an unswizzle routine?
		if( GenerateTexels( &texels, &palette, ti, texture->GetFormat(), texture->GetStride(), texture->GetBytesRequired() ) )
		{
			// NB - this does not include the mirrored texels

			// NB we use the palette from the native texture. This is a total hack.
			// We have to do this because the palette texels come from emulated tmem, rather
			// than ram. This means that when we dump out the texture here, tmem won't necessarily
			// contain our pixels.
			const void * native_palette = texture->GetPalette();

			PngSaveImage( filepath.c_str(), texels, native_palette, texture->GetFormat(), texture->GetStride(), ti.GetWidth(), ti.GetHeight(), true );
		}
	}
}
#endif // DAEDALUS_DEBUG_DISPLAYLIST
