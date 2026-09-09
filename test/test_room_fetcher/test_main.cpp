/// Unit tests for RoomFetcher through mocks.

#include "room_fetcher.h"
#include "room_target_store.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <cstring>
#include <unity.h>

void setUp(void) {
    mock_http_reset();
    mock_set_millis(0);
    _secure_client_insecure = false;
    Preferences::reset_all();
}

void tearDown(void) {
    mock_http_reset();
}

void test_fetch_parses_keys_in_document_order(void) {
    mock_http_set_response(200, R"({
        "living": {"name": "Living Room", "entity": "media_player.living"},
        "bedroom": {"name": "Bedroom", "entity_id": "media_player.bedroom"}
    })");

    RoomFetcher::Result r = RoomFetcher::fetch("http", "ha.local", 8123);

    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL(2, r.count);
    TEST_ASSERT_EQUAL_STRING("living", r.keys[0]);
    TEST_ASSERT_EQUAL_STRING("bedroom", r.keys[1]);
    TEST_ASSERT_EQUAL_STRING("http://ha.local:8123/api/home_intercom/rooms", _http_mock.last_url.c_str());
    TEST_ASSERT_EQUAL(1, _http_mock.get_call_count);
    TEST_ASSERT_EQUAL(0, _http_mock.post_call_count);
    TEST_ASSERT_EQUAL_STRING("", _http_mock.last_device_id_header.c_str());
}

void test_fetch_empty_object_is_ok(void) {
    mock_http_set_response(200, "{}");
    RoomFetcher::Result r = RoomFetcher::fetch("http", "ha.local", 8123);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL(0, r.count);
}

void test_fetch_missing_host_does_not_get(void) {
    RoomFetcher::Result r = RoomFetcher::fetch("http", "", 8123);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(0, _http_mock.get_call_count);
}

void test_fetch_null_host_does_not_get(void) {
    RoomFetcher::Result r = RoomFetcher::fetch("http", nullptr, 8123);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(0, _http_mock.get_call_count);
}

void test_fetch_http_error(void) {
    mock_http_set_response(500, "Internal Server Error");
    RoomFetcher::Result r = RoomFetcher::fetch("http", "ha.local", 8123);
    TEST_ASSERT_FALSE(r.ok);
}

void test_fetch_connection_error(void) {
    mock_http_set_error(-11);
    RoomFetcher::Result r = RoomFetcher::fetch("http", "ha.local", 8123);
    TEST_ASSERT_FALSE(r.ok);
}

void test_fetch_invalid_json(void) {
    mock_http_set_response(200, "{broken");
    RoomFetcher::Result r = RoomFetcher::fetch("http", "ha.local", 8123);
    TEST_ASSERT_FALSE(r.ok);
}

void test_fetch_array_is_rejected(void) {
    mock_http_set_response(200, R"(["living","bedroom"])");
    RoomFetcher::Result r = RoomFetcher::fetch("http", "ha.local", 8123);
    TEST_ASSERT_FALSE(r.ok);
}

void test_fetch_https_scheme(void) {
    mock_http_set_response(200, R"({"study":{}})");
    RoomFetcher::Result r = RoomFetcher::fetch("https", "ha.example.com", 443);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_STRING("https://ha.example.com:443/api/home_intercom/rooms", _http_mock.last_url.c_str());
    TEST_ASSERT_TRUE(_secure_client_insecure);
}

void test_apply_maps_pins_by_index(void) {
    RoomTargetStore store;
    store.begin();
    const uint8_t pins[] = {4, 5, 12, 13};
    RoomFetcher::Result rooms;
    rooms.ok = true;
    rooms.count = 2;
    strncpy(rooms.keys[0], "living", MAX_ROOM_KEY_LEN);
    strncpy(rooms.keys[1], "bedroom", MAX_ROOM_KEY_LEN);

    TEST_ASSERT_EQUAL(2, RoomFetcher::apply(store, pins, 4, rooms));
    TEST_ASSERT_EQUAL_STRING("living", store.get_room(4).c_str());
    TEST_ASSERT_EQUAL_STRING("bedroom", store.get_room(5).c_str());
    // Extra pins keep hardcoded / previous NVS
    TEST_ASSERT_EQUAL_STRING("cinema", store.get_room(12).c_str());
    TEST_ASSERT_EQUAL_STRING("bedroom", store.get_room(13).c_str());
}

void test_apply_ignores_failed_fetch(void) {
    RoomTargetStore store;
    store.begin();
    store.set_room(4, "study");
    const uint8_t pins[] = {4};
    RoomFetcher::Result rooms;
    rooms.ok = false;
    rooms.count = 1;
    strncpy(rooms.keys[0], "living", MAX_ROOM_KEY_LEN);

    TEST_ASSERT_EQUAL(0, RoomFetcher::apply(store, pins, 1, rooms));
    TEST_ASSERT_EQUAL_STRING("study", store.get_room(4).c_str());
}

void test_apply_caps_to_pin_count(void) {
    RoomTargetStore store;
    store.begin();
    const uint8_t pins[] = {4};
    RoomFetcher::Result rooms;
    rooms.ok = true;
    rooms.count = 3;
    strncpy(rooms.keys[0], "living", MAX_ROOM_KEY_LEN);
    strncpy(rooms.keys[1], "bedroom", MAX_ROOM_KEY_LEN);
    strncpy(rooms.keys[2], "cinema", MAX_ROOM_KEY_LEN);

    TEST_ASSERT_EQUAL(1, RoomFetcher::apply(store, pins, 1, rooms));
    TEST_ASSERT_EQUAL_STRING("living", store.get_room(4).c_str());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fetch_parses_keys_in_document_order);
    RUN_TEST(test_fetch_empty_object_is_ok);
    RUN_TEST(test_fetch_missing_host_does_not_get);
    RUN_TEST(test_fetch_null_host_does_not_get);
    RUN_TEST(test_fetch_http_error);
    RUN_TEST(test_fetch_connection_error);
    RUN_TEST(test_fetch_invalid_json);
    RUN_TEST(test_fetch_array_is_rejected);
    RUN_TEST(test_fetch_https_scheme);
    RUN_TEST(test_apply_maps_pins_by_index);
    RUN_TEST(test_apply_ignores_failed_fetch);
    RUN_TEST(test_apply_caps_to_pin_count);
    return UNITY_END();
}
