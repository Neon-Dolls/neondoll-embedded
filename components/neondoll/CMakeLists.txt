set(wg_sources)
if(WIREGUARD_SOURCE_DIR)
    set(wg_sources "src/esp32_wireguard.cpp")
endif()
idf_component_register(SRCS ${wg_sources} "src/neondoll.cpp"
                           "src/identity.cpp"
                           "src/identity_storage.cpp"
                           "src/esp32_platform.cpp"
                           "src/esp32_network.cpp"
                           "src/network_supervisor.cpp"
                           "src/wireguard_transport.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES lwip mbedtls esp_system nvs_flash esp_timer bootloader_support log esp_wifi esp_netif esp_event freertos)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

set_target_properties(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS YES)

if(WIREGUARD_SOURCE_DIR)
    idf_component_optional_requires(PUBLIC wireguard)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC NEONDOLL_HAS_WIREGUARD=1)
endif()
