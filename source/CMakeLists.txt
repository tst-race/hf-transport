# Common dependencies
if(ANDROID)
    find_library(LIB_CURL curl)
    find_package(Boost COMPONENTS filesystem system REQUIRED)
    find_library(LIB_CPPREST cpprest)
    find_library(LIB_LOG log)
    set(LIB_DEPS ${LIB_CURL} Boost::system Boost::filesystem ${LIB_CPPREST} ${LIB_LOG} ${RACE_JAVA_SHIMS})
else()
    find_library(LIB_CURL curl)
    find_library(LIB_CPPREST cpprest)
    find_library(LIB_CRYPTO crypto)
    find_library(LIB_SSL ssl)
    # UUID library - find_library doesn't find libuuid.so.1 symlink, use direct path
    set(LIB_UUID /usr/lib/x86_64-linux-gnu/libuuid.so.1)
    find_library(RACE_SDK_COMMON raceSdkCommon HINTS ENV LD_LIBRARY_PATH)
    set(LIB_DEPS ${RACE_SDK_COMMON} ${LIB_CURL} ${LIB_CPPREST} ${LIB_CRYPTO} ${LIB_SSL} ${LIB_UUID})
endif()

set(PLUGIN_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../kit)
set(COMMON_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)
set(RACE_SDK_INCLUDE_DIR /linux/x86_64/include)

function(setup_component_target)
    cmake_parse_arguments(COMPONENT "" "TARGET" "SOURCES" ${ARGN})
    add_library(${COMPONENT_TARGET} SHARED ${COMPONENT_SOURCES})
    target_include_directories(${COMPONENT_TARGET} PRIVATE ${COMMON_SRC_DIR} ${RACE_SDK_INCLUDE_DIR})
    target_link_libraries(${COMPONENT_TARGET} PRIVATE ${LIB_DEPS})
    target_compile_definitions(${COMPONENT_TARGET} PUBLIC BUILD_VERSION="${BUILD_VERSION}")
    set_target_properties(${COMPONENT_TARGET} PROPERTIES CXX_VISIBILITY_PRESET hidden NO_SONAME ON)
    
    # Install to kit artifacts
    if(ANDROID)
        # Android artifact copying would go here
    else()
        add_custom_command(TARGET ${COMPONENT_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-client/PluginHFTransport
            COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginHFTransport
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${COMPONENT_TARGET}> ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-client/PluginHFTransport/
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${COMPONENT_TARGET}> ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginHFTransport/
        )
    endif()
endfunction()

# Manifest handling
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/manifest.json
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json ${CMAKE_CURRENT_BINARY_DIR}/manifest.json
    COMMENT "Copying manifest.json"
)

add_custom_command(
    OUTPUT ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-client/PluginHFTransport/manifest.json
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-client/PluginHFTransport
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-client/PluginHFTransport/manifest.json
    COMMENT "Installing manifest.json to client kit"
)

add_custom_command(
    OUTPUT ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginHFTransport/manifest.json
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginHFTransport
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json ${PLUGIN_BASE_DIR}/artifacts/${NODE_TYPE}-server/PluginHFTransport/manifest.json
    COMMENT "Installing manifest.json to server kit"
)

add_subdirectory(common)
add_subdirectory(transport)