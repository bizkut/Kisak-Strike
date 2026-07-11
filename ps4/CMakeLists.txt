cmake_minimum_required(VERSION 3.16)
project(kisak_ps4_bootstrap C)

option(KISAK_PS4_MONOLITHIC "Link registered Source modules and call LauncherMain directly" OFF)

if(NOT ORBIS)
    message(FATAL_ERROR "Configure with -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/OpenOrbis.cmake")
endif()
if(NOT EXISTS "${OPENORBIS_ROOT}/link.x")
    message(FATAL_ERROR "OpenOrbis linker script is missing: ${OPENORBIS_ROOT}/link.x")
endif()

add_executable(kisak_ps4_bootstrap bootstrap_main.c)
set_target_properties(kisak_ps4_bootstrap PROPERTIES OUTPUT_NAME kisak_ps4_bootstrap SUFFIX "")
target_compile_definitions(kisak_ps4_bootstrap PRIVATE ORBIS PLATFORM_PS4 PLATFORM_CONSOLE)
if(KISAK_PS4_MONOLITHIC)
    target_compile_definitions(kisak_ps4_bootstrap PRIVATE KISAK_PS4_MONOLITHIC)
endif()
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
target_link_options(kisak_ps4_bootstrap PRIVATE
    -m elf_x86_64
    -pie
    "--script=${OPENORBIS_ROOT}/link.x"
    --eh-frame-hdr
)
target_link_directories(kisak_ps4_bootstrap PRIVATE "${OPENORBIS_ROOT}/lib")
target_link_libraries(kisak_ps4_bootstrap PRIVATE
    "${OPENORBIS_ROOT}/lib/crt1.o"
    "${OPENORBIS_ROOT}/lib/crti.o"
    c
    kernel
    "${OPENORBIS_ROOT}/lib/crtn.o"
)

find_program(CREATE_FSELF
    NAMES create-fself-macos create-fself
    HINTS "${OPENORBIS_ROOT}/bin/macos" "${OPENORBIS_ROOT}/bin"
)
if(CREATE_FSELF)
    add_custom_command(TARGET kisak_ps4_bootstrap POST_BUILD
        COMMAND "${CREATE_FSELF}"
            -in=$<TARGET_FILE:kisak_ps4_bootstrap>
            -out=${CMAKE_CURRENT_BINARY_DIR}/kisak_ps4_bootstrap.oelf
            -eboot=${CMAKE_CURRENT_BINARY_DIR}/eboot.bin
            --paid 0x3800000000000011
        BYPRODUCTS
            ${CMAKE_CURRENT_BINARY_DIR}/kisak_ps4_bootstrap.oelf
            ${CMAKE_CURRENT_BINARY_DIR}/eboot.bin
        VERBATIM
    )
else()
    message(WARNING "create-fself was not found; the ELF will build without eboot.bin conversion")
endif()
