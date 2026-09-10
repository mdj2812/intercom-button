#include "server_config.h"

#include "consts.hpp"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <cstring>
#include <sstream>

static const char* TAG = "cfghttp";

ServerConfig::Result ServerConfig::fetch(const char* server_scheme, const char* server_host, uint16_t server_port) {
    Result result;

    if (!server_host || server_host[0] == '\0') {
        result.error = "missing server host";
        Serial.printf("[%s] %s\n", TAG, result.error);
        return result;
    }

    const bool use_https = server_scheme && strcmp(server_scheme, "https") == 0;

    std::ostringstream oss;
    oss << (use_https ? "https" : "http") << "://" << server_host << ":" << server_port << AUDIO_CONFIG_HTTP_PATH;
    std::string url_str = oss.str();

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

    http.setTimeout(5000);

    Serial.printf("[%s] GET %s\n", TAG, url_str.c_str());
    int code = http.GET();
    String response;
    String transport_error;
    if (code > 0) {
        response = http.getString();
    } else {
        transport_error = http.errorToString(code);
    }
    http.end();

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

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, response.c_str());
    if (err) {
        result.error = "invalid json";
        Serial.printf("[%s] JSON parse error: %s\n", TAG, err.c_str());
        return result;
    }

    const uint32_t sample_rate = doc["sample_rate"] | 0u;
    const uint32_t max_record_secs = doc["max_record_secs"] | 0u;

    if (sample_rate == 0 && max_record_secs == 0) {
        result.error = "missing fields";
        Serial.printf("[%s] no sample_rate or max_record_secs\n", TAG);
        return result;
    }

    result.ok = true;
    result.sample_rate = sample_rate;
    result.max_record_secs = max_record_secs;
    Serial.printf("[%s] OK rate=%u max=%us\n", TAG, result.sample_rate, result.max_record_secs);
    return result;
}
