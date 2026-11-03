cmake_minimum_required(VERSION 3.20)
project(nesrecompiler LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(SDL2 CONFIG QUIET)

if (NOT SDL2_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.9
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL2)
endif()

add_library(nes_runtime STATIC
    runtime/cpu.c
    runtime/bus.c
    runtime/ppu.c
    runtime/input.c
    runtime/interp.c
    runtime/mapper0.c
)
target_include_directories(nes_runtime PUBLIC runtime)

add_executable(nes_window runtime/main.c)
if (MSVC)
    target_link_options(nes_window PRIVATE "/SUBSYSTEM:CONSOLE")
endif()
if (TARGET SDL2::SDL2-static)
    target_link_libraries(nes_window PRIVATE nes_runtime SDL2::SDL2-static)
elseif (TARGET SDL2::SDL2)
    target_link_libraries(nes_window PRIVATE nes_runtime SDL2::SDL2)
else()
    message(FATAL_ERROR "SDL2 was found but did not provide an SDL2 target")
endif()

if (EXISTS "${CMAKE_SOURCE_DIR}/build/game.c")
    add_executable(game runtime/main.c build/game.c)
    target_compile_definitions(game PRIVATE NES_RECOMPILED)
    if (MSVC)
        target_link_options(game PRIVATE "/SUBSYSTEM:CONSOLE")
    endif()
    target_link_libraries(game PRIVATE nes_runtime)
    if (TARGET SDL2::SDL2-static)
        target_link_libraries(game PRIVATE SDL2::SDL2-static)
    elseif (TARGET SDL2::SDL2)
        target_link_libraries(game PRIVATE SDL2::SDL2)
    endif()
    if (TARGET SDL2::SDL2main)
        target_link_libraries(game PRIVATE SDL2::SDL2main)
    endif()
endif()

add_executable(nestest_runner runtime/nestest_runner.c)
target_link_libraries(nestest_runner PRIVATE nes_runtime)

add_executable(m3_runtime_test tests/m3_runtime_test.c)
target_link_libraries(m3_runtime_test PRIVATE nes_runtime)

add_executable(m6_runtime_test tests/m6_runtime_test.c)
target_link_libraries(m6_runtime_test PRIVATE nes_runtime)

if (TARGET SDL2::SDL2main)
    target_link_libraries(nes_window PRIVATE SDL2::SDL2main)
endif()
