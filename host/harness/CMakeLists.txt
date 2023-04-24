cmake_minimum_required(VERSION 3.15)

project(clemens_test_harness LANGUAGES C CXX)

set(CLEMENS_TEST_COMMON_ASSETS
    "${CMAKE_CURRENT_BINARY_DIR}/data/dos_3_3_master.woz"
    "${CMAKE_CURRENT_BINARY_DIR}/data/ProDOS 16v1_3.2mg"
    "${CMAKE_CURRENT_BINARY_DIR}/data/ProDOS_2_4_2.dsk"
    "${CMAKE_CURRENT_BINARY_DIR}/data/System.Disk.po")

add_custom_command(OUTPUT ${CLEMENS_TEST_COMMON_ASSETS}
    COMMAND ${CMAKE_COMMAND} -E tar xzf ${CLEMENS_TEST_ASSETS_ARCHIVE}
    WORKING_DIRECTORY ${CLEMENS_TEST_WORKING_DIRECTORY}
    DEPENDS ${CLEMENS_TEST_ASSETS_ARCHIVE}
    COMMENT "Unarchiving test assets into ${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM)

add_executable(clemens_test_harness
    main.cpp
    machine.cpp
    ${CLEMENS_HOST_SOURCE_DIR}/disklib/clem_disk_asset.cpp
    ${CLEMENS_HOST_SOURCE_DIR}/disklib/clem_disk_utils.cpp
    ${CLEMENS_HOST_SOURCE_DIR}/disklib/clem_prodos_disk.cpp
    ${CLEMENS_HOST_SOURCE_DIR}/disklib/clem_storage_unit.cpp
    ${CLEMENS_TEST_COMMON_ASSETS})

target_link_libraries(clemens_test_harness
    PRIVATE clemens_host_core)
