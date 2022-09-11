
project(imgui LANGUAGES CXX)


set(IMGUI_EXTENSION_SOURCES
   "${CMAKE_CURRENT_SOURCE_DIR}/imgui_filedialog/ImGuiFileDialog.cpp")

set(IMGUI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imstb_rectpack.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imstb_textedit.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imstb_truetype.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui_widgets.cpp"
    ${IMGUI_EXTENSION_SOURCES})

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#[[
target_compile_features( imgui PRIVATE cxx_std_17 )

# TODO: this should be part of a toolchain file
if(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(imgui PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(imgui PUBLIC -fno-exceptions)
    target_compile_options(imgui PUBLIC -fno-rtti)
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

    string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/EHsc*" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    target_compile_definitions(imgui PUBLIC _CRT_SECURE_NO_WARNINGS _ITERATOR_DEBUG_LEVEL=0)
    target_compile_options(imgui PUBLIC /EHs-c-)
    target_compile_definitions(imgui PUBLIC _HAS_EXCEPTIONS=0 FMT_EXCEPTIONS=0)
    target_compile_options(imgui PUBLIC /GR-)
else()
    message(WARNING "Unsupported compiler")
endif()
]]
