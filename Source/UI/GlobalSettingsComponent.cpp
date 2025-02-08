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


#include "Base/Types.h"


#include <stdio.h>

#include "Graphics/ColourValue.h"

// #include "AdjustDeadzoneScreen.h"
#include "GlobalSettingsComponent.h"
#include "DrawTextUtilities.h"
#include "Dialogs.h"
#include "UIContext.h"
#include "UIScreen.h"
#include "UISetting.h"
#include "Menu.h"


#include "System/Thread.h"
#include "Utility/FramerateLimiter.h"
#include "Interface/Preferences.h"

#include "Utility/Translate.h"

#include "Input/InputManager.h"


extern u32 HAVE_DVE;
extern u32 PSP_TV_CABLE;
extern bool PSP_IS_SLIM;


namespace
{

	class CViewPortSetting : public CUISetting
	{
	public:
		CViewPortSetting( const char * name, const char * description )
			:	CUISetting( name, description )
		{
		}

		virtual	void	OnNext()				{ gGlobalPreferences.ViewportType = EViewportType( (gGlobalPreferences.ViewportType+1) % NUM_VIEWPORT_TYPES ); }
		virtual	void	OnPrevious()			{ gGlobalPreferences.ViewportType = EViewportType( (gGlobalPreferences.ViewportType + NUM_VIEWPORT_TYPES - 1) % NUM_VIEWPORT_TYPES ); }

		virtual const char *	GetSettingName() const
		{
			if ( gGlobalPreferences.TVEnable )
			{
				if ( gGlobalPreferences.TVType == TT_4_3 )
				{
					switch( gGlobalPreferences.ViewportType )
					{
					case VT_UNSCALED_4_3:	return "4:3 no overscan (640x448)";
					case VT_SCALED_4_3:		return "4:3 overscan (658x460)";
					case VT_FULLSCREEN:		return "16:9 stretch (720x460)";
					case VT_FULLSCREEN_HD:	return "16:9 (720x460)";
					}
					#ifdef DAEDALUS_DEBUG_CONSOLE
					DAEDALUS_ERROR( "Unhandled viewport type" );
					#endif
					return "?";
				}
				else
				{
					switch( gGlobalPreferences.ViewportType )
					{
					case VT_UNSCALED_4_3:	return "4:3 no overscan (528x448)";
					case VT_SCALED_4_3:		return "4:3 overscan (542x460)";
					case VT_FULLSCREEN:		return "16:9 stretch (720x460)";
					case VT_FULLSCREEN_HD:	return "16:9 (720x460)";
					}
						#ifdef DAEDALUS_DEBUG_CONSOLE
					DAEDALUS_ERROR( "Unhandled viewport type" );
					#endif
					return "?";
				}
			}
			else
			{
				switch( gGlobalPreferences.ViewportType )
				{
				case VT_UNSCALED_4_3:	return "4:3 unscaled (320x240)";
				case VT_SCALED_4_3:		return "4:3 scaled (362x272)";
				case VT_FULLSCREEN:		return "16:9 stretch (480x272)";
				case VT_FULLSCREEN_HD:	return "16:9 (480x272)";
				}
					#ifdef DAEDALUS_DEBUG_CONSOLE
				DAEDALUS_ERROR( "Unhandled viewport type" );
				#endif
				return "?";
			}
		}
	};

	class CTVTypeSetting : public CUISetting
	{
	public:
		CTVTypeSetting( const char * name, const char * description )
			:	CUISetting( name, description )
		{
		}

		virtual	void	OnNext()				{ gGlobalPreferences.TVType = ETVType( gGlobalPreferences.TVType ^ 1 ); }
		virtual	void	OnPrevious()			{ gGlobalPreferences.TVType = ETVType( gGlobalPreferences.TVType ^ 1 ); }

		virtual const char *	GetSettingName() const
		{
			switch( gGlobalPreferences.TVType )
			{
			case TT_4_3:			return "4:3 SDTV";
			case TT_WIDESCREEN:		return "16:9 HDTV";
			}
				#ifdef DAEDALUS_DEBUG_CONSOLE
			DAEDALUS_ERROR( "Unhandled TV type" );
			#endif
			return "?";
		}
	};

	class CLanguage : public CUISetting
	{
	public:
		CLanguage(  const char * name, const char * description )
			:	CUISetting( name, description )
		{
		}

		virtual	void		OnNext()		{ if (gGlobalPreferences.Language < Translate_Number() ) gGlobalPreferences.Language++; }
		virtual	void		OnPrevious()	{ if (gGlobalPreferences.Language > 0) gGlobalPreferences.Language--; }

		virtual	void			OnSelected()
		{
			Translate_Read( gGlobalPreferences.Language, "Languages/");
		}
		virtual const char *	GetSettingName() const
		{
			return Translate_Name( gGlobalPreferences.Language );
		}
	};


	class CResetSetting : public CUISetting
	{
	public:
		CResetSetting(  CUIContext * p_context, const char * name, const char * description )
			:	CUISetting( name, description )
			,	mpContext( p_context )
		{
		}

		virtual	void			OnSelected()
		{
			if(gShowDialog->Render( mpContext,"Reset HLE cache?", false) )
			{
		
				if(gShowDialog->Render( mpContext,"Reset settings?", false) )
				{
					std::filesystem::remove("preferences.ini");
					std::filesystem::remove("rom.db");
					ThreadSleepMs(1000);	//safety wait for s

					gShowDialog->Render( mpContext,"Daedalus will exit now",true);

					exit(0);
				}
			}
			if(gShowDialog->Render( mpContext,"Reset settings?", false) )
			{
				std::filesystem::remove("preferences.ini");
				std::filesystem::remove("rom.db");

				ThreadSleepMs(1000);	//safety wait for s

				gShowDialog->Render( mpContext,"Daedalus will exit now",true);

				exit(0);
			}
		}

		virtual const char *	GetSettingName() const
		{
			return "Press X to Start";
		}

	private:
		CUIContext *			mpContext;
	};

	class CColorSetting : public CUISetting
	{
	public:
		CColorSetting(  const char * name, const char * description )
			:	CUISetting( name, description )
		{
		}

		virtual	void		OnNext()		{ gGlobalPreferences.GuiColor = EGuiColor( (gGlobalPreferences.GuiColor+1) % NUM_COLOR_TYPES ); }
		virtual	void		OnPrevious()	{ gGlobalPreferences.GuiColor = EGuiColor( (gGlobalPreferences.GuiColor + NUM_COLOR_TYPES - 1) % NUM_COLOR_TYPES ); }

		virtual const char *	GetSettingName() const
		{
			switch ( gGlobalPreferences.GuiColor )
			{
				case BLACK:		return "Black";
				case RED:		return "Red";
				case GREEN:		return "Green";
				case MAGENTA:	return "Magenta";
				case BLUE:		return "Blue";
				case TURQUOISE:	return "Turquoise";
				case ORANGE:	return "Orange";
				case PURPLE:	return "Purple";
				case GREY:		return "Grey";
			}
				#ifdef DAEDALUS_DEBUG_CONSOLE
			DAEDALUS_ERROR( "Unknown Color" );
			#endif
			return "?";
		}
	};

	class CInfoSetting : public CUISetting
	{
	public:
		CInfoSetting(  const char * name, const char * description )
			:	CUISetting( name, description )
		{
		}

		virtual	void		OnNext()		{ gGlobalPreferences.DisplayFramerate = !gGlobalPreferences.DisplayFramerate; }
		virtual	void		OnPrevious()	{ gGlobalPreferences.DisplayFramerate = !gGlobalPreferences.DisplayFramerate; }

		virtual const char *	GetSettingName() const
		{
			if ( gGlobalPreferences.DisplayFramerate )
				return "FS + VB + SYNC";
			else
				return "None";
		}
	};

}

class IGlobalSettingsComponent : public CGlobalSettingsComponent
{
	public:

		IGlobalSettingsComponent( CUIContext * p_context );
		~IGlobalSettingsComponent();

		// CUIComponent
		virtual void				Update( float elapsed_time, const v2 & stick, u32 old_buttons, u32 new_buttons );
		virtual void				Render();

	private:
		CUIElementBag				mElements;
};


CGlobalSettingsComponent::CGlobalSettingsComponent( CUIContext * p_context )
:	CUIComponent( p_context )
{}


CGlobalSettingsComponent::~CGlobalSettingsComponent() {}


CGlobalSettingsComponent *	CGlobalSettingsComponent::Create( CUIContext * p_context )
{
	return new IGlobalSettingsComponent( p_context );
}

IGlobalSettingsComponent::IGlobalSettingsComponent( CUIContext * p_context )
:	CGlobalSettingsComponent( p_context )
{

	mElements.Add(std::make_unique<CInfoSetting>( "Display Info", "Whether to show additional info while the rom is running. Some modes are only available in DEBUG mode") );
	mElements.Add(std::make_unique<CViewPortSetting>( "Viewport Size", "The size of the viewport on the PSP." ) );

#if DAEDALUS_PSP
	if (HAVE_DVE && PSP_TV_CABLE > 0)
	{
		mElements.Add(std::make_unique<CBoolSetting>( &gGlobalPreferences.TVEnable, "TV Output", "Whether to direct the video to the TV out.", "Yes", "No" ) );
		mElements.Add(std::make_unique<CBoolSetting>( &gGlobalPreferences.TVLaced, "TV Interlaced", "Whether the TV needs interlaced output.", "Yes", "No" ) );
		mElements.Add(std::make_unique<CTVTypeSetting>( "TV Type", "The aspect ratio of the TV." ) );
	}
	else
	{
		gGlobalPreferences.TVEnable = false;
	}
#endif
	mElements.Add(std::make_unique<CBoolSetting>( &gGlobalPreferences.ForceLinearFilter,"Force Linear Filter", "Enable to force linear filter, this can improve the look of textures", "Yes", "No" ) );
	mElements.Add(std::make_unique<CBoolSetting>( &gGlobalPreferences.RumblePak,"Controller add-on", "Enable either MemPak or RumblePak.", "RumblePak", "MemPak" ) );
	// mElements.Add(std::make_unique<CAdjustDeadzoneSetting>( mpContext, "Stick Deadzone", "Adjust the size of the deadzone applied to the PSP stick while playing. Press Start/X to edit." ) );

#if DAEDALUS_PSP
	if (PSP_IS_SLIM) 
		mElements.Add(std::make_unique<CBoolSetting>( &gGlobalPreferences.LargeROMBuffer, "ROM Buffering Mode", "File Cache, faster ROM boot but can stutter due to MS reads. ROM Buffer, no stutter but long boot time loading whole ROM into memory. Takes effect only @ ROM boot.", "File Cache", "ROM Buffer" ) );
#endif

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	mElements.Add(std::make_unique<CBoolSetting>( &gGlobalPreferences.HighlightInexactBlendModes, "Highlight Inexact Blend Modes",	"Replace inexact blend modes with a placeholder texture.", "Yes", "No" ) );
	mElements.Add(std::make_unique<CBoolSetting>( &gGlobalPreferences.CustomBlendModes, "Use Custom Blend Modes",	"Debugging tool to disable custom blendmodes.", "Yes", "No" ) );
#endif
	mElements.Add(std::make_unique<CBoolSetting>( &gGlobalPreferences.BatteryWarning, "Low Battery Warning",	"Whether to allow Daedalus to notify when the battery is low.", "Yes", "No" ) );
	mElements.Add(std::make_unique<CColorSetting>( "GUI Color", "Change GUI Color" ) );
	mElements.Add(std::make_unique<CLanguage>( "Language", "Press X to load language" ) );
	mElements.Add(std::make_unique<CResetSetting>( mpContext, "Reset Settings", "Will guide you to reset preferences to default, and hle cache files. Note : emulator will exit if resetting settings" ) );

}


IGlobalSettingsComponent::~IGlobalSettingsComponent() {}


void	IGlobalSettingsComponent::Update( float elapsed_time [[maybe_unused]], const v2 & stick [[maybe_unused]], u32 old_buttons, u32 new_buttons )
{
	if(old_buttons != new_buttons)
	{
		if( new_buttons & PSP_CTRL_UP )
		{
			mElements.SelectPrevious();
		}
		if( new_buttons & PSP_CTRL_DOWN )
		{
			mElements.SelectNext();
		}


		auto element =   mElements.GetSelectedElement();
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
				element->OnSelected();
			}
		}
	}
}


void	IGlobalSettingsComponent::Render()
{
	mElements.Draw( mpContext, LIST_TEXT_LEFT, LIST_TEXT_WIDTH, AT_CENTRE, BELOW_MENU_MIN );

	auto	element =  mElements.GetSelectedElement();
	if( element != NULL )
	{
		const auto	p_description =  element->GetDescription();
		mpContext->DrawTextArea( DESCRIPTION_AREA_LEFT,
								 DESCRIPTION_AREA_TOP,
								 DESCRIPTION_AREA_RIGHT - DESCRIPTION_AREA_LEFT,
								 DESCRIPTION_AREA_BOTTOM - DESCRIPTION_AREA_TOP,
								 p_description,
								 DrawTextUtilities::TextWhite,
								 VA_BOTTOM );
	}

}
