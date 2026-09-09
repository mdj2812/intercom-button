/// Unit tests for DeviceId through the WiFi mock.

#include "device_id.h"
#include <Arduino.h>
#include <WiFi.h>
#include <unity.h>

void setUp() {
    mock_wifi_reset();
}

void tearDown() {}

void test_mac_default_is_uppercase_colon_separated() {
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", DeviceId::mac());
}

void test_mac_is_uppercased() {
    mock_wifi_set_mac("aa:bb:cc:dd:ee:ff");
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", DeviceId::mac());
}

void test_mac_follows_wifi_mock() {
    mock_wifi_set_mac("11:22:33:44:55:66");
    TEST_ASSERT_EQUAL_STRING("11:22:33:44:55:66", DeviceId::mac());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_mac_default_is_uppercase_colon_separated);
    RUN_TEST(test_mac_is_uppercased);
    RUN_TEST(test_mac_follows_wifi_mock);
    return UNITY_END();
}
