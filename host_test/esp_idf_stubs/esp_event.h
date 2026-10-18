#pragma once
#include "esp_err.h"
#include <cstdint>
using esp_event_base_t = const char*;
using esp_event_handler_instance_t = void*;
using esp_event_handler_t = void (*)(void*, esp_event_base_t, int32_t, void*);
extern esp_event_base_t WIFI_EVENT, IP_EVENT;
constexpr int32_t ESP_EVENT_ANY_ID=-1;
esp_err_t esp_event_loop_create_default();
esp_err_t esp_event_loop_delete_default();
esp_err_t esp_event_handler_instance_register(esp_event_base_t,int32_t,esp_event_handler_t,void*,esp_event_handler_instance_t*);
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t,int32_t,esp_event_handler_instance_t);
