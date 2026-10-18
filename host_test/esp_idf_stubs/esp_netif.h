#pragma once
#include "esp_err.h"
#include "esp_event.h"
struct esp_netif_t { int id; };
struct esp_ip4_addr_t { uint32_t addr; };
struct esp_netif_ip_info_t { esp_ip4_addr_t ip{}, netmask{}, gw{}; };
struct ip_event_got_ip_t { esp_netif_t* esp_netif; esp_netif_ip_info_t ip_info; bool ip_changed; };
constexpr int IP_EVENT_STA_GOT_IP=10, IP_EVENT_STA_LOST_IP=11;
esp_err_t esp_netif_init();
esp_netif_t* esp_netif_get_handle_from_ifkey(const char*);
esp_err_t esp_netif_get_ip_info(esp_netif_t*,esp_netif_ip_info_t*);
