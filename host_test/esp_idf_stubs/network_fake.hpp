#pragma once
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <map>
#include <string>
#include <vector>
namespace network_fake {
struct Handler { esp_event_base_t base; esp_event_handler_t callback; void* arg; uintptr_t id; };
struct State {
    std::string fail;
    bool loop=false, wifi=false, started=false, queue_fail=false, netif_fail=false;
    bool external_netif=false;
    esp_netif_t netif{1};
    uint32_t address=0;
    int queues=0, interfaces=0, connect_calls=0, disconnect_calls=0;
    uintptr_t next_handler=0;
    std::vector<std::string> calls;
    std::vector<Handler> handlers;
    std::string ssid, password;
};
State& state();
void reset();
void emit(esp_event_base_t,int32_t,void* = nullptr);
void got_ip(uint32_t);
}
