add_subdirectory(Debug)
add_subdirectory(HLEGraphics)
add_subdirectory(Utility)


list(APPEND sys_libraries "dl")
set(sys_libraries ${sys_libraries} PARENT_SCOPE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/../daedalus DESTINATION DaedalusX64
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)

        if(DAEDALUS_GL)
install(FILES ${PROJECT_SOURCE_DIR}/Source/SysGL/HLEGraphics/n64.psh DESTINATION DaedalusX64)
        endif(DAEDALUS_GL)

        if(DAEDALUS_GLES)
install(FILES ${PROJECT_SOURCE_DIR}/Source/SysGLES/HLEGraphics/n64.psh DESTINATION DaedalusX64)
        endif(DAEDALUS_GLES)

install(FILES ${PROJECT_BINARY_DIR}/../Data/roms.ini DESTINATION DaedalusX64)
install(FILES ${PROJECT_BINARY_DIR}/../Data/readme.txt DESTINATION DaedalusX64)
install(FILES ${PROJECT_BINARY_DIR}/../Data/copying.txt DESTINATION DaedalusX64)

set(daed_libs ${daed_libs} PARENT_SCOPE)
set(plat_main SysPosix/main.cpp PARENT_SCOPE)
