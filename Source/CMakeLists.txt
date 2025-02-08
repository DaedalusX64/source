	# DaedalusX64 CMake Build File 
# Created and Maintained by Wally 
# Yes right now this is a mess, hoping to simplify things further in future but please deal with it for now :)

set(CMAKE_CXX_STANDARD 23)

# Below is related to Macros within the project, a lot of these could be deprecated and deleted.
# For now they will remain here so they're visible and can be switched on and off as desired.

#TODO: Separate SDL Options
list(APPEND plat_main )

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    	message(STATUS "GCC detected: ${CMAKE_CXX_COMPILER_VERSION}")
    
    	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13.0")
        	message(STATUS "GCC version is older than 13. Using LibFormat.")
        	message("GCC_LEGACY=ON")
    		option(GCC_LEGACY "Use LibFormat" ON)
    	endif()
endif()


## if DEBUG is added to the build_daedalus script it will execute teh Debug section otherwise by default do release.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	message("Debug Build")

	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
	add_compile_definitions(DAEDALUS_CONFIG_VERSION="Dev") ## Show the build type in the UI
	option(DAEDALUS_ENABLE_ASSERTS "Enable Static Asserts" ON)
	option(DAEDALUS_DEBUG_CONSOLE "Enable Debug Console" ON)
	option(DAEDALUS_DEBUG_DISPLAYLIST "Options for debugging the display list, useful for determining texture issues" OFF)
	option(DAEDALUS_LOG "Enable various logging" ON)
	option(DAEDALUS_SILENT "Shhh, no debug console message, on by default" OFF)
if(NOT WIN32)
	add_compile_options("-Wall")
	add_compile_options("-Wextra")
endif()
else()
	message("Release Build")
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	add_compile_definitions(DAEDALUS_CONFIG_VERSION="Release") ## Show the build type in the UI
	option(DAEDALUS_SILENT "Shhh, no debug console message, on by default" ON)

	option(DAEDALUS_DEBUG_CONSOLE "Enable Debug Console" OFF)
	message (${CMAKE_CXX_FLAGS_RELEASE})
	if(DAEDALUS_CTR)
	set( CMAKE_CXX_FLAGS_RELEASE  "-O3 -DNDEBUG")
	endif()
	endif()
	#Determine Dynarec compatibility and link in appropriate Dynarec
	#Todo: Move all relevant Dynarec to the Dynarec/arch folder and enable here when correct platform is detected
	if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm64" )
		option(DAEDALUS_ENABLE_DYNAREC  "Enable Dynarec" OFF)
		message("No Dynarec on arm64 yet")
	elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "mips" )
		option(DAEDALUS_ENABLE_DYNAREC "Enable Dynarec" ON)
		set(DYNAREC DynaRec) #Enable Dynarec Linker Flag
	elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86")
		option(DAEDALUS_ENABLE_DYNAREC "Enable Dynarec" OFF)
		## X86 Dynarec Code needs to be moved from W32 emulator
	elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "armv")
		message("ARM Dynarec Enabled")
		option(DAEDALUS_ENABLE_DYNAREC "Enable Dynarec" ON)
		set(DYNAREC DynaRec) #Enable Dynarec Linker Flag
	elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
		message("x86_64 Dynarec Enabled")
		option(DAEDALUS_ENABLE_DYNAREC "Enable Dynarec" ON)
	elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
		message("AMD64 Dynarec Enabled")
		option(DAEDALUS_ENABLE_DYNAREC "Enable Dynarec" ON)
	elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "riscv64")
		message("No Dynarec avaliable for Riscv64")
		option(DAEDALUS_ENABLE_DYNAREC "Enable Dynarec" OFF)
	elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm64")
		message("No Dynarec available for arm64")
		option(DAEDALUS_ENABLE_DYNAREC "Enable Dynarec" OFF)
	else()
		message("Platform unknown, Dynarec disabled")
		option(DAEDALUS_ENABLE_DYNAREC "Enable Dynarec" OFF)
	endif()
	
	#Check the Endian of the host platform and apply endian rules. 
	include (TestBigEndian)
	TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
	if(IS_BIG_ENDIAN)
		message(STATUS "Big Endian Detected")
		option(DAEDALUS_ENDIAN_BIG "Big Endian Processor" ON)
	else()
		message(STATUS "Little Endian Detected")
		option(DAEDALUS_ENDIAN_LITTLE "Litle Endian Processor" ON)
	endif()

	# General options

	option(DAEDALUS_BATCH_TEST_ENABLED "Enable batch testing for compatibility" OFF)

	option(DAEDALUS_COMPRESSED_ROM_SUPPORT "Enable Compressed ROM Support" OFF) ## Probably should be a default option. 
	option(DAEDALUS_LOG "Log stuff " OFF) ## NEeds to be off on the PSP unless debugging

	# Additional Debug Options
	option(SHOW_MEM "Show memory usage" OFF)
	option(DAEDALUS_DEBUG_MEMORY "Debug memory issues" OFF)
	option(DAEDALUS_DEBUG_PIF "Enable Debugging Options for Peripheral Interface (PIF)" OFF)
	option(DAEDALUS_DEBUG_DYNAREC "Enable Various Debugging options for the Dynarec" OFF)
	option(LOG_ABORTED_TRACES "Log aborted DynaRec Traces to file" OFF)
	option(DAEDALUS_ENABLE_SYNCHRONISATION "Enable for sync testing" OFF)
	option(DAEDALUS_ENABLE_PROFILING "Enable built in profiler" OFF)
	option(DAEDALUS_PROFILE_EXECUTION "Keep track of various execution stats" OFF)
	option(DUMPOSFUNCTIONS "Dump OSHLE Functions" OFF)
	option(DAEDALUS_FRAMERATE_ANALYSIS "Output a CSV file containing framerate analysis" OFF)
	option(DAEDALUS_DEBUG_AUDIO "Output some Audio Samples to raw file" OFF)


	# Handle specific options (These were all found in platforms.h and now belong here)
	# PSP / Vita / CTR options are required as non-linux embedded platforms are not able to be reliably detected.
	# Windows, Linux and macOS builds are auto detected.


	if(PSP) 
		message("PSP")
		option(DAEDALUS_DIALOGS "Enable UI Dialogs" OFF)
		option(DAEDALUS_ENABLE_DYNAREC "Enable Dynamic Recompilation, Disable if system not supported" ON)
		option(DAEDALUS_PSP "PSP specific Options" ON)
		option(DAEDALUS_PSP_USE_ME "Use the Media Engine" ON)
		option(DAEDALUS_PSP_USE_VFPU "Enable VFPU Instructions" ON)
		option(DAEDALUS_SIM_DOUBLES "Simulate Doubles" ON)
		option(INTRAFONT "Use Native Fonts" ON) #If this is off, SDL will be enabled
		option(DAEDALUS_SDL "Enable SDL" OFF)
		option(DAEDALUS_ENABLE_OS_HOOKS "Enable High Level Emulation Hooks" ON) 

	elseif(UNIX)
		if ((${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64" AND CMAKE_SYSTEM_NAME STREQUAL "Linux") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv" AND CMAKE_SYSTEM_NAME STREQUAL "Linux"))
			option(DAEDALUS_GLES "OpenGLES 3.1 Renderer" ON)
		else()
		option(DAEDALUS_GL "OpenGL Renderer" ON)
		endif()
		option(DAEDALUS_SDL "SDL Build" ON)
		option(DAEDALUS_POSIX "Posix specific options" ON)
		option(DAEDALUS_ACCURATE_TMEM "Accurate TMEM" ON)
		option(DAEDALUS_128BIT_MULT "128 Bit Multiplication, not supported on PSP" ON)
		option(DAEDALUS_ACCURATE_CVT "Convert using the rounding mode specified in the Floating Control/Status register (FCSR)" ON)

		

	elseif(WIN32)
		option(DAEDALUS_SDL "SDL Build" ON)
		option(DAED_USE_VIRTUAL_ALLOC "Use Windows Memory specifiers" ON) # We probably need to remove and simplfy this using modern C++
		option(DAEDALUS_ACCURATE_CVT "Convert using the rounding mode specified in the Floating Control/Status register (FCSR)" ON)
		option(DAEDALUS_W32 "Windows specific Options" ON)
		option(DAEDALUS_ACCURATE_TMEM "Accurate TMEM" ON)
		option(DAEDALUS_BREAKPOINTS_ENABLED "Enable Breakpoints" OFF)
		option(DAEDALUS_GL "OpenGL Renderer" ON)
		enable_language(CXX ASM_MASM)
	elseif(CTR)
		option(DAEDALUS_ENABLE_OS_HOOKS "Enable High Level Emulation Hooks" OFF) ## Causes some weird black screen issues
		option(DAEDALUS_CTR "3DS Specific Options" ON)
	endif()

# Profiler Options

	option(DAEDALUS_PROFILE_AUDIOPLUGIN "" OFF)
	option(DAEDALUS_PROFILE_CORE "" OFF)
	option(DAEDALUS_PROFILE_DEBUG "" OFF)
	option(DAEDALUS_PROFILE_DYNAREC "" OFF)
	option(DAEDALUS_PROFILE_GRAPHICS "" OFF)
	option(DAEDALUS_PROFILE_HLEAUDIO "" OFF)
	option(DAEDALUS_PROFILE_HLEGRAPHICS "" OFF)
	option(DAEDALUS_PROFILE_INPUT "" OFF)
	option(DAEDALUS_PROFILE_INTERFACE "" OFF)
	option(DAEDALUS_PROFILE_MATH "" OFF)
	option(DAEDALUS_PROFILE_OSHLE "" OFF)
	option(DAEDALUS_PROFILE_ROMFILE "" OFF)
	option(DAEDALUS_PROFILE_SYSTEM "" OFF)
	option(DAEDALUS_PROFILE_UI "" OFF)
	option(DAEDALUS_PROFILE_UTILITY "" OFF)

	option(DAEDALUS_PROFILE_UTILITY_PSP "" OFF)
	option(DAEDALUS_PROFILE_HLEGRAPHICS_PSP "" OFF)
	option(DAEDALUS_PROFILE_GRAPHICS_PSP "" OFF )
	option(DAEDALUS_PROFILE_DEBUG_PSP "" OFF)

	option(DAEDALUS_PROFILE_GRAPHICS_CTR "" OFF)
	option(DAEDALUS_PROFILE_HLEGRAPHICS_CTR "" OFF)
	option(DAEDALUS_PROFILE_UTILITY_CTR "" OFF)
	option(DAEDALUS_PROFILE_UI_CTR "" OFF)

	option(DAEDALUS_PROFILE_GRAPHICS_GL "" OFF)
	option(DAEDALUS_PROFILE_UTILITY_POSIX "" OFF)

	if(DAEDALUS_GPROF)
	link_libraries(-pg)
	endif()

	# Needed below the options to post process
	include(${PROJECT_SOURCE_DIR}/Options.cmake) 

	#Setup directories to include
	list(APPEND include_dirs "${PROJECT_SOURCE_DIR}/Source")
	include_directories( BEFORE ${include_dirs})


if(UNIX)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
if(NOT APPLE)
	add_link_options("-Wl,-z,noexecstack")
endif()
endif(UNIX)

if (APPLE)
	list(APPEND sys_libraries "-framework OpenGL -framework AudioToolbox -framework Cocoa -framework IOKit -framework CoreVideo")
	## Homebrew uses a separate path on Apple Silicon
	##Ugh is there a better way??
	if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm64" )
		include_directories(/opt/homebrew/include)
		link_directories(/opt/homebrew/lib)
	else()
		include_directories(/usr/local/include)
		link_directories(/usr/local/lib)
	endif()
endif (APPLE)
if(CTR)
	include_directories($ENV{DEVKITPRO}/portlibs/3ds/include)
	include_directories($ENV{DEVKITPRO}/picaGL/include)
	link_directories($ENV{DEVKITPRO}/picaGL/lib)
	include_directories($ENV{DEVKITPRO}/portlibs/3ds/lib)
endif(CTR)

if(DAEDALUS_SDL)
	find_package(SDL2 CONFIG REQUIRED)

	if(GCC_LEGACY)
	find_package(fmt REQUIRED)
	endif(GCC_LEGACY)

	link_libraries(SDL2::SDL2)
	link_libraries(${SDL2_LIBRARIES})

	if(NOT PSP)
		# PSP is not needed as it uses built-in native font
		find_package(SDL2_ttf QUIET)
		if(SDL2_ttf_FOUND)
			link_libraries(SDL2_ttf::SDL2_ttf)
		else()
			PKG_SEARCH_MODULE(SDL2_TTF REQUIRED SDL2_ttf>=2.0.0)
			link_libraries(${SDL2_TTF_LIBRARIES})
		endif()
	endif()
endif(DAEDALUS_SDL)

if(DAEDALUS_GLES)
	find_package(OpenGL REQUIRED)
	link_libraries(OpenGL::GL)
endif(DAEDALUS_GLES)

if(DAEDALUS_GL)
	find_package(OpenGL REQUIRED)
	link_libraries(OpenGL::GL)

	find_package(GLEW REQUIRED)
	link_libraries(GLEW::GLEW)
endif(DAEDALUS_GL)

find_package(PNG REQUIRED)
link_libraries(PNG::PNG)


find_package(ZLIB REQUIRED)
link_libraries(ZLIB::ZLIB)

		## Daedalus Core Build
		add_subdirectory(Core)
		add_subdirectory(Debug)
		add_subdirectory(DynaRec)
		add_subdirectory(Graphics)
		add_subdirectory(HLEAudio)
		add_subdirectory(HLEGraphics)
		add_subdirectory(Interface)
		add_subdirectory(Input)
		add_subdirectory(Math)
		add_subdirectory(OSHLE)
		add_subdirectory(RomFile)
		add_subdirectory(System)
		add_subdirectory(Utility)
	if(NOT CTR)
		add_subdirectory(UI)
	endif()
	if(UNIX)
		add_subdirectory(SysPosix)
		
		if(DAEDALUS_GL)
		add_subdirectory(SysGL)
		endif(DAEDALUS_GL)

		if(DAEDALUS_GLES)
		add_subdirectory(SysGLES)
		endif(DAEDALUS_GLES)

	elseif(WIN32) ## Probably keep this generic as we'll build this on Linux too?
		add_subdirectory(SysGL)
		add_subdirectory(SysW32)
	elseif(CTR)
		add_subdirectory(SysCTR)
	elseif(PSP)
		add_subdirectory(SysPSP)
	endif()
	
	# ---  Build ---- 
	add_executable(daedalus ${plat_main})
	
	if(GCC_LEGACY)
	target_link_libraries(daedalus PRIVATE ${daed_libs} ${default_libraries} ${sys_libraries} fmt::fmt)
	else()
	target_link_libraries(daedalus PRIVATE ${daed_libs} ${default_libraries} ${sys_libraries} )
	endif(GCC_LEGACY)
	
	# --- Post Build ---
	## PRX Builds add about 600kb to the binary as they're not stripped at this point. Maybe we 
	if(PSP)
		create_pbp_file(
			TARGET daedalus
			PREVIEW_PATH NULL
			ICON_PATH ${PROJECT_SOURCE_DIR}/Source/SysPSP/Resources/eboot_icons/icon0.png
			BACKGROUND_PATH ${PROJECT_SOURCE_DIR}/Source/SysPSP/Resources/eboot_icons/pic1.png
			TITLE DaedalusX64
			BUILD_PRX
			)
	endif(PSP)

	if(CTR)
    set(APP_TITLE "DaedalusX64")
    set(APP_DESCRIPTION "DaedalusX64 port for 3DS")
    set(APP_AUTHOR "MasterFeizz / DaedalusX64 Team")

    set(APP_SMDH "${PROJECT_SOURCE_DIR}/Source/SysCTR/Resources/daedalus.smdh")
    set(APP_BANNER "${PROJECT_SOURCE_DIR}/Source/SysCTR/Resources/banner.bnr")
    set(APP_AUDIO "${PROJECT_SOURCE_DIR}/Source/SysCTR/Resources/audio_silent.wav")
    set(APP_RSF "${PROJECT_SOURCE_DIR}/Source/SysCTR/Resources/template.rsf")
    set(APP_ROMFS_DIR "${PROJECT_SOURCE_DIR}/Source/SysCTR/Resources/romfs")
    set(APP_ROMFS_BIN "${PROJECT_SOURCE_DIR}/Source/SysCTR/Resources/romfs.bin")

    add_custom_command(
		TARGET daedalus
		POST_BUILD COMMAND
		3dsxtool daedalus.elf DaedalusX64.3dsx --romfs=${APP_ROMFS_DIR} --smdh=${APP_SMDH}
       COMMENT "Calling 3dsxtool"
    )

    add_custom_command(
		TARGET daedalus
		POST_BUILD COMMAND
       		makerom -f cia
            -target t
            -exefslogo
            -o ${CMAKE_CURRENT_BINARY_DIR}/DaedalusX64.cia
            -elf daedalus.elf
            -rsf ${APP_RSF}
            -banner ${APP_BANNER}
            -icon ${APP_SMDH}
            -romfs ${APP_ROMFS_BIN}
			COMMENT "Calling makerom"
    )

endif(CTR)
	
	## Install and setup basic directories
	install(DIRECTORY ${PROJECT_BINARY_DIR}/../Data/ DESTINATION DaedalusX64)
	install(DIRECTORY DESTINATION DaedalusX64/Roms)
	install(DIRECTORY DESTINATION DaedalusX64/SaveGames)
	install(DIRECTORY DESTINATION DaedalusX64/SaveStates)
