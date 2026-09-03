cmake_minimum_required(VERSION 3.25)
project(Meg VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

include(CTest)
add_subdirectory(targets)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
