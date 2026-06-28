/// Unit tests for HTTPUploader through mocks.
/// Tests the real src/http_uploader.cpp against mock HTTPClient/Arduino.

#include "http_uploader.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <unity.h>

static uint8_t test_data[256];

void setUp(void) {
    mock_http_reset();
    mock_set_millis(0);
    for (size_t i = 0; i < sizeof(test_data); i++) {
        test_data[i] = (uint8_t) (i & 0xFF);
    }
}

void tearDown(void) {
    mock_http_reset();
}

// ── Test 1: Successful upload (HTTP 200 + {"ok":true}) ──
void test_upload_success(void) {
    // Default mock: 200 + {"ok":true}
    bool result = HTTPUploader::upload(test_data, sizeof(test_data), "10.0.0.1", 5000, "kitchen");
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, _http_mock.post_call_count);
    // URL should contain host, port, path, and room target
    TEST_ASSERT_TRUE(_http_mock.last_url.find("10.0.0.1:5000") != std::string::npos);
    TEST_ASSERT_TRUE(_http_mock.last_url.find("/record?target=kitchen") != std::string::npos);
}

// ── Test 2: HTTP error triggers retries (3 attempts) ──
void test_upload_http_error_retries(void) {
    mock_http_set_response(500, "Internal Server Error");
    bool result = HTTPUploader::upload(test_data, sizeof(test_data), "192.168.1.50", 8080, "living");
    // After 3 attempts exhausted, returns true (soft success — audio delivery priority)
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(3, _http_mock.post_call_count);
    TEST_ASSERT_TRUE(_http_mock.last_url.find("192.168.1.50:8080") != std::string::npos);
}

// ── Test 3: Connection error still returns true (delivery priority) ──
void test_upload_connection_error_still_returns_true(void) {
    mock_http_set_error(-11);
    bool result = HTTPUploader::upload(test_data, sizeof(test_data), "server.local", 3000, "garage");
    // Even with connection errors, audio delivery is the priority
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(3, _http_mock.post_call_count);
}

// ── Test 4a: NULL data returns false ──
void test_upload_null_data_returns_false(void) {
    bool result = HTTPUploader::upload(nullptr, 1024, "10.0.0.1", 5000, "room");
    TEST_ASSERT_FALSE(result);
}

// ── Test 4b: Zero size returns false ──
void test_upload_zero_size_returns_false(void) {
    bool result = HTTPUploader::upload(test_data, 0, "10.0.0.1", 5000, "room");
    TEST_ASSERT_FALSE(result);
}

// ── Test 5: Content-Type header set to audio/wav ──
void test_upload_sets_content_type_wav(void) {
    mock_http_set_response(200, R"({"ok":true})");
    HTTPUploader::upload(test_data, sizeof(test_data), "server", 1234, "office");
    TEST_ASSERT_EQUAL_STRING("audio/wav", _http_mock.last_content_type.c_str());
}

// ── Test 6: URL format — /record?target=<room> ──
void test_upload_url_format_is_record_endpoint(void) {
    HTTPUploader::upload(test_data, sizeof(test_data), "192.168.1.1", 8764, "书房");
    std::string expected = "http://192.168.1.1:8764/record?target=书房";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), _http_mock.last_url.c_str());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_upload_success);
    RUN_TEST(test_upload_http_error_retries);
    RUN_TEST(test_upload_connection_error_still_returns_true);
    RUN_TEST(test_upload_null_data_returns_false);
    RUN_TEST(test_upload_zero_size_returns_false);
    RUN_TEST(test_upload_sets_content_type_wav);
    RUN_TEST(test_upload_url_format_is_record_endpoint);
    return UNITY_END();
}
