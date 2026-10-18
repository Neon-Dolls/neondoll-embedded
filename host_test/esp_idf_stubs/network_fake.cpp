#include "network_fake.hpp"
#include "freertos/queue.h"
#include <algorithm>
#include <cstring>
#include <deque>
#include <mutex>

struct FakeQueue {
    size_t capacity, item_size;
    std::mutex mutex;
    std::deque<std::vector<unsigned char>> items;
};
namespace network_fake {
State& state() { static State s; return s; }
void reset() { state()=State{}; }
int call(const char* name) {
    state().calls.emplace_back(name);
    return state().fail==name ? ESP_FAIL : ESP_OK;
}
void emit(esp_event_base_t base,int32_t id,void* data) {
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_STOP)state().started=false;
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED)state().address=0;
    const auto handlers=state().handlers;
    for(const auto& handler:handlers)
        if(handler.base==base)handler.callback(handler.arg,base,id,data);
}
void got_ip(uint32_t address) {
    state().address=address;
    ip_event_got_ip_t event{&state().netif,{{address},{0},{0}},true};
    emit(IP_EVENT,IP_EVENT_STA_GOT_IP,&event);
}
}
using network_fake::state;
using network_fake::call;
esp_event_base_t WIFI_EVENT="WIFI", IP_EVENT="IP";
QueueHandle_t xQueueCreate(unsigned size,unsigned item_size) {
    if(state().queue_fail)return nullptr;
    ++state().queues;
    auto* queue=new FakeQueue;queue->capacity=size;queue->item_size=item_size;return queue;
}
int xQueueSend(QueueHandle_t queue,const void* item,unsigned) {
    std::lock_guard<std::mutex> lock(queue->mutex);
    if(queue->items.size()==queue->capacity)return pdFALSE;
    const auto* bytes=static_cast<const unsigned char*>(item);
    queue->items.emplace_back(bytes,bytes+queue->item_size);return pdTRUE;
}
int xQueueReceive(QueueHandle_t queue,void* item,unsigned) {
    std::lock_guard<std::mutex> lock(queue->mutex);
    if(queue->items.empty())return pdFALSE;
    std::memcpy(item,queue->items.front().data(),queue->item_size);queue->items.pop_front();return pdTRUE;
}
int xQueueReset(QueueHandle_t queue) { std::lock_guard<std::mutex> lock(queue->mutex);queue->items.clear();return pdTRUE; }
void vQueueDelete(QueueHandle_t queue){--state().queues;delete queue;}
int esp_netif_init(){return call("netif_init");}
int esp_event_loop_create_default(){
    auto err=call("loop_create");if(err)return err;
    if(state().loop)return ESP_ERR_INVALID_STATE;
    state().loop=true;return ESP_OK;
}
int esp_event_loop_delete_default(){auto err=call("loop_delete");if(!err)state().loop=false;return err;}
esp_netif_t* esp_netif_get_handle_from_ifkey(const char*) {return state().external_netif ? &state().netif : nullptr;}
esp_netif_t* esp_netif_create_default_wifi_sta(){
    call("netif_create");if(state().netif_fail)return nullptr;
    ++state().interfaces;return &state().netif;
}
void esp_netif_destroy_default_wifi(void*){call("netif_destroy");--state().interfaces;}
int esp_netif_get_ip_info(esp_netif_t*,esp_netif_ip_info_t* info){
    auto err=call("get_ip");info->ip.addr=state().address;return err;
}
int esp_event_handler_instance_register(esp_event_base_t base,int32_t,esp_event_handler_t cb,void* arg,void** out){
    auto err=call(base==WIFI_EVENT ? "register_wifi" : "register_ip");if(err)return err;
    auto id=++state().next_handler;state().handlers.push_back({base,cb,arg,id});*out=reinterpret_cast<void*>(id);return ESP_OK;
}
int esp_event_handler_instance_unregister(esp_event_base_t base,int32_t,void* instance){
    auto err=call(base==WIFI_EVENT ? "unregister_wifi" : "unregister_ip");if(err)return err;
    auto& handlers=state().handlers;
    handlers.erase(std::remove_if(handlers.begin(),handlers.end(),[&](const auto& h){return h.id==reinterpret_cast<uintptr_t>(instance);}),handlers.end());return ESP_OK;
}
int esp_wifi_init(const wifi_init_config_t* config){if(config->nvs_enable)return ESP_FAIL;auto err=call("wifi_init");if(!err)state().wifi=true;return err;}
int esp_wifi_deinit(){auto err=call("wifi_deinit");if(!err && state().started)return ESP_ERR_WIFI_NOT_STOPPED;if(!err){state().wifi=false;state().started=false;}return err;}
int esp_wifi_set_storage(int storage){if(storage!=WIFI_STORAGE_RAM)return ESP_FAIL;return call("storage_ram");}
int esp_wifi_set_mode(int mode){if(mode!=WIFI_MODE_STA)return ESP_FAIL;return call("mode_sta");}
int esp_wifi_set_config(int,wifi_config_t* config){
    state().ssid.assign(reinterpret_cast<char*>(config->sta.ssid),strnlen(reinterpret_cast<char*>(config->sta.ssid),32));
    state().password.assign(reinterpret_cast<char*>(config->sta.password),strnlen(reinterpret_cast<char*>(config->sta.password),64));return call("wifi_config");
}
int esp_wifi_start(){auto err=call("wifi_start");if(!err)state().started=true;return err;}
int esp_wifi_stop(){auto err=call("wifi_stop");if(!err && !state().started)return ESP_ERR_WIFI_NOT_STARTED;if(!err)state().started=false;return err;}
int esp_wifi_connect(){++state().connect_calls;return call("connect");}
int esp_wifi_disconnect(){++state().disconnect_calls;return call("disconnect");}
