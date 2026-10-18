idf_component_register(SRCS "src/neondoll.cpp"
                           "src/identity.cpp"
                           "src/identity_storage.cpp"
                           "src/esp32_platform.cpp"
                           "src/esp32_network.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_system nvs_flash esp_timer bootloader_support log esp_wifi esp_netif esp_event freertos)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

set_target_properties(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS YES)
