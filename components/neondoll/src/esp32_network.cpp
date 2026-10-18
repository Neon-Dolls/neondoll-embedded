// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#include <neondoll/esp32_network.hpp>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <cstring>
#include <limits>
#include <mutex>

namespace neondoll {
namespace {
constexpr unsigned queue_depth = 16;
struct Event { uintptr_t epoch; esp_event_base_t base; int32_t id; uint32_t address; };
// Station ownership and residual cleanup outlive an adapter if IDF teardown
// fails. No new owner registers handlers until residual resources are released.
struct Resources {
    Esp32NetworkAvailability* owner = nullptr;
    QueueHandle_t queue = nullptr;
    esp_netif_t* netif = nullptr;
    esp_event_handler_instance_t wifi_handler = nullptr, ip_handler = nullptr;
    bool own_loop = false, wifi = false;
} resources;
struct Ingress {
    std::mutex mutex;
    uintptr_t epoch = 0;
    QueueHandle_t queue = nullptr;
    esp_netif_t* netif = nullptr;
    bool overflow = false;
} ingress;
uintptr_t next_epoch = 0; // Owner-task only; refuse wrap rather than reuse tokens.

void handle_event(void* arg, esp_event_base_t base, int32_t id, void* data) {
    const auto epoch = reinterpret_cast<uintptr_t>(arg);
    std::lock_guard<std::mutex> lock(ingress.mutex);
    if (!epoch || epoch != ingress.epoch || !ingress.queue) return;
    uint32_t address = 0;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        if (!data) return;
        const auto* event = static_cast<ip_event_got_ip_t*>(data);
        if (event->esp_netif != ingress.netif) return;
        address = event->ip_info.ip.addr;
    } else if (!(base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) &&
               !(base == WIFI_EVENT && (id == WIFI_EVENT_STA_DISCONNECTED ||
                                        id == WIFI_EVENT_STA_STOP))) return;
    Event event{epoch, base, id, address};
    if (xQueueSend(ingress.queue, &event, 0) != pdTRUE) ingress.overflow = true;
}
void disable_ingress() {
    std::lock_guard<std::mutex> lock(ingress.mutex);
    ingress.epoch = 0; ingress.queue = nullptr; ingress.netif = nullptr;
    ingress.overflow = false;
}
int release_station() {
    disable_ingress(); // Synchronize with senders before unregistering/freeing.
    int result = ESP_OK;
    auto check = [&](int err) { if (err != ESP_OK && result == ESP_OK) result = err; };
    if (resources.wifi_handler) {
        auto err = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, resources.wifi_handler);
        check(err); if (err == ESP_OK) resources.wifi_handler = nullptr;
    }
    if (resources.ip_handler) {
        auto err = esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, resources.ip_handler);
        check(err); if (err == ESP_OK) resources.ip_handler = nullptr;
    }
    if (resources.wifi) {
        // Queued stop events and prior cleanup attempts do not establish the
        // current driver state. Always stop before each deinit attempt.
        auto err = esp_wifi_stop();
        if (err == ESP_ERR_WIFI_NOT_STARTED) err = ESP_OK;
        check(err);
    }
    if (resources.wifi) {
        auto err = esp_wifi_deinit();
        check(err);
        if (err == ESP_OK) resources.wifi = false;
    }
    if (!resources.wifi && resources.netif) {
        esp_netif_destroy_default_wifi(resources.netif); resources.netif = nullptr;
    }
    if (resources.own_loop && !resources.wifi && !resources.wifi_handler && !resources.ip_handler) {
        auto err = esp_event_loop_delete_default();
        check(err); if (err == ESP_OK) resources.own_loop = false;
    }
    return result;
}
}

Esp32NetworkAvailability::~Esp32NetworkAvailability() { deinit(); }
int Esp32NetworkAvailability::failed(int error) { error_ = error; return -3; }
int Esp32NetworkAvailability::init() {
    if (polling_) return -2;
    if (initialized_) return 0;
    if (resources.owner) return -2;
    auto err = release_station();
    if (err != ESP_OK) return failed(err);
    resources.queue = xQueueCreate(queue_depth, sizeof(Event));
    if (!resources.queue) return failed(ESP_ERR_NO_MEM);
    resources.owner = this;
    initialized_ = true; error_ = 0;
    return 0;
}
int Esp32NetworkAvailability::set_callback(Callback cb, void* arg) {
    if (polling_) return -2;
    if (!initialized_) return -1;
    if (cb == callback_ && arg == callback_arg_) return 0;
    callback_ = cb; callback_arg_ = arg; pending_ = cb != nullptr;
    return 0;
}
int Esp32NetworkAvailability::start(const char* ssid, const char* password, uint8_t limit) {
    if (polling_) return -2;
    if (!initialized_ || !ssid || !password) return -1;
    if (started_) return -2;
    const size_t ssid_size = strnlen(ssid, 33), password_size = strnlen(password, 65);
    if (!ssid_size || ssid_size > 32 || password_size > 64) return -1;
    auto err = release_station();
    if (err != ESP_OK) return failed(err);
    if (next_epoch == std::numeric_limits<uintptr_t>::max()) return failed(ESP_ERR_INVALID_STATE);
    epoch_ = ++next_epoch;
    xQueueReset(resources.queue);
    err = esp_netif_init();
    if (err == ESP_OK) {
        err = esp_event_loop_create_default();
        if (err == ESP_OK) resources.own_loop = true;
        else if (err == ESP_ERR_INVALID_STATE) err = ESP_OK; // Application-owned loop.
    }
    if (err == ESP_OK && esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) err = ESP_ERR_INVALID_STATE;
    if (err == ESP_OK) {
        resources.netif = esp_netif_create_default_wifi_sta();
        if (!resources.netif) err = ESP_ERR_NO_MEM;
    }
    if (err == ESP_OK) {
        wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
        config.nvs_enable = 0; // Credentials and radio configuration stay in RAM.
        err = esp_wifi_init(&config);
        if (err == ESP_OK) resources.wifi = true;
    }
    if (err == ESP_OK) err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) {
        wifi_config_t config{};
        std::memcpy(config.sta.ssid, ssid, ssid_size);
        std::memcpy(config.sta.password, password, password_size);
        err = esp_wifi_set_config(WIFI_IF_STA, &config);
        volatile unsigned char* bytes = reinterpret_cast<volatile unsigned char*>(&config);
        for (size_t i = 0; i < sizeof(config); ++i) bytes[i] = 0;
    }
    if (err == ESP_OK) {
        std::lock_guard<std::mutex> lock(ingress.mutex);
        ingress.epoch = epoch_; ingress.queue = resources.queue;
        ingress.netif = resources.netif; ingress.overflow = false;
    }
    if (err == ESP_OK) err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        handle_event, reinterpret_cast<void*>(epoch_), &resources.wifi_handler);
    if (err == ESP_OK) err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
        handle_event, reinterpret_cast<void*>(epoch_), &resources.ip_handler);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) { release_station(); return failed(err); }
    started_ = true; desired_ = false;
    retry_limit_ = limit; retries_ = 0; error_ = 0;
    return 0;
}
int Esp32NetworkAvailability::connect() {
    if (polling_) return -2;
    if (!initialized_ || !started_) return -1;
    if (desired_) return 0;
    auto err = esp_wifi_connect();
    if (err != ESP_OK) return failed(err);
    desired_ = true; retries_ = 0; error_ = 0;
    return 0;
}
void Esp32NetworkAvailability::transition(bool available, uint32_t address) {
    if (available_ == available && address_ == address) return;
    change_ = available ? (available_ ? Change::changed : Change::available) : Change::offline;
    available_ = available; address_ = address; pending_ = true;
}
void Esp32NetworkAvailability::deliver() {
    if (!pending_) return;
    pending_ = false;
    if (callback_) callback_(callback_arg_, available_);
}
int Esp32NetworkAvailability::disconnect() {
    if (polling_) return -2;
    if (!initialized_ || !started_) return -1;
    desired_ = false; transition(false);
    auto err = esp_wifi_disconnect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT) return failed(err);
    error_ = 0; return 0;
}
int Esp32NetworkAvailability::stop() {
    if (polling_) return -2;
    if (!initialized_) return 0;
    desired_ = false; started_ = false; epoch_ = 0;
    auto err = release_station();
    xQueueReset(resources.queue);
    transition(false);
    return err == ESP_OK ? 0 : failed(err);
}
int Esp32NetworkAvailability::deinit() {
    if (polling_) return -2;
    if (!initialized_) return 0;
    callback_ = nullptr; callback_arg_ = nullptr; pending_ = false;
    auto result = stop();
    vQueueDelete(resources.queue); resources.queue = nullptr; resources.owner = nullptr;
    initialized_ = false; available_ = false; address_ = 0; change_ = Change::offline; pending_ = false;
    return result;
}
int Esp32NetworkAvailability::poll() {
    if (!initialized_) return -1;
    if (polling_) return -2;
    polling_ = true;
    struct Guard { bool& flag; ~Guard() { flag = false; } } guard{polling_};
    bool overflow;
    {
        std::lock_guard<std::mutex> lock(ingress.mutex);
        overflow = ingress.overflow; ingress.overflow = false;
    }
    if (overflow) {
        desired_ = false; xQueueReset(resources.queue); transition(false); deliver();
        if (started_) esp_wifi_disconnect();
        error_ = ESP_ERR_NO_MEM; return -4;
    }
    deliver();
    Event event{};
    for (unsigned count = 0; count < queue_depth && xQueueReceive(resources.queue, &event, 0) == pdTRUE; ++count) {
        if (event.epoch != epoch_ || !started_) continue;
        if (event.base == IP_EVENT && event.id == IP_EVENT_STA_GOT_IP) {
            if (desired_ && event.address) { transition(true, event.address); retries_ = 0; error_ = 0; }
        } else if (event.base == IP_EVENT && event.id == IP_EVENT_STA_LOST_IP) {
            // Ignore delayed lost-IP timers if the interface already has an IP.
            esp_netif_ip_info_t info{};
            auto err = esp_netif_get_ip_info(resources.netif, &info);
            if (err != ESP_OK || !info.ip.addr) transition(false);
        } else {
            transition(false);
            if (event.id == WIFI_EVENT_STA_STOP) { desired_ = false; started_ = false; }
            else if (desired_) {
                if (retries_ < retry_limit_) {
                    ++retries_;
                    auto err = esp_wifi_connect();
                    if (err != ESP_OK) { desired_ = false; error_ = err; }
                } else { desired_ = false; error_ = ESP_FAIL; }
            }
        }
        deliver();
    }
    return error_ == ESP_OK ? 0 : -3;
}
} // namespace neondoll
