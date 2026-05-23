cmake_minimum_required(VERSION 3.16)

set(target_name "zcheckers")

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(ZOS_TOOLCHAIN sdcc)
include($ENV{ZOS_PATH}/cmake/zos_init.cmake)

zos_use(zvb)
find_package(ZVB_SDK REQUIRED)

zos_use(coreutils)
find_package(COREUTILS REQUIRED)

zos_use(zgdk)
find_package(ZGDK REQUIRED)

project(${target_name} C ASM)

add_executable(${target_name}
    src/main.c
    src/input.c
    src/assets.c
    src/render.c
    src/checkers.c
    src/audio.c
    src/splash.c
)

# Prevent linking against stdlib, matching the existing Zeal workflow.
target_compile_options(${target_name} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:--nostdlib>
)

target_link_libraries(${target_name} PUBLIC core)
target_link_libraries(${target_name} PUBLIC zvb_gfx zvb_sound)
target_link_libraries(${target_name} PUBLIC zgdk)

gif2zeal(${target_name} FILES
    "assets/checkers.gif"
    STRIP 160 # last 10 rows of 256x256
    BIT 4
    COLORS 16
)

tiled2zeal(${target_name} FILES
    "assets/checkers.tmx"
)

copy_assets(${target_name} FILES
    "assets/checkers.zts"
    "assets/checkers.ztp"
)

zgdk_add_outputs(${target_name})
