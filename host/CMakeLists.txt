cmake_minimum_required(VERSION 3.15)

project(clements_iigs LANGUAGES C CXX)

set(CLEM_HOST_VERSION_MAJOR 0)
set(CLEM_HOST_VERSION_MINOR 6)
set(CLEM_HOST_VERSION_PATCH 0)
string(TIMESTAMP CLEM_HOST_COPYRIGHT_YEAR "%Y")

configure_file(version.h.in version.h)

if(APPLE)
    enable_language(OBJC)
endif()

set(CMAKE_CXX_STANDARD 17)

# build header-only source
# build imgui
# build app

add_subdirectory(ext/imgui)

set(CINEK_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/cinek/ckdebug.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/cinek/buffer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/cinek/encode.c")

set(SOKOL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/sokol/sokol_gfx.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/sokol/sokol_app.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/sokol/sokol_glue.h")

set(FMT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/fmt/core.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/fmt/format-inl.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/fmt/format.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/fmt/format.cc")

set(EXT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/ini.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/miniz.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/stb.c")

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/host_windows.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/clem_host.cpp")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/host_linux.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/clem_host.cpp")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/host_macos.m"
        "${CMAKE_CURRENT_SOURCE_DIR}/clem_host.mm")
endif()

set(PROJECT_SOURCES 
    ${PLATFORM_SOURCES}
    ${CINEK_SOURCES}
    ${EXT_SOURCES}
    ${FMT_SOURCES}
    ${SOKOL_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_assets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_audio.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_display.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_backend.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_command_queue.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_configuration.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_disk_library.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_disk_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_front.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host_app.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_import_disk.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_interpreter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_l10n.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_preamble.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_program_trace.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_ui_load_snapshot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_ui_save_snapshot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_serializer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_startup_view.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_smartport_disk.cpp")

add_executable(clemens_iigs ${PROJECT_SOURCES})
target_include_directories(clemens_iigs
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/ext)
target_link_libraries(clemens_iigs
    PRIVATE
        clemens_65816_mmio
        clemens_65816_render
        clemens_65816_serializer
        clemens_65816_iocards
        clemens_65816_smartport_devices
        imgui)

target_compile_features( clemens_iigs PRIVATE cxx_std_17 )

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(clemens_iigs PRIVATE CK3D_BACKEND_D3D11)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(clemens_iigs PRIVATE CK3D_BACKEND_GL)
    target_link_libraries(clemens_iigs PRIVATE asound X11 Xi Xcursor GL dl pthread uuid)
else()
    find_library(FOUNDATION_LIB Foundation)
    find_library(APPKIT_LIB AppKit)
    find_library(CORE_AUDIO_LIB CoreAudio)
    find_library(AUDIO_TOOLBOX_LIB AudioToolbox)
    find_library(CORE_GRAPHICS_LIB CoreGraphics)
    find_library(OPENGL_LIB OpenGL)
    find_library(GAME_CONTROLLER_LIB GameController)
    target_compile_definitions(clemens_iigs PRIVATE CK3D_BACKEND_GL)
    target_link_libraries(clemens_iigs PRIVATE ${FOUNDATION_LIB} 
                                               ${APPKIT_LIB}
                                               ${CORE_AUDIO_LIB}
                                               ${AUDIO_TOOLBOX_LIB}
                                               ${CORE_GRAPHICS_LIB}
                                               ${OPENGL_LIB}
                                               ${GAME_CONTROLLER_LIB})
    set_target_properties(clemens_iigs PROPERTIES 
        OUTPUT_NAME "Clemens IIGS"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Clemens IIGS"
        MACOSX_BUNDLE_BUNDLE_VERSION "${CLEM_HOST_VERSION_MAJOR}.${CLEM_HOST_VERSION_MINOR}.${CLEM_HOST_VERSION_PATCH}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CLEM_HOST_VERSION_MAJOR}.${CLEM_HOST_VERSION_MINOR}.${CLEM_HOST_VERSION_PATCH}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright ${CLEM_HOST_COPYRIGHT_YEAR} Samir Sinha DBA Cinekine"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos_bundle_info_plist.in"
        XCODE_ATTRIBUTE_EXECUTABLE_NAME "Clemens IIGS")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(clemens_iigs PRIVATE -Wall -Wextra -Wno-missing-field-initializers -pedantic)
    target_compile_options(clemens_iigs PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-fno-exceptions -fno-rtti>)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC 8 doesn't link the fs library even with C++17 enabled
        target_link_libraries(clemens_iigs PRIVATE $<$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>:stdc++fs>)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Removing MSVC options that are less necessary for game / simulation
    # development.
    #
    # CRT_SECURE_NO_WARNINGS removes the C11-ish enforcement of using secure
    # versions of classic C APIs that MSVC enforces instead of relying on a
    # catch-all language dialect option like GCC/Clang
    #
    # ITERATOR_DEBUG_LEVEL set to 0 removes std container debugging enhancements
    # specific to MSVC's implementation
    #
    # Remove Exceptions and RTTI ON defaults to eliminate warnings when we disable them

    # TODO: this should be part of a toolchain file
    string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/EHsc*" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    target_compile_definitions(clemens_iigs PRIVATE _CRT_SECURE_NO_WARNINGS _ITERATOR_DEBUG_LEVEL=0)
    target_compile_options(clemens_iigs PRIVATE /EHs-c-)
    target_compile_definitions(clemens_iigs PRIVATE _HAS_EXCEPTIONS=0 FMT_EXCEPTIONS=0)
    target_compile_options(clemens_iigs PRIVATE /GR-)
else()
    message(WARNING "Unsupported compiler")
endif()
