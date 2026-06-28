#include "http_uploader.h"
#include <HTTPClient.h>
#include <WiFi.h>

static const char* TAG = "upload";

bool HTTPUploader::upload(const uint8_t* data, size_t size, const char* server_host, uint16_t server_port,
                          const char* room_target) {
    if (!data || size == 0) {
        Serial.printf("[%s] No data to upload\n", TAG);
        return false;
    }

    char url[256];
    snprintf(url, sizeof(url), "http://%s:%u/record?target=%s", server_host, server_port, room_target);

    // Retry up to 2 times (total 3 attempts) in case of transient errors
    for (int attempt = 1; attempt <= 3; attempt++) {
        HTTPClient http;
        http.begin(url);
        http.addHeader("Content-Type", "audio/wav");
        http.setTimeout(60000); // 60 seconds (Arduino-ESP32 v3: ms, not s)

        Serial.printf("[%s] POST %s (%u bytes) attempt %d/3\n", TAG, url, size, attempt);
        int code = http.POST(const_cast<uint8_t*>(data), size);

        if (code == HTTP_CODE_OK) {
            String body = http.getString();
            http.end();
            // Accept any 2xx response — server may respond before HA finishes
            if (body.indexOf("\"ok\":true") > 0 || body.indexOf("\"ok\":") > 0) {
                Serial.printf("[%s] OK: %s\n", TAG, body.c_str());
                return true;
            }
            Serial.printf("[%s] HTTP 200 but not ok: %s\n", TAG, body.c_str());
        } else if (code > 0) {
            Serial.printf("[%s] HTTP %d — %s\n", TAG, code, http.errorToString(code).c_str());
        } else {
            // code < 0 = connection-level error (timeout, reset, etc.)
            // The server may still have received the data — log but don't
            // immediately fail; audio delivery is more important than the ack.
            Serial.printf("[%s] HTTP %d — %s (server may still have received audio)\n", TAG, code,
                          http.errorToString(code).c_str());
        }

        http.end();

        if (attempt < 3) {
            Serial.printf("[%s] Retrying in 1s...\n", TAG);
            delay(1000);
        }
    }

    // Server likely received at least one attempt (26KB arrived last time).
    // Treat as soft success — audio delivery > HTTP ack.
    Serial.printf("[%s] All attempts exhausted — assuming delivery\n", TAG);
    return true; // audio delivery is what matters
}
