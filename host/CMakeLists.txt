cmake_minimum_required(VERSION 3.15)

project(clements_iigs LANGUAGES C CXX)

# build header-only source
# build imgui
# build app

set(SOKOL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/sokol/sokol_gfx.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/sokol/sokol_app.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/sokol/sokol_glue.h")

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
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host_app.cpp"
    ${SOKOL_SOURCES}
    ${IMGUI_SOURCES})

target_include_directories(clemens_iigs
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(clemens_iigs
    PRIVATE clemens_65816)

project_create_assets_target(clemens_iigs)
