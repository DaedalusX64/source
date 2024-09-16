#include <stdio.h>
#include <vitaGL.h>
#include <imgui_vita.h>
#include "stdafx.h"

#define TEMP_DOWNLOAD_NAME "ux0:data/daedalusx64.tmp"
#define LOG_DOWNLOAD_NAME "ux0:data/daedalusx64.log"
#define FUNC_TO_NAME(x) #x
#define stringify(x) FUNC_TO_NAME(x)

#define NUM_DB_CHUNKS     5 // Number of pages to download for the compat list

#define ALERT_TIME 5000000 // Timer for alerts to disappear in microseconds

#define UI_SCALE (gBigText ? 1.5f : 1.0f)

// Auto updater passes
enum {
	UPDATER_CHECK_UPDATES,
	UPDATER_DOWNLOAD_CHANGELIST,
	UPDATER_DOWNLOAD_UPDATE,
	NUM_UPDATE_PASSES
};

// Anti-aliasing modes
enum {
	ANTIALIASING_DISABLED,
	ANTIALIASING_MSAA_2X,
	ANTIALIASING_MSAA_4X
};

// Roms sorting modes
enum {
	SORT_A_TO_Z,
	SORT_Z_TO_A,
	SORT_PLAYTIME_DESC,
	SORT_PLAYTIME_ASC
};

// CPU emulation modes
enum {
	CPU_DYNAREC,
	CPU_CACHED_INTERPRETER,
	CPU_INTERPRETER
};

// UI themes
enum {
	DARK_THEME,
	LIGHT_THEME,
	CLASSIC_THEME
};

// Texture caching modes
enum {
	TEX_CACHE_DISABLED,
	TEX_CACHE_FAST,
	TEX_CACHE_ACCURATE
};

// Translation strings
#define LANG_STRINGS_NUM 173

#define FOREACH_STR(FUNC) \
	FUNC(STR_DOWNLOADER_COMPAT_LIST) \
	FUNC(STR_DOWNLOADER_UPDATE) \
	FUNC(STR_DOWNLOADER_CHECK_UPDATE) \
	FUNC(STR_UNKNOWN) \
	FUNC(STR_UNUSED) \
	FUNC(STR_USED) \
	FUNC(STR_REQUIRED) \
	FUNC(STR_GAME_NAME) \
	FUNC(STR_REGION) \
	FUNC(STR_CIC_TYPE) \
	FUNC(STR_ROM_SIZE) \
	FUNC(STR_SAVE_TYPE) \
	FUNC(STR_TAGS) \
	FUNC(STR_GAME_PLAYABLE) \
	FUNC(STR_GAME_INGAME_PLUS) \
	FUNC(STR_GAME_INGAME_MINUS) \
	FUNC(STR_GAME_CRASH) \
	FUNC(STR_GAME_SLOW) \
	FUNC(STR_PLAYABLE_DESC) \
	FUNC(STR_INGAME_PLUS_DESC) \
	FUNC(STR_INGAME_MINUS_DESC) \
	FUNC(STR_CRASH_DESC) \
	FUNC(STR_SLOW_DESC) \
	FUNC(STR_MENU_LANG) \
	FUNC(STR_MENU_EXTRA) \
	FUNC(STR_MENU_EMULATION) \
	FUNC(STR_MENU_GRAPHICS) \
	FUNC(STR_MENU_AUDIO) \
	FUNC(STR_MENU_INPUT) \
	FUNC(STR_MENU_OPTIONS) \
	FUNC(STR_MENU_DEBUGGER) \
	FUNC(STR_MENU_CREDITS) \
	FUNC(STR_MENU_GLOBAL_SETTINGS) \
	FUNC(STR_MENU_GAME_SETTINGS) \
	FUNC(STR_MENU_UNLOAD_GAME_SETTINGS) \
	FUNC(STR_MENU_UI_THEME) \
	FUNC(STR_MENU_MENUBAR) \
	FUNC(STR_MENU_AUTOUPDATE) \
	FUNC(STR_MENU_COMPAT_LIST) \
	FUNC(STR_MENU_LOG) \
	FUNC(STR_MENU_TEX_DUMPER) \
	FUNC(STR_MENU_DYNAREC) \
	FUNC(STR_MENU_DYNAREC_CONFIG) \
	FUNC(STR_MENU_CACHED_INTERP) \
	FUNC(STR_MENU_INTERP) \
	FUNC(STR_MENU_HLE) \
	FUNC(STR_MENU_FRAME_LIMIT) \
	FUNC(STR_MENU_VIDEO_RATE) \
	FUNC(STR_MENU_AUDIO_RATE) \
	FUNC(STR_MENU_ASPECT_RATIO) \
	FUNC(STR_MENU_RATIO_UNSTRETCHED) \
	FUNC(STR_MENU_RATIO_ORIGINAL) \
	FUNC(STR_MENU_BRIGHTNESS) \
	FUNC(STR_MENU_TEX_CACHE) \
	FUNC(STR_MENU_BILINEAR) \
	FUNC(STR_MENU_MIPMAPS) \
	FUNC(STR_MENU_HIRES_TEX) \
	FUNC(STR_MENU_VRAM) \
	FUNC(STR_MENU_WAIT_REND) \
	FUNC(STR_MENU_FILES) \
	FUNC(STR_MENU_CHEATS) \
	FUNC(STR_MENU_SAVE_STATE) \
	FUNC(STR_MENU_LOAD_STATE) \
	FUNC(STR_MENU_RESTART_ROM) \
	FUNC(STR_MENU_CLOSE_ROM) \
	FUNC(STR_MENU_SORT_ROMS) \
	FUNC(STR_MENU_MP3_INSTR) \
	FUNC(STR_MENU_REARPAD) \
	FUNC(STR_MENU_CTRL_MAP) \
	FUNC(STR_SORT_A_TO_Z) \
	FUNC(STR_SORT_Z_TO_A) \
	FUNC(STR_DISABLED) \
	FUNC(STR_SLOT) \
	FUNC(STR_VFLUX_CONFIG) \
	FUNC(STR_VFLUX_COLOR) \
	FUNC(STR_VFLUX_ENABLE) \
	FUNC(STR_CREDITS_AUTHOR) \
	FUNC(STR_CREDITS_PATRONERS) \
	FUNC(STR_CREDITS_TRANSLATORS) \
	FUNC(STR_CREDITS_THANKS) \
	FUNC(STR_CREDITS_1) \
	FUNC(STR_CREDITS_2) \
	FUNC(STR_CREDITS_3) \
	FUNC(STR_CREDITS_4) \
	FUNC(STR_CREDITS_5) \
	FUNC(STR_CREDITS_6) \
	FUNC(STR_CREDITS_7) \
	FUNC(STR_CREDITS_8) \
	FUNC(STR_CREDITS_9) \
	FUNC(STR_CART_ID) \
	FUNC(STR_GFX_UCODE) \
	FUNC(STR_AUDIO_UCODE) \
	FUNC(STR_SYNC) \
	FUNC(STR_ASYNC) \
	FUNC(STR_CONTROLLER) \
	FUNC(STR_ACCESSORY) \
	FUNC(STR_ACCURATE) \
	FUNC(STR_FAST) \
	FUNC(STR_DESC_MP3_INSTR) \
	FUNC(STR_DESC_REARPAD) \
	FUNC(STR_DESC_VFLUX) \
	FUNC(STR_DESC_VRAM) \
	FUNC(STR_DESC_WAIT_REND) \
	FUNC(STR_DESC_HIRES_TEX) \
	FUNC(STR_DESC_MIPMAPS) \
	FUNC(STR_DESC_BILINEAR) \
	FUNC(STR_DESC_CACHE_FAST) \
	FUNC(STR_DESC_CACHE_ACCURATE) \
	FUNC(STR_DESC_CACHE_DISABLED) \
	FUNC(STR_DESC_AUDIO_RATE) \
	FUNC(STR_DESC_VIDEO_RATE) \
	FUNC(STR_DESC_FRAME_LIMIT) \
	FUNC(STR_DESC_HLE) \
	FUNC(STR_DESC_INTERP) \
	FUNC(STR_DESC_CACHED_INTERP) \
	FUNC(STR_MENU_DYNAREC_WORDS_OPT) \
	FUNC(STR_DESC_DYNAREC) \
	FUNC(STR_DESC_TEX_DUMPER) \
	FUNC(STR_THEME_DARK) \
	FUNC(STR_THEME_LIGHT) \
	FUNC(STR_THEME_CLASSIC) \
	FUNC(STR_REGION_GER) \
	FUNC(STR_REGION_USA) \
	FUNC(STR_REGION_FRA) \
	FUNC(STR_REGION_ITA) \
	FUNC(STR_REGION_JAP) \
	FUNC(STR_REGION_EUR) \
	FUNC(STR_REGION_ESP) \
	FUNC(STR_REGION_AUS) \
	FUNC(STR_ANTI_ALIASING) \
	FUNC(STR_REBOOT_REQ) \
	FUNC(STR_ALERT_GLOBAL_SETTINGS_SAVE) \
	FUNC(STR_ALERT_GAME_SETTINGS_SAVE) \
	FUNC(STR_ALERT_GLOBAL_SETTINGS_LOAD) \
	FUNC(STR_ALERT_GAME_SETTINGS_LOAD) \
	FUNC(STR_ALERT_STATE_SAVE) \
	FUNC(STR_ALERT_STATE_LOAD) \
	FUNC(STR_BIG_TEXT) \
	FUNC(STR_ROM_LAUNCH) \
	FUNC(STR_CUSTOM_PATH) \
	FUNC(STR_DLG_CUSTOM_PATH) \
	FUNC(STR_SEARCH) \
	FUNC(STR_DLG_SEARCH_ROM) \
	FUNC(STR_EXTRACTING) \
	FUNC(STR_DOWNLOAD_DATA) \
	FUNC(STR_DLG_DOWNLOAD_DATA) \
	FUNC(STR_FILTER_BY) \
	FUNC(STR_NO_FILTER) \
	FUNC(STR_NO_TAGS) \
	FUNC(STR_MENU_POST_PROCESSING) \
	FUNC(STR_DESC_POST_PROCESSING) \
	FUNC(STR_MENU_OVERLAYS) \
	FUNC(STR_DESC_OVERLAYS) \
	FUNC(STR_DLG_RETRIEVE_NET_PATH) \
	FUNC(STR_NET_PATH) \
	FUNC(STR_DLG_NET_PATH) \
	FUNC(STR_DLG_ROM_LAUNCH) \
	FUNC(STR_GAME_NET) \
	FUNC(STR_GAME_LOCAL) \
	FUNC(STR_GAME_ONLINE) \
	FUNC(STR_MENU_LEGACY_REND) \
	FUNC(STR_MENU_MODERN_REND) \
	FUNC(STR_DESC_LEGACY_REND) \
	FUNC(STR_DESC_MODERN_REND) \
	FUNC(STR_PLAYTIME) \
	FUNC(STR_PLAYTIME_DESC) \
	FUNC(STR_PLAYTIME_ASC) \
	FUNC(STR_DLG_NET_BOOT) \
	FUNC(STR_MENU_DYNAREC_LOOPS_OPT) \
	FUNC(STR_SHADER_COMPILER_ERROR) \
	FUNC(STR_DOWNLOADER_CHANGELIST) \
	FUNC(STR_UPDATE_CHANGES) \
	FUNC(STR_CONTINUE)

#define GET_VALUE(x) x,
#define GET_STRING(x) #x,

extern GLuint cur_prog;
extern GLuint cur_overlay;
extern float *vflux_vertices;
extern float *vflux_texcoords;

enum {
	FOREACH_STR(GET_VALUE)
};

#define LANG_ID_SIZE   64
#define LANG_STR_SIZE 256
extern char lang_identifiers[LANG_STRINGS_NUM][LANG_ID_SIZE];
extern char lang_strings[LANG_STRINGS_NUM][LANG_STR_SIZE];
extern bool show_menubar;

// Dialog types
enum {
	DIALOG_MESSAGE,
	DIALOG_KEYBOARD
};

// Alert types
enum {
	ALERT_MESSAGE
};

// Download types
enum {
	FILE_DOWNLOAD,
	MEM_DOWNLOAD
};

// Button status types
enum {
	BUTTON_SHORT_HOLD,
	BUTTON_SHORT_RELEASED,
	BUTTON_LONG_HOLD,
	BUTTON_LONG_RELEASED,
	BUTTON_NEUTRAL
};

struct Download {
	void (*post_func)();
	char url[256];
	char msg[128];
	int size;
	uint8_t type;
};

struct Dialog {
	void (*yes_func)();
	void (*no_func)();
	uint8_t type;
};

struct Alert {
	char msg[256];
	uint64_t tick;
	uint8_t type;
};

struct PostProcessingEffect {
	char name[32];
	char desc[128];
	bool compiled;
	bool customizable;
	PostProcessingEffect *next;
};

struct Overlay {
	char name[32];
	Overlay *next;
};

struct ButtonSce {
	bool down;
	uint32_t downTime;
	uint32_t longPressTime;
	const SceCtrlButtons btn;
};

enum UnifType {
	UNIF_FLOAT = 1,
	UNIF_COLOR = 3
};

struct Uniform {
	GLint idx;
	char name[30];
	UnifType type;
	float value[3];
};

extern Uniform prog_uniforms[8];

extern Dialog cur_dialog;
extern Alert cur_alert;
extern Download cur_download;

extern Overlay *overlays_list;
extern PostProcessingEffect *effects_list;

// Language identifiers for languages not supported by PSVITA nativeely
#define SCE_SYSTEM_PARAM_LANG_CATALAN   20
#define SCE_SYSTEM_PARAM_LANG_GREEK     21
#define SCE_SYSTEM_PARAM_LANG_ROMANIAN  22
#define SCE_SYSTEM_PARAM_LANG_RYUKYUAN  23
#define SCE_SYSTEM_PARAM_LANG_UKRAINIAN 24

// Config Variables
extern bool gUseRendererLegacy;
extern bool gHideMenubar;
extern int  gLanguageIndex;
extern int  gUseCdram;
extern int  gUseVSync;
extern int  gCpuMode;
extern int  gTexCacheMode;
extern bool gUseMp3;
extern bool gWaitRendering;
extern bool gUseExpansionPak;
extern bool gUseHighResTextures;
extern bool gTexturesDumper;
extern bool gUseMipmaps;
extern bool gSkipCompatListUpdate;
extern bool gAutoUpdate;
extern bool gUseRearpad;
extern bool gNetBoot;
extern int  gPostProcessing;
extern int  gOverlay;
extern int  gSortOrder;
extern int  gUiTheme;
extern int  gAntiAliasing;
extern bool gBigText;
extern char gCustomRomPath[256];
extern char gNetRomPath[256];

extern bool pendingDialog;
extern bool pendingAlert;
extern bool pendingDownload;
extern bool custom_path_str_dirty;
extern bool net_path_str_dirty;
extern bool is_main_menu;
extern bool fontDirty;

extern uint64_t cur_playtime;
extern char *raw_net_romlist;
extern uint8_t *rom_mem_buffer;
extern volatile uint32_t temp_download_size;

char *DrawRomSelector(bool skip_reloads);
void DrawInGameMenu();
void DrawMenuBar();
void DrawInGameMenuBar();
void DrawDownloaderScreen(int index, float downloaded_bytes, float total_bytes, char *text, int passes);
void DrawDownloaderScreenCompat(float downloaded_bytes, float total_bytes, char *text);
void DrawExtractorScreen(int index, float file_extracted_bytes, float extracted_bytes, float file_total_bytes, float total_bytes, char *filename, int num_files);
void DrawChangeListScreen(FILE *f);
void DrawPendingDialog();
void DrawPendingAlert();
void DrawFastForwardIcon();

void EnableMenuButtons(bool status);
void SetupVFlux();
void SetupPostProcessingLists();
void setCpuMode(int cpu_mode);
void setUiTheme(int theme);
void setTranslation(int idx);
void setTexCacheMode(int mode);
void setOverlay(int idx, Overlay *p);
bool setPostProcessingEffect(int idx, PostProcessingEffect *p);
void stripGameName(char *name);
void showDialog(char *text, void (*yes_func)(), void (*no_func)(), int type, char *args);
void getDialogTextResult(char *text);
void showAlert(char *text, int type);
void queueDownload(char *text, char *url, int size, void (*post_func)(), int type);
void reloadFont();
void resetRomList();

void extract_file(char *file, char *dir);
int download_file(char *url, char *file, char *msg, float int_total_bytes, bool has_temp_file);
void sort_overlaylist(Overlay *start);
void sort_shaderlist(PostProcessingEffect *start);
void set_net_folder();

void log2file(const char *format, ...);
void dump2file(void *ptr, uint32_t size, char *filename);

void dummy_func();

int update_button(ButtonSce* btn, const SceCtrlData* pad, uint32_t ticks);
