cmake_minimum_required(VERSION 3.15)

project(clements_iigs LANGUAGES C CXX)

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


if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/host_windows.c")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/host_linux.c")
endif()

add_executable(clemens_iigs
    ${PLATFORM_SOURCES}
    ${CINEK_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_audio.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_display.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_backend.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_disk_library.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_disk_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_front.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host_app.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_import_disk.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_program_trace.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_serializer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_smartport_disk.cpp"
    ${FMT_SOURCES}
    ${SOKOL_SOURCES}
    ${IMGUI_SOURCES})

target_include_directories(clemens_iigs
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
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
    target_compile_definitions(clemens_iigs PRIVATE CK3D_BACKEND_GL)
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
