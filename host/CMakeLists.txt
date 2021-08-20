cmake_minimum_required(VERSION 3.15)

project(clements_iigs LANGUAGES C CXX)

# build header-only source
# build imgui
# build app

set(SOKOL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/sokol/sokol_gfx.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/sokol/sokol_app.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/sokol/sokol_glue.h")

set(FMT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/fmt/core.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/fmt/format-inl.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/fmt/format.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/fmt/format.cc")

set(IMGUI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imstb_rectpack.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imstb_textedit.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imstb_truetype.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp")


if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/host_windows.c")
endif()

add_executable(clemens_iigs
    ${PLATFORM_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_audio.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_display.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host_app.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_program_trace.cpp"
    ${FMT_SOURCES}
    ${SOKOL_SOURCES}
    ${IMGUI_SOURCES})

target_include_directories(clemens_iigs
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(clemens_iigs
    PRIVATE clemens_65816)

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
