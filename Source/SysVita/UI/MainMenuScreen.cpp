#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>

#include <vitasdk.h>
#include <vitaGL.h>

#include "BuildOptions.h"
#include "Config/ConfigOptions.h"
#include "Core/Cheats.h"
#include "Core/CPU.h"
#include "Core/Memory.h"
#include "Core/PIF.h"
#include "Core/RomSettings.h"
#include "Core/Save.h"
#include "Debug/DBGConsole.h"
#include "Debug/DebugLog.h"
#include "Graphics/GraphicsContext.h"
#include "HLEGraphics/BaseRenderer.h"
#include "HLEGraphics/TextureCache.h"
#include "Input/InputManager.h"
#include "Interface/RomDB.h"
#include "System/Paths.h"
#include "System/System.h"
#include "Test/BatchTest.h"
#include "Utility/IO.h"
#include "Utility/Preferences.h"
#include "Utility/Profiler.h"
#include "Utility/Thread.h"
#include "Utility/ROMFile.h"
#include "Utility/Timer.h"
#include "SysVita/UI/Menu.h"

#define STB_IMAGE_IMPLEMENTATION
#include "Utility/stb_image.h"

#define PREVIEW_PADDING 6
#define PREVIEW_HEIGHT 268.0f
#define PREVIEW_WIDTH  387.0f
#define MIN(x,y) ((x) > (y) ? (y) : (x))

#define ROMS_FOLDERS_NUM 5
#define FILTER_MODES_NUM 8

extern void loadConfig(const char *game);

char rom_game_name[256];
char playtime_str[32];
char selectedRom[256];
char rom_name_filter[128] = {0};

GLuint bg_image = 0xDEADBEEF;

struct CompatibilityList {
	char name[128];
	bool playable;
	bool ingame_plus;
	bool ingame_low;
	bool crash;
	bool slow;
	CompatibilityList *next;
};

struct RomSelection {
	char name[128];
	char fullpath[256];
	char preview[128];
	char title[128];
	bool is_online;
	RomID id;
	u32 size;
	ESaveType save;
	ECicType cic;
	uint64_t playtime;
	CompatibilityList *status;
	RomSelection *next;
};

static RomSelection *last_launched = nullptr;
static RomSelection *list = nullptr;
static CompatibilityList *comp = nullptr;
static RomSelection *old_hovered = nullptr;
static bool has_preview_icon = false;
static int preview_width, preview_height, preview_x, preview_y;
CRefPtr<CNativeTexture> mpPreviewTexture;
GLuint preview_icon = 0;
uint64_t cur_playtime = 0;

int oldSortOrder = -1;

int filter_idx = 0;
const char *filter_modes[] = {
	lang_strings[STR_NO_FILTER],
	lang_strings[STR_GAME_PLAYABLE],
	lang_strings[STR_GAME_INGAME_PLUS],
	lang_strings[STR_GAME_INGAME_MINUS],
	lang_strings[STR_GAME_CRASH],
	lang_strings[STR_NO_TAGS],
	lang_strings[STR_GAME_LOCAL],
	lang_strings[STR_GAME_ONLINE]
};

// Filter modes enum
enum {
	FILTER_DISABLED,
	FILTER_PLAYABLE,
	FILTER_INGAME_PLUS,
	FILTER_INGAME_MINUS,
	FILTER_CRASH,
	FILTER_NO_TAGS,
	FILTER_LOCAL,
	FILTER_ONLINE
};

void apply_rom_name_filter() {
	getDialogTextResult(rom_name_filter);
}

void resetRomList() {
	RomSelection *p = list;
	while (p) {
		RomSelection *old = p;
		p = p->next;
		free(old);
	}
	list = nullptr;
}

void swap_roms(RomSelection *a, RomSelection *b) {
	RomSelection tmp;
	
	// Swapping everything except next leaf pointer
	sceClibMemcpy(&tmp, a, sizeof(RomSelection) - 4);
	sceClibMemcpy(a, b, sizeof(RomSelection) - 4);
	sceClibMemcpy(b, &tmp, sizeof(RomSelection) - 4);
}

void swap_shaders(PostProcessingEffect *a, PostProcessingEffect *b) {
	PostProcessingEffect tmp;
	
	// Swapping everything except next leaf pointer
	sceClibMemcpy(&tmp, a, sizeof(PostProcessingEffect) - 4);
	sceClibMemcpy(a, b, sizeof(PostProcessingEffect) - 4);
	sceClibMemcpy(b, &tmp, sizeof(PostProcessingEffect) - 4);
}

void swap_overlays(Overlay *a, Overlay *b) {
	Overlay tmp;
	
	// Swapping everything except next leaf pointer
	sceClibMemcpy(&tmp, a, sizeof(Overlay) - 4);
	sceClibMemcpy(a, b, sizeof(Overlay) - 4);
	sceClibMemcpy(b, &tmp, sizeof(Overlay) - 4);
}

void sort_romlist(RomSelection *start, int order) { 
	// Checking for empty list
	if (start == NULL) 
		return; 
	
	int swapped, i; 
	RomSelection *ptr1; 
	RomSelection *lptr = NULL; 
  
	do { 
		swapped = 0; 
		ptr1 = start; 
  
		while (ptr1->next != lptr && ptr1->next) {
			switch (order) {
			case SORT_Z_TO_A:
				{
					if (strcasecmp(ptr1->name,ptr1->next->name) < 0) {
						swap_roms(ptr1, ptr1->next); 
						swapped = 1; 
					}
				}
				break;
			case SORT_A_TO_Z:
				{
					if (strcasecmp(ptr1->name,ptr1->next->name) > 0) {
						swap_roms(ptr1, ptr1->next); 
						swapped = 1; 
					}
				}
				break;
			case SORT_PLAYTIME_DESC:
				{
					if (ptr1->playtime < ptr1->next->playtime) {
						swap_roms(ptr1, ptr1->next); 
						swapped = 1; 
					}
				}
				break;
			case SORT_PLAYTIME_ASC:
				{
					if (ptr1->playtime > ptr1->next->playtime) {
						swap_roms(ptr1, ptr1->next); 
						swapped = 1; 
					}
				}
				break;
			default:
				break;
			}
			ptr1 = ptr1->next; 
		} 
		lptr = ptr1; 
	} while (swapped); 
}

void sort_shaderlist(PostProcessingEffect *start) {
	int swapped, i; 
	PostProcessingEffect *ptr1; 
	PostProcessingEffect *lptr = NULL;
	
	/* Checking for empty list */
	if (start == NULL) 
		return; 
	
	do {
		swapped = 0;
		ptr1 = start;
		
		while (ptr1->next != lptr) {
			if (strcasecmp(ptr1->name,ptr1->next->name) > 0) {  
				swap_shaders(ptr1, ptr1->next); 
				swapped = 1; 
			}
			ptr1 = ptr1->next; 
		} 
		lptr = ptr1;
	} while (swapped);
}

void sort_overlaylist(Overlay *start) {
	int swapped, i; 
	Overlay *ptr1; 
	Overlay *lptr = NULL;
	
	/* Checking for empty list */
	if (start == NULL) 
		return; 
	
	do {
		swapped = 0;
		ptr1 = start;
		
		while (ptr1->next != lptr) {
			if (strcasecmp(ptr1->name,ptr1->next->name) > 0) {  
				swap_overlays(ptr1, ptr1->next); 
				swapped = 1; 
			}
			ptr1 = ptr1->next; 
		} 
		lptr = ptr1;
	} while (swapped);
}

void LoadBackground() {
	IO::Filename preview_filename;
	IO::Path::Combine(preview_filename, DAEDALUS_VITA_PATH("Resources/"), "bg.png" );
	int w, h;
	uint8_t *bg_data = stbi_load(preview_filename, &w, &h, NULL, 4);
	if (bg_data) {
		glGenTextures(1, &bg_image);
		glBindTexture(GL_TEXTURE_2D, bg_image);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bg_data);
		free(bg_data);
	}
}

void LoadPlaytimeData(RomSelection *p) {
	if (p->is_online) {
		p->playtime = 0;
		return;
	}
	char fname[64];
	sprintf(fname, "%08x%08x-%02x.bin", p->id.CRC[0], p->id.CRC[1], p->id.CountryID);
	IO::Filename fullpath_filename;
	IO::Path::Combine(fullpath_filename, DAEDALUS_VITA_PATH("Playtimes/"), fname );
	FILE *f = fopen(fullpath_filename, "rb");
	if (f) {
		fscanf(f, "%llu", &p->playtime);
		fclose(f);
	} else p->playtime = 0;
}

char *FormatPlaytime(uint64_t playtime) {
	uint64_t seconds = playtime % 60;
	uint64_t min_raw = (playtime / 60);
	uint64_t minutes = min_raw % 60;
	uint64_t hours = min_raw / 60;
	sprintf(playtime_str, "%02llu:%02llu:%02llu", hours, minutes, seconds);
	return playtime_str;
}

float *bg_attributes = nullptr;
void DrawBackground()
{
	if (!bg_attributes) bg_attributes = (float*)malloc(sizeof(float) * 22);

	glBindTexture(GL_TEXTURE_2D, bg_image);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	bg_attributes[0] = 0.0f;
	bg_attributes[1] = 0.0f;
	bg_attributes[2] = 0.0f;
	bg_attributes[3] = SCR_WIDTH;
	bg_attributes[4] = 0.0f;
	bg_attributes[5] = 0.0f;
	bg_attributes[6] = 0.0f;
	bg_attributes[7] = SCR_HEIGHT;
	bg_attributes[8] = 0.0f;
	bg_attributes[9] = SCR_WIDTH;
	bg_attributes[10] = SCR_HEIGHT;
	bg_attributes[11] = 0.0f;
	vglVertexPointerMapped(3, bg_attributes);
	
	bg_attributes[12] = 0.0f;
	bg_attributes[13] = 0.0f;
	bg_attributes[14] = 1.0f;
	bg_attributes[15] = 0.0f;
	bg_attributes[16] = 0.0f;
	bg_attributes[17] = 1.0f;
	bg_attributes[18] = 1.0f;
	bg_attributes[19] = 1.0f;
	vglTexCoordPointerMapped(&bg_attributes[12]);
	
	uint16_t *bg_indices = (uint16_t*)&bg_attributes[20];
	bg_indices[0] = 0;
	bg_indices[1] = 1;
	bg_indices[2] = 2;
	bg_indices[3] = 3;
	vglIndexPointerMapped(bg_indices);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 960, 544, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	vglDrawObjects(GL_TRIANGLE_STRIP, 4, GL_TRUE);
}

bool LoadPreview(RomSelection *rom) {
	if (old_hovered == rom) return has_preview_icon;
	old_hovered = rom;
	
	IO::Filename preview_filename;
	IO::Path::Combine(preview_filename, DAEDALUS_VITA_PATH("Resources/Preview/"), rom->preview );
	uint8_t *icon_data = stbi_load(preview_filename, &preview_width, &preview_height, NULL, 4);
	if (icon_data) {
		if (!preview_icon) glGenTextures(1, &preview_icon);
		glBindTexture(GL_TEXTURE_2D, preview_icon);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, preview_width, preview_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, icon_data);
		f32 scale = MIN(PREVIEW_WIDTH / (f32)preview_width, PREVIEW_HEIGHT / (f32)preview_height);
		preview_width = scale * (f32)preview_width;
		preview_height = scale * (f32)preview_height;
		preview_x = (PREVIEW_WIDTH - preview_width) / 2;
		preview_y = (PREVIEW_HEIGHT - preview_height) / 2;
		free(icon_data);
		return true;
	}
	
	return false;
}

// TODO: Use a proper json lib for more safety
void AppendCompatibilityDatabase(const char *file) {
	FILE *f = fopen(file, "rb");
	if (f) {
		fseek(f, 0, SEEK_END);
		uint64_t len = ftell(f);
		fseek(f, 0, SEEK_SET);
		char *buffer = (char*)malloc(len + 1);
		fread(buffer, 1, len, f);
		buffer[len] = 0;
		char *ptr = buffer;
		char *end;
		do {
			ptr = strstr(ptr, "\"title\":");
			if (ptr) {
				CompatibilityList *node = (CompatibilityList*)malloc(sizeof(CompatibilityList));
				
				// Extracting title
				ptr += 10;
				end = strstr(ptr, "\"");
				sceClibMemcpy(node->name, ptr, end - ptr);
				node->name[end - ptr] = 0;
				
				// Extracting tags
				bool perform_slow_check = true;
				ptr += 1000; // Let's skip some data to improve performances
				ptr = strstr(ptr, "\"labels\":");
				ptr = strstr(ptr + 150, "\"name\":");
				ptr += 9;
				if (ptr[0] == 'P') {
					node->playable = true;
					node->ingame_low = false;
					node->ingame_plus = false;
					node->crash = false;
				} else if (ptr[0] == 'C') {
					node->playable = false;
					node->ingame_low = false;
					node->ingame_plus = false;
					node->slow = false;
					node->crash = true;
					perform_slow_check = false;
				} else {
					node->playable = false;
					node->crash = false;
					end = strstr(ptr, "\"");
					if ((end - ptr) == 13) {
						node->ingame_plus = true;
						node->ingame_low = false;
					}else {
						node->ingame_low = true;
						node->ingame_plus = false;
					}
				}
				ptr += 120; // Let's skip some data to improve performances
				if (perform_slow_check) {
					end = ptr;
					ptr = strstr(ptr, "]");
					if ((ptr - end) > 200) node->slow = true;
					else node->slow = false;
				}
				
				ptr += 350; // Let's skip some data to improve performances
				node->next = comp;
				comp = node;
			}
		} while (ptr);
		fclose(f);
		free(buffer);
	}
}

CompatibilityList *SearchForCompatibilityData(const char *name) {
	CompatibilityList *node = comp;
	char tmp[128];
	sprintf(tmp, name);
	stripGameName(tmp);
	while (node) {
		if (strcasecmp(node->name, tmp) == 0) return node;
		node = node->next;
	}
	return nullptr;
}

void SetTagDescription(const char *text) {
	ImGui::SameLine();
	ImGui::TextWrapped(": %s", text);
}

bool filterRoms(RomSelection *p) {
	if (filter_idx < FILTER_LOCAL) {
		if (!p->status) return filter_idx != FILTER_NO_TAGS;
		else {
			if (filter_idx == FILTER_NO_TAGS) return true;
			else if ((!p->status->playable && filter_idx == FILTER_PLAYABLE) ||
				(!p->status->ingame_plus && filter_idx == FILTER_INGAME_PLUS) ||
				(!p->status->ingame_low && filter_idx == FILTER_INGAME_MINUS) ||
				(!p->status->crash && filter_idx == FILTER_CRASH)) {
				return true;
			}
		}
	} else {
		if (!p->is_online && filter_idx == FILTER_ONLINE) return true;
		else if (p->is_online && filter_idx == FILTER_LOCAL) return true;
	}
	return false;
}

char *DrawRomSelector(bool skip_reloads) {
	bool selected = false;
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (bg_image != 0xDEADBEEF) DrawBackground();
	DrawMenuBar();
	
	if (last_launched && !skip_reloads) {
		LoadPlaytimeData(last_launched);
		last_launched = nullptr;
	}
		
	if (!list) {
		oldSortOrder = -1;
		
		if (!comp) {
			LoadBackground();
			for (int i = 1; i <= NUM_DB_CHUNKS; i++) {
				char dbname[64];
				sprintf(dbname, "%sdb%ld.json", DAEDALUS_VITA_MAIN_PATH, i);
				AppendCompatibilityDatabase(dbname);
			}
		}
		std::string			full_path;

		IO::FindHandleT		find_handle;
		IO::FindDataT		find_data;
		
		const char *rom_folders[ROMS_FOLDERS_NUM] = {
			DAEDALUS_VITA_PATH_EXT("ux0:" , "Roms/"),
			DAEDALUS_VITA_PATH_EXT("uma0:", "Roms/"),
			DAEDALUS_PSP_PATH_EXT("ux0:" , "Roms/"),
			DAEDALUS_PSP_PATH_EXT("uma0:", "Roms/"),
			gCustomRomPath
		};
		
		for (int i = 0; i < ROMS_FOLDERS_NUM; i++) {
			if(IO::FindFileOpen( rom_folders[i], &find_handle, find_data ))
			{
				do
				{
					const char * rom_filename( find_data.Name );
					if(IsRomfilename( rom_filename ))
					{
						std::string full_path = rom_folders[i];
						full_path += rom_filename;
						RomSelection *node = (RomSelection*)malloc(sizeof(RomSelection));
						node->is_online = false;
						strcpy(node->name, rom_filename);
						strcpy(node->fullpath, full_path.c_str());
						if (ROM_GetRomDetailsByFilename(full_path.c_str(), &node->id, &node->size, &node->cic)) {
							node->size = node->size / (1024 * 1024);
							RomSettings tmpsettings;
							if (!CRomSettingsDB::Get()->GetSettings(node->id, &tmpsettings )) {
								tmpsettings.Reset();
								std::string game_name;
								if (!ROM_GetRomName(full_path.c_str(), game_name )) game_name = full_path;
								game_name = game_name.substr(0, 63);
								tmpsettings.GameName = game_name.c_str();
								CRomSettingsDB::Get()->SetSettings(node->id, tmpsettings);
							}
							strcpy(node->title, tmpsettings.GameName.c_str());
							strcpy(node->preview, tmpsettings.Preview.c_str());
							node->save = tmpsettings.SaveType;
							node->status = SearchForCompatibilityData(node->title);
						}
						LoadPlaytimeData(node);
						node->next = list;
						list = node;
					}
				}
				while(IO::FindFileNext( find_handle, find_data ));

				IO::FindFileClose( find_handle );
			}
		}
		
		if (raw_net_romlist) {
			char *p = (char*)raw_net_romlist;
			while (p) {
				char *r = strcasestr(p, "a href");
				if (r) {
					char name[128], tmp[128];
					r = strstr(r, "\">");
					char *r2 = strcasestr(r, "</a");
					sceClibMemcpy(name, r + 2, (r2 - (r + 2)));
					name[(r2 - (r + 2))] = 0;
					if (name[0] == ' ') {
						int len = strlen(&name[1]);
						memmove(name, &name[1], len);
						name[len] = 0;
					}
					if (IsRomfilename(name) && (!strstr(name, ".zip"))) {
						RomSelection *node = (RomSelection*)malloc(sizeof(RomSelection));
						node->is_online = true;
						strcpy(node->name, name);
						node->next = list;
						node->status = nullptr;
						list = node;
					}
					p = r + 2;
				} else break;
			}
		}
	}
	
	if (oldSortOrder != gSortOrder) {
		oldSortOrder = gSortOrder;
		sort_romlist(list, gSortOrder);
	}
	
	if (bg_image != 0xDEADBEEF) ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::SetNextWindowPos(ImVec2(0, 19 * UI_SCALE), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(SCR_WIDTH - 400, SCR_HEIGHT - 19 * UI_SCALE), ImGuiSetCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin("Selector Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
	RomSelection *hovered = nullptr;
	RomSelection *p = list;
	
	ImGui::AlignTextToFramePadding();
	ImGui::Text(lang_strings[STR_SEARCH]);
	ImGui::SameLine();
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
	if (ImGui::Button(rom_name_filter, ImVec2(-1.0f, 0.0f))) {
		showDialog(lang_strings[STR_DLG_SEARCH_ROM], apply_rom_name_filter, dummy_func, DIALOG_KEYBOARD, rom_name_filter);
	}
	ImGui::PopStyleVar();
	ImGui::AlignTextToFramePadding();
	ImGui::Text(lang_strings[STR_FILTER_BY]);
	ImGui::SameLine();
	ImGui::PushItemWidth(-1.0f);
	if (ImGui::BeginCombo("##combo", filter_modes[filter_idx])) {
		for (int n = 0; n < FILTER_MODES_NUM; n++) {
			bool is_selected = filter_idx == n;
			if (ImGui::Selectable(filter_modes[n], is_selected))
				filter_idx = n;
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();
	ImGui::Separator();
	
	if (strlen(rom_name_filter) > 0) { // Filter results with searchbox
		while (p) {
			if (strcasestr(p->name, rom_name_filter)) {
				if (filter_idx > 0) { // Apply filters
					if (filterRoms(p)) {
						p = p->next;
						continue;
					}
				}
				if (p->is_online) {
					ImGui::PushStyleColor(ImGuiCol_Button, 0x880015FF);
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0xFF0000FF);
				}
				if (ImGui::Button(p->name, ImVec2(-1.0f, 0.0f))){
					if (p->is_online) {
						char url[512];
						sprintf(url, "%s%s", gNetRomPath, p->name);
						queueDownload(lang_strings[STR_DLG_ROM_LAUNCH], url, 8 * 1024 * 1024, dummy_func, MEM_DOWNLOAD);
						sprintf(selectedRom, "/%s.net", p->name);
					}
					else strcpy(selectedRom, p->fullpath);
					selected = true;
				}
				if (p->is_online) {
					ImGui::PopStyleColor(2);
				}
				if (ImGui::IsItemHovered()) {
					hovered = p;
					if (!p->is_online) {
						SceCtrlData pad;
						static uint32_t oldpad = 0;
						sceCtrlPeekBufferPositive(0, &pad, 1);
						if ((pad.buttons & SCE_CTRL_TRIANGLE) && (!(oldpad & SCE_CTRL_TRIANGLE))) {
							System_ExtractName(p->fullpath, rom_game_name);
							loadConfig(rom_game_name);
						}
						oldpad = pad.buttons;
					}
				}
			}
			p = p->next;
		}
	} else { // No filters
		while (p) {
			if (filter_idx > 0) { // Apply filters
				if (filterRoms(p)) {
					p = p->next;
					continue;
				}
			}
			if (p->is_online) {
				ImGui::PushStyleColor(ImGuiCol_Button, 0x880015FF);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0xFF0000FF);
			}
			if (ImGui::Button(p->name, ImVec2(-1.0f, 0.0f))){
				if (p->is_online) {
					char url[512];
					sprintf(url, "%s%s", gNetRomPath, p->name);
					queueDownload(lang_strings[STR_DLG_ROM_LAUNCH], url, 8 * 1024 * 1024, dummy_func, MEM_DOWNLOAD);
					sprintf(selectedRom, "/%s.net", p->name);
				}
				else strcpy(selectedRom, p->fullpath);
				selected = true;
			}
			if (p->is_online) {
				ImGui::PopStyleColor(2);
			}
			if (ImGui::IsItemHovered()) {
				hovered = p;
				if (!p->is_online) {
					SceCtrlData pad;
					static uint32_t oldpad = 0;
					sceCtrlPeekBufferPositive(0, &pad, 1);
					if ((pad.buttons & SCE_CTRL_TRIANGLE) && (!(oldpad & SCE_CTRL_TRIANGLE))) {
						System_ExtractName(p->fullpath, rom_game_name);
						loadConfig(rom_game_name);
					}
					oldpad = pad.buttons;
				}
			}
			p = p->next;
		}
	}

	ImGui::End();
	
	if (bg_image != 0xDEADBEEF) ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::SetNextWindowPos(ImVec2(SCR_WIDTH - 400, 19 * UI_SCALE), ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(ImVec2(400, SCR_HEIGHT - 19 * UI_SCALE), ImGuiSetCond_Always);
	ImGui::Begin("Info Window", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
	if (hovered) {
		if (hovered->is_online) {
			ImGui::TextWrapped(lang_strings[STR_GAME_NET]);
		} else {
			if (has_preview_icon = LoadPreview(hovered)) {
				ImGui::SetCursorPos(ImVec2(preview_x + PREVIEW_PADDING, preview_y + PREVIEW_PADDING));
				ImGui::Image((void*)preview_icon, ImVec2(preview_width, preview_height));
			}
			ImGui::Text("%s: %s", lang_strings[STR_GAME_NAME], strlen(hovered->title) > 0 ? hovered->title : lang_strings[STR_UNKNOWN]);
			ImGui::Text("%s: %s", lang_strings[STR_REGION], ROM_GetCountryNameFromID(hovered->id.CountryID));
			ImGui::Text("%s: %s", lang_strings[STR_PLAYTIME], FormatPlaytime(hovered->playtime));
			ImGui::Text("CRC: %08x%08x-%02x", hovered->id.CRC[0], hovered->id.CRC[1], hovered->id.CountryID);
			ImGui::Text("%s: %s", lang_strings[STR_CIC_TYPE], ROM_GetCicName(hovered->cic));
			ImGui::Text("%s: %lu MBs", lang_strings[STR_ROM_SIZE], hovered->size);
			ImGui::Text("%s: %s", lang_strings[STR_SAVE_TYPE], ROM_GetSaveTypeName(hovered->save));
			if (hovered->status) {
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
				if (!gBigText) ImGui::Text("%s:", lang_strings[STR_TAGS]);
				if (hovered->status->playable) {
					ImGui::TextColored(ImVec4(0, 0.75f, 0, 1.0f), "%s", lang_strings[STR_GAME_PLAYABLE]);
					if (!gBigText) SetTagDescription(lang_strings[STR_PLAYABLE_DESC]);
				}
				if (hovered->status->ingame_plus) {
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0, 1.0f), "%s", lang_strings[STR_GAME_INGAME_PLUS]);
					if (gBigText) ImGui::SameLine();
					else SetTagDescription(lang_strings[STR_INGAME_PLUS_DESC]);
				}
				if (hovered->status->ingame_low) {
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.25f, 1.0f), "%s", lang_strings[STR_GAME_INGAME_MINUS]);
					if (!gBigText) SetTagDescription(lang_strings[STR_INGAME_MINUS_DESC]);
				}
				if (hovered->status->crash) {
					ImGui::TextColored(ImVec4(1.0f, 0, 0, 1.0f), "%s", lang_strings[STR_GAME_CRASH]);
					if (!gBigText) SetTagDescription(lang_strings[STR_CRASH_DESC]);
				}
				if (hovered->status->slow) {
					if (gBigText) {
						ImGui::SameLine();
						ImGui::Text("&");
						ImGui::SameLine();
					}
					ImGui::TextColored(ImVec4(0.5f, 0, 1.0f, 1.0f), "%s", lang_strings[STR_GAME_SLOW]);
					if (!gBigText) SetTagDescription(lang_strings[STR_SLOW_DESC]);
				}
			}
		}
	}
	
	ImGui::End();
	ImGui::PopStyleVar();
	DrawPendingAlert();
	
	glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
	ImGui::Render();
	ImGui_ImplVitaGL_RenderDrawData(ImGui::GetDrawData());
	DrawPendingDialog();
	vglSwapBuffers(GL_FALSE);
	
	if (pendingDownload) {
		switch (cur_download.type) {
		case FILE_DOWNLOAD:
			{
				if (download_file(cur_download.url, TEMP_DOWNLOAD_NAME, cur_download.msg, cur_download.size, true) >= 0)
					cur_download.post_func();
			}
			break;
		case MEM_DOWNLOAD:
			{
				if (download_file(cur_download.url, TEMP_DOWNLOAD_NAME, cur_download.msg, cur_download.size, false) >= 0)
					cur_download.post_func();
			}
			break;
		}
		pendingDownload = false;
	}
	
	if (selected && !skip_reloads) {
		cur_playtime = hovered->playtime;
		last_launched = hovered;
		CheatCodes_Read(hovered->title, "Daedalus.cht", hovered->id.CountryID);
		return selectedRom;
	}
	return nullptr;
}