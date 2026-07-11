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
    set(USE_SCALEFORM OFF CACHE BOOL "" FORCE)
    set(USE_ROCKETUI ON CACHE BOOL "" FORCE)
    set(USE_KISAK_PHYSICS ON CACHE BOOL "" FORCE)

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
    add_subdirectory(materialsystem)
    add_subdirectory(datacache)
    add_subdirectory(studiorender)
    add_subdirectory(soundemittersystem)
    add_subdirectory(vscript)
    add_subdirectory(dmxloader)
    add_subdirectory(vgui2/vgui_surfacelib)
    add_subdirectory(vgui2/src)
    add_subdirectory(vgui2/vgui_controls)
    add_subdirectory(vguimatsurface)
    add_subdirectory(appframework)
    add_subdirectory(launcher)

    file(READ "${OPENORBIS_ROOT}/link.x" KISAK_OPENORBIS_LINK_SCRIPT)
    string(REPLACE "\t.data.sce_process_param"
        "\t.kisak_ctors : {\n\t\t__kisak_ctors_start = .;\n\t\tKEEP(*(.ctors .ctors.*));\n\t\t__kisak_ctors_end = .;\n\t}\n\n\t.data.sce_process_param"
        KISAK_OPENORBIS_LINK_SCRIPT "${KISAK_OPENORBIS_LINK_SCRIPT}")
    set(KISAK_PS4_LINK_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/kisak-link.x")
    file(WRITE "${KISAK_PS4_LINK_SCRIPT}" "${KISAK_OPENORBIS_LINK_SCRIPT}")

    set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
    add_executable(kisak_ps4_monolithic ps4/bootstrap_main.c ps4/engine_bootstrap.cpp)
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
    )
    target_link_options(kisak_ps4_monolithic PRIVATE
        -m elf_x86_64
        -pie
        "--script=${KISAK_PS4_LINK_SCRIPT}"
        --eh-frame-hdr
    )
    target_link_directories(kisak_ps4_monolithic PRIVATE "${OPENORBIS_ROOT}/lib")
    target_link_libraries(kisak_ps4_monolithic PRIVATE
        "${OPENORBIS_ROOT}/lib/crt1.o"
        "${OPENORBIS_ROOT}/lib/crti.o"
        --start-group
        launcher_client
        appframework_client
        libvstdlib_client
        tier3_client
        tier2_client
        tier1_client
        inputsystem_client
        kisakvphysics_client
        materialsystem_client
        datacache_client
        studiorender_client
        soundemittersystem_client
        vscript_client
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
