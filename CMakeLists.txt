cmake_minimum_required(VERSION 3.20)
project(hf-stego-transport LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_VERSION "" CACHE STRING "Version of the plugin being built")
if("${BUILD_VERSION}" STREQUAL "")
    message(SEND_ERROR "Missing or empty BUILD_VERSION argument")
endif()

# SDK paths (provided by raceboat-plugin-builder image)
set(SDK_PREFIX "/linux/x86_64")
if(NOT EXISTS "${SDK_PREFIX}")
    # Try arm64
    set(SDK_PREFIX "/linux/arm64-v8a")
endif()

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

# Plugin kit directory
set(PLUGIN_KIT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kit/artifacts/linux-x86_64-server/PluginHFTransport)

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
target_compile_definitions(PluginHFTransport PUBLIC
    BUILD_VERSION="${BUILD_VERSION}"
)
set_target_properties(PluginHFTransport PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    NO_SONAME ON
    PREFIX "lib"
    SUFFIX ".so"
    OUTPUT_NAME "PluginHFTransport"
)

# Copy built library to kit directory
add_custom_command(TARGET PluginHFTransport POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_KIT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:PluginHFTransport> ${PLUGIN_KIT_DIR}/
    BYPRODUCTS ${PLUGIN_KIT_DIR}/libPluginHFTransport.so
)

# Copy manifest to kit directory
add_custom_command(
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_KIT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/source/manifest.json ${PLUGIN_KIT_DIR}/
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/manifest.json
    OUTPUT ${PLUGIN_KIT_DIR}/manifest.json
)
add_custom_target(PluginHFTransportManifest ALL DEPENDS ${PLUGIN_KIT_DIR}/manifest.json)
