file(GLOB SRC_FILES Graphics/*.cpp HLEGraphics/*.cpp)

add_library(SysGLES OBJECT ${SRC_FILES})

target_include_directories(SysGLES PUBLIC ${OPENGLES_INCLUDE_DIR})
target_include_directories(SysGLES PUBLIC ${SDL2_INCLUDE_DIRS})
target_include_directories(SysGLES PUBLIC ${SDL2_TTF_INCLUDE_DIRS})

install(FILES ${PROJECT_SOURCE_DIR}/Source/SysGLES/HLEGraphics/n64.psh DESTINATION DaedalusX64)

if(DAEDALUS_PROFILE_GRAPHICS_GL)
target_compile_options(SysGLES PRIVATE -pg)
endif(DAEDALUS_PROFILE_GRAPHICS_GL)

list(APPEND daed_libs "SysGLES")
set(daed_libs ${daed_libs} PARENT_SCOPE)
