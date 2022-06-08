project(clem_tests LANGUAGES C)

add_subdirectory(munit)

add_library(test_lib util.c)
target_link_libraries(test_lib clemens_65816 munit)

add_executable(test_clem test_clem.c)
target_link_libraries(test_clem test_lib)

add_executable(test_lcbank test_lcbank.c)
target_link_libraries(test_lcbank test_lib)

add_executable(test_c035_shadow test_c035_shadow.c)
target_link_libraries(test_c035_shadow test_lib)

# add_executable(test_woz test_woz.c)
# target_link_libraries(test_woz test_lib)

add_test(NAME clem COMMAND test_clem)
add_test(NAME lcbank COMMAND test_lcbank)
add_test(NAME shadow COMMAND test_c035_shadow)
# add_test(NAME disk COMMAND test_woz WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
