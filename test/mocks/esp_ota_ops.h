#pragma once
#include <Arduino.h>
#include <cstdint>

#define ESP_OK 0

using esp_err_t = int;

struct esp_partition_t {
    uint8_t subtype = 0;
    const char* label = "factory";
    uint32_t address = 0;
};

inline esp_partition_t _mock_running_partition{};
inline esp_partition_t* _mock_next_update_partition = nullptr;

inline const esp_partition_t* esp_ota_get_running_partition() {
    return &_mock_running_partition;
}

inline const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*) {
    return _mock_next_update_partition;
}

inline esp_err_t esp_ota_set_boot_partition(const esp_partition_t*) {
    return ESP_OK;
}

inline void esp_ota_mark_app_valid_cancel_rollback() {}

inline void esp_ota_mark_app_invalid_rollback_and_reboot() {
    ESP.restart();
}
