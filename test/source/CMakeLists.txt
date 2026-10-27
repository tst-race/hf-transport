# Built from the top-level CMakeLists.txt when -DBUILD_UNIT_TESTS=ON (./build_artifacts_in_docker_image.sh
# --unit-tests). Run them with ../../run_unit_tests.sh (they need the raceboat-runtime image).
find_package(GTest REQUIRED CONFIG)
find_package(Threads REQUIRED)

# Same dependency set as source/CMakeLists.txt (its variables are not visible from here).
set(RACE_SDK_INCLUDE_DIR /linux/x86_64/include)
find_library(LIB_CURL curl)
find_library(LIB_CPPREST cpprest)
find_library(LIB_CRYPTO crypto)
find_library(LIB_SSL ssl)
set(LIB_UUID /usr/lib/x86_64-linux-gnu/libuuid.so.1)
find_library(RACE_SDK_COMMON raceSdkCommon REQUIRED)
set(LIB_DEPS ${RACE_SDK_COMMON} ${LIB_CURL} ${LIB_CPPREST} ${LIB_CRYPTO} ${LIB_SSL} ${LIB_UUID})

set(PLUGIN_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../source)

add_executable(hf_transport_tests
    ${PLUGIN_SOURCE_DIR}/common/LinkMap.cpp
    ${PLUGIN_SOURCE_DIR}/common/PersistentStorageHelpers.cpp
    ${PLUGIN_SOURCE_DIR}/common/log.cpp
    ${PLUGIN_SOURCE_DIR}/transport/HfTransportStub.cpp
    ${PLUGIN_SOURCE_DIR}/transport/Link.cpp
    ${PLUGIN_SOURCE_DIR}/transport/PluginHFTransport.cpp
    main.cpp
    transport/TestLinkAddress.cpp
    transport/TestLinkMap.cpp
    transport/TestPluginHFTransport.cpp
)

# TESTBUILD removes the extern "C" createTransport/destroyTransport entry points.
target_compile_definitions(hf_transport_tests PRIVATE TESTBUILD BUILD_VERSION="test")

target_include_directories(hf_transport_tests PRIVATE
    common
    ${PLUGIN_SOURCE_DIR}/common
    ${PLUGIN_SOURCE_DIR}/transport
    ${RACE_SDK_INCLUDE_DIR}
)

target_link_libraries(hf_transport_tests
    GTest::gmock
    GTest::gtest
    ${LIB_DEPS}
    Threads::Threads
    stdc++fs
)

# The builder image's libraceSdkCommon.so was built against a newer glibc than the image ships, so
# the binary links here but can only RUN in the raceboat-runtime image (see run_unit_tests.sh).
target_link_options(hf_transport_tests PRIVATE -Wl,--allow-shlib-undefined)

add_test(NAME hf_transport_tests COMMAND hf_transport_tests)
