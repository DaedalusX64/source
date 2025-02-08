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


#include "Base/Types.h"


#include <filesystem>
#if defined(GCC_LEGACY)
#include <fmt/core.h> 
#else 
#include <format>
#endif
#include <chrono>
#include <iostream>
#include <string_view>

#include "Core/ROM.h"
#include "Interface/SaveState.h"
#include "Graphics/NativeTexture.h"
#include "Math/Vector2.h"
#include "DrawTextUtilities.h"
#include "UICommand.h"
#include "UIContext.h"
#include "UIElement.h"
#include "UIScreen.h"
#include "Menu.h"
#include "SavestateSelectorComponent.h"
#include "Utility/Paths.h"
#include "Utility/Stream.h"
#include "Utility/Translate.h"


class ISavestateSelectorComponent : public CSavestateSelectorComponent
{
	public:

		ISavestateSelectorComponent( CUIContext * p_context, EAccessType accetype, std::function<void (const char *)> on_slot_selected, const std::filesystem::path& running_rom );
		~ISavestateSelectorComponent();

		// CUIScreen
		virtual void				Update( float elapsed_time, const v2 & stick, u32 old_buttons, u32 new_buttons );
		virtual void				Render();
		virtual bool				IsFinished() const									{ return mIsFinished; }
	public:
		std::filesystem::path			current_slot_path;
		bool					isGameRunning;
	
	private:
		void				OnSlotSelected( u32 slot_idx );
		void				OnFolderSelected( u32 index );
		void				LoadFolders();
		void				LoadSlots();
		void				deleteSlot(u32 id_ss);
		bool					isDeletionAllowed;

	private:
		EAccessType				mAccessType;
		std::function<void(const char *)> 	mOnSlotSelected;

		u32					mSelectedSlot;
		bool					mIsFinished;
		bool	deleteButtonTriggered;

		CUIElementBag				mElements;
		std::vector<std::string> 		mElementTitle;
		bool					mSlotEmpty[ NUM_SAVESTATE_SLOTS ];
		std::filesystem::path		mPVFilename[ NUM_SAVESTATE_SLOTS ];
		std::filesystem::path		mPVScreenShot [ NUM_SAVESTATE_SLOTS ];
		s8						mPVExists[ NUM_SAVESTATE_SLOTS ];	//0=skip, 1=file exists, -1=show no preview
		std::shared_ptr<CNativeTexture>	mPreviewTexture;
		u32						mLastPreviewLoad;

};


CSavestateSelectorComponent::~CSavestateSelectorComponent() {}


CSavestateSelectorComponent::CSavestateSelectorComponent( CUIContext * p_context )
:	CUIComponent( p_context )
{}


CSavestateSelectorComponent *	CSavestateSelectorComponent::Create( CUIContext * p_context, EAccessType accetype, std::function<void( const char *)>  on_slot_selected, const std::filesystem::path& running_rom )
{
	return new ISavestateSelectorComponent( p_context, accetype, on_slot_selected, running_rom );
}

namespace
{
	   void MakeSaveSlotPath(std::filesystem::path& path, std::filesystem::path& png_path, u32 slot_idx, const std::filesystem::path& slot_path)
    {
		//XXXX Todo Add support for alternative directories
        std::filesystem::path save_directory = setBasePath("SaveStates");
        std::filesystem::path full_directory = save_directory / slot_path;
		std::filesystem::create_directory(full_directory);

		#if defined(GCC_LEGACY)
		std::string filename_png = fmt::format("saveslot{}.ss.png", slot_idx);
        std::string filename_ss = fmt::format("saveslot{}.ss", slot_idx);
		#else 
		std::string filename_png = std::format("saveslot{}.ss.png", slot_idx);
        std::string filename_ss = std::format("saveslot{}.ss", slot_idx);
		#endif

        path = full_directory / filename_ss;
        png_path = full_directory / filename_png;
    }
}

ISavestateSelectorComponent::ISavestateSelectorComponent( CUIContext * p_context, EAccessType accetype, std::function<void (const char *)> on_slot_selected, const std::filesystem::path& running_rom )
:	CSavestateSelectorComponent( p_context )
,	mAccessType( accetype )
,	mOnSlotSelected( on_slot_selected )
,	mSelectedSlot( INVALID_SLOT )
,	mIsFinished( false )
,	deleteButtonTriggered(false)
{

	if(!running_rom.empty()){
		isGameRunning = true;
		current_slot_path = running_rom;
		LoadSlots();
		isDeletionAllowed=true;
	} else {
		isGameRunning = false;
		LoadFolders();
		isDeletionAllowed=false;
	}
}

void ISavestateSelectorComponent::LoadFolders() {
    u32 folderIndex = 0;
    const char* const description_text = (mAccessType == AT_SAVING) ? "Select the slot in which to save" : "Select the slot from which to load";

    // Clear unused vector or perform any other necessary cleanup
    mElements.Clear();

    // Clear variables if needed
    for (u32 i = 0; i < NUM_SAVESTATE_SLOTS; ++i) {
        mPVExists[i] = 0;
        mLastPreviewLoad = ~0;
    }
		std::filesystem::path saveStateDir = setBasePath("SaveStates");
		for( const auto& entry : std::filesystem::directory_iterator(saveStateDir))
		{
			if (entry.is_directory()) 
			{
				std::string directoryName = entry.path().filename().string();
				if ((directoryName.size() > 2) && (directoryName != ".git"))
				{
					std::string str = directoryName;
					auto onSelected = [this, folderIndex]() { OnFolderSelected(folderIndex); };
					std::function<void()> functor = onSelected;
					auto element = std::make_unique<CUICommandImpl>(functor, str, description_text);
					mElements.Add(std::move(element));
					mElementTitle.push_back(directoryName);
					folderIndex++; 
				}
			}
		}
}
void ISavestateSelectorComponent::LoadSlots() {
    const char* description_text = (mAccessType == AT_SAVING) ? "Select the slot in which to save [X:save O:back]" : "Select the slot from which to load [X:load O:back []:delete]";
    char date_string[30];
    
    // Clear unused vector
    mElements.Clear();
    mLastPreviewLoad = ~0;

    for (u32 i = 0; i < NUM_SAVESTATE_SLOTS; ++i) {
       std::string str = std::string("Slot ") + std::to_string(i + 1);
		 
        // std::filesystem::path filename_ss;
        MakeSaveSlotPath(mPVFilename [i], mPVScreenShot[i], i, current_slot_path);
		// Is outputting filenames correctly
        mPVExists[i] = std::filesystem::exists(mPVFilename[i]) ? 1 : -1;

		// Don't show unused slots on loading
		if (mAccessType == AT_LOADING && mPVExists[i]  != 1)
		{
			continue;
		}
        if (mPVExists[i] == 1) {

            mSlotEmpty[i] = false;
         } else {
             str = "Empty";
             mSlotEmpty[i] = true;
        }

        // Create UI elements based on slot availability
        std::unique_ptr<CUIElement> element = nullptr;
        if (mAccessType == AT_LOADING && mSlotEmpty[i]) {
            element = std::make_unique<CUICommandDummy>(str.c_str(), description_text);
        } else {
            auto onSelected = [this, i]() { OnSlotSelected(i); };
            std::function<void()> functor = onSelected;
            element = std::make_unique<CUICommandImpl>(functor, str.c_str(), description_text);
        }

        mElements.Add(std::move(element));
    }
}



ISavestateSelectorComponent::~ISavestateSelectorComponent() {}


void	ISavestateSelectorComponent::Update( float elapsed_time [[maybe_unused]], const v2 & stick [[maybe_unused]], [[maybe_unused]] u32 old_buttons, u32 new_buttons )
{
	//	Trigger the save on the first update AFTER mSelectedSlot was set.
	//	This ensures we get at least one frame where we can display "Saving..." etc.
	if( mSelectedSlot != INVALID_SLOT && !mIsFinished )
	{
		mIsFinished = true;

		std::filesystem::path filename_ss;
		std::filesystem::path filename_png;
		MakeSaveSlotPath( filename_ss, filename_png, mSelectedSlot, current_slot_path );

		mOnSlotSelected( filename_ss.string().c_str());
	}

	if(old_buttons != new_buttons)
	{
	  if(mAccessType == AT_LOADING && deleteButtonTriggered)
		{
		  if( new_buttons & PSP_CTRL_TRIANGLE )
			{
				deleteSlot(mElements.GetSelectedIndex());
			}
		}

		if( new_buttons & PSP_CTRL_UP )
		{
			mElements.SelectPrevious();
			if(mAccessType == AT_LOADING)
			    deleteButtonTriggered=false;
		}
		if( new_buttons & PSP_CTRL_DOWN )
		{
			mElements.SelectNext();
			if(mAccessType == AT_LOADING)
			    deleteButtonTriggered=false;
		}

		auto	element = mElements.GetSelectedElement();
		if( element != NULL )
		{
			if( new_buttons & PSP_CTRL_LEFT )
			{
				element->OnPrevious();
			}
			if( new_buttons & PSP_CTRL_RIGHT )
			{
				element->OnNext();
			}
			if( new_buttons & (PSP_CTRL_CROSS|PSP_CTRL_START) )
			{
				// Commit settings
				element->OnSelected();
			}
			if( new_buttons & PSP_CTRL_SQUARE)
			{
				//delete savestate
				if(mAccessType == AT_LOADING && isDeletionAllowed)
				{
				  if ((mElements.GetSelectedElement())->IsSelectable())
				    deleteButtonTriggered=true;
				}

			}
			if( new_buttons & (PSP_CTRL_CIRCLE|PSP_CTRL_SELECT) )
			{
				// Discard settings
				deleteButtonTriggered=false;
				if(isGameRunning == false)
				{
				  LoadFolders();
				  isDeletionAllowed=false;
				}
				else
					mIsFinished = true;
			}
		}
	}
}

// Disabled for now, will be fixed soon
void	ISavestateSelectorComponent::deleteSlot(u32 slot_idx [[maybe_unused]])
{
	// To reimplement we need to define path and sub_path for local SaveStates Directory and ms0:/n64 (maybe)
	
	// if (std::filesystem::exists(path))
    // {
    //   remove(path);
    //   deleteButtonTriggered=false;
    //   LoadSlots();
    // }

	// if (std::filesystem::exists(png_path))
    // {
    //   remove(png_path);
    //   deleteButtonTriggered=false;
    //   LoadSlots();
    // }
}
void	ISavestateSelectorComponent::Render()
{
	const u32	font_height = mpContext->GetFontHeight();

	if( mSelectedSlot == INVALID_SLOT )
	{
		mElements.Draw( mpContext, LIST_TEXT_LEFT, LIST_TEXT_WIDTH, AT_LEFT, BELOW_MENU_MIN + 30 - mElements.GetSelectedIndex()*(font_height+2) );

		auto element = mElements.GetSelectedElement();
		if( element != NULL )
		{

			if( mPVExists[ mElements.GetSelectedIndex() ] == 1 )
			{
				// Render Preview Image
				v2	tl( PREVIEW_IMAGE_LEFT+2, BELOW_MENU_MIN+2 );
				v2	wh( PREVIEW_IMAGE_WIDTH-4, PREVIEW_IMAGE_HEIGHT-4 );

				if( mPreviewTexture == NULL || mElements.GetSelectedIndex() != mLastPreviewLoad )
				{
					mPreviewTexture = CNativeTexture::CreateFromPng( mPVFilename[ mElements.GetSelectedIndex() ], TexFmt_8888 );
					mLastPreviewLoad = mElements.GetSelectedIndex();
				}

				mpContext->DrawRect( PREVIEW_IMAGE_LEFT, BELOW_MENU_MIN, PREVIEW_IMAGE_WIDTH, PREVIEW_IMAGE_HEIGHT, c32::Black );
				mpContext->RenderTexture( mPreviewTexture, tl, wh, c32::White );
			}
			else if( mPVExists[ mElements.GetSelectedIndex() ] == -1 && mElements.GetSelectedIndex() < NUM_SAVESTATE_SLOTS )
			{
				mpContext->DrawRect( PREVIEW_IMAGE_LEFT, BELOW_MENU_MIN, PREVIEW_IMAGE_WIDTH, PREVIEW_IMAGE_HEIGHT, c32::Black );
				mpContext->DrawRect( PREVIEW_IMAGE_LEFT+2, BELOW_MENU_MIN+2, PREVIEW_IMAGE_WIDTH-4, PREVIEW_IMAGE_HEIGHT-4, c32::Black );
				mpContext->DrawTextAlign( PREVIEW_IMAGE_LEFT, PREVIEW_IMAGE_LEFT + PREVIEW_IMAGE_WIDTH, AT_CENTRE, BELOW_MENU_MIN+PREVIEW_IMAGE_HEIGHT/2, "No Preview Available", c32::White );
			}
			// Render Text 
			const std::string& p_description = element->GetDescription();

			mpContext->DrawTextArea( DESCRIPTION_AREA_LEFT,
									 DESCRIPTION_AREA_TOP,
									 DESCRIPTION_AREA_RIGHT - DESCRIPTION_AREA_LEFT,
									 DESCRIPTION_AREA_BOTTOM - DESCRIPTION_AREA_TOP,
									 p_description,
									 DrawTextUtilities::TextWhite,
									 VA_BOTTOM );
		}
	}
	else
	{
		const char * title_text = mAccessType == AT_SAVING ? SAVING_STATUS_TEXT : LOADING_STATUS_TEXT;

		s32 y = mpContext->GetScreenHeight() - (font_height / 2);
		mpContext->DrawTextAlign( 0, mpContext->GetScreenWidth(), AT_CENTRE, y, title_text, mpContext->GetDefaultTextColour() );
	}

	if(deleteButtonTriggered)
	  mpContext->DrawTextAlign(0,SCREEN_HEIGHT,AT_CENTRE,135,"Press Delete Button to delete this savestate",DrawTextUtilities::TextRed,DrawTextUtilities::TextWhite);
}


void	ISavestateSelectorComponent::OnSlotSelected( u32 slot_idx )
{
	if( slot_idx >= NUM_SAVESTATE_SLOTS )
		return;

	// Don't allow empty slots to be loaded
	if( mAccessType == AT_LOADING && mSlotEmpty[ slot_idx ] )
		return;

	mSelectedSlot = slot_idx;
}

void	ISavestateSelectorComponent::OnFolderSelected( u32 index )
{
	current_slot_path = mElementTitle[index].c_str();
	mElementTitle.clear();
	LoadSlots();
	isDeletionAllowed=true;
}
