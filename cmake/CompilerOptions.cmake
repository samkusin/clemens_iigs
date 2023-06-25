

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT}")
  string(REGEX REPLACE "/EHsc*" "" CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT}")
endif()

if (CMAKE_C_COMPILER_ID MATCHES "MSVC")
  string(REGEX REPLACE "/Ob0" "/Ob1" CMAKE_C_FLAGS_DEBUG_INIT "${CMAKE_C_FLAGS_DEBUG_INIT}")
endif()

#[[
get_cmake_property(_varNames VARIABLES)
list (REMOVE_DUPLICATES _varNames)
list (SORT _varNames)
foreach (_varName ${_varNames})
    if (_varName MATCHES "_INIT$")
        message(STATUS "${_varName}=${${_varName}}")
    endif()
endforeach()
]]
