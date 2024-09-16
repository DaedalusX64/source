add_library(UI_Vita OBJECT 
                      DownloaderScreen.cpp
                      InGameMenuScreen.cpp
                      MainMenuScreen.cpp
                      MenuBarScreen.cpp
                      TranslationStrings.cpp
            )

if(DAEDALUS_PROFILE_UI_VITA)
    target_compile_options(UI_Vita PRIVATE -pg)
endif(DAEDALUS_PROFILE_UI_VITA)
            
list(APPEND daed_libs "UI_Vita")
set(daed_libs ${daed_libs} PARENT_SCOPE)
