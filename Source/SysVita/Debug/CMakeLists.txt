iadd_library(Debug_Vita OBJECT 
DaedalusAssertVita.cpp 
DBGConsoleVita.cpp 
)

if(DAEDALUS_PROFILE_DEBUG_VITA)
target_compile_options(Debug_Vita PRIVATE -pg)
endif(DAEDALUS_PROFILE_DEBUG_VITA)

list(APPEND daed_libs "Debug_Vita")
set(daed_libs ${daed_libs} PARENT_SCOPE)
