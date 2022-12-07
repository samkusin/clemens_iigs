
set(SMARTPORT_DEVICE_SOURCES
"${CMAKE_CURRENT_SOURCE_DIR}/prodos_hdd32.c")

add_library(clemens_65816_smartport_devices STATIC
${SMARTPORT_DEVICE_SOURCES})

get_filename_component(
_CLEM_INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY
)

target_include_directories(clemens_65816_smartport_devices
PUBLIC $<BUILD_INTERFACE:${_CLEM_INCLUDE_DIRECTORY}>)

target_link_libraries(clemens_65816_smartport_devices
    PUBLIC clemens_65816_mmio clemens_65816_serializer)
