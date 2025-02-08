/*
Copyright (C) 2013 StrmnNrmn

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
#include "Graphics/NativeTexture.h"
#include "Graphics/ColourValue.h"
#include "Graphics/NativePixelFormat.h"

#include "Utility/MathUtil.h"

#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <filesystem> 

static const u32 kPalette4BytesRequired = 16 * sizeof( NativePf8888 );
static const u32 kPalette8BytesRequired = 256 * sizeof( NativePf8888 );

static u32 GetTextureBlockWidth( u32 dimension, ETextureFormat texture_format )
{
	DAEDALUS_ASSERT( GetNextPowerOf2( dimension ) == dimension, "This is not a power of 2" );

	// Ensure that the pitch is at least 16 bytes
	while( CalcBytesRequired( dimension, texture_format ) < 16 )
	{
		dimension *= 2;
	}

	return dimension;
}

static inline u32 CorrectDimension( u32 dimension )
{
	static const u32 MIN_TEXTURE_DIMENSION = 1;
	return std::max( GetNextPowerOf2( dimension ), MIN_TEXTURE_DIMENSION );
}

std::shared_ptr<CNativeTexture>	CNativeTexture::Create( u32 width, u32 height, ETextureFormat texture_format )
{
	return std::make_shared<CNativeTexture>( width, height, texture_format );
}

CNativeTexture::CNativeTexture( u32 w, u32 h, ETextureFormat texture_format )
:	mTextureFormat( texture_format )
,	mWidth( w )
,	mHeight( h )
,	mCorrectedWidth( CorrectDimension( w ) )
,	mCorrectedHeight( CorrectDimension( h ) )
,	mTextureBlockWidth( GetTextureBlockWidth( mCorrectedWidth, texture_format ) )
,	mpData( NULL )
,	mpPalette( NULL )
,	mTextureId( 0 )
{
	glGenTextures( 1, &mTextureId );

	size_t data_len = GetBytesRequired();
	mpData = malloc(data_len);
	memset(mpData, 0, data_len);

	if (texture_format == TexFmt_CI4_8888)
	{
		mpPalette = malloc(kPalette8BytesRequired);
	}
	else if (texture_format == TexFmt_CI8_8888)
	{
		mpPalette = malloc(kPalette4BytesRequired);
	}
}

CNativeTexture::~CNativeTexture()
{
	if (mpData)
		free(mpData);
	if (mpPalette)
		free(mpPalette);

	glDeleteTextures( 1, &mTextureId );
}

bool CNativeTexture::HasData() const
{
	return mTextureId != 0;
}

void CNativeTexture::InstallTexture() const
{
	glBindTexture( GL_TEXTURE_2D, mTextureId );
}


namespace
{
	template< typename T >
	void ReadPngData( u32 width, u32 height, u32 stride, u8 ** p_row_table, int color_type, T * p_dest )
	{
		u8 r=0, g=0, b=0, a=0;

		for ( u32 y = 0; y < height; ++y )
		{
			const u8 * pRow = p_row_table[ y ];

			T * p_dest_row( p_dest );

			for ( u32 x = 0; x < width; ++x )
			{
				switch ( color_type )
				{
				case PNG_COLOR_TYPE_GRAY:
					r = g = b = *pRow++;
					if ( r == 0 && g == 0 && b == 0 )	a = 0x00;
					else								a = 0xff;
					break;
				case PNG_COLOR_TYPE_GRAY_ALPHA:
					r = g = b = *pRow++;
					if ( r == 0 && g == 0 && b == 0 )	a = 0x00;
					else								a = 0xff;
					pRow++;
					break;
				case PNG_COLOR_TYPE_RGB:
					b = *pRow++;
					g = *pRow++;
					r = *pRow++;
					if ( r == 0 && g == 0 && b == 0 )	a = 0x00;
					else								a = 0xff;
					break;
				case PNG_COLOR_TYPE_RGB_ALPHA:
					b = *pRow++;
					g = *pRow++;
					r = *pRow++;
					a = *pRow++;
					break;
				}

				p_dest_row[ x ] = T( r, g, b, a );
			}

			p_dest = reinterpret_cast< T * >( reinterpret_cast< u8 * >( p_dest ) + stride );
		}
	}

	//*****************************************************************************
	//	Thanks 71M/Shazz
	//	p_texture is either an existing texture (in case it must be of the
	//	correct dimensions and format) else a new texture is created and returned.
	//*****************************************************************************
	std::shared_ptr<CNativeTexture>	LoadPng( const std::filesystem::path p_filename, ETextureFormat texture_format )
	{
		const size_t	SIGNATURE_SIZE = 8;
		u8	signature[ SIGNATURE_SIZE ];

		FILE * fh = fopen( p_filename.string().c_str(),"rb" );
		if (fh == NULL)
		{
			return NULL;
		}

		if (fread( signature, sizeof(u8), SIGNATURE_SIZE, fh ) != SIGNATURE_SIZE)
		{
			fclose(fh);
			return NULL;
		}

		if (!png_check_sig( signature, SIGNATURE_SIZE ))
		{
			return NULL;
		}

		png_struct * p_png_struct = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
		if (p_png_struct == NULL)
		{
			return NULL;
		}

		png_info * p_png_info = png_create_info_struct( p_png_struct );
		if (p_png_info == NULL)
		{
			png_destroy_read_struct( &p_png_struct, NULL, NULL );
			return NULL;
		}

		if (setjmp( png_jmpbuf(p_png_struct) ) != 0)
		{
			png_destroy_read_struct( &p_png_struct, NULL, NULL );
			return NULL;
		}

		png_init_io( p_png_struct, fh );
		png_set_sig_bytes( p_png_struct, SIGNATURE_SIZE );
		png_read_png( p_png_struct, p_png_info, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_BGR, NULL );

		png_uint_32 width  = png_get_image_width( p_png_struct, p_png_info );
		png_uint_32 height = png_get_image_height( p_png_struct, p_png_info );

		std::shared_ptr<CNativeTexture>	texture = CNativeTexture::Create( width, height, texture_format );

		DAEDALUS_ASSERT( texture->GetWidth() >= width, "Width is unexpectedly small" );
		DAEDALUS_ASSERT( texture->GetHeight() >= height, "Height is unexpectedly small" );
		DAEDALUS_ASSERT( texture_format == texture->GetFormat(), "Texture format doesn't match" );

		u8 * buffer = new u8[ texture->GetBytesRequired() ];
		if( !buffer )
		{
			texture = NULL;
		}
		else
		{
			u32 	stride       = texture->GetStride();
			u8 ** 	row_pointers = png_get_rows( p_png_struct, p_png_info );
			int 	color_type   = png_get_color_type( p_png_struct, p_png_info );

			switch( texture_format )
			{
			case TexFmt_5650:
				ReadPngData< NativePf5650 >( width, height, stride, row_pointers, color_type, reinterpret_cast< NativePf5650 * >( buffer ) );
				break;
			case TexFmt_5551:
				ReadPngData< NativePf5551 >( width, height, stride, row_pointers, color_type, reinterpret_cast< NativePf5551 * >( buffer ) );
				break;
			case TexFmt_4444:
				ReadPngData< NativePf4444 >( width, height, stride, row_pointers, color_type, reinterpret_cast< NativePf4444 * >( buffer ) );
				break;
			case TexFmt_8888:
				ReadPngData< NativePf8888 >( width, height, stride, row_pointers, color_type, reinterpret_cast< NativePf8888 * >( buffer ) );
				break;

			case TexFmt_CI4_8888:
			case TexFmt_CI8_8888:
				DAEDALUS_ERROR( "Can't use palettised format for png." );
				break;

			default:
				DAEDALUS_ERROR( "Unhandled texture format" );
				break;
			}

			texture->SetData( buffer, NULL );
		}

		//
		// Cleanup
		//
		delete [] buffer;
		png_destroy_read_struct( &p_png_struct, &p_png_info, NULL );
		fclose(fh);

		return texture;
	}
}

std::shared_ptr<CNativeTexture>	CNativeTexture::CreateFromPng( const std::filesystem::path p_filename, ETextureFormat texture_format )
{
	return LoadPng( p_filename, texture_format );
}


void CNativeTexture::SetData( void * data, void * palette )
{
	// It's pretty gross that we don't pass this in, or better yet, provide a way for
	// the caller to write directly to our buffers instead of setting the data.
	size_t data_len = GetBytesRequired();
	memcpy(mpData, data, data_len);

	if (mTextureFormat == TexFmt_CI4_8888)
	{
		memcpy(mpPalette, palette, kPalette4BytesRequired);
	}
	else if (mTextureFormat == TexFmt_CI8_8888)
	{
		memcpy(mpPalette, palette, kPalette8BytesRequired);
	}

	if (HasData())
	{
		glBindTexture( GL_TEXTURE_2D, mTextureId );
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

		switch (mTextureFormat)
		{
		case TexFmt_5650:
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
						  mCorrectedWidth, mCorrectedHeight,
						  0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data );
			break;
		case TexFmt_5551:
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
						  mCorrectedWidth, mCorrectedHeight,
						  0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, data );
			break;
		case TexFmt_4444:
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
						  mCorrectedWidth, mCorrectedHeight,
						  0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, data );

			break;
		case TexFmt_8888:
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
						  mCorrectedWidth, mCorrectedHeight,
						  0, GL_RGBA, GL_UNSIGNED_BYTE, data );

			break;
		case TexFmt_CI4_8888:
			{
				// Convert palletised texture to non-palletised. This is wsteful - we should avoid generating these updated for OSX.
				const NativePfCI44 * pix_ptr = static_cast< const NativePfCI44 * >( data );
				const NativePf8888 * pal_ptr = static_cast< const NativePf8888 * >( palette );

				NativePf8888 * out = static_cast<NativePf8888 *>( malloc(mCorrectedWidth * mCorrectedHeight * sizeof(NativePf8888)) );
				NativePf8888 * out_ptr = out;

				u32 pitch = GetStride();

				for (u32 y = 0; y < mCorrectedHeight; ++y)
				{
					for (u32 x = 0; x < mCorrectedWidth; ++x)
					{
						NativePfCI44	colors  = pix_ptr[ x / 2 ];
						u8				pal_idx = (x&1) ? colors.GetIdxA() : colors.GetIdxB();

						*out_ptr = pal_ptr[ pal_idx ];
						out_ptr++;
					}

					pix_ptr = reinterpret_cast<const NativePfCI44 *>( reinterpret_cast<const u8 *>(pix_ptr) + pitch );
				}

				glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
							  mCorrectedWidth, mCorrectedHeight,
							  0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, out );

				free(out);
			}
			break;

		case TexFmt_CI8_8888:
			{
				// Convert palletised texture to non-palletised. This is wsteful - we should avoid generating these updated for OSX.
				const NativePfCI8 *  pix_ptr = static_cast< const NativePfCI8 * >( data );
				const NativePf8888 * pal_ptr = static_cast< const NativePf8888 * >( palette );

				NativePf8888 * out = static_cast<NativePf8888 *>( malloc(mCorrectedWidth * mCorrectedHeight * sizeof(NativePf8888)) );
				NativePf8888 * out_ptr = out;

				u32 pitch = GetStride();

				for (u32 y = 0; y < mCorrectedHeight; ++y)
				{
					for (u32 x = 0; x < mCorrectedWidth; ++x)
					{
						u8	pal_idx = pix_ptr[ x ].Bits;

						*out_ptr = pal_ptr[ pal_idx ];
						out_ptr++;
					}

					pix_ptr = reinterpret_cast<const NativePfCI8 *>( reinterpret_cast<const u8 *>(pix_ptr) + pitch );
				}

				glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
							  mCorrectedWidth, mCorrectedHeight,
							  0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, out );

				free(out);
			}
			break;

		default:
			DAEDALUS_ASSERT( !IsTextureFormatPalettised( mTextureFormat ), "Unhandled palette texture" );
			DAEDALUS_ASSERT( palette == NULL, "Palette provided when not needed" );
			break;
		}
	}
}

u32	CNativeTexture::GetStride() const
{
	return CalcBytesRequired( mTextureBlockWidth, mTextureFormat );
}

u32 CNativeTexture::GetBytesRequired() const
{
	return GetStride() * mCorrectedHeight;
}
