cmake_minimum_required(VERSION 3.24)
project(OpenECE VERSION 0.2.1 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(OPENECE_BUILD_GUI "Build the Qt desktop workbench" ON)
option(OPENECE_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
include(CTest)

# Project flags are private to our targets, never imposed on dependencies/consumers.
add_library(openece_options INTERFACE)
target_compile_options(openece_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)
if(OPENECE_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "Sanitizer configuration currently supports GCC and Clang")
    endif()
    target_compile_options(openece_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(openece_options INTERFACE -fsanitize=address,undefined)
endif()

add_subdirectory(core)
add_subdirectory(signals)
add_subdirectory(dsp)
if(OPENECE_BUILD_GUI)
    add_subdirectory(gui)
endif()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
