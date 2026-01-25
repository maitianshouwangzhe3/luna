cmake_minimum_required(VERSION 3.14)

project(luna CXX)

set(CMAKE_CXX_STANDARD 17)

option(BUILD_STATIC_LIBS "Build static library version" ON)
option(BUILD_SHARED_LIBS "Build shared library version" ON)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/lua_archiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/luna.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lz4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/var_int.cpp
)

if(BUILD_STATIC_LIBS)
    add_library(luna_static STATIC ${SOURCES})
    set_target_properties(luna_static PROPERTIES OUTPUT_NAME lua PREFIX "lib")
endif()

if(BUILD_SHARED_LIBS)
    add_library(luna_shared SHARED ${SOURCES})
    set_target_properties(luna_shared PROPERTIES OUTPUT_NAME lua PREFIX "lib")
endif()

include(GNUInstallDirs)

install(TARGETS luna_shared
    RUNTIME DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS luna_static
    RUNTIME DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES
    luna.h
    lua_archiver.h
    var_int.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)