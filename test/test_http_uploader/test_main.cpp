/// Unit tests for HTTPUploader through mocks.
/// Tests the real src/http_uploader.cpp against mock HTTPClient/Arduino.

#include "http_uploader.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <unity.h>

static uint8_t test_data[256];
static const char* DEVICE_MAC = "AA:BB:CC:DD:EE:FF";

void setUp(void) {
    mock_http_reset();
    mock_set_millis(0);
    _secure_client_insecure = false;
    for (size_t i = 0; i < sizeof(test_data); i++) {
        test_data[i] = (uint8_t) (i & 0xFF);
    }
}

void tearDown(void) {
    mock_http_reset();
}

// ── Test 1: Successful upload (HTTP 200 + {"ok":true}) ──
void test_upload_success(void) {
    bool result = HTTPUploader::upload(test_data, sizeof(test_data), "http", "10.0.0.1", 5000, "kitchen", DEVICE_MAC);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, _http_mock.post_call_count);
    TEST_ASSERT_TRUE(_http_mock.last_url.find("10.0.0.1:5000") != std::string::npos);
    TEST_ASSERT_TRUE(_http_mock.last_url.find("/api/home_intercom/device/record?target=kitchen") != std::string::npos);
}

// ── Test 2: HTTP error triggers retries (3 attempts) ──
void test_upload_http_error_retries(void) {
    mock_http_set_response(500, "Internal Server Error");
    bool result =
        HTTPUploader::upload(test_data, sizeof(test_data), "http", "192.168.1.50", 8080, "living", DEVICE_MAC);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(3, _http_mock.post_call_count);
    TEST_ASSERT_TRUE(_http_mock.last_url.find("192.168.1.50:8080") != std::string::npos);
}

// ── Test 3: Connection error still returns true (delivery priority) ──
void test_upload_connection_error_still_returns_true(void) {
    mock_http_set_error(-11);
    bool result =
        HTTPUploader::upload(test_data, sizeof(test_data), "http", "server.local", 3000, "garage", DEVICE_MAC);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(3, _http_mock.post_call_count);
}

// ── Test 4a: NULL data returns false ──
void test_upload_null_data_returns_false(void) {
    bool result = HTTPUploader::upload(nullptr, 1024, "http", "10.0.0.1", 5000, "room", DEVICE_MAC);
    TEST_ASSERT_FALSE(result);
}

// ── Test 4b: Zero size returns false ──
void test_upload_zero_size_returns_false(void) {
    bool result = HTTPUploader::upload(test_data, 0, "http", "10.0.0.1", 5000, "room", DEVICE_MAC);
    TEST_ASSERT_FALSE(result);
}

// ── Test 5: Content-Type header set to audio/wav ──
void test_upload_sets_content_type_wav(void) {
    mock_http_set_response(200, R"({"ok":true})");
    HTTPUploader::upload(test_data, sizeof(test_data), "http", "server", 1234, "office", DEVICE_MAC);
    TEST_ASSERT_EQUAL_STRING("audio/wav", _http_mock.last_content_type.c_str());
}

// ── Test 6: URL uses the hardware device/record endpoint ──
void test_upload_url_uses_device_record_endpoint(void) {
    // UTF-8 bytes for "房间"
    HTTPUploader::upload(test_data, sizeof(test_data), "http", "192.168.1.1", 8764, "\346\210\277\351\227\264",
                         DEVICE_MAC);
    std::string expected = "http://192.168.1.1:8764/api/home_intercom/device/record?target=\346\210\277\351\227\264";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), _http_mock.last_url.c_str());
}

// ── Test 7: X-Device-ID header is sent ──
void test_upload_sends_device_id_header(void) {
    mock_http_set_response(200, R"({"ok":true})");
    HTTPUploader::upload(test_data, sizeof(test_data), "http", "ha.local", 8123, "living", DEVICE_MAC);
    TEST_ASSERT_EQUAL_STRING(DEVICE_MAC, _http_mock.last_device_id_header.c_str());
    TEST_ASSERT_EQUAL_STRING("", _http_mock.last_auth_header.c_str());
    TEST_ASSERT_TRUE(_http_mock.last_url.find("/api/home_intercom/device/record?target=living") != std::string::npos);
}

// ── Test 8: No X-Device-ID when device_id is empty ──
void test_upload_no_device_id_header_when_empty(void) {
    mock_http_set_response(200, R"({"ok":true})");
    HTTPUploader::upload(test_data, sizeof(test_data), "http", "server", 8764, "study", "");
    TEST_ASSERT_EQUAL_STRING("", _http_mock.last_device_id_header.c_str());
}

// ── Test 9: No X-Device-ID when device_id is nullptr ──
void test_upload_no_device_id_header_when_null(void) {
    mock_http_set_response(200, R"({"ok":true})");
    HTTPUploader::upload(test_data, sizeof(test_data), "http", "server", 8764, "study", nullptr);
    TEST_ASSERT_EQUAL_STRING("", _http_mock.last_device_id_header.c_str());
}

// ── Test 10: HTTPS scheme creates a secure client and HTTPS URL ──
void test_upload_https_scheme(void) {
    HTTPUploader::upload(test_data, sizeof(test_data), "https", "ha.example.com", 443, "study", DEVICE_MAC);
    TEST_ASSERT_EQUAL_STRING("https://ha.example.com:443/api/home_intercom/device/record?target=study",
                             _http_mock.last_url.c_str());
    TEST_ASSERT_TRUE(_secure_client_insecure);
}

// ── Test 11: 403 is a hard failure — no retry, no assumed delivery ──
void test_upload_forbidden_returns_false_without_retry(void) {
    mock_http_set_response(HTTP_CODE_FORBIDDEN, R"({"ok":false,"error":"unknown device"})");
    bool result = HTTPUploader::upload(test_data, sizeof(test_data), "http", "ha.local", 8123, "study", DEVICE_MAC);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(1, _http_mock.post_call_count);
}

// ── Test 12: 401 is a hard failure ──
void test_upload_unauthorized_returns_false_without_retry(void) {
    mock_http_set_response(HTTP_CODE_UNAUTHORIZED, R"({"ok":false,"error":"unauthorized"})");
    bool result = HTTPUploader::upload(test_data, sizeof(test_data), "http", "ha.local", 8123, "study", DEVICE_MAC);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(1, _http_mock.post_call_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_upload_success);
    RUN_TEST(test_upload_http_error_retries);
    RUN_TEST(test_upload_connection_error_still_returns_true);
    RUN_TEST(test_upload_null_data_returns_false);
    RUN_TEST(test_upload_zero_size_returns_false);
    RUN_TEST(test_upload_sets_content_type_wav);
    RUN_TEST(test_upload_url_uses_device_record_endpoint);
    RUN_TEST(test_upload_sends_device_id_header);
    RUN_TEST(test_upload_no_device_id_header_when_empty);
    RUN_TEST(test_upload_no_device_id_header_when_null);
    RUN_TEST(test_upload_https_scheme);
    RUN_TEST(test_upload_forbidden_returns_false_without_retry);
    RUN_TEST(test_upload_unauthorized_returns_false_without_retry);
    return UNITY_END();
}
