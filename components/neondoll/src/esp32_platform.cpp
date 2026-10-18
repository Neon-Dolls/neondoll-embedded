// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#include <neondoll/esp32_platform.hpp>
#include <bootloader_random.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <cstring>

namespace neondoll {
namespace {
// A committed creation marker prevents an interrupted first write (or lost blob)
// from being mistaken for a fresh device on the next boot.
constexpr char marker_key[] = "identity_init";
constexpr char marker_value[] = "NDI1";
int identity_marker(nvs_handle_t handle) {
    char value[sizeof(marker_value)]{};
    size_t size = sizeof(value);
    const auto err = nvs_get_blob(handle, marker_key, value, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) return 0;
    if (err == ESP_ERR_NVS_TYPE_MISMATCH || err == ESP_ERR_NVS_INVALID_LENGTH) return -4;
    if (err != ESP_OK) return -3;
    return size == sizeof(marker_value) &&
        std::memcmp(value, marker_value, sizeof(value)) == 0 ? 1 : -4;
}
const char* nvs_key(const char* key) {
    if (!key) return nullptr;
    if (std::strcmp(key, "neondoll/identity") == 0) return "identity";
    const auto len = std::strlen(key);
    if (!len || len > 15 || std::strchr(key, '/') || std::strcmp(key, marker_key) == 0)
        return nullptr;
    return key;
}
}
int Esp32Storage::init() {
    if (initialized_) return 0;
    // In particular, do NOT erase on NO_FREE_PAGES or NEW_VERSION_FOUND.
    auto err = nvs_flash_init();
    if (err != ESP_OK) {
        esp_log_write(ESP_LOG_ERROR, "neondoll", "NVS initialization failed: %d; no erase performed\n", err);
        return -3;
    }
    err = nvs_open("neondoll", NVS_READWRITE, &handle_);
    if (err != ESP_OK) return -3;
    initialized_ = true;
    write_failed_ = false;
    return 0;
}
int Esp32Storage::deinit() {
    if (initialized_) nvs_close(handle_);
    initialized_ = false;
    handle_ = 0;
    // NVS is shared with the application. Do not globally deinitialize it.
    return 0;
}
std::ptrdiff_t Esp32Storage::read(const char* key, void* out, size_t max_size) {
    const auto* mapped = nvs_key(key);
    if (!initialized_ || write_failed_ || !mapped || !out) return -3;
    size_t size = 0;
    auto err = nvs_get_blob(handle_, mapped, nullptr, &size);
    if (std::strcmp(mapped, "identity") == 0) {
        const int marker = identity_marker(handle_);
        if (marker < 0) return marker;
        if (err == ESP_ERR_NVS_NOT_FOUND && marker == 1) return -4;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) return -1;
    if (err == ESP_ERR_NVS_TYPE_MISMATCH) return -4;
    if (err != ESP_OK) return -3;
    if (size > max_size) return -2;
    // An empty existing blob remains an existing (corrupt) identity.
    if (size == 0) return 0;
    err = nvs_get_blob(handle_, mapped, out, &size);
    if (err == ESP_ERR_NVS_INVALID_LENGTH) return -2;
    if (err != ESP_OK) return -3;
    return static_cast<std::ptrdiff_t>(size);
}
int Esp32Storage::write(const char* key, const void* in, size_t size) {
    const auto* mapped = nvs_key(key);
    if (!initialized_ || write_failed_ || !mapped || !in) return -3;
    esp_err_t err = ESP_OK;
    if (std::strcmp(mapped, "identity") == 0) {
        const int marker = identity_marker(handle_);
        if (marker < 0) { write_failed_ = true; return -3; }
        if (marker == 0) {
            err = nvs_set_blob(handle_, marker_key, marker_value, sizeof(marker_value));
            if (err == ESP_OK) err = nvs_commit(handle_);
        }
    }
    if (err == ESP_OK) err = nvs_set_blob(handle_, mapped, in, size);
    if (err == ESP_OK) err = nvs_commit(handle_);
    if (err != ESP_OK) {
        // The durable outcome may be uncertain. Refuse further use until reopen.
        write_failed_ = true;
        esp_log_write(ESP_LOG_ERROR, "neondoll", "NVS identity write/commit failed: %d\n", err);
        return -3;
    }
    return 0;
}
int Esp32EntropySource::get_random(void* out, size_t len) {
    if (!initialized_ || !out || !len) return -1;
    bootloader_random_enable();
    esp_fill_random(out, len);
    bootloader_random_disable();
    return 0;
}
uint64_t Esp32Clock::now_ms() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }
int Esp32Logger::log(int level, const char* tag, const char* msg) {
    if (!tag || !msg) return -1;
    const auto severity = level >= 3 ? ESP_LOG_ERROR : level == 2 ? ESP_LOG_WARN : ESP_LOG_INFO;
    esp_log_write(severity, tag, "%.255s\n", msg);
    return 0;
}
} // namespace neondoll
