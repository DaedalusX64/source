#include <3ds.h>
#include <GL/picaGL.h>
#include <stdio.h>
#if defined(GCC_LEGACY)
#include <fmt/core.h> 
#else 
#include <format>
#endif

#include "UserInterface.h"
#include "InGameMenu.h"
#include <iostream>


#include "Interface/ConfigOptions.h"
#include "Interface/Cheats.h"
#include "Core/CPU.h"
#include "Core/Memory.h"
#include "Core/PIF.h"
#include "RomFile/RomSettings.h"
#include "Core/Save.h"
#include "Debug/DBGConsole.h"
#include "Debug/DebugLog.h"
#include "Graphics/GraphicsContext.h"
#include "HLEGraphics/TextureCache.h"
#include "Input/InputManager.h"
#include "Interface/RomDB.h"
#include "System/SystemInit.h"
#include "Utility/BatchTest.h"

#include "Interface/Preferences.h"
#include "Utility/Profiler.h"
#include "System/Thread.h"
#include "RomFile/RomFile.h"
#include "Utility/Timer.h"
#include "Utility/Paths.h"

extern float gCurrentFramerate;
extern EFrameskipValue gFrameskipValue;
extern RomInfo g_ROM;

static uint8_t currentPage = 0;


static void ExecSaveState(int slot)
{
	std::filesystem::path savePath = setBasePath("SaveStates");
	std::string name = g_ROM.settings.GameName;
	savePath /= name;
	std::filesystem::create_directories(savePath);

		#if defined(GCC_LEGACY)
		std::string filename = fmt::format("saveslot{}.ss", slot);
		#else 
		std::string filename = std::format("saveslot{}.ss", slot);
		#endif
	
	savePath /= filename;

	std::cout << "Save Save Slot Path" << savePath << std::endl;
	CPU_RequestSaveState(savePath);
}

static void LoadSaveState(int slot)
{
	std::filesystem::path savePath = setBasePath("SaveStates");
	std::string name = g_ROM.settings.GameName;
	savePath /= name;
	std::filesystem::create_directories(savePath);

		#if defined(GCC_LEGACY)
		std::string filename = fmt::format("saveslot{}.ss", slot);
		#else 
		std::string filename = std::format("saveslot{}.ss", slot);
		#endif

	
	savePath /= filename;

	std::cout << "Load Save Slot Path" << savePath << std::endl;
	// snprintf(full_path),sizeof(full_path), "%s%s.ss%d", "SaveStates/", g_ROM.settings.GameName.c_str(), slot);

	CPU_RequestLoadState(savePath);
}

static bool SaveStateExists(int slot)
{
	std::filesystem::path savePath = setBasePath("SaveStates");
	std::string name = g_ROM.settings.GameName;
	savePath /= name;
	std::filesystem::create_directories(savePath);

		#if defined(GCC_LEGACY)
		std::string filename = fmt::format("saveslot{}.ss", slot);
		#else 
		std::string filename = std::format("saveslot{}.ss", slot);
		#endif

	
	savePath /= filename;

	// std::string path = fmt::format("{}/{}{}.ss", "SaveStates", g_ROM.settings.GameName.c_str(), slot);
	// full_path = path;
		std::cout << "Slot Exists Path" << savePath << std::endl;
	// snprintf(full_path, sizeof(full_path), "%s%s.ss%d", DAEDALUS_CTR_PATH("SaveStates/"), g_ROM.settings.GameName.c_str(), slot);

	// snprintf(full_path, sizeof(full_path),  "%s%s.ss%d", "SaveStates/", g_ROM.settings.GameName.c_str(), slot);

	return std::filesystem::exists(savePath);
}

static void DrawSaveStatePage()
{
	ImGui_Impl3DS_NewFrame();
	ImGui::SetNextWindowPos( ImVec2(0, 0) );
	ImGui::SetNextWindowSize( ImVec2(320, 240) );

	ImGui::Begin("Save state", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

	char buttonString[20];

	float buttonWidth = ImGui::GetContentRegionAvail().x;

	for(int i = 0; i < 5; i++)
	{
		snprintf(buttonString, sizeof(buttonString),  "Save slot: %i", i);

		if(ImGui::ColoredButton(buttonString, SaveStateExists(i) ? 0.16f : 0.40f, ImVec2(buttonWidth, 30)))
		{
			ExecSaveState(i);
		}
	}

	if(ImGui::ColoredButton("Cancel", 0, ImVec2(buttonWidth, 30))) currentPage = 0;

	ImGui::End();
	ImGui::Render();

	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

static void DrawLoadStatePage()
{
	ImGui_Impl3DS_NewFrame();
	ImGui::SetNextWindowPos( ImVec2(0, 0) );
	ImGui::SetNextWindowSize( ImVec2(320, 240) );

	ImGui::Begin("Load state", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

	char buttonString[20];

	float buttonWidth = ImGui::GetContentRegionAvail().x;
	
	for(int i = 0; i < 5; i++)
	{
		snprintf(buttonString, sizeof(buttonString),  "Load slot: %i", i);

		if( SaveStateExists(i) )
		{
			if(ImGui::ColoredButton(buttonString, SaveStateExists(i) ? 0.40f : 0.0f, ImVec2(buttonWidth, 30)))
			{
				LoadSaveState(i);
			}
		}
		else
		{
			ImGui::Button(buttonString, ImVec2(buttonWidth, 30));
		}
	}

	if(ImGui::ColoredButton("Cancel", 0, ImVec2(buttonWidth, 30))) currentPage = 0;

	ImGui::End();
	ImGui::Render();

	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

static void HelpMarker(const char* desc)
{
	ImGui::SameLine();
	ImGui::SetCursorPosX(284);
    ImGui::TextDisabled("(?)");

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static SRomPreferences romPreferences;

void UI::LoadRomPreferences(RomID mRomID)
{
	CPreferences::Get()->GetRomPreferences( mRomID, &romPreferences );
}

bool UI::DrawOptionsPage(RomID mRomID)
{
	currentPage = 3;

	int currentSelection = 0;

	ImGui_Impl3DS_NewFrame();
	ImGui::SetNextWindowPos( ImVec2(0, 0) );
	ImGui::SetNextWindowSize( ImVec2(320, 240) );

	ImGui::Begin("Options", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
	ImGui::PushItemWidth(-1);
	ImGui::Spacing();

	ImGui::BeginTabBar("Tabs", ImGuiTabBarFlags_None);

	if (ImGui::BeginTabItem("Core"))
	{
		ImGui::Text("Dynamic Recompilation");
		ImGui::Checkbox("##DynaRec", &romPreferences.DynarecEnabled);
		ImGui::SameLine();
		ImGui::Text(romPreferences.DynarecEnabled ? "Enabled" : "Disabled");

		ImGui::Spacing();

		ImGui::Text("Dynarec Loop Optimizations");
		HelpMarker("Enable loop optimizations for better performance\n Can cause freezes!!!");
		ImGui::Checkbox("##loopopt", (bool*)&romPreferences.DynarecLoopOptimisation);
		ImGui::SameLine();
		ImGui::Text(romPreferences.DynarecLoopOptimisation ? "Enabled" : "Disabled");

		ImGui::Spacing();

		ImGui::Text("Dynarec Memory Access Optimizations");
		HelpMarker("Enable memory access optimizations for better performance\n Can cause freezes!!!");
		ImGui::Checkbox("##memaccess", (bool*)&romPreferences.MemoryAccessOptimisation);
		ImGui::SameLine();
		ImGui::Text(romPreferences.MemoryAccessOptimisation ? "Enabled" : "Disabled");

		ImGui::Spacing();

		ImGui::Text("Limit Framerate");
		ImGui::Checkbox("##speedsync", (bool*)&romPreferences.SpeedSyncEnabled);
		ImGui::SameLine();
		ImGui::Text(romPreferences.SpeedSyncEnabled ? "Enabled" : "Disabled");

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Audio"))
	{
		const char* audioOptions[] = { "Disabled", "Asynchronous", "Synchronous" };
		ImGui::Text("Audio Plugin");
		currentSelection = (int)romPreferences.AudioEnabled;
		ImGui::Combo("##audio_combo", &currentSelection, audioOptions, 3);
		romPreferences.AudioEnabled = EAudioPluginMode(currentSelection);

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Video"))
	{
		ImGui::Text("Sync Video Rate");
		HelpMarker("Speeds up video logic to match framerate.");
		ImGui::Checkbox("##VideoRateMatch", &romPreferences.VideoRateMatch);
		ImGui::SameLine();
		ImGui::Text(romPreferences.VideoRateMatch ? "Enabled" : "Disabled");

		ImGui::Spacing();

		const char* hashOptions[] = { "Disabled", "Every frame", "Every 2 frames", "Every 4 frames", "Every 8 frames", "Every 16 frames", "Every 32 frames" };
		ImGui::Text("Texture Hash Check Frequency");
		HelpMarker( "Frequency in which to check for texture changes.\n"
					"Disabled is the fastest, but can cause graphical glitches.\n");
		currentSelection = (int)romPreferences.CheckTextureHashFrequency;
		ImGui::Combo("##hash_frequency", &currentSelection, hashOptions, NUM_THF);
		romPreferences.CheckTextureHashFrequency = ETextureHashFrequency(currentSelection);

		ImGui::Spacing();

		const char* frameskipOptions[] = { "Disabled", "Auto 1", "Auto 2", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
		ImGui::Text("Frameskip");
		currentSelection = (int)romPreferences.Frameskip;
		ImGui::Combo("##frameskip_combo", &currentSelection, frameskipOptions, NUM_FRAMESKIP_VALUES);
		romPreferences.Frameskip = EFrameskipValue(currentSelection);

		ImGui::Spacing();

		const char* viewporOptions[] = { "4:3", "Widescreen (Stretched)", "Widescreen (Hack)" };
		ImGui::Text("Aspect Ratio");
		currentSelection = (int)gGlobalPreferences.ViewportType;
		ImGui::Combo("##viewport_combo", &currentSelection, viewporOptions, 3);
		gGlobalPreferences.ViewportType = EViewportType(currentSelection);

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Input"))
	{
		if(ImGui::BeginCombo("Configuration", CInputManager::Get()->GetConfigurationName(romPreferences.ControllerIndex)) )
		{
			for( unsigned i = 0; i < CInputManager::Get()->GetNumConfigurations(); i++ )
			{
				const bool isSelected = (romPreferences.ControllerIndex == i);

				if( ImGui::Selectable(CInputManager::Get()->GetConfigurationName(i), isSelected) )
                    romPreferences.ControllerIndex = i;
			}

			ImGui::EndCombo();
		}

		ImGui::Text(CInputManager::Get()->GetConfigurationDescription(romPreferences.ControllerIndex) );

		ImGui::EndTabItem();
	}
	
	if (ImGui::BeginTabItem("Misc"))
	{
		ImGui::Text("Display FPS");
		ImGui::Checkbox("##DisplayFPS", &gGlobalPreferences.DisplayFramerate);
		ImGui::SameLine();
		ImGui::Text(gGlobalPreferences.DisplayFramerate ? "Enabled" : "Disabled");

		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();
	ImGui::Spacing();
	ImGui::Separator();

	int buttonWidth = (ImGui::GetContentRegionAvail().x - 6) / 2;

	if( ImGui::Button("Cancel", ImVec2(buttonWidth, 30)) )
	{
		currentPage = 0;
	}

	ImGui::SameLine(0, 4);

	if( ImGui::Button("Save", ImVec2(buttonWidth, 30)) )
	{
		CPreferences::Get()->SetRomPreferences( mRomID, romPreferences );
		CPreferences::Get()->Commit();
	
		currentPage = 0;
	}

	ImGui::PopItemWidth();
	ImGui::End();
	ImGui::Render();

	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
	
	romPreferences.Apply();

	return currentPage == 3;
}

static void showFPS()
{
	ImGui::SetNextWindowPos( ImVec2(250,0) );
	ImGui::SetNextWindowSize( ImVec2(70, 22) );

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2)); 
	ImGui::BeginTooltip();
	ImGui::Text("FPS: %.2f", gCurrentFramerate);
	ImGui::End();
	ImGui::PopStyleVar();
}

static void DrawMainPage()
{
	ImGui_Impl3DS_NewFrame();

	ImGui::SetNextWindowPos( ImVec2(0, 0) );
	ImGui::SetNextWindowSize( ImVec2(320, 240) );

	ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

	if( ImGui::Button("Save State", ImVec2(ImGui::GetContentRegionAvail().x, 60)) ) currentPage = 1;
	if( ImGui::Button("Load State", ImVec2(ImGui::GetContentRegionAvail().x, 60)) ) currentPage = 2;

	int buttonWidth = (ImGui::GetContentRegionAvail().x - 6) / 2;
	if( ImGui::ColoredButton("Close ROM", 0.02f, ImVec2(buttonWidth, 60)) )  ImGui::OpenPopup("Are you sure?");
	ImGui::SameLine();
	if( ImGui::ColoredButton("Options",   0.55f, ImVec2(buttonWidth, 60)) )
	{
		UI::LoadRomPreferences( g_ROM.mRomID );
		currentPage = 3;
	}

	ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Are you sure?", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("Any unsaved progress will be lost\n\n");

        if (ImGui::Button("OK", ImVec2(120, 0)))
        { 
        	currentPage = 0;
	 		CPU_Halt("Exiting");
	 		ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
        	ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

	ImGui::End();

	if(gGlobalPreferences.DisplayFramerate)
		showFPS();

	ImGui::Render();
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

void UI::DrawInGameMenu()
{
	UI::RestoreRenderState();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	switch(currentPage)
	{
		case 0: DrawMainPage(); break;
		case 1: DrawSaveStatePage(); break;
		case 2: DrawLoadStatePage(); break;
		case 3: DrawOptionsPage(g_ROM.mRomID); break;
	}

	pglSelectScreen(GFX_TOP, GFX_LEFT);
}