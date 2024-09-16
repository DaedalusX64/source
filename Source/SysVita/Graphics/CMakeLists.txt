add_library(Graphics_Vita OBJECT 
                            GraphicsContextVita.cpp 
                            NativeTextureVita.cpp
            )

if(DAEDALUS_PROFILE_DEBUG_PSP)
    target_compile_options(Debug_PSP PRIVATE -pg)
endif(DAEDALUS_PROFILE_DEBUG_PSP)
            
list(APPEND daed_libs "Debug_PSP")
set(daed_libs ${daed_libs} PARENT_SCOPE)
