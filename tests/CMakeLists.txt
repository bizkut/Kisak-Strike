cmake_minimum_required(VERSION 3.16)
project(kisak_platform_tests CXX)

set(ORBIS ON)
include(../cmake/detect_platform.cmake)
if(NOT PS4 OR NOT POSIX OR NOT STATIC_LINK OR NOT _DLL_EXT STREQUAL "")
    message(FATAL_ERROR "Orbis platform detection did not establish the monolithic PS4 build contract")
endif()

enable_testing()
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
target_compile_definitions(ps4_static_modules_test PRIVATE POSIX _POSIX GNUC COMPILER_GCC)
set_target_properties(ps4_static_modules_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_static_modules COMMAND ps4_static_modules_test)

add_executable(ps4_content_paths_test
    ps4_content_paths_test.cpp
    ../launcher/ps4_content_paths.cpp
)
target_include_directories(ps4_content_paths_test PRIVATE ../launcher)
set_target_properties(ps4_content_paths_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_content_paths COMMAND ps4_content_paths_test)

add_executable(ps4_gnm_device_test
    ps4_gnm_device_test.cpp
    ../materialsystem/ps4gnm/ps4_gnm_device.cpp
    ../materialsystem/ps4gnm/ps4_gnm_memory.cpp
)
target_include_directories(ps4_gnm_device_test PRIVATE ../materialsystem/ps4gnm)
set_target_properties(ps4_gnm_device_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
add_test(NAME ps4_gnm_device COMMAND ps4_gnm_device_test)
