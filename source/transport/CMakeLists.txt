setup_component_target(
    TARGET HFTransport
    SOURCES
        PluginHFTransport.cpp
        Link.cpp
        HfTransportStub.cpp
        ../common/LinkMap.cpp
        ../common/log.cpp
)

target_link_libraries(HFTransport PUBLIC hf_transport_common stdc++fs)