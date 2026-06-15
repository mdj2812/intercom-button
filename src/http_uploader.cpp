#include "http_uploader.h"
#include <HTTPClient.h>
#include <WiFi.h>

static const char *TAG = "upload";

bool HTTPUploader::upload(const uint8_t *data, size_t size,
                           const char *server_host, uint16_t server_port,
                           const char *room_target) {
    if (!data || size == 0) {
        Serial.printf("[%s] No data to upload\n", TAG);
        return false;
    }

    char url[256];
    snprintf(url, sizeof(url), "http://%s:%u/convert?target=%s",
             server_host, server_port, room_target);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "audio/wav");
    http.setTimeout(30); // seconds, enough for 10s WAV + LAN transfer

    Serial.printf("[%s] POST %s (%u bytes)\n", TAG, url, size);
    int code = http.POST(const_cast<uint8_t *>(data), size);

    bool ok = false;
    if (code == HTTP_CODE_OK) {
        String body = http.getString();
        ok = body.indexOf("\"ok\":true") > 0;
        Serial.printf("[%s] Server: %s\n", TAG, body.c_str());
    } else {
        Serial.printf("[%s] HTTP %d — %s\n", TAG, code, http.errorToString(code).c_str());
    }

    http.end();
    return ok;
}
