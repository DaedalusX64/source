add_library(Utility_Vita OBJECT 
             ThreadVita.cpp
            )

if(DAEDALUS_PROFILE_UTILITY_VITA)
    target_compile_options(Utility_Vita PRIVATE -pg)
endif(DAEDALUS_PROFILE_UTILITY_VITA)
            
list(APPEND daed_libs "Utility_Vita")
set(daed_libs ${daed_libs} PARENT_SCOPE)
