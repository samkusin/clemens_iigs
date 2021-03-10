add_library(ctest_unity STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/unity.c)
target_include_directories(ctest_unity PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
