// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#ifndef NEONDOLL_ESP32_NETWORK_HPP
#define NEONDOLL_ESP32_NETWORK_HPP
#include <neondoll/platform.hpp>

namespace neondoll {
/** Single owned ESP32 station. Serialize all public methods/destruction on one
 * application task, and call poll() regularly. Callbacks run only in poll();
 * they may inspect getters but must not reenter lifecycle methods or destroy the
 * adapter. IDF's event task never calls application code or retains this object.
 * init() does not initialize/start Wi-Fi: call start() after M1 identity init.
 */
class Esp32NetworkAvailability : public NetworkAvailability {
public:
    enum class Change { offline, available, changed };
    Esp32NetworkAvailability() = default;
    ~Esp32NetworkAvailability() override;
    Esp32NetworkAvailability(const Esp32NetworkAvailability&) = delete;
    Esp32NetworkAvailability& operator=(const Esp32NetworkAvailability&) = delete;
    int init() override;
    int deinit() override;
    int set_callback(Callback cb, void* arg) override;
    // Credentials and retry policy belong to the application. Stored in RAM only.
    // start() starts STA without connecting; connect() requests association.
    int start(const char* ssid, const char* password, uint8_t reconnect_limit = 3);
    int connect();
    int disconnect();
    int stop();
    // Bounded drain (16 events). Overflow disconnects and fails closed; the
    // application must explicitly connect() again. Returns -4 on overflow.
    int poll();
    bool available() const { return available_; }
    uint32_t ipv4_address() const { return address_; } // network byte order
    Change last_change() const { return change_; }
    int last_error() const { return error_; } // ESP-IDF error, 0 when recovered
private:
    void transition(bool available, uint32_t address = 0);
    void deliver();
    int failed(int error);
    bool initialized_ = false, started_ = false, desired_ = false;
    bool available_ = false, pending_ = false, polling_ = false;
    uint8_t retries_ = 0, retry_limit_ = 3;
    uintptr_t epoch_ = 0;
    uint32_t address_ = 0;
    int error_ = 0;
    Change change_ = Change::offline;
    Callback callback_ = nullptr;
    void* callback_arg_ = nullptr;
};
} // namespace neondoll
#endif
