add_library(hf_transport_common STATIC
    LinkMap.cpp
    log.cpp
    PersistentStorageHelpers.cpp
)

target_include_directories(hf_transport_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${RACE_SDK_INCLUDE_DIR})
target_link_libraries(hf_transport_common PUBLIC ${LIB_DEPS})