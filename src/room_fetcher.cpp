#include "room_fetcher.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <cstdio>
#include <cstring>
#include <sstream>

static const char* TAG = "rooms";
static const char* ROOMS_PATH = "/api/home_intercom/rooms";
static char last_ok_fp[96] = {};

static void copy_key(char* dest, size_t dest_len, const char* src) {
    if (!dest || dest_len == 0)
        return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, dest_len - 1);
    dest[dest_len - 1] = '\0';
}

RoomFetcher::Result RoomFetcher::fetch(const char* server_scheme, const char* server_host, uint16_t server_port) {
    Result result;

    if (!server_host || server_host[0] == '\0') {
        result.error = "missing host";
        Serial.printf("[%s] %s\n", TAG, result.error);
        return result;
    }

    const bool use_https = server_scheme && strcmp(server_scheme, "https") == 0;

    std::ostringstream oss;
    oss << (use_https ? "https" : "http") << "://" << server_host << ":" << server_port << ROOMS_PATH;
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

    http.setTimeout(10000);

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

    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, response.c_str());
    if (err) {
        result.error = "invalid json";
        Serial.printf("[%s] JSON parse error: %s\n", TAG, err.c_str());
        return result;
    }

    if (!doc.is<JsonObject>()) {
        result.error = "expected object";
        Serial.printf("[%s] %s\n", TAG, result.error);
        return result;
    }

    JsonObject obj = doc.as<JsonObject>();
    for (JsonPair kv : obj) {
        if (result.count >= MAX_BUTTONS)
            break;
        copy_key(result.keys[result.count], MAX_ROOM_KEY_LEN, kv.key().c_str());
        if (result.keys[result.count][0] == '\0')
            continue;
        result.count++;
    }

    result.ok = true;

    char fp[96];
    size_t used = 0;
    fp[0] = '\0';
    for (uint8_t i = 0; i < result.count && used + 1 < sizeof(fp); i++) {
        int w = snprintf(fp + used, sizeof(fp) - used, "%s;", result.keys[i]);
        if (w < 0)
            break;
        used += static_cast<size_t>(w);
        if (used >= sizeof(fp)) {
            fp[sizeof(fp) - 1] = '\0';
            break;
        }
    }
    if (strcmp(fp, last_ok_fp) != 0) {
        strncpy(last_ok_fp, fp, sizeof(last_ok_fp) - 1);
        last_ok_fp[sizeof(last_ok_fp) - 1] = '\0';
        Serial.printf("[%s] GET %s — OK count=%u", TAG, url_str.c_str(), result.count);
        for (uint8_t i = 0; i < result.count; i++)
            Serial.printf(" %s", result.keys[i]);
        Serial.println();
    }
    return result;
}

uint8_t RoomFetcher::apply(RoomTargetStore& store, const uint8_t* pins, uint8_t pin_count, const Result& rooms) {
    if (!pins || !rooms.ok)
        return 0;

    uint8_t written = 0;
    for (uint8_t i = 0; i < pin_count; i++) {
        if (i < rooms.count && rooms.keys[i][0] != '\0') {
            if (store.set_room(pins[i], rooms.keys[i]))
                written++;
        } else {
            store.set_room(pins[i], "");
        }
    }
    return written;
}
