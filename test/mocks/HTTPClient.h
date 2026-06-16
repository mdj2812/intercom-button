#pragma once
#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "WString.h"

// ── HTTP status ─────────────────────────────────────
#define HTTP_CODE_OK 200

// ── Mockable HTTP state ─────────────────────────────
struct HTTPMockState {
    int response_code = 200;
    std::string response_body = R"({"ok":true})";
    int connect_error = 0;          // < 0 = connection error (timeout, reset)
    std::string last_url;
    std::string last_content_type;
    size_t last_body_size = 0;
    int post_call_count = 0;
};
inline HTTPMockState _http_mock;

inline void mock_http_reset() {
    _http_mock = HTTPMockState{};
}
inline void mock_http_set_response(int code, const std::string& body) {
    _http_mock.response_code = code;
    _http_mock.response_body = body;
    _http_mock.connect_error = 0;
}
inline void mock_http_set_error(int error_code) {
    _http_mock.connect_error = error_code;
}

// ── HTTPClient class ────────────────────────────────
class HTTPClient {
public:
    bool begin(const char* url) {
        _http_mock.last_url = url ? url : "";
        return true;
    }

    void addHeader(const char* name, const char* value) {
        if (name && strcmp(name, "Content-Type") == 0)
            _http_mock.last_content_type = value ? value : "";
    }

    void setTimeout(uint32_t ms) {
        (void)ms;
    }

    int POST(uint8_t* data, size_t len) {
        (void)data;
        _http_mock.last_body_size = len;
        _http_mock.post_call_count++;

        if (_http_mock.connect_error != 0)
            return _http_mock.connect_error;
        return _http_mock.response_code;
    }

    String getString() {
        return String(_http_mock.response_body);
    }

    String errorToString(int code) {
        char buf[64];
        snprintf(buf, sizeof(buf), "HTTP error %d", code);
        return String(buf);
    }

    void end() {}
};
