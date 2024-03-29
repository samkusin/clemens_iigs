project(clem_tests LANGUAGES C)

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

add_library(clem_test_utils STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utils.c)

add_executable(test_adc cpu/test_adc.c)
target_link_libraries(test_adc clemens_65816 unity)

add_executable(test_emulate_minimal test_emulate_minimal.c)
target_link_libraries(test_emulate_minimal clemens_65816 unity)

add_executable(test_gameport test_gameport.c)
target_link_libraries(test_gameport clemens_65816_mmio unity)

add_executable(test_mmio_video_switches test_mmio_video_switches.c)
target_link_libraries(test_mmio_video_switches clemens_65816_mmio unity)

add_executable(test_disk_nib test_disk_nib.c)
target_link_libraries(test_disk_nib clemens_disktypes clem_test_utils unity )

add_executable(test_disk_2img test_disk_2img.c ${CLEMENS_TEST_COMMON_ASSETS})
target_link_libraries(test_disk_2img clemens_disktypes clem_test_utils unity)

add_executable(test_disk_woz test_disk_woz.c ${CLEMENS_TEST_COMMON_ASSETS})
target_link_libraries(test_disk_woz clemens_disktypes clem_test_utils unity)

add_executable(test_scc test_scc.c)
target_link_libraries(test_scc clemens_65816_serial_devices clemens_65816_mmio unity)

add_test(NAME minimal COMMAND test_emulate_minimal)
add_test(NAME cpu_adc COMMAND test_adc)
add_test(NAME disk_nib COMMAND test_disk_nib)
add_test(NAME disk_2img COMMAND test_disk_2img)
add_test(NAME disk_woz COMMAND test_disk_woz)
add_test(NAME gameport COMMAND test_gameport)
add_test(NAME mmio_video_switches COMMAND test_mmio_video_switches)
add_test(NAME scc COMMAND test_scc)


# add_library(test_lib util.c)
# target_link_libraries(test_lib clemens_65816 unity)

# add_executable(test_clem test_clem.c)
# target_link_libraries(test_clem test_lib)

# add_executable(test_lcbank test_lcbank.c)
# target_link_libraries(test_lcbank test_lib)

# add_executable(test_c035_shadow test_c035_shadow.c)
# target_link_libraries(test_c035_shadow test_lib)

# add_executable(test_woz test_woz.c)
# target_link_libraries(test_woz test_lib)

# add_test(NAME clem COMMAND test_clem)
# add_test(NAME lcbank COMMAND test_lcbank)
# add_test(NAME shadow COMMAND test_c035_shadow)
# add_test(NAME disk COMMAND test_woz WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
