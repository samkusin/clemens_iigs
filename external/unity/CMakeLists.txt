add_library(unity STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/unity.c)

target_compile_definitions(unity PUBLIC UNITY_INCLUDE_CONFIG_H)
target_include_directories(unity PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
