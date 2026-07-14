cmake_minimum_required(VERSION 3.16)
project(kisak_platform_tests CXX)

set(ORBIS ON)
include(../cmake/detect_platform.cmake)
if(NOT PS4 OR NOT POSIX OR NOT STATIC_LINK OR NOT _DLL_EXT STREQUAL "")
    message(FATAL_ERROR "Orbis platform detection did not establish the monolithic PS4 build contract")
endif()

enable_testing()
set_source_files_properties(
    ../../opengnm/src/dataformat.c
    ../../opengnm/src/error.c
    PROPERTIES LANGUAGE CXX)
add_executable(static_module_registry_test
    static_module_registry_test.cpp
    ../appframework/StaticModuleRegistry.cpp
)
target_include_directories(static_module_registry_test PRIVATE ../public)
target_compile_definitions(static_module_registry_test PRIVATE POSIX _POSIX GNUC COMPILER_GCC)
set_target_properties(static_module_registry_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME static_module_registry COMMAND static_module_registry_test)

add_executable(ps4_static_modules_test
    ps4_static_modules_test.cpp
    ../appframework/Ps4StaticModules.cpp
    ../appframework/StaticModuleRegistry.cpp
)
target_include_directories(ps4_static_modules_test PRIVATE ../public)
target_compile_definitions(ps4_static_modules_test PRIVATE POSIX _POSIX GNUC COMPILER_GCC KISAK_PS4_STATIC_FACTORY_STUB)
set_target_properties(ps4_static_modules_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_static_modules COMMAND ps4_static_modules_test)

add_executable(ps4_content_paths_test
    ps4_content_paths_test.cpp
    ../launcher/ps4_content_paths.cpp
)
target_include_directories(ps4_content_paths_test PRIVATE ../launcher)
set_target_properties(ps4_content_paths_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_content_paths COMMAND ps4_content_paths_test)

add_executable(ps4_scaleform_asset_path_test
    ps4_scaleform_asset_path_test.cpp
    ../ps4/scaleform_asset_path.cpp
)
target_include_directories(ps4_scaleform_asset_path_test PRIVATE ..)
set_target_properties(ps4_scaleform_asset_path_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_scaleform_asset_path COMMAND ps4_scaleform_asset_path_test)

add_executable(ps4_scaleform_menu_actions_test
    ps4_scaleform_menu_actions_test.cpp
)
target_include_directories(ps4_scaleform_menu_actions_test PRIVATE ..)
set_target_properties(ps4_scaleform_menu_actions_test PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_scaleform_menu_actions COMMAND ps4_scaleform_menu_actions_test)

add_executable(ps4_offline_launch_request_test
    ps4_offline_launch_request_test.cpp
    ../ps4/offline_launch_request.cpp
)
target_include_directories(ps4_offline_launch_request_test PRIVATE ..)
set_target_properties(ps4_offline_launch_request_test PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_offline_launch_request COMMAND ps4_offline_launch_request_test)

add_executable(ps4_gnm_device_test
    ps4_gnm_device_test.cpp
    ../materialsystem/ps4gnm/ps4_gnm_buffer.cpp
    ../materialsystem/ps4gnm/ps4_gnm_device.cpp
    ../materialsystem/ps4gnm/ps4_gnm_memory.cpp
    ../materialsystem/ps4gnm/ps4_gnm_runtime.cpp
    ../materialsystem/ps4gnm/ps4_gnm_vertex_declaration.cpp
    ../../opengnm/src/dataformat.c
    ../../opengnm/src/error.c
)
target_include_directories(ps4_gnm_device_test PRIVATE
    ../materialsystem/ps4gnm
    ../../opengnm/include)
set_target_properties(ps4_gnm_device_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_gnm_device COMMAND ps4_gnm_device_test)

add_executable(ps4_gnm_buffer_test
    ps4_gnm_buffer_test.cpp
    ../materialsystem/ps4gnm/ps4_gnm_buffer.cpp
    ../../opengnm/src/dataformat.c
    ../../opengnm/src/error.c
)
target_include_directories(ps4_gnm_buffer_test PRIVATE
    ../materialsystem/ps4gnm
    ../../opengnm/include)
set_target_properties(ps4_gnm_buffer_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_gnm_buffer COMMAND ps4_gnm_buffer_test)

add_executable(ps4_gnm_vertex_declaration_test
    ps4_gnm_vertex_declaration_test.cpp
    ../materialsystem/ps4gnm/ps4_gnm_vertex_declaration.cpp
    ../../opengnm/src/dataformat.c
    ../../opengnm/src/error.c
)
target_include_directories(ps4_gnm_vertex_declaration_test PRIVATE
    ../materialsystem/ps4gnm
    ../../opengnm/include)
set_target_properties(ps4_gnm_vertex_declaration_test PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_gnm_vertex_declaration COMMAND ps4_gnm_vertex_declaration_test)

add_executable(ps4_gnm_constants_test
    ps4_gnm_constants_test.cpp
    ../materialsystem/ps4gnm/ps4_gnm_constants.cpp
    ../materialsystem/ps4gnm/ps4_gnm_memory.cpp
    ../../opengnm/src/dataformat.c
    ../../opengnm/src/error.c
)
target_include_directories(ps4_gnm_constants_test PRIVATE
    ../materialsystem/ps4gnm
    ../../opengnm/include)
set_target_properties(ps4_gnm_constants_test PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_gnm_constants COMMAND ps4_gnm_constants_test)

add_executable(ps4_shadow_state_translate_test
    ps4_shadow_state_translate_test.cpp
    ../materialsystem/ps4gnm/ps4_shadow_state_translate.cpp
)
target_include_directories(ps4_shadow_state_translate_test PRIVATE
    ../public
    ../materialsystem/ps4gnm
    ../../opengnm/include
)
target_compile_definitions(ps4_shadow_state_translate_test PRIVATE POSIX _POSIX GNUC COMPILER_GCC)
set_target_properties(ps4_shadow_state_translate_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_shadow_state_translate COMMAND ps4_shadow_state_translate_test)

add_executable(ps4_shader_manifest_test
    ps4_shader_manifest_test.cpp
    ../materialsystem/ps4gnm/ps4_shader_manifest.cpp
)
target_include_directories(ps4_shader_manifest_test PRIVATE ../materialsystem/ps4gnm)
set_target_properties(ps4_shader_manifest_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_shader_manifest COMMAND ps4_shader_manifest_test)

add_executable(ps4_gnm_shader_handles_test
    ps4_gnm_shader_handles_test.cpp
    ../materialsystem/ps4gnm/ps4_gnm_shader_handles.cpp
)
target_include_directories(ps4_gnm_shader_handles_test PRIVATE ../materialsystem/ps4gnm)
set_target_properties(ps4_gnm_shader_handles_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_gnm_shader_handles COMMAND ps4_gnm_shader_handles_test)

add_executable(ps4_scaleform_hal_test
    ps4_scaleform_hal_test.cpp
    ../ps4/scaleform_gnm_hal.cpp
)
target_include_directories(ps4_scaleform_hal_test PRIVATE
    ..
    ../ps4
    ../../opengnm/include)
target_compile_definitions(ps4_scaleform_hal_test PRIVATE
    POSIX _POSIX GNUC COMPILER_GCC)
set_target_properties(ps4_scaleform_hal_test PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_scaleform_hal COMMAND ps4_scaleform_hal_test)
