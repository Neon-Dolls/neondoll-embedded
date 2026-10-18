#pragma once
using esp_log_level_t = int;
constexpr int ESP_LOG_ERROR=1, ESP_LOG_WARN=2, ESP_LOG_INFO=3;
void esp_log_write(esp_log_level_t, const char*, const char*, ...);
