cmake_minimum_required(VERSION 3.15)

project(clements_iigs VERSION 0.6.0 LANGUAGES C CXX)

################################################################################

set(CINEK_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/cinek/ckdebug.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/cinek/buffer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/cinek/encode.c")

set(FMT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/fmt/core.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/fmt/format-inl.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/fmt/format.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/fmt/format.cc")

set(EXT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/ini.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/miniz.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/stb.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/spdlog/spdlog.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/spdlog/async.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/spdlog/color_sinks.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/spdlog/file_sinks.cpp")

add_library(clemens_host_shared
    ${CINEK_SOURCES}
    ${FMT_SOURCES}
    ${EXT_SOURCES})
target_compile_features(clemens_host_shared PUBLIC cxx_std_17)
target_compile_definitions(clemens_host_shared PUBLIC SPDLOG_COMPILED_LIB
                        $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>)
target_include_directories(clemens_host_shared
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ext)

if(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(clemens_host_shared
        PUBLIC -Wall -Wextra -Wno-missing-field-initializers -pedantic)
    target_compile_options(clemens_host_shared
        PUBLIC $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fno-exceptions -fno-rtti>)
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

    target_compile_definitions(clemens_host_shared
        PUBLIC _CRT_SECURE_NO_WARNINGS _ITERATOR_DEBUG_LEVEL=0)
    target_compile_options(clemens_host_shared PUBLIC /EHs-c-)
    target_compile_definitions(clemens_host_shared
        PUBLIC _HAS_EXCEPTIONS=0 FMT_EXCEPTIONS=0)
    target_compile_options(clemens_host_shared PUBLIC /GR-)
else()
    message(WARNING "Unsupported compiler")
endif()

################################################################################

add_library(clemens_host_core
    "${CMAKE_CURRENT_SOURCE_DIR}/core/clem_apple2gs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/clem_disk_asset.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/clem_disk_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/clem_prodos_disk.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/clem_snapshot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/clem_storage_unit.cpp")
target_compile_features( clemens_host_core PUBLIC cxx_std_17 )
target_link_libraries(clemens_host_core
    PUBLIC
        clemens_65816_mmio
        clemens_65816_render
        clemens_65816_serializer
        clemens_65816_iocards
        clemens_65816_smartport_devices
        clemens_host_shared)
target_include_directories(clemens_host_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(harness)


################################################################################

string(TIMESTAMP CLEM_HOST_COPYRIGHT_YEAR "%Y")
set(CLEMENS_HOMEPAGE "https://samirsinha.com/tags/clemens/")

configure_file(version.h.in version.h)

if(APPLE)
    enable_language(OBJC)
endif()
if(MSVC)
    enable_language(RC)
endif()

# build header-only source
# build imgui
# build app

add_subdirectory(ext/imgui)

set(SOKOL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/sokol/sokol_gfx.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/sokol/sokol_app.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/sokol/sokol_glue.h")

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(clemens_ICON_FILE "")
    enable_language(RC)
    set(PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/clemens.rc"
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/host_windows.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/clem_host.cpp")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(clemens_ICON_FILE "")
    set(PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/host_linux.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/clem_host.cpp")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(clemens_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.icns")
    set_source_files_properties(${clemens_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set(PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/host_macos.m"
        "${CMAKE_CURRENT_SOURCE_DIR}/clem_host.mm")
endif()

set(PROJECT_SOURCES
    ${PLATFORM_SOURCES}
    ${SOKOL_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_assets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_asset_browser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_audio.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_display.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_backend.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_command_queue.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_configuration.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_debugger.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_file_browser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_frame_state.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_front.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host_app.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_host_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_interpreter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_l10n.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_preamble.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_program_trace.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_ui_load_snapshot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_ui_save_snapshot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_ui_settings.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/clem_startup_view.cpp")

add_executable(clemens_iigs ${clemens_ICON_FILE} ${PROJECT_SOURCES})
target_link_libraries(clemens_iigs
    PRIVATE
        clemens_host_core
        imgui)
target_include_directories(clemens_iigs
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(clemens_iigs PRIVATE CK3D_BACKEND_D3D11)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(clemens_iigs PRIVATE CK3D_BACKEND_GL)
    target_link_libraries(clemens_iigs PRIVATE asound X11 Xi Xcursor GL dl pthread uuid)
elseif(APPLE)
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
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CLEM_HOST_VERSION_MAJOR}.${CLEM_HOST_VERSION_MINOR}.${CLEM_HOST_VERSION_PATCH}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright ${CLEM_HOST_COPYRIGHT_YEAR} Samir Sinha DBA Cinekine"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos_bundle_info_plist.in"
        MACOSX_BUNDLE_ICON_FILE icon.icns
        XCODE_ATTRIBUTE_EXECUTABLE_NAME "Clemens IIGS")

endif()

# handle packaging - this is an abbreviated form of the CPack packaging scripts
# from https://github.com/kracejic/cleanCppProject/blob/master/packaging/CMakeLists.txt
#
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist" CACHE PATH
        "Install path prefix, prepended onto install directories." FORCE )
endif()

install(TARGETS clemens_iigs
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION bin)

set(CPACK_PACKAGE_VENDOR "samkusin")
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_CONTACT "Samir Sinha <samir@cinekine.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "A cross-platform Apple IIGS emulator.")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Clemens_IIGS")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/out")
set(CPACK_PACKAGE_CHECKSUM SHA256)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CPACK_GENERATOR "STGZ;ZIP")
    set(CPACK_PACKAGE_FILE_NAME "Clemens_IIGS-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-windows-x64")
elseif(APPLE)
    #--------------------------------------------------------------------------
    # Apple specific
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")

    #set_source_files_properties(${PROJECT_SOURCE_DIR}/packaging/exampleApp.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    set(CPACK_DMG_VOLUME_NAME "ClemensIIGS")
    #set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${PROJECT_SOURCE_DIR}/packaging/DS_Store.scpt")
    set(CPACK_OSX_PACKAGE_VERSION "10.15") #min package version
    set(CPACK_PACKAGE_FILE_NAME "Clemens_IIGS-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-macOS-universal")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    #-----------------------------------------------------------------------------
    # Linux specific
    set(CPACK_GENERATOR "DEB;TBZ2;TXZ")
    set(CPACK_PACKAGE_FILE_NAME "Clemens_IIGS-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-linux-x64")
    message(STATUS "Package generation - UNIX")
    message(STATUS "   + DEB                                  YES ")
    message(STATUS "   + TBZ2                                 YES ")
    message(STATUS "   + TXZ                                  YES ")

    # debian/ubuntu/and variants
    find_program(RPMBUILD_PATH rpmbuild)
    if(RPMBUILD_PATH)
        message(STATUS "   + RPM                                  YES ")
        set(CPACK_GENERATOR "${CPACK_GENERATOR};RPM")
        set(CPACK_RPM_PACKAGE_LICENSE "MIT")
        # set(CPACK_RPM_PACKAGE_REQUIRES "gtkmm30")
        # exclude folders which clash with default ones
        set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST
            ${CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST}
            /usr
            /usr/bin
            /usr/share
            /usr/share/applications
            /usr/share/doc
            /usr/share/icons
            /usr/share/icons/hicolor
            /usr/share/icons/hicolor/256x256
            /usr/share/icons/hicolor/256x256/apps
            /usr/share/icons/gnome
            /usr/share/icons/gnome/256x256
            /usr/share/icons/gnome/256x256/apps)
    endif()

    # TODO do this better
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CLEMENS_HOMEPAGE}")
    # set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
    # set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgtkmm-3.0")

    # Icon and app shortcut for Linux systems
    # Note: .desktop file must have same name as executable
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/exampleApp.desktop
        DESTINATION share/applications/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/exampleApp.png
        DESTINATION share/icons/hicolor/256x256/apps/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/exampleApp.png
        DESTINATION share/icons/gnome/256x256/apps/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
endif()

include(CPack)
