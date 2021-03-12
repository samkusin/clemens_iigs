
add_subdirectory(munit)

add_executable(test_clem clem.c)
target_link_libraries(test_clem munit clemens_65816)
