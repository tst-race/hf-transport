cmake_minimum_required(VERSION 3.20)
project(hf-stego-transport LANGUAGES CXX)

set(CMAKE_MODULE_PATH /opt/race/race-cmake-modules)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(BUILD_VERSION "" CACHE STRING "Version of the plugin being built")
if("${BUILD_VERSION}" STREQUAL "")
    message(SEND_ERROR "Missing or empty BUILD_VERSION argument")
endif()

include(race/clang-format)
include(race/coverage)
include(race/lint)
include(race/test-targets)
include(race/valgrind)
include(race/warnings)

setup_project_test_targets(hf-stego-transport UNIT)
setup_project_format_targets(hf-stego-transport)

add_subdirectory(source)
# add_subdirectory(test)  # Disabled for now