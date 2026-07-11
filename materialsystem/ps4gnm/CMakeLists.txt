cmake_minimum_required(VERSION 3.16)
project(shaderapips4_client)

set(SRCDIR "${CMAKE_SOURCE_DIR}")
set(CMAKE_MODULE_PATH ${SRCDIR}/cmake)
set(OUTBINNAME "shaderapips4_client")
set(OUTBINDIR ${SRCDIR}/../game/bin)

include(${CMAKE_MODULE_PATH}/detect_platform.cmake)
include(${CMAKE_MODULE_PATH}/source_dll_base.cmake)

target_include_directories(${OUTBINNAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${KISAK_OPENGNM_ROOT}/include
)
target_compile_definitions(${OUTBINNAME} PRIVATE SHADER_DLL_EXPORT PROTECTED_THINGS_ENABLE)
target_sources(${OUTBINNAME} PRIVATE
    ps4_gnm_device.cpp
    ps4_gnm_draw_state.cpp
    ps4_gnm_memory.cpp
    ps4_shader_manifest.cpp
    ps4_shadow_state_translate.cpp
    ps4_gnm_texture.cpp
    shaderapips4.cpp
)
