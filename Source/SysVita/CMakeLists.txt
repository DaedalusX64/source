add_subdirectory(Debug)
add_subdirectory(Graphics)
# add_subdirectory(HLEAudio) # Use SDL Audio
add_subdirectory(HLEGraphics)
add_subdirectory(Utility)



list(APPEND daed_libs)
set(daed_libs ${daed_libs} PARENT_SCOPE)

list(APPEND sys_libraries SDL2_ttf freetype harfbuzz bz2)

set(sys_libraries ${sys_libraries} PARENT_SCOPE)

set(plat_main SysVita/main.cpp PARENT_SCOPE)
