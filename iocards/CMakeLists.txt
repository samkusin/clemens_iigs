
set(IOCARDS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/mockingboard.c")

add_library(clemens_65816_iocards STATIC
    ${IOCARDS_SOURCES})

get_filename_component(
    _CLEM_INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY
)

target_include_directories(clemens_65816_iocards
    PUBLIC $<BUILD_INTERFACE:${_CLEM_INCLUDE_DIRECTORY}>)
