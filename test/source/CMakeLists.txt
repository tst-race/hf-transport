cmake_minimum_required(VERSION 3.20)

# Find GTest
find_package(GTest REQUIRED)

# Race SDK paths
set(RACE_SDK_INCLUDE_DIR /linux/x86_64/include)

# Common dependencies (same as main CMakeLists.txt)
find_library(LIB_CURL curl)
find_library(LIB_CPPREST cpprest)
find_library(LIB_CRYPTO crypto)
find_library(LIB_SSL ssl)
set(LIB_UUID /usr/lib/x86_64-linux-gnu/libuuid.so.1)
find_library(RACE_SDK_COMMON raceSdkCommon HINTS ENV LD_LIBRARY_PATH)
set(LIB_DEPS ${RACE_SDK_COMMON} ${LIB_CURL} ${LIB_CPPREST} ${LIB_CRYPTO} ${LIB_SSL} ${LIB_UUID})

# Test executable
add_executable(hf_transport_tests
    main.cpp
    transport/TestLinkAddress.cpp
    transport/TestLinkMap.cpp
    transport/TestPluginHFTransport.cpp
)

target_include_directories(hf_transport_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/../source
    ${CMAKE_SOURCE_DIR}/../source/common
    ${CMAKE_SOURCE_DIR}/../source/transport
    ${RACE_SDK_INCLUDE_DIR}
)

target_link_libraries(hf_transport_tests PRIVATE
    GTest::GTest
    GTest::Main
    ${LIB_DEPS}
    hf_transport_common
)

# Enable testing
enable_testing()
add_test(NAME hf_transport_tests COMMAND hf_transport_tests)