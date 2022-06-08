cmake_minimum_required (VERSION 2.8)

project(TextWindows)

configure_file(src/windows.h.in ${PROJECT_BINARY_DIR}/windows.h)

include_directories(${PROJECT_BINARY_DIR})
include_directories(src)
add_compile_options(-O0 -g3 -W -Wall -Wno-unused-function 
    -Wno-unused-parameter -Wno-unused-variable -Wno-write-strings
    -Wno-implicit-fallthrough -Wno-delete-non-virtual-dtor)

add_library(windows SHARED ${C_SOURCES} src/windows.cpp)
add_library(appbase SHARED ${C_SOURCES} src/appbase.cpp)

Set (CPP_SOURCES
        src/screen.cpp
        src/main.cpp
    )

link_libraries(pthread windows) 
add_executable(textwindows ${CPP_SOURCES})
