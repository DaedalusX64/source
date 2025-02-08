/*
Copyright (C) 2006 StrmnNrmn

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

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#if defined(GCC_LEGACY)
#include <fmt/core.h> 
#else 
#include <format>
#endif
#include <iostream>
#include <cctype>
#include <string_view>

#include "Base/Types.h"
#include "UI/Menu.h"
#include "Core/ROM.h"
#include "RomFile/RomSettings.h"
#include "Graphics/ColourValue.h"
#include "Graphics/NativeTexture.h"
#include "Input/InputManager.h"
#include "Utility/MathUtil.h"
#include "Math/Vector2.h"
#include "DrawTextUtilities.h"
#include "Menu.h"
#include "RomSelectorComponent.h"
#include "UIContext.h"
#include "UIScreen.h"



#include "Interface/Preferences.h"
#include "RomFile/RomFile.h"

namespace
{

#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_STATIC_ASSERT( std::size( gCategoryLetters ) == NUM_CATEGORIES +1 );
#endif
	ECategory		GetCategory( char c )
	{
		if (c < 0)
			return C_UNK;

		if( std::isalpha( c ))
		{
			c = std::tolower( c );
			return ECategory( C_A + (c - 'a') );
		}
		else if(std::isdigit(c) )
		{
			return C_NUMBERS;
		}
		else
		{
			return C_UNK;
		}
	}

	char	GetCategoryLetter( ECategory category )
	{
#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( category >= 0 && category < NUM_CATEGORIES, "Invalid category" );
		#endif
		return gCategoryLetters[ category ];
	}

}

struct SRomInfo
{
	std::filesystem::path	mFilename;
	RomID			mRomID;
	u32				mRomSize;
	ECicType		mCicType;
	RomSettings		mSettings;

	SRomInfo( const std::filesystem::path& filename )
		:	mFilename( filename )
	{
		if ( ROM_GetRomDetailsByFilename( filename, &mRomID, &mRomSize, &mCicType ) )
		{
			if ( !CRomSettingsDB::Get()->GetSettings( mRomID, &mSettings ) )
			{
				// Create new entry, add
				mSettings.Reset();
				mSettings.Comment = "Unknown";

				// Get internal file name for rom from header, otherwise get filename
				if ( !ROM_GetRomName( filename, mSettings.GameName ) )
				{
					mSettings.GameName = filename.string();
				}
				mSettings.GameName = mSettings.GameName.substr(0,63);
				CRomSettingsDB::Get()->SetSettings( mRomID, mSettings );
			}
		}
		else
		{
			mSettings.GameName = "Can't get rom info";
		}

	}
};

ECategory Categorise( std::string_view name )
{
	return GetCategory(name[0]);
}

bool SortByGameName( const SRomInfo * a, const SRomInfo * b )
{
	// Sort by the category first, then on the actual string.
	auto	cat_a =  Categorise( a->mSettings.GameName.c_str() );
	auto	cat_b = Categorise( b->mSettings.GameName.c_str() );

	if( cat_a != cat_b )
	{
		return cat_a < cat_b;
	}

	return ( a->mSettings.GameName < b->mSettings.GameName );
}

//Lifting this out makes it remmember last choosen ROM
//Could probably be fixed better but C++ is giving me an attitude //Corn
u32 mCurrentSelection = 0;

class IRomSelectorComponent : public CRomSelectorComponent
{
	public:

		IRomSelectorComponent( CUIContext * p_context, std::function<void(const char*)> on_rom_selected );
		~IRomSelectorComponent();

		// CUIComponent
		virtual void				Update( float elapsed_time, const v2 & stick, u32 old_buttons, u32 new_buttons );
		virtual void				Render();

	private:
				void				UpdateROMList();
				void				RenderPreview();
				void				RenderRomList();
				void				RenderCategoryList();

				void				AddRomDirectory(const std::filesystem::path& p_roms_dir, std::vector<SRomInfo*> & roms);

				ECategory			GetCurrentCategory() const;

				void				DrawInfoText( CUIContext * p_context, s32 y, const char * field_txt, const char * value_txt );

	private:
		std::function<void(const char*)> mOnRomSelected;
		std::vector<SRomInfo*>		mRomsList;
		std::map< ECategory, u32> 	mRomCategoryMap;
		s32							mCurrentScrollOffset;
		float						mSelectionAccumulator;
		std::string					mSelectedRom;

		bool						mDisplayFilenames;
	//	bool						mDisplayInfo;

		std::shared_ptr<CNativeTexture>		mpPreviewTexture;
		u32							mPreviewIdx;
		float						mPreviewLoadedTime;		// How long the preview has been loaded (so we can fade in)
		float						mTimeSinceScroll;		

		bool						mRomDelete;
};

CRomSelectorComponent::CRomSelectorComponent( CUIContext * p_context )
:	CUIComponent( p_context )
{}

CRomSelectorComponent::~CRomSelectorComponent() {}


CRomSelectorComponent *	CRomSelectorComponent::Create( CUIContext * p_context, std::function<void(const char*)> on_rom_selected )
{
	return new IRomSelectorComponent( p_context, on_rom_selected );
}

IRomSelectorComponent::IRomSelectorComponent( CUIContext * p_context, std::function<void(const char*)> on_rom_selected )
:	CRomSelectorComponent( p_context )
,	mOnRomSelected( std::move(on_rom_selected) )
//,	mCurrentSelection( 0 )
,	mCurrentScrollOffset( 0 )
,	mSelectionAccumulator( 0 )
,	mpPreviewTexture( nullptr )
,	mPreviewIdx( static_cast<u32>(-1) )
,	mPreviewLoadedTime( 0.0f )
,	mTimeSinceScroll( 0.0f )
,	mRomDelete(false)
{

	AddRomDirectory( "Roms", mRomsList );
	std::stable_sort( mRomsList.begin(), mRomsList.end(), SortByGameName );

	// Build up a map of the first location for each initial letter
	for( u32 i = 0; i < mRomsList.size(); ++i )
	{
		std::string_view gameName = mRomsList[ i ]->mSettings.GameName;
		auto		category = Categorise( gameName );

		if( mRomCategoryMap.find( category ) == mRomCategoryMap.end() )
		{
			mRomCategoryMap[ category ] = i;
		}
	}
}

IRomSelectorComponent::~IRomSelectorComponent()
{
	for(std::vector<SRomInfo*>::iterator it = mRomsList.begin(); it != mRomsList.end(); ++it)
	{
		auto	p_rominfo = *it;

		delete p_rominfo;
	}
	mRomsList.clear();

}
void	IRomSelectorComponent::AddRomDirectory(const std::filesystem::path &p_roms_dir, std::vector<SRomInfo*> & roms)
{
	
	for (const auto& entry : std::filesystem::directory_iterator(p_roms_dir))
	{
		if (entry.is_regular_file())
		{
			const std::filesystem::path& rom_filename = entry.path().filename();
			if(std::find(valid_extensions.begin(), valid_extensions.end(), rom_filename.extension()) != valid_extensions.end())
			{
				auto p_rom_info = new SRomInfo(entry);
				roms.emplace_back( p_rom_info);
			}
		}
	}
}
//Refresh ROM list

void	IRomSelectorComponent::UpdateROMList()
{
	mRomsList.clear();

	mCurrentScrollOffset = 0;
	mSelectionAccumulator = 0;
	mpPreviewTexture = NULL;
	mPreviewIdx= u32(-1);

	AddRomDirectory( "Roms", mRomsList );
	stable_sort( mRomsList.begin(), mRomsList.end(), SortByGameName );

	// Build up a map of the first location for each initial letter
	for( u32 i = 0; i < mRomsList.size(); ++i )
	{
		const char *	p_gamename = mRomsList[ i ]->mSettings.GameName.c_str();
		auto	category =  Categorise( p_gamename );

		if( mRomCategoryMap.find( category ) == mRomCategoryMap.end() )
		{
			mRomCategoryMap[ category ] = i;
		}
	}
}

ECategory	IRomSelectorComponent::GetCurrentCategory() const
{
	if( !mRomsList.empty() )
	{
		return Categorise( mRomsList[ mCurrentSelection ]->mSettings.GameName.c_str() );
	}

	return C_NUMBERS;
}

void IRomSelectorComponent::DrawInfoText(  CUIContext * p_context, s32 y, const char * field_txt, const char * value_txt  )
{
	c32			colour(	p_context->GetDefaultTextColour() );

	p_context->DrawTextAlign( PREVIEW_IMAGE_LEFT, PREVIEW_IMAGE_LEFT + PREVIEW_IMAGE_WIDTH, AT_LEFT, y, field_txt, colour );
	p_context->DrawTextAlign( PREVIEW_IMAGE_LEFT, PREVIEW_IMAGE_LEFT + PREVIEW_IMAGE_WIDTH, AT_RIGHT, y, value_txt, colour );
}

void IRomSelectorComponent::RenderPreview()
{
	// Preview Image
	v2	tl( PREVIEW_IMAGE_LEFT, BELOW_MENU_MIN );
	v2	wh( PREVIEW_IMAGE_WIDTH, PREVIEW_IMAGE_HEIGHT );

	if( mpPreviewTexture != NULL )
	{
		c32	colour = c32::White;

		if ( mPreviewLoadedTime < PREVIEW_FADE_TIME )
		{
			colour = c32( 255, 255, 255, u8( mPreviewLoadedTime * 255.f / PREVIEW_FADE_TIME ) );
		}
		
		mpContext->DrawRect( PREVIEW_IMAGE_LEFT, BELOW_MENU_MIN, PREVIEW_IMAGE_WIDTH, PREVIEW_IMAGE_HEIGHT, c32::Black );
		mpContext->RenderTexture( mpPreviewTexture, tl, wh, colour );
	}
	else
	{
		mpContext->DrawRect( PREVIEW_IMAGE_LEFT, BELOW_MENU_MIN, PREVIEW_IMAGE_WIDTH, PREVIEW_IMAGE_HEIGHT, c32::White );
		mpContext->DrawRect( PREVIEW_IMAGE_LEFT+2, BELOW_MENU_MIN+2, PREVIEW_IMAGE_WIDTH-4, PREVIEW_IMAGE_HEIGHT-4, c32::Black );
		mpContext->DrawTextAlign( PREVIEW_IMAGE_LEFT, PREVIEW_IMAGE_LEFT + PREVIEW_IMAGE_WIDTH, AT_CENTRE, BELOW_MENU_MIN+PREVIEW_IMAGE_HEIGHT/2, "No Preview Available", c32::White );
	}

		// Description Area
	u32	line_height = mpContext->GetFontHeight() + 1;

	s32 y = BELOW_MENU_MIN + PREVIEW_IMAGE_HEIGHT + 1 + mpContext->GetFontHeight();
	
	if( mCurrentSelection < mRomsList.size() )
	{
		auto	p_rominfo =  mRomsList[ mCurrentSelection ];

		#if defined(GCC_LEGACY)
		std::string rom_size = fmt::format("{} MB", p_rominfo->mRomSize  / (1024 * 1024));
		#else 
		std::string rom_size = std::format("{} MB", p_rominfo->mRomSize  / (1024 * 1024));
		#endif

		DrawInfoText( mpContext, y, "Boot:", ROM_GetCicName( p_rominfo->mCicType ));	y += line_height;
		DrawInfoText( mpContext, y, "Country:", ROM_GetCountryNameFromID( p_rominfo->mRomID.CountryID));	y += line_height;
		DrawInfoText( mpContext, y, "Size:", rom_size.c_str());	y += line_height;

		DrawInfoText( mpContext, y, "Save:", ROM_GetSaveTypeName( p_rominfo->mSettings.SaveType ) ); y += line_height;
		DrawInfoText( mpContext, y, "EPak:", ROM_GetExpansionPakUsageName( p_rominfo->mSettings.ExpansionPakUsage ) ); y += line_height;

	}
	else
	{
		DrawInfoText( mpContext, y, "Boot:", "" );		y += line_height;
		DrawInfoText( mpContext, y, "Country:", "" );	y += line_height;
		DrawInfoText( mpContext, y, "Size:", "" );		y += line_height;

		DrawInfoText( mpContext, y, "Save:", "" );		y += line_height;
		DrawInfoText( mpContext, y, "EPak:", "" );		y += line_height;
	}
}
void IRomSelectorComponent::RenderRomList()
{
	u32	line_height = mpContext->GetFontHeight() + 2 ;

	s32 x = LIST_TEXT_LEFT;
	s32 y = LIST_TEXT_TOP + mCurrentScrollOffset + mpContext->GetFontHeight();

#ifdef DAEDALUS_PSP
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuScissor(LIST_TEXT_LEFT, BELOW_MENU_MIN, LIST_TEXT_LEFT+LIST_TEXT_WIDTH, BELOW_MENU_MIN+LIST_TEXT_HEIGHT);
#endif

	const char * const	ptr_text = "> " ;

	u32	 ptr_text_width = mpContext->GetTextWidth( ptr_text );
	for(u32 i = 0; i < mRomsList.size(); ++i)
	{
		 std::string p_gamename;
		if( mDisplayFilenames )
		{
			p_gamename = std::string(mRomsList[ i ]->mFilename.string());
		}
		else
		{
			p_gamename = std::string(mRomsList[ i ]->mSettings.GameName);
		}

		//
		// Check if this entry would be onscreen
		//
		// If y + line height is greater than or equal to BELOW_MENU MIN and y - line_height is greater than BELOW_MENU_MIN + lIST_TEXT HEIGHT
		// Probably needs to go out of this time. 
		#ifdef PSP
		if(static_cast<s32>(y+line_height) >= static_cast<s32>(LIST_TEXT_TOP) 
			&& static_cast<s32>(y-line_height) < static_cast<s32>(LIST_TEXT_TOP + LIST_TEXT_HEIGHT))
		#else
		if(static_cast<s32>(y+line_height) >= static_cast<s32>(LIST_TEXT_TOP + 100) 
			&& static_cast<s32>(y-line_height) < static_cast<s32>(LIST_TEXT_TOP + LIST_TEXT_HEIGHT))
		#endif
		{
			c32		colour;

			if(i == mCurrentSelection)
			{
				colour = mpContext->GetSelectedTextColour();
				mpContext->DrawText( x, y, ptr_text, colour );
			}
			else
			{
				colour = mpContext->GetDefaultTextColour();
			}
			mpContext->DrawText( x + ptr_text_width, y, p_gamename, colour );
		}
		y += line_height;
	}

#ifdef DAEDALUS_PSP
	// Restore scissoring
	sceGuScissor(0,0, 480,272);
#endif
}

void IRomSelectorComponent::RenderCategoryList()
{
	s32 x = CATEGORY_TEXT_LEFT;
	s32 y = CATEGORY_TEXT_TOP + mpContext->GetFontHeight();

	auto current_category = GetCurrentCategory();

	for( u32 i = 0; i < NUM_CATEGORIES; ++i )
	{
		auto category = ECategory( i );
		c32			colour;

		auto it =  mRomCategoryMap.find( category );
		if( it != mRomCategoryMap.end() )
		{
			colour = (current_category == category) ? mpContext->GetSelectedTextColour() : mpContext->GetDefaultTextColour();
		}
		else
		{
			colour = c32( 180, 180, 180 );
		}

		// char str[ 16 ];
		// snprintf( str, sizeof(str), "%c ", GetCategoryLetter( category ) );
		#if defined(GCC_LEGACY)
		std::string str = fmt::format("{} ", GetCategoryLetter( category));
		#else 
		std::string str = std::format("{} ", GetCategoryLetter( category));
		#endif
		x += mpContext->DrawText( x, y, str, colour );
	}
}

void IRomSelectorComponent::Render()
{
	static u32 count = 0;

	const char * const		message[] =
	{	"(X) -> Load",
		"([ ]) -> Settings",
		"(/\\) -> File Names",
		"(HOME) -> Quit",
		"(SELECT) -> Delete"};


	RenderPreview();
	// Draw No Roms available Text 
	if( mRomsList.empty() )
	{
		 s32 offset =  0;
		for( u32 i = 0; i < std::size( gNoRomsText ); ++i )
		{
			offset += mpContext->DrawTextArea( LIST_TEXT_LEFT, BELOW_MENU_MIN + offset, LIST_TEXT_WIDTH, LIST_TEXT_HEIGHT - offset, gNoRomsText[ i ], DrawTextUtilities::TextWhite, VA_TOP );
			offset += 4;
		}
	}
	else
	{
		RenderRomList();
	}

	RenderCategoryList();


	//Show tool tip
	c32 color;
	if(count & 0x80) color = c32( ~count<<1, 0, 0, 255);
	else color = c32( count<<1, 0, 0, 255);

	if(mRomDelete)
	{
		mpContext->DrawTextAlign(0,SCREEN_WIDTH,AT_RIGHT,CATEGORY_TEXT_TOP + mpContext->GetFontHeight(),"(X) -> Confirm", color);
	}
	else
	{
		mpContext->DrawTextAlign(0,SCREEN_WIDTH,AT_RIGHT,CATEGORY_TEXT_TOP + mpContext->GetFontHeight(),	message[(count >> 8) % std::size( message )], color);
	}

	count++;
}


void	IRomSelectorComponent::Update( float elapsed_time, const v2 & stick, u32 old_buttons, u32 new_buttons )
{
	static const float	SCROLL_RATE_PER_SECOND = 25.0f;		// 25 roms/second

	/*Apply stick deadzone preference in the RomSelector menu*/
	v2 stick_dead = ApplyDeadzone( stick, gGlobalPreferences.StickMinDeadzone, gGlobalPreferences.StickMaxDeadzone );

	mSelectionAccumulator += stick_dead.y * SCROLL_RATE_PER_SECOND * elapsed_time;

	 //  Tricky thing to get the stick to work in every cases
	 // for the 100/100 case for example
	 //  without it, the accumulator gets weirdly set to a NaN value and
	 // everything is blocked... So it keeps the accumulator out of a NaN value.

	if( !(mSelectionAccumulator<0) && !(mSelectionAccumulator>0))
	  mSelectionAccumulator=0.0f;

	auto current_category =  GetCurrentCategory();

	u32	initial_selection = mCurrentSelection;

	mDisplayFilenames = (new_buttons & PSP_CTRL_TRIANGLE) != 0;

	if(old_buttons != new_buttons)
	{
		if(new_buttons & PSP_CTRL_LEFT)
		{
			// Search for the next valid predecessor
			while(current_category > 0)
			{
				current_category = ECategory( current_category - 1 );

				auto it = mRomCategoryMap.find( current_category );
				
				if( it != mRomCategoryMap.end() )
				{
					mCurrentSelection = it->second;
					break;
				}
			}
		}

		if(new_buttons & PSP_CTRL_RIGHT)
		{
			// Search for the next valid predecessor
			while(current_category < NUM_CATEGORIES-1)
			{
				current_category = ECategory( current_category + 1 );
				auto it =  mRomCategoryMap.find( current_category );
				if( it != mRomCategoryMap.end() )
				{
					mCurrentSelection = it->second;
					break;
				}
			}
		}

		if(new_buttons & PSP_CTRL_UP)
		{
			if(mCurrentSelection > 0)
			{
				mCurrentSelection--;
			}
		}

		if(new_buttons & PSP_CTRL_DOWN)
		{
			if(mCurrentSelection < mRomsList.size() - 1)
			{
				mCurrentSelection++;
			}
		}
		if(new_buttons & PSP_CTRL_SELECT)
		{
			exit(0);
		}
		if(new_buttons & PSP_CTRL_CROSS && mRomDelete)	// DONT CHANGE ORDER
		{
			remove( mSelectedRom.c_str() );
			mRomDelete = false;
			UpdateROMList();
		}
		else if((new_buttons & PSP_CTRL_START) ||
			(new_buttons & PSP_CTRL_CROSS))
		{
			if(mCurrentSelection < mRomsList.size())
			{
				mSelectedRom = mRomsList[ mCurrentSelection ]->mFilename.string();

				if(mOnRomSelected != NULL)
				{
					mOnRomSelected( mSelectedRom.data() );
				}
			}
		}

		if(new_buttons != 0) mRomDelete = false; // DONT CHANGE ORDER clear it if any button has been pressed
		if(new_buttons & PSP_CTRL_SELECT)
		{
			if(mCurrentSelection < mRomsList.size())
			{
				mSelectedRom = mRomsList[ mCurrentSelection ]->mFilename.string();
				mRomDelete = true;
			}
		}
	}
	
	//	Apply the selection accumulator
	f32		current_vel =  mSelectionAccumulator;
	while(mSelectionAccumulator >= 1.0f)
	{
		if(mCurrentSelection < mRomsList.size() - 1)
		{
			mCurrentSelection++;
		}
		mSelectionAccumulator -= 1.0f;
		mRomDelete = false;
	}
	while(mSelectionAccumulator <= -1.0f)
	{
		if(mCurrentSelection > 0)
		{
			mCurrentSelection--;
		}
		mSelectionAccumulator += 1.0f;
		mRomDelete = false;
	}

	//	Scroll to keep things in view
	//	We add on 'current_vel * 2' to keep the selection highlight as close to the
	//	center as possible (as if we're predicting 2 frames ahead)

	const u32	line_height = mpContext->GetFontHeight() + 2;

	if( mRomsList.size() * line_height > LIST_TEXT_HEIGHT )
	{
		s32		current_selection_y = static_cast<s32>((mCurrentSelection + current_vel * 2) * line_height) + (line_height/2) + mCurrentScrollOffset;

		s32		adjust_amount = (LIST_TEXT_HEIGHT/2) - current_selection_y;

		float d = 1.0f - powf(0.993f, elapsed_time * 1000.0f);

		u32		total_height = mRomsList.size() * line_height;
		s32		min_offset = LIST_TEXT_HEIGHT - total_height;

		s32	new_scroll_offset = mCurrentScrollOffset + static_cast<s32>(static_cast<float>(adjust_amount) * d);

		mCurrentScrollOffset = std::clamp( new_scroll_offset, min_offset, static_cast<s32>(0) );
	}
	else
	{
		mCurrentScrollOffset = 0;
	}

	//	Increase a timer is the current selection is still the same (i.e. if we've not scrolled)
	if( initial_selection == mCurrentSelection )
	{
		mTimeSinceScroll += elapsed_time;
	}
	else
	{
		mTimeSinceScroll = 0;
	}

	//	If the current selection is different from the preview, invalidate the picture.
	if( mCurrentSelection < mRomsList.size() && mPreviewIdx != mCurrentSelection )
	{
		//mPreviewIdx = u32(-1);

		mPreviewLoadedTime -= elapsed_time;
		if(mPreviewLoadedTime < 0.0f)
			mPreviewLoadedTime = 0.0f;

		//	If we've waited long enough since starting to scroll, try and load the preview image
		//	Note that it may fail, so we sort out the other flags regardless.

		if( mTimeSinceScroll > PREVIEW_SCROLL_WAIT )
		{
			mpPreviewTexture = nullptr;
			mPreviewLoadedTime = 0.0f;
			mPreviewIdx = mCurrentSelection;

			if( !mRomsList[ mCurrentSelection ]->mSettings.Preview.empty() )
			{
				const std::filesystem::path gPreviewDirectory = "Resources/Preview";

				const std::filesystem::path previewPath = mRomsList[mCurrentSelection]->mSettings.Preview;
				const std::filesystem::path preview_filename = gPreviewDirectory / previewPath;

				mpPreviewTexture = CNativeTexture::CreateFromPng( preview_filename.c_str(), TexFmt_8888 );
			}
		}
	}

	//	Once the preview has been loaded, increase a timer to fade us in.

	if( mPreviewIdx == mCurrentSelection )
	{
		mPreviewLoadedTime += elapsed_time;
		if(mPreviewLoadedTime > PREVIEW_FADE_TIME)
			mPreviewLoadedTime = PREVIEW_FADE_TIME;
	}
}
