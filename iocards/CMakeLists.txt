
get_filename_component(
    _CLEM_INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY
)

add_library(clemens_65816_iocards STATIC
    mockingboard.c)

target_include_directories(clemens_65816_iocards
    PUBLIC $<BUILD_INTERFACE:${_CLEM_INCLUDE_DIRECTORY}>)

add_library(clemens_65816_peripherals STATIC
    clem_peer.c)

target_include_directories(clemens_65816_peripherals
    PUBLIC $<BUILD_INTERFACE:${_CLEM_INCLUDE_DIRECTORY}>)
