cmake_minimum_required(VERSION 3.17)
project(flatco VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

string(TOLOWER ${CMAKE_SYSTEM_NAME} osname)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${osname})

set(FLATCO_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${FLATCO_INC_DIR})

add_subdirectory(./src)
add_subdirectory(./test)
