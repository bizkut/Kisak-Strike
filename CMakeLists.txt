# Kisak-Strike: Gentoo Offensive
# Main CMake File

# List of targets from VPC
#                        Dmserializers*         Matchmaking_DS_CSGO*   ServerBrowser*         tier3*
#                        Dmxloader*             materialsystem*        Server_CSGO*           Unitlib*
# appframework*          engine*                mathlib*               shaderapiempty*        valve_avi*
# bitmap*                engine_ds              mathlib_extended*      shaderlib*             vgui2*
# bitmap_byteswap*       Fgdlib*                matsys_controls*                              vgui_controls*
# bonesetup*             filesystem_stdio*      meshutils*                                    vguimatsurface*
# Bzip2*                 inputsystem*           particles*             soundemittersystem*    vgui_surfacelib*
# choreoobjects*         interfaces*            quickhull*             soundsystem_lowlevel*  videocfg*
#                        launcher*              Raytrace*              stdshader_dx9*         vpklib*
#                        launcher_main*                                studiorender*          vscript*
#                        localize*                                                            vstdlib*
# Client_CSGO*           matchmakingbase*       resourcefile*          tier0*                 vtf*
# datacache*             matchmakingbase_ds*    responserules_runtime* tier1*
# Dedicated*             Matchmaking_CSGO*      SceneFileCache*        tier2*

cmake_minimum_required(VERSION 3.16)
project(KisakStrike)

include(kisak-strike-build-options.cmake)

if(ORBIS OR CMAKE_SYSTEM_NAME STREQUAL "Orbis")
    set(ORBIS ON)
    set(STATIC_LINK ON)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS ON)
    add_compile_options(-mcx16)
    option(KISAK_PS4_ENABLE_SCALEFORM "Build the externally supplied Scaleform SDK integration" ON)
    set(KISAK_SCALEFORM_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../scaleform_sdk" CACHE PATH "External Scaleform 4.2 SDK root")
    if(KISAK_PS4_ENABLE_SCALEFORM)
        foreach(KISAK_SCALEFORM_REQUIRED_FILE
            "Include/GFx.h"
            "Include/GFxVersion.h"
            "Src/Kernel/SF_Types.h"
            "Src/Render/D3D9/D3D9_HAL.h")
            if(NOT EXISTS "${KISAK_SCALEFORM_SDK_ROOT}/${KISAK_SCALEFORM_REQUIRED_FILE}")
                message(FATAL_ERROR "Scaleform SDK is incomplete at ${KISAK_SCALEFORM_SDK_ROOT}: missing ${KISAK_SCALEFORM_REQUIRED_FILE}")
            endif()
        endforeach()

        file(STRINGS "${KISAK_SCALEFORM_SDK_ROOT}/Projects/libgfx.txt"
            KISAK_SCALEFORM_PROJECT_LINES)
        set(KISAK_SCALEFORM_KERNEL_SOURCES)
        foreach(KISAK_SCALEFORM_PROJECT_LINE IN LISTS KISAK_SCALEFORM_PROJECT_LINES)
            if(KISAK_SCALEFORM_PROJECT_LINE MATCHES "^Src/Kernel/.*\\.cpp$" AND
               NOT KISAK_SCALEFORM_PROJECT_LINE MATCHES "^Src/Kernel/HeapPT/" AND
               NOT KISAK_SCALEFORM_PROJECT_LINE MATCHES "(PS3|PSVita|Wii|WinAPI|WinRT|3DS).*\\.cpp$" AND
               EXISTS "${KISAK_SCALEFORM_SDK_ROOT}/${KISAK_SCALEFORM_PROJECT_LINE}")
                list(APPEND KISAK_SCALEFORM_KERNEL_SOURCES
                    "${KISAK_SCALEFORM_SDK_ROOT}/${KISAK_SCALEFORM_PROJECT_LINE}")
            endif()
        endforeach()
        if(NOT KISAK_SCALEFORM_KERNEL_SOURCES)
            message(FATAL_ERROR "Scaleform Kernel source inventory is empty")
        endif()

        add_library(kisak_scaleform_kernel STATIC ${KISAK_SCALEFORM_KERNEL_SOURCES})
        target_include_directories(kisak_scaleform_kernel PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${KISAK_SCALEFORM_SDK_ROOT}/Include"
            "${KISAK_SCALEFORM_SDK_ROOT}/Src")
        target_compile_definitions(kisak_scaleform_kernel PUBLIC
            SF_BUILD_STATICLIB
            SF_CONFIG_H="ps4/scaleform_orbis_config.h")
        set_target_properties(kisak_scaleform_kernel PROPERTIES
            CXX_STANDARD 11
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS ON)

        set(KISAK_SCALEFORM_ZLIB_ROOT "${KISAK_SCALEFORM_SDK_ROOT}/3rdParty/zlib-1.2.7")
        add_library(kisak_scaleform_zlib STATIC
            "${KISAK_SCALEFORM_ZLIB_ROOT}/adler32.c"
            "${KISAK_SCALEFORM_ZLIB_ROOT}/compress.c"
            "${KISAK_SCALEFORM_ZLIB_ROOT}/crc32.c"
            "${KISAK_SCALEFORM_ZLIB_ROOT}/deflate.c"
            "${KISAK_SCALEFORM_ZLIB_ROOT}/infback.c"
            "${KISAK_SCALEFORM_ZLIB_ROOT}/inffast.c"
            "${KISAK_SCALEFORM_ZLIB_ROOT}/inflate.c"
            "${KISAK_SCALEFORM_ZLIB_ROOT}/inftrees.c"
            "${KISAK_SCALEFORM_ZLIB_ROOT}/trees.c"
            "${KISAK_SCALEFORM_ZLIB_ROOT}/uncompr.c"
            "${KISAK_SCALEFORM_ZLIB_ROOT}/zutil.c")
        target_include_directories(kisak_scaleform_zlib PUBLIC
            "${KISAK_SCALEFORM_ZLIB_ROOT}")

        set(KISAK_SCALEFORM_RENDER_SOURCES)
        set(KISAK_SCALEFORM_GFX_SOURCES)
        foreach(KISAK_SCALEFORM_PROJECT_LINE IN LISTS KISAK_SCALEFORM_PROJECT_LINES)
            if(KISAK_SCALEFORM_PROJECT_LINE MATCHES "^Src/Render/.*\\.cpp$" AND
               NOT KISAK_SCALEFORM_PROJECT_LINE MATCHES "(PS3|PSVita|WinAPI|FT2|GXT|KTX|PVR).*\\.cpp$" AND
               EXISTS "${KISAK_SCALEFORM_SDK_ROOT}/${KISAK_SCALEFORM_PROJECT_LINE}")
                list(APPEND KISAK_SCALEFORM_RENDER_SOURCES
                    "${KISAK_SCALEFORM_SDK_ROOT}/${KISAK_SCALEFORM_PROJECT_LINE}")
            elseif(KISAK_SCALEFORM_PROJECT_LINE MATCHES "^Src/GFx/.*\\.cpp$" AND
                   NOT KISAK_SCALEFORM_PROJECT_LINE MATCHES "^Src/GFx/(AMP|IME)/" AND
                   NOT KISAK_SCALEFORM_PROJECT_LINE MATCHES "(PS3|PSVita|X360|Win32|Wii|3DS).*\\.cpp$" AND
                   EXISTS "${KISAK_SCALEFORM_SDK_ROOT}/${KISAK_SCALEFORM_PROJECT_LINE}")
                list(APPEND KISAK_SCALEFORM_GFX_SOURCES
                    "${KISAK_SCALEFORM_SDK_ROOT}/${KISAK_SCALEFORM_PROJECT_LINE}")
            endif()
        endforeach()

        file(STRINGS "${KISAK_SCALEFORM_SDK_ROOT}/Projects/libgfx_as2.txt"
            KISAK_SCALEFORM_AS2_PROJECT_LINES)
        set(KISAK_SCALEFORM_AS2_SOURCES)
        foreach(KISAK_SCALEFORM_PROJECT_LINE IN LISTS KISAK_SCALEFORM_AS2_PROJECT_LINES)
            if(KISAK_SCALEFORM_PROJECT_LINE MATCHES "^Src/GFx/(AS2/.*|GFx_AS2Support)\\.cpp$" AND
               EXISTS "${KISAK_SCALEFORM_SDK_ROOT}/${KISAK_SCALEFORM_PROJECT_LINE}")
                list(APPEND KISAK_SCALEFORM_AS2_SOURCES
                    "${KISAK_SCALEFORM_SDK_ROOT}/${KISAK_SCALEFORM_PROJECT_LINE}")
            endif()
        endforeach()
        list(APPEND KISAK_SCALEFORM_GFX_SOURCES
            "${KISAK_SCALEFORM_SDK_ROOT}/Src/GFx/AMP/Amp_ProfileFrame.cpp"
            "${KISAK_SCALEFORM_SDK_ROOT}/Src/GFx/AMP/Amp_ViewStats.cpp"
            "${KISAK_SCALEFORM_SDK_ROOT}/Src/GFx/IME/GFx_IMEManager.cpp")
        if(NOT KISAK_SCALEFORM_RENDER_SOURCES OR
           NOT KISAK_SCALEFORM_GFX_SOURCES OR
           NOT KISAK_SCALEFORM_AS2_SOURCES)
            message(FATAL_ERROR "Scaleform Render/GFx/AS2 source inventory is incomplete")
        endif()

        foreach(KISAK_SCALEFORM_TARGET IN ITEMS
            kisak_scaleform_render kisak_scaleform_gfx kisak_scaleform_as2)
            if(KISAK_SCALEFORM_TARGET STREQUAL "kisak_scaleform_render")
                set(KISAK_SCALEFORM_TARGET_SOURCES ${KISAK_SCALEFORM_RENDER_SOURCES})
            elseif(KISAK_SCALEFORM_TARGET STREQUAL "kisak_scaleform_gfx")
                set(KISAK_SCALEFORM_TARGET_SOURCES ${KISAK_SCALEFORM_GFX_SOURCES})
            else()
                set(KISAK_SCALEFORM_TARGET_SOURCES ${KISAK_SCALEFORM_AS2_SOURCES})
            endif()
            add_library(${KISAK_SCALEFORM_TARGET} STATIC ${KISAK_SCALEFORM_TARGET_SOURCES})
            target_include_directories(${KISAK_SCALEFORM_TARGET} PUBLIC
                "${CMAKE_CURRENT_SOURCE_DIR}"
                "${KISAK_SCALEFORM_SDK_ROOT}/Include"
                "${KISAK_SCALEFORM_SDK_ROOT}/Src"
                "${KISAK_SCALEFORM_ZLIB_ROOT}")
            target_compile_definitions(${KISAK_SCALEFORM_TARGET} PUBLIC
                SF_BUILD_STATICLIB
                SF_CONFIG_H="ps4/scaleform_orbis_config.h")
            target_compile_options(${KISAK_SCALEFORM_TARGET} PRIVATE
                -Wno-c++11-narrowing
                -Wno-deprecated-register)
            set_target_properties(${KISAK_SCALEFORM_TARGET} PROPERTIES
                CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS ON)
        endforeach()
        target_link_libraries(kisak_scaleform_render PUBLIC kisak_scaleform_kernel)
        target_link_libraries(kisak_scaleform_gfx PUBLIC
            kisak_scaleform_render kisak_scaleform_zlib)
        target_link_libraries(kisak_scaleform_as2 PUBLIC kisak_scaleform_gfx)
    endif()
    # The legacy Source ScaleformUI app-system has a very large D3D9/GL
    # surface and is not the PS4 entry point yet. The focused PS4 GFx manager
    # below links the SDK directly while the full interface is migrated to the
    # OpenGNM HAL.
    set(USE_SCALEFORM OFF CACHE BOOL "" FORCE)
    set(USE_ROCKETUI OFF CACHE BOOL "" FORCE)
    set(USE_KISAK_PHYSICS ON CACHE BOOL "" FORCE)
    set(NO_STEAM ON CACHE BOOL "Disable proprietary Steam runtime on PS4" FORCE)
    option(KISAK_PS4_BUILD_SOURCE_ENGINE
        "Compile the real Source engine as the next monolithic integration gate" ON)
    option(KISAK_PS4_BUILD_GAME_DLLS
        "Compile the CS:GO client and listen-server archives for PS4" ON)

    set(KISAK_OPENGNM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../opengnm" CACHE PATH "OpenGNM PS4 source/build root")

    add_subdirectory(interfaces)
    add_subdirectory(tier0)
    add_subdirectory(tier1)
    add_subdirectory(tier2)
    add_subdirectory(tier3)
    add_subdirectory(vstdlib)
    add_subdirectory(mathlib)
    add_subdirectory(vpklib)
    add_subdirectory(filesystem)
    add_subdirectory(inputsystem)
    add_subdirectory(ivp/havana/havok/hk_base)
    add_subdirectory(ivp/havana/havok/hk_math)
    add_subdirectory(ivp/havana/havok/hk_physics/constraint)
    add_subdirectory(ivp/ivp_compact_builder)
    add_subdirectory(ivp/ivp_physics)
    add_subdirectory(vphysics)
    add_subdirectory(bitmap)
    add_subdirectory(vtf)
    add_subdirectory(materialsystem/shaderlib)
    add_subdirectory(materialsystem/shaderapiempty)
    add_subdirectory(materialsystem/ps4gnm)
    add_subdirectory(materialsystem)
    add_subdirectory(datacache)
    add_subdirectory(studiorender)
    add_subdirectory(soundemittersystem)
    add_subdirectory(vscript)
    add_subdirectory(localize)
    add_subdirectory(dmxloader)
    add_subdirectory(vgui2/vgui_surfacelib)
    add_subdirectory(vgui2/src)
    add_subdirectory(vgui2/vgui_controls)
    add_subdirectory(vguimatsurface)
    add_subdirectory(appframework)
    add_subdirectory(launcher)
    if(KISAK_PS4_BUILD_SOURCE_ENGINE)
        set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
        add_subdirectory(thirdparty/protobuf-2.5.0/cmake)
        add_subdirectory(utils/bzip2)
        add_subdirectory(utils/jpeglib)
        add_subdirectory(videocfg)
        add_subdirectory(thirdparty/quickhull)
        set(DISABLE_ASM ON CACHE BOOL "Disable legacy Crypto++ inline assembly for OpenOrbis Clang" FORCE)
        add_subdirectory(external/crypto++-5.61)
        add_subdirectory(soundsystem/lowlevel)
        add_subdirectory(vgui2/matsys_controls)
        add_subdirectory(engine)
        if(KISAK_PS4_BUILD_GAME_DLLS)
            add_subdirectory(gcsdk)
            add_subdirectory(bonesetup)
            add_subdirectory(choreoobjects)
            add_subdirectory(meshutils)
            add_subdirectory(particles)
            add_subdirectory(raytrace)
            add_subdirectory(resourcefile)
            add_subdirectory(responserules/runtime)
            add_subdirectory(scenefilecache)
            add_subdirectory(game/client)
            add_subdirectory(game/server)
        endif()
    endif()

    file(READ "${OPENORBIS_ROOT}/link.x" KISAK_OPENORBIS_LINK_SCRIPT)
    string(REPLACE "\t.data.sce_process_param"
        "\t.kisak_ctors : {\n\t\t__kisak_ctors_start = .;\n\t\tKEEP(*(.ctors .ctors.*));\n\t\t__kisak_ctors_end = .;\n\t}\n\n\t.data.sce_process_param"
        KISAK_OPENORBIS_LINK_SCRIPT "${KISAK_OPENORBIS_LINK_SCRIPT}")
    set(KISAK_PS4_LINK_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/kisak-link.x")
    file(WRITE "${KISAK_PS4_LINK_SCRIPT}" "${KISAK_OPENORBIS_LINK_SCRIPT}")

    set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
    add_executable(kisak_ps4_monolithic
        ps4/bootstrap_main.c
        ps4/engine_bootstrap.cpp
        ps4/gnm_submission_bootstrap.cpp
        ps4/videoout_bootstrap.cpp
        ps4/scaleformui_bootstrap.cpp
        ps4/scaleform_asset_path.cpp
        ps4/offline_launch_request.cpp
        ps4/steam_api_stubs.cpp
        ps4/gameui_desktop_stubs.cpp
        ps4/scaleform_gfx_manager.cpp
        ps4/scaleform_gnm_hal.cpp
    )
    if(KISAK_PS4_ENABLE_SCALEFORM)
        target_sources(kisak_ps4_monolithic PRIVATE ps4/scaleform_sdk_probe.cpp)
        target_include_directories(kisak_ps4_monolithic PRIVATE
            "${KISAK_SCALEFORM_SDK_ROOT}/Include"
            "${KISAK_SCALEFORM_SDK_ROOT}/Src")
        target_compile_definitions(kisak_ps4_monolithic PRIVATE
            SF_BUILD_STATICLIB
            SF_CONFIG_H="ps4/scaleform_orbis_config.h")
        target_link_libraries(kisak_ps4_monolithic PRIVATE kisak_scaleform_as2)
    endif()
    set_target_properties(kisak_ps4_monolithic PROPERTIES
        LINKER_LANGUAGE CXX
        OUTPUT_NAME kisak_ps4_monolithic
        SUFFIX ""
    )
    target_compile_definitions(kisak_ps4_monolithic PRIVATE
        KISAK_PS4_MONOLITHIC
        ORBIS PLATFORM_PS4 PLATFORM_CONSOLE PLATFORM_64BITS
        POSIX _POSIX GNUC COMPILER_GCC STATIC_LINK
        CSTRIKE15 CSTRIKE_REL_BUILD RAD_TELEMETRY_DISABLED
    )
    target_include_directories(kisak_ps4_monolithic PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/public"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/inputsystem"
        "${CMAKE_CURRENT_SOURCE_DIR}/common"
        "${KISAK_OPENGNM_ROOT}/include"
    )
    target_link_options(kisak_ps4_monolithic PRIVATE
        -m elf_x86_64
        -pie
        "--script=${KISAK_PS4_LINK_SCRIPT}"
        --eh-frame-hdr
    )
    if(KISAK_PS4_BUILD_SOURCE_ENGINE)
        # Source's original DLLs intentionally repeat a small set of globals
        # and helper implementations. The monolithic PS4 image resolves those
        # to the first module in link order while PRX isolation is unavailable.
        target_link_options(kisak_ps4_monolithic PRIVATE --allow-multiple-definition)
    endif()
    target_link_directories(kisak_ps4_monolithic PRIVATE "${OPENORBIS_ROOT}/lib")
    target_link_libraries(kisak_ps4_monolithic PRIVATE
        "${OPENORBIS_ROOT}/lib/crt1.o"
        "${OPENORBIS_ROOT}/lib/crti.o"
        --start-group
        launcher_client
        engine_client
        client_client
        server_client
        scenefilecache_client
        "${KISAK_OPENGNM_ROOT}/libopengnm.a"
        appframework_client
        libvstdlib_client
        tier3_client
        tier2_client
        tier1_client
        inputsystem_client
        kisakvphysics_client
        materialsystem_client
        shaderapips4_client
        shaderapiempty_client
        datacache_client
        studiorender_client
        soundemittersystem_client
        vscript_client
        localize_client
        vgui2_client
        vgui_surfacelib_client
        vgui_controls_client
        vguimatsurface_client
        dmxloader_client
        shaderlib_client
        vtf_client
        bitmap_client
        bitmap_byteswap_client
        havana_constraints
        ivp_compactbuilder
        ivp_physics
        hk_math
        hk_base
        mathlib_client
        filesystem_stdio_client
        vpklib_client
        interfaces_client
        libtier0_client
        --end-group
        c++
        c++abi
        c
        kernel
        SceGnmDriver
        SceVideoOut
        SceUserService
        ScePad
        SceNet
        "${OPENORBIS_ROOT}/lib/crtn.o"
    )

    find_program(KISAK_CREATE_FSELF
        NAMES create-fself-macos create-fself
        HINTS "${OPENORBIS_ROOT}/bin/macos" "${OPENORBIS_ROOT}/bin"
    )
    if(KISAK_CREATE_FSELF)
        add_custom_command(TARGET kisak_ps4_monolithic POST_BUILD
            COMMAND "${KISAK_CREATE_FSELF}"
                -in=$<TARGET_FILE:kisak_ps4_monolithic>
                -out=${CMAKE_CURRENT_BINARY_DIR}/kisak_ps4_monolithic.oelf
                -eboot=${CMAKE_CURRENT_BINARY_DIR}/kisak_ps4_monolithic.bin
                --paid 0x3800000000000011
            BYPRODUCTS
                ${CMAKE_CURRENT_BINARY_DIR}/kisak_ps4_monolithic.oelf
                ${CMAKE_CURRENT_BINARY_DIR}/kisak_ps4_monolithic.bin
            VERBATIM
        )
    endif()
    return()
endif()

add_subdirectory(appframework)
add_subdirectory(external/crypto++-5.61)
add_subdirectory(interfaces)
add_subdirectory(fgdlib)
add_subdirectory(gcsdk)
add_subdirectory(tier0)
add_subdirectory(tier1)
add_subdirectory(tier2)
add_subdirectory(tier3)
add_subdirectory(vstdlib)
add_subdirectory(mathlib) #also does mathlib_extended.
add_subdirectory(vpklib)
add_subdirectory(vgui2/matsys_controls)
add_subdirectory(vgui2/src)
add_subdirectory(vgui2/vgui_controls)
add_subdirectory(vgui2/vgui_surfacelib)
add_subdirectory(soundsystem/lowlevel)
add_subdirectory(thirdparty/quickhull)
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "e2k")
    # We use sustem gperftools-2.5 on OS Elbrus
else()
    add_subdirectory(thirdparty/gperftools-2.8.1) #We include this version instead of distro-pkg because there is a false positive in ASAN
endif()
add_subdirectory(thirdparty/protobuf-2.5.0/cmake)
add_subdirectory(utils/bzip2)
add_subdirectory(utils/jpeglib)
add_subdirectory(unitlib)
add_subdirectory(videocfg)
add_subdirectory(vtf)

add_subdirectory(avi)
add_subdirectory(bitmap) #also does bitmap_byteswap
add_subdirectory(bonesetup)
add_subdirectory(choreoobjects)
add_subdirectory(datacache)
add_subdirectory(dedicated)
add_subdirectory(dedicated_main)
add_subdirectory(dmserializers)
add_subdirectory(dmxloader)
add_subdirectory(engine)
add_subdirectory(engine_ds)
add_subdirectory(filesystem)
add_subdirectory(inputsystem)
add_subdirectory(localize)
add_subdirectory(materialsystem)
add_subdirectory(materialsystem/shaderlib)
add_subdirectory(materialsystem/shaderapiempty)
add_subdirectory(materialsystem/stdshaders)
add_subdirectory(materialsystem/shaderapidx9)
add_subdirectory(meshutils)
add_subdirectory(particles)
add_subdirectory(raytrace)
add_subdirectory(resourcefile)
add_subdirectory(responserules/runtime)
add_subdirectory(scenefilecache)
add_subdirectory(serverbrowser)
add_subdirectory(soundemittersystem)
add_subdirectory(studiorender)
add_subdirectory(togl)
add_subdirectory(vguimatsurface)
add_subdirectory(vscript)

add_subdirectory(game/client) #CSGO Client
add_subdirectory(game/server) #CSGO Server
add_subdirectory(matchmaking) #Does all 4 matchmaking projects

add_subdirectory(launcher)
add_subdirectory(launcher_main) #The actual executable that just dlopen's launcher

if( USE_SCALEFORM )
    add_subdirectory(scaleformui) #DUMMY cmake project so CLion can do analysis
elseif( USE_ROCKETUI )
    #Custom Kisak-Strike UI powered by RmlUI
    add_subdirectory(thirdparty/RmlUi)
    add_subdirectory(rocketui)
endif()

#Frankenstein physics lib sewn together from 3 different leaks by mad scientist lwss
if( USE_KISAK_PHYSICS )
    add_definitions(-DUSE_KISAK_PHYSICS)
    message("Using Open-Source Kisakstrike Physics rebuild!\n")
    add_subdirectory(ivp/havana/havok/hk_base)
    add_subdirectory(ivp/havana/havok/hk_math)
    add_subdirectory(ivp/havana/havok/hk_physics/constraint)
    add_subdirectory(ivp/ivp_compact_builder)
    add_subdirectory(ivp/ivp_physics)
    add_subdirectory(vphysics)
#Experimental open source Bullet Physics
elseif( USE_BULLET_PHYSICS )
    add_definitions(-DUSE_BULLET_PHYSICS)
    #lwss: threading is experimental
    if( USE_BULLET_PHYSICS_THREADED )
        add_definitions(-DBT_THREADSAFE -DBT_USE_OPENMP)
        set(BULLET2_MULTITHREADING ON CACHE BOOL "" FORCE)
        set(BULLET2_USE_OPEN_MP_MULTITHREADING ON CACHE BOOL "" FORCE)
    endif()
    message("Using Bullet Open-Source Physics\n")
    set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
    set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
    set(BUILD_BULLET3 OFF CACHE BOOL "" FORCE)
    set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_ENET OFF CACHE BOOL "" FORCE)
    set(BUILD_CLSOCKET OFF CACHE BOOL "" FORCE)
    set(OpenGL_GL_PREFERENCE LEGACY)
    if (CMAKE_BUILD_TYPE EQUAL "DEBUG")
        message("Adding Bullet debug flag\n")
        add_definitions(-DBT_DEBUG)
        add_definitions(-DBT_DEBUG_MEMORY_ALLOCATIONS)
    endif()
    add_subdirectory(thirdparty/bullet3)
    add_subdirectory(vphysics_bullet)
else()
    message("Using Valve Standard VPhysics .so blob!\n")
endif()
