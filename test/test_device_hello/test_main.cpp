/// Unit tests for DeviceHello through mocks.

#include "consts.hpp"
#include "device_hello.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <unity.h>

static const char* DEVICE_MAC = "AA:BB:CC:DD:EE:FF";

void setUp(void) {
    mock_http_reset();
    mock_set_millis(0);
    _secure_client_insecure = false;
}

void tearDown(void) {
    mock_http_reset();
}

void test_hello_success_parses_payload(void) {
    mock_http_set_response(200, R"({
        "status": "ok",
        "device_name": "Study Button",
        "room": "study",
        "sample_rate": 16000,
        "max_record_secs": 60
    })");

    DeviceHello::Result r = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.1.0");

    TEST_ASSERT_EQUAL(static_cast<int>(DeviceHello::Status::Ok), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL_STRING("Study Button", r.device_name);
    TEST_ASSERT_EQUAL_STRING("study", r.room);
    TEST_ASSERT_EQUAL(16000, r.sample_rate);
    TEST_ASSERT_EQUAL(60, r.max_record_secs);
    TEST_ASSERT_FALSE(r.ota);
    TEST_ASSERT_EQUAL_STRING("http://ha.local:8123/api/home_intercom/devices/hello", _http_mock.last_url.c_str());
    TEST_ASSERT_EQUAL_STRING(DEVICE_MAC, _http_mock.last_device_id_header.c_str());
    TEST_ASSERT_EQUAL_STRING("application/json", _http_mock.last_content_type.c_str());
    TEST_ASSERT_TRUE(_http_mock.last_body.find("\"firmware_version\":\"0.1.0\"") != std::string::npos);
    TEST_ASSERT_EQUAL(1, _http_mock.post_call_count);
}

void test_hello_missing_device_id_does_not_post(void) {
    DeviceHello::Result r = DeviceHello::send("http", "ha.local", 8123, "", "0.1.0");
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceHello::Status::Error), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL(0, _http_mock.post_call_count);
}

void test_hello_null_device_id_does_not_post(void) {
    DeviceHello::Result r = DeviceHello::send("http", "ha.local", 8123, nullptr, "0.1.0");
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceHello::Status::Error), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL(0, _http_mock.post_call_count);
}

void test_hello_revoked_is_403(void) {
    mock_http_set_response(HTTP_CODE_FORBIDDEN, R"({"status":"error","error":"device revoked"})");
    DeviceHello::Result r = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.1.0");
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceHello::Status::Revoked), static_cast<int>(r.status));
}

void test_hello_pending_status(void) {
    mock_http_set_response(200, R"({"status":"pending"})");
    DeviceHello::Result r = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.1.0");
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceHello::Status::Pending), static_cast<int>(r.status));
}

void test_hello_http_error(void) {
    mock_http_set_response(500, "Internal Server Error");
    DeviceHello::Result r = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.1.0");
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceHello::Status::Error), static_cast<int>(r.status));
}

void test_hello_connection_error(void) {
    mock_http_set_error(-11);
    DeviceHello::Result r = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.1.0");
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceHello::Status::Error), static_cast<int>(r.status));
}

void test_hello_invalid_json(void) {
    mock_http_set_response(200, "{broken");
    DeviceHello::Result r = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.1.0");
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceHello::Status::Error), static_cast<int>(r.status));
}

void test_hello_status_error_body(void) {
    mock_http_set_response(200, R"({"status":"error","error":"device registry unavailable"})");
    DeviceHello::Result r = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.1.0");
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceHello::Status::Error), static_cast<int>(r.status));
}

void test_hello_ota_true(void) {
    mock_http_set_response(200, R"({
        "status": "ok",
        "device_name": "Study Button",
        "room": "study",
        "sample_rate": 16000,
        "max_record_secs": 60,
        "ota": true
    })");

    DeviceHello::Result r = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.2.0");

    TEST_ASSERT_EQUAL(static_cast<int>(DeviceHello::Status::Ok), static_cast<int>(r.status));
    TEST_ASSERT_TRUE(r.ota);
}

void test_firmware_http_path(void) {
    TEST_ASSERT_EQUAL_STRING("/api/home_intercom/firmware", FIRMWARE_HTTP_PATH);
}

void test_hello_https_scheme(void) {
    mock_http_set_response(200, R"({"status":"ok","device_name":"Btn","room":""})");
    DeviceHello::Result r = DeviceHello::send("https", "ha.example.com", 443, DEVICE_MAC, "0.1.0");
    TEST_ASSERT_EQUAL(static_cast<int>(DeviceHello::Status::Ok), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL_STRING("https://ha.example.com:443/api/home_intercom/devices/hello", _http_mock.last_url.c_str());
    TEST_ASSERT_TRUE(_secure_client_insecure);
}

void test_parsed_hello_confirms_ota_boot(void) {
    TEST_ASSERT_TRUE(DeviceHello::confirms_ota_boot(DeviceHello::Status::Ok));
    TEST_ASSERT_TRUE(DeviceHello::confirms_ota_boot(DeviceHello::Status::Pending));
    TEST_ASSERT_TRUE(DeviceHello::confirms_ota_boot(DeviceHello::Status::Revoked));
    TEST_ASSERT_FALSE(DeviceHello::confirms_ota_boot(DeviceHello::Status::Error));
}

void test_hello_ok_pending_revoked_confirm_ota_boot(void) {
    mock_http_set_response(200, R"({"status":"ok","device_name":"Btn","room":""})");
    DeviceHello::Result ok = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.2.1");
    TEST_ASSERT_TRUE(DeviceHello::confirms_ota_boot(ok.status));

    mock_http_reset();
    mock_http_set_response(200, R"({"status":"pending"})");
    DeviceHello::Result pending = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.2.1");
    TEST_ASSERT_TRUE(DeviceHello::confirms_ota_boot(pending.status));

    mock_http_reset();
    mock_http_set_response(HTTP_CODE_FORBIDDEN, R"({"status":"error","error":"device revoked"})");
    DeviceHello::Result revoked = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.2.1");
    TEST_ASSERT_TRUE(DeviceHello::confirms_ota_boot(revoked.status));
}

void test_hello_transport_errors_do_not_confirm_ota_boot(void) {
    mock_http_set_error(-11);
    DeviceHello::Result timeout = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.2.1");
    TEST_ASSERT_FALSE(DeviceHello::confirms_ota_boot(timeout.status));

    mock_http_reset();
    mock_http_set_response(500, "Internal Server Error");
    DeviceHello::Result http_err = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.2.1");
    TEST_ASSERT_FALSE(DeviceHello::confirms_ota_boot(http_err.status));

    mock_http_reset();
    mock_http_set_response(200, "{broken");
    DeviceHello::Result bad_json = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.2.1");
    TEST_ASSERT_FALSE(DeviceHello::confirms_ota_boot(bad_json.status));

    mock_http_reset();
    mock_http_set_response(200, R"({"status":"error","error":"device registry unavailable"})");
    DeviceHello::Result rejected = DeviceHello::send("http", "ha.local", 8123, DEVICE_MAC, "0.2.1");
    TEST_ASSERT_FALSE(DeviceHello::confirms_ota_boot(rejected.status));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hello_success_parses_payload);
    RUN_TEST(test_hello_missing_device_id_does_not_post);
    RUN_TEST(test_hello_null_device_id_does_not_post);
    RUN_TEST(test_hello_revoked_is_403);
    RUN_TEST(test_hello_pending_status);
    RUN_TEST(test_hello_http_error);
    RUN_TEST(test_hello_connection_error);
    RUN_TEST(test_hello_invalid_json);
    RUN_TEST(test_hello_status_error_body);
    RUN_TEST(test_hello_ota_true);
    RUN_TEST(test_firmware_http_path);
    RUN_TEST(test_hello_https_scheme);
    RUN_TEST(test_parsed_hello_confirms_ota_boot);
    RUN_TEST(test_hello_ok_pending_revoked_confirm_ota_boot);
    RUN_TEST(test_hello_transport_errors_do_not_confirm_ota_boot);
    return UNITY_END();
}
