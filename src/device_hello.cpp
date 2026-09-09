#include "device_hello.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <cstring>
#include <sstream>

static const char* TAG = "hello";
static const char* HELLO_PATH = "/api/home_intercom/devices/hello";

static void copy_field(char* dest, size_t dest_len, const char* src) {
    if (!dest || dest_len == 0)
        return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, dest_len - 1);
    dest[dest_len - 1] = '\0';
}

DeviceHello::Result DeviceHello::send(const char* server_scheme, const char* server_host, uint16_t server_port,
                                      const char* device_id, const char* firmware_version) {
    Result result;

    if (!device_id || device_id[0] == '\0') {
        result.error = "missing device id";
        Serial.printf("[%s] %s\n", TAG, result.error);
        return result;
    }

    const bool use_https = server_scheme && strcmp(server_scheme, "https") == 0;

    std::ostringstream oss;
    oss << (use_https ? "https" : "http") << "://" << server_host << ":" << server_port << HELLO_PATH;
    std::string url_str = oss.str();

    String body("{\"firmware_version\":\"");
    body.concat(firmware_version ? firmware_version : "");
    body.concat("\"}");

    WiFiClient plain_client;
    WiFiClientSecure secure_client;
    HTTPClient http;

    bool began = false;
    if (use_https) {
        secure_client.setInsecure();
        began = http.begin(secure_client, url_str.c_str());
    } else {
        began = http.begin(plain_client, url_str.c_str());
    }
    if (!began) {
        result.error = "http begin failed";
        Serial.printf("[%s] Failed to initialize %s client\n", TAG, use_https ? "HTTPS" : "HTTP");
        return result;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-ID", device_id);
    http.setTimeout(HELLO_HTTP_TIMEOUT_MS);

    Serial.printf("[%s] POST %s\n", TAG, url_str.c_str());
    int code = http.POST(body);
    String response;
    String transport_error;
    if (code > 0) {
        response = http.getString();
    } else {
        transport_error = http.errorToString(code);
    }
    http.end();

    if (code == HTTP_CODE_FORBIDDEN) {
        result.status = Status::Revoked;
        result.error = "device revoked";
        Serial.printf("[%s] HTTP 403 — device revoked\n", TAG);
        return result;
    }

    if (code <= 0) {
        result.error = "connection error";
        Serial.printf("[%s] HTTP %d — %s\n", TAG, code, transport_error.c_str());
        return result;
    }

    if (code != HTTP_CODE_OK) {
        result.error = "http error";
        Serial.printf("[%s] HTTP %d — %s\n", TAG, code, response.c_str());
        return result;
    }

    StaticJsonDocument<384> doc;
    DeserializationError err = deserializeJson(doc, response.c_str());
    if (err) {
        result.error = "invalid json";
        Serial.printf("[%s] JSON parse error: %s\n", TAG, err.c_str());
        return result;
    }

    const char* status = doc["status"] | "";
    if (strcmp(status, "ok") == 0) {
        result.status = Status::Ok;
        copy_field(result.device_name, sizeof(result.device_name), doc["device_name"] | "");
        copy_field(result.room, sizeof(result.room), doc["room"] | "");
        result.sample_rate = doc["sample_rate"] | 0;
        result.max_record_secs = doc["max_record_secs"] | 0;
        // Hello `room` is the device's HA area (often empty) — not the button map.
        Serial.printf("[%s] OK name=%s rate=%u max=%us\n", TAG, result.device_name, result.sample_rate,
                      result.max_record_secs);
        return result;
    }

    if (strcmp(status, "pending") == 0) {
        result.status = Status::Pending;
        result.error = "pending";
        Serial.printf("[%s] pending approval\n", TAG);
        return result;
    }

    result.error = "rejected";
    Serial.printf("[%s] status=%s error=%s\n", TAG, status, doc["error"] | "");
    return result;
}
