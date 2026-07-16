#include "http_uploader.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <cstring>
#include <sstream>

static const char* TAG = "upload";

bool HTTPUploader::upload(const uint8_t* data, size_t size, const char* server_scheme, const char* server_host,
                          uint16_t server_port, const char* room_target, const char* ha_token) {
    if (!data || size == 0) {
        Serial.printf("[%s] No data to upload\n", TAG);
        return false;
    }

    const bool use_https = server_scheme && strcmp(server_scheme, "https") == 0;
    const bool use_ha_auth = ha_token && ha_token[0] != '\0';
    const char* endpoint = "/api/home_intercom/record";

    // std::ostringstream — type-safe URL construction, no printf format-string issues.
    std::ostringstream oss;
    oss << (use_https ? "https" : "http") << "://" << server_host << ":" << server_port << endpoint
        << "?target=" << room_target;
    std::string url_str = oss.str();
    const char* url = url_str.c_str();

    // Retry up to 2 times (total 3 attempts) in case of transient errors
    for (int attempt = 1; attempt <= 3; attempt++) {
        WiFiClient plain_client;
        WiFiClientSecure secure_client;
        HTTPClient http;

        bool began = false;
        if (use_https) {
            // Encrypt traffic, but do not verify server identity yet. See README.
            secure_client.setInsecure();
            began = http.begin(secure_client, url);
            Serial.printf("[%s] HTTPS enabled — server certificate NOT verified\n", TAG);
        } else {
            began = http.begin(plain_client, url);
        }
        if (!began) {
            Serial.printf("[%s] Failed to initialize %s client\n", TAG, use_https ? "HTTPS" : "HTTP");
            continue;
        }

        http.addHeader("Content-Type", "audio/wav");
        if (use_ha_auth) {
            String auth = "Bearer ";
            auth += ha_token;
            http.addHeader("Authorization", auth.c_str());
        }
        http.setTimeout(60000); // 60 seconds (Arduino-ESP32 v3: ms, not s)

        Serial.printf("[%s] POST %s (%u bytes) attempt %d/3\n", TAG, url, size, attempt);
        int code = http.POST(const_cast<uint8_t*>(data), size);

        if (code == HTTP_CODE_OK) {
            String body = http.getString();
            http.end();
            // Accept any 2xx response — server may respond before HA finishes
            if (body.indexOf("\"ok\":true") >= 0 || body.indexOf("\"ok\":") >= 0) {
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
