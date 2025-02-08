/*
Copyright (C) 2005-2007 StrmnNrmn

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


#ifndef GRAPHICS_NATIVETEXTURE_H_
#define GRAPHICS_NATIVETEXTURE_H_

#include "TextureFormat.h"

#include "Math/Vector2.h"
#include <memory>
#include <filesystem>

#if defined(DAEDALUS_GL)
#include "SysGL/GL.h"
#endif
#if defined(DAEDALUS_GLES)
#include "SysGLES/GL.h"
#endif

#ifdef DAEDALUS_CTR
#include <GL/picaGL.h>
#endif
class c32;

class CNativeTexture 
{

		public:
		CNativeTexture( u32 w, u32 h, ETextureFormat texture_format );
		~CNativeTexture();

	public:
		static	std::shared_ptr<CNativeTexture>		Create( u32 width, u32 height, ETextureFormat texture_format );
		static	std::shared_ptr<CNativeTexture>		CreateFromPng( const std::filesystem::path p_filename, ETextureFormat texture_format );

		void							InstallTexture() const;

		void							SetData( void * data, void * palette );

		inline u32						GetBlockWidth() const			{ return mTextureBlockWidth; }
		inline u32						GetWidth() const				{ return mWidth; }
		inline u32						GetHeight() const				{ return mHeight; }
		inline u32						GetCorrectedWidth() const		{ return mCorrectedWidth; }
		inline u32						GetCorrectedHeight() const		{ return mCorrectedHeight; }
		u32								GetStride() const;
		inline ETextureFormat			GetFormat() const				{ return mTextureFormat; }

		inline const void *				GetPalette() const				{ return mpPalette; }
		inline const void *				GetData() const					{ return mpData; }
		inline void *					GetData()						{ return mpData; }

#if defined(DAEDALUS_CTR)
		inline GLuint					GetTextureId() const				{ return mTextureId; }

#endif

#if defined(DAEDALUS_PSP) || defined(DAEDALUS_CTR)
		inline f32						GetScaleX() const				{ return mScale.x; }
		inline f32						GetScaleY() const				{ return mScale.y; }
#endif

		u32								GetBytesRequired() const;
		bool							HasData() const;				// If we run out of texture memory, this will return true

	private:
		ETextureFormat		mTextureFormat;
		u32					mWidth;
		u32					mHeight;
		u32					mCorrectedWidth;
		u32					mCorrectedHeight;
		u32					mTextureBlockWidth;		// Multiple of 16 bytes

		void *				mpData;
		void *				mpPalette;

#if defined(DAEDALUS_GL) || defined(DAEDALUS_CTR) || defined(DAEDALUS_GLES)
		GLuint				mTextureId;
#endif

#if defined(DAEDALUS_PSP) || defined(DAEDALUS_CTR) || defined(DAEDALUS_GLES)
		v2					mScale;
		bool				mIsDataVidMem;
		bool				mIsPaletteVidMem;
		bool				mIsSwizzled;
#ifdef DAEDALUS_ENABLE_ASSERTS
		bool				mPaletteSet;
#endif
#endif // DAEDALUS_PSP
};

#endif // GRAPHICS_NATIVETEXTURE_H_
