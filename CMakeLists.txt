cmake_minimum_required(VERSION 3.15)

project(clements_65816 LANGUAGES C)

function(project_create_assets_target ARG_TARGET_NAME)
    set(_TARGET_RUNTIME_OUTPUT_DIRECTORY
        "${PROJECT_RUNTIME_OUTPUT_DIRECTORY}/${ARG_TARGET_NAME}")
    set_target_properties(${ARG_TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${_TARGET_RUNTIME_OUTPUT_DIRECTORY})

    file( TO_NATIVE_PATH "${CMAKE_CURRENT_LIST_DIR}/data"
          _APP_DATA_DIR )

    if( CMAKE_SYSTEM_NAME MATCHES "Darwin" OR CMAKE_SYSTEM_NAME MATCHES "Linux" )
        set( _EXT_COPY_COMMAND "rsync" )
        set( _EXT_COPY_ARGS "-a" )
        file( TO_NATIVE_PATH ${_TARGET_RUNTIME_OUTPUT_DIRECTORY}
              _APP_DATA_DIR_TARGET )
    elseif( CMAKE_SYSTEM_NAME MATCHES "Windows" )
        set( _EXT_COPY_COMMAND "xcopy" )
        set( _EXT_COPY_ARGS /s /e /y /i )
        file( TO_NATIVE_PATH "${_TARGET_RUNTIME_OUTPUT_DIRECTORY}"
              _APP_DATA_DIR_TARGET )
    else()
        message( FATAL_ERROR "${CMAKE_SYSTEM_NAME} Platform unsupported" )
    endif()

    add_custom_target(app_assets_${ARG_TARGET_NAME}
        COMMAND ${_EXT_COPY_COMMAND} ${_EXT_COPY_ARGS}
            ${_APP_DATA_DIR}
            ${_APP_DATA_DIR_TARGET}
    )
    add_dependencies(app_assets_${ARG_TARGET_NAME} ${ARG_TARGET_NAME})
endfunction()

enable_testing()

set(PROJECT_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}-run")

set(EMULATOR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_adb.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_debug.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_rtc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_timer.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_mem.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/emulator.c")

add_library(clemens_65816 STATIC
    ${EMULATOR_SOURCES})

target_include_directories(clemens_65816
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

add_subdirectory(tests)

add_executable(clemens_65816_emulator
    "${CMAKE_CURRENT_SOURCE_DIR}/app.c")

target_link_libraries(clemens_65816_emulator PRIVATE clemens_65816)

project_create_assets_target(clemens_65816_emulator)

add_subdirectory(host)
