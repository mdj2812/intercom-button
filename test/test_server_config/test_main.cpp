/// Unit tests for ServerConfig (GET /api/home_intercom/config).

#include "consts.hpp"
#include "server_config.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <unity.h>

void setUp(void) {
    mock_http_reset();
    _secure_client_insecure = false;
}

void tearDown(void) {
    mock_http_reset();
}

void test_config_path(void) {
    TEST_ASSERT_EQUAL_STRING("/api/home_intercom/config", AUDIO_CONFIG_HTTP_PATH);
}

void test_fetch_success_parses_payload(void) {
    mock_http_set_response(200, R"({"sample_rate":16000,"max_record_secs":60})");

    ServerConfig::Result r = ServerConfig::fetch("http", "ha.local", 8123);

    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL(16000, r.sample_rate);
    TEST_ASSERT_EQUAL(60, r.max_record_secs);
    TEST_ASSERT_EQUAL_STRING("http://ha.local:8123/api/home_intercom/config", _http_mock.last_url.c_str());
    TEST_ASSERT_EQUAL(1, _http_mock.get_call_count);
    TEST_ASSERT_EQUAL(0, _http_mock.post_call_count);
    TEST_ASSERT_EQUAL_STRING("", _http_mock.last_device_id_header.c_str());
}

void test_fetch_partial_sample_rate_only(void) {
    mock_http_set_response(200, R"({"sample_rate":8000})");
    ServerConfig::Result r = ServerConfig::fetch("http", "ha.local", 8123);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL(8000, r.sample_rate);
    TEST_ASSERT_EQUAL(0, r.max_record_secs);
}

void test_fetch_empty_object_fails(void) {
    mock_http_set_response(200, R"({})");
    ServerConfig::Result r = ServerConfig::fetch("http", "ha.local", 8123);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL_STRING("missing fields", r.error);
}

void test_fetch_missing_host_does_not_get(void) {
    ServerConfig::Result r = ServerConfig::fetch("http", "", 8123);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(0, _http_mock.get_call_count);
}

void test_fetch_null_host_does_not_get(void) {
    ServerConfig::Result r = ServerConfig::fetch("http", nullptr, 8123);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(0, _http_mock.get_call_count);
}

void test_fetch_http_error(void) {
    mock_http_set_response(500, "Internal Server Error");
    ServerConfig::Result r = ServerConfig::fetch("http", "ha.local", 8123);
    TEST_ASSERT_FALSE(r.ok);
}

void test_fetch_connection_error(void) {
    mock_http_set_error(-11);
    ServerConfig::Result r = ServerConfig::fetch("http", "ha.local", 8123);
    TEST_ASSERT_FALSE(r.ok);
}

void test_fetch_invalid_json(void) {
    mock_http_set_response(200, "{broken");
    ServerConfig::Result r = ServerConfig::fetch("http", "ha.local", 8123);
    TEST_ASSERT_FALSE(r.ok);
}

void test_fetch_https_scheme(void) {
    mock_http_set_response(200, R"({"sample_rate":16000,"max_record_secs":60})");
    ServerConfig::Result r = ServerConfig::fetch("https", "ha.example.com", 443);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_STRING("https://ha.example.com:443/api/home_intercom/config", _http_mock.last_url.c_str());
    TEST_ASSERT_TRUE(_secure_client_insecure);
}

void test_merge_audio_updates(void) {
    ServerConfig::AudioSettings cur;
    ServerConfig::AudioSettings next = ServerConfig::merge_audio(cur, 8000, 30);
    TEST_ASSERT_EQUAL(8000, next.sample_rate);
    TEST_ASSERT_EQUAL(30, next.max_record_secs);
}

void test_merge_audio_zero_keeps_current(void) {
    ServerConfig::AudioSettings cur{22050, 45};
    ServerConfig::AudioSettings next = ServerConfig::merge_audio(cur, 0, 0);
    TEST_ASSERT_EQUAL(22050, next.sample_rate);
    TEST_ASSERT_EQUAL(45, next.max_record_secs);
}

void test_merge_audio_out_of_range_is_ignored(void) {
    ServerConfig::AudioSettings cur{16000, 60};
    ServerConfig::AudioSettings next = ServerConfig::merge_audio(cur, 1000, 999);
    TEST_ASSERT_EQUAL(16000, next.sample_rate);
    TEST_ASSERT_EQUAL(60, next.max_record_secs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_config_path);
    RUN_TEST(test_fetch_success_parses_payload);
    RUN_TEST(test_fetch_partial_sample_rate_only);
    RUN_TEST(test_fetch_empty_object_fails);
    RUN_TEST(test_fetch_missing_host_does_not_get);
    RUN_TEST(test_fetch_null_host_does_not_get);
    RUN_TEST(test_fetch_http_error);
    RUN_TEST(test_fetch_connection_error);
    RUN_TEST(test_fetch_invalid_json);
    RUN_TEST(test_fetch_https_scheme);
    RUN_TEST(test_merge_audio_updates);
    RUN_TEST(test_merge_audio_zero_keeps_current);
    RUN_TEST(test_merge_audio_out_of_range_is_ignored);
    return UNITY_END();
}
