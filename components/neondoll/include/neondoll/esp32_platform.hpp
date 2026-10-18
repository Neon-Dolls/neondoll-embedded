// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#ifndef NEONDOLL_ESP32_PLATFORM_HPP
#define NEONDOLL_ESP32_PLATFORM_HPP
#include <neondoll/platform.hpp>
#include <nvs.h>
#include <neondoll/esp32_network.hpp>

namespace neondoll {
/** NVS namespace "neondoll". Maps logical "neondoll/identity" to NVS "identity".
 * Never erases flash on initialization failure. Caller serializes all operations.
 */
class Esp32Storage : public Storage {
public:
    Esp32Storage() = default;
    Esp32Storage(const Esp32Storage&) = delete;
    Esp32Storage& operator=(const Esp32Storage&) = delete;
    ~Esp32Storage() override { deinit(); }
    int init() override;
    int deinit() override;
    std::ptrdiff_t read(const char* key, void* out, size_t max_size) override;
    int write(const char* key, const void* in, size_t size) override;
private:
    nvs_handle_t handle_ = 0;
    bool initialized_ = false;
    bool write_failed_ = false;
};
/** Bootstrap entropy: use before starting RF/ADC/I2S, with exclusive access to
 * the internal entropy source. A future live-radio adapter must use that radio's
 * documented entropy guarantees instead of enabling the bootstrap source.
 */
class Esp32EntropySource : public EntropySource {
public:
    int init() override { initialized_ = true; return 0; }
    int deinit() override { initialized_ = false; return 0; }
    int get_random(void* out, size_t len) override;
private:
    bool initialized_ = false;
};
class Esp32Clock : public Clock {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    uint64_t now_ms() override;
};
class Esp32Logger : public Logger {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    int log(int level, const char* tag, const char* msg) override;
};
} // namespace neondoll
#endif
