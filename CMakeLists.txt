cmake_minimum_required(VERSION 3.9)

project(mersive-http CXX)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)
include(BuildType)
add_subdirectory(src)
