cmake_minimum_required(VERSION 4.1)
project(nuke_lol)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")

add_executable(nuke_lol src/main.cpp
        src/efi.h)
# add_library(nuke_lol OBJECT main.cpp)

# for idiots (me)
add_custom_command(TARGET nuke_lol POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rename
        $<TARGET_FILE:nuke_lol>
        $<TARGET_FILE:nuke_lol>.danger
)