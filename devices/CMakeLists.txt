
# get_filename_component(
#    _CLEM_INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY
#)

add_library(clemens_65816_smartport STATIC
    hddcard.c
    mockingboard.c
    prodos_hdd32.c)

target_link_libraries(clemens_65816_smartport
    PUBLIC clemens_65816_serializer clemens_65816_mmio)

add_library(clemens_65816_mockingboard STATIC
    mockingboard.c)

target_link_libraries(clemens_65816_mockingboard
    PUBLIC clemens_65816_serializer)
