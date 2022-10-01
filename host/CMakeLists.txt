cmake_minimum_required(VERSION 3.15)

project(clements_iigs LANGUAGES C CXX)

# build header-only source
# build imgui
# build app

add_subdirectory(ext/imgui)

# TODO: make header-only version when ready
set(CKAUDIO_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ckaudio/core.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ckaudio/types.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ckaudio/context.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ckaudio/core.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ckaudio/queue.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ckaudio/queue.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ckaudio/wincoreaudio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ckaudio/wintime.c")

add_library(ckaudio STATIC ${CKAUDIO_SOURCES})

target_include_directories(ckaudio
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)


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
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host_app.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_import_disk.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_program_trace.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_serializer.cpp"
    ${FMT_SOURCES}
    ${SOKOL_SOURCES}
    ${IMGUI_SOURCES})

target_include_directories(clemens_iigs
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/ext)
target_link_libraries(clemens_iigs
    PRIVATE
        clemens_65816
        clemens_65816_render
        clemens_65816_serializer
        clemens_65816_iocards
        ckaudio
        imgui)

target_compile_features( clemens_iigs PRIVATE cxx_std_17 )
if(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(clemens_iigs PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(clemens_iigs PUBLIC -fno-exceptions)
    target_compile_options(clemens_iigs PUBLIC -fno-rtti)
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

    target_compile_definitions(clemens_iigs PUBLIC _CRT_SECURE_NO_WARNINGS _ITERATOR_DEBUG_LEVEL=0)
    target_compile_options(clemens_iigs PUBLIC /EHs-c-)
    target_compile_definitions(clemens_iigs PUBLIC _HAS_EXCEPTIONS=0 FMT_EXCEPTIONS=0)
    target_compile_options(clemens_iigs PUBLIC /GR-)
else()
    message(WARNING "Unsupported compiler")
endif()

project_create_assets_target(clemens_iigs)
