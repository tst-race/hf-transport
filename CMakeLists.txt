cmake_minimum_required(VERSION 3.16)
project(hf-stego-transport VERSION 1.0.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build shared libraries
set(BUILD_SHARED_LIBS ON)

# Detect architecture for SDK paths
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(TARGET_ARCH "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(TARGET_ARCH "arm64-v8a")
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# SDK paths (provided by raceboat-plugin-builder image)
set(SDK_PREFIX "/linux/${TARGET_ARCH}")
set(SDK_INCLUDE_DIR "${SDK_PREFIX}/include")
set(SDK_LIB_DIR "${SDK_PREFIX}/lib")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${SDK_INCLUDE_DIR}
)

# Find required packages
find_package(nlohmann_json 3.10 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# Raceboat SDK - use raceSdkCommon library
set(RACE_SDK_LIBRARY "${SDK_LIB_DIR}/libraceSdkCommon.so")
if(NOT EXISTS "${RACE_SDK_LIBRARY}")
    message(FATAL_ERROR "Raceboat SDK not found at ${RACE_SDK_LIBRARY}")
endif()

# Source files - ONLY transport component
set(TRANSPORT_SOURCES
    source/PluginHFTransport.cpp
    source/Link.cpp
    source/HFTransportStub.cpp
    source/log.cpp
)

# Common link libraries
set(COMMON_LIBS
    ${RACE_SDK_LIBRARY}
    nlohmann_json::nlohmann_json
    ${CURL_LIBRARIES}
    ssl crypto
)

# Transport component plugin
add_library(PluginHFTransport MODULE ${TRANSPORT_SOURCES})
target_link_libraries(PluginHFTransport ${COMMON_LIBS})
set_target_properties(PluginHFTransport PROPERTIES
    PREFIX "lib"
    SUFFIX ".so"
    OUTPUT_NAME "PluginHFTransport"
)

# Install targets - only transport library
install(TARGETS PluginHFTransport
    LIBRARY DESTINATION .
)

# Install manifest
install(FILES source/manifest.json DESTINATION .)

# CPack for packaging (optional)
include(CPack)