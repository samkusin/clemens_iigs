
add_subdirectory(munit)

add_executable(test_clem test_clem.c util.c)
target_link_libraries(test_clem munit clemens_65816)

add_executable(test_lcbank test_lcbank.c util.c)
target_link_libraries(test_lcbank munit clemens_65816)
