cmake_minimum_required (VERSION 2.8)

project(test)


include_directories(src)
include_directories(/home/szx/TextWindows/build)
link_directories(/home/szx/TextWindows/build)
add_compile_options(-O0 -g3 -W -Wall -Wno-unused-function 
    -Wno-unused-parameter -Wno-unused-variable -Wno-write-strings
    -Wno-implicit-fallthrough)

Set (CPP_SOURCES
        main.cpp
    )

link_libraries(pthread windows appbase) 
add_executable(test ${CPP_SOURCES})
