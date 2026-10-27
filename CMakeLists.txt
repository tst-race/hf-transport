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

# Unit tests are built only on request: cmake -DBUILD_UNIT_TESTS=ON (./build_artifacts_in_docker_image.sh
# --unit-tests); run them with ./run_unit_tests.sh.
option(BUILD_UNIT_TESTS "Build the gtest unit tests" OFF)
if(BUILD_UNIT_TESTS)
    enable_testing()
    add_subdirectory(test/source)
endif()
