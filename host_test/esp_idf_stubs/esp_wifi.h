#pragma once
#include "esp_err.h"
#include <cstdint>
struct wifi_init_config_t { int value; int nvs_enable; };
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{0,1}
struct wifi_sta_config_t { uint8_t ssid[32]; uint8_t password[64]; };
struct wifi_config_t { wifi_sta_config_t sta; };
constexpr int WIFI_STORAGE_RAM=1, WIFI_MODE_STA=1, WIFI_IF_STA=0;
constexpr int WIFI_EVENT_STA_DISCONNECTED=20, WIFI_EVENT_STA_STOP=21, WIFI_EVENT_STA_START=22, WIFI_EVENT_STA_CONNECTED=23;
esp_err_t esp_wifi_init(const wifi_init_config_t*);
esp_err_t esp_wifi_deinit();
esp_err_t esp_wifi_set_storage(int);
esp_err_t esp_wifi_set_mode(int);
esp_err_t esp_wifi_set_config(int,wifi_config_t*);
esp_err_t esp_wifi_start();
esp_err_t esp_wifi_stop();
esp_err_t esp_wifi_connect();
esp_err_t esp_wifi_disconnect();
