if(DAEDALUS_PSP)
add_library(Input OBJECT PSP/InputManagerPSP.cpp)
elseif(DAEDALUS_CTR)
add_library(Input OBJECT CTR/InputManagerCTR.cpp)
elseif(DAEDALUS_VITA)
add_library(Input OBJECT Vita/InputManagerVita.cpp)
else()
add_library(Input OBJECT SDL/InputManagerSDL.cpp)
endif()

if(DAEDALUS_PROFILE_INPUT)
target_compile_options(Input PRIVATE -pg)
endif(DAEDALUS_PROFILE_INPUT)

list(APPEND daed_libs "Input")
set(daed_libs ${daed_libs} PARENT_SCOPE)
             