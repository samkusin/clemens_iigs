cmake_minimum_required(VERSION 3.15)

project(clemens_65816 LANGUAGES C)

set (CMAKE_C_STANDARD 11)

add_library(clemens_65816 STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_debug.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_mem.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/emulator.c")

target_include_directories(clemens_65816
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

add_library(clemens_65816_mmio STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_adb.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_audio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_2img.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_drive.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_drive35.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_iwm.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_mmio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_rtc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_scc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_timer.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_vgc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_woz.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/emulator_mmio.c")

target_include_directories(clemens_65816_mmio
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(clemens_65816_mmio PUBLIC clemens_65816)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(clemens_65816_mmio PUBLIC m)
endif()

set(MPACK_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/external/mpack.c"
    )

add_library(clemens_65816_serializer STATIC
    ${MPACK_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/serializer.c")

target_link_libraries(clemens_65816_serializer PUBLIC clemens_65816_mmio)
target_include_directories(clemens_65816_serializer
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

add_library(clemens_65816_render STATIC
    ${MPACK_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/render.c")

target_link_libraries(clemens_65816_render PUBLIC clemens_65816_mmio)
target_include_directories(clemens_65816_render
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

if(BUILD_TESTING)
enable_testing()
add_subdirectory(tests)
endif()

add_executable(clemens_65816_emulator
    "${CMAKE_CURRENT_SOURCE_DIR}/app.c")

target_link_libraries(clemens_65816_emulator PRIVATE clemens_65816_mmio)

add_executable(clemens_mpack_format
    ${MPACK_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_mpack_format.c")

target_include_directories(clemens_mpack_format
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

add_subdirectory(iocards)
add_subdirectory(host)
