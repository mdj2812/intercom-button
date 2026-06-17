#include "../src/room_target_store.h"
#include "Preferences.h"
#include <unity.h>

static RoomTargetStore store;

void setUp() {
    Preferences::reset_all();
    store = RoomTargetStore();
    store.begin();
}

void tearDown() {}

// ── Default mapping ──────────────────────────────

void test_default_pin_4_is_study() {
    TEST_ASSERT_EQUAL_STRING("study", store.get_room(4));
}

void test_default_pin_5_is_living() {
    TEST_ASSERT_EQUAL_STRING("living", store.get_room(5));
}

void test_default_pin_12_is_cinema() {
    TEST_ASSERT_EQUAL_STRING("cinema", store.get_room(12));
}

void test_default_pin_13_is_bedroom() {
    TEST_ASSERT_EQUAL_STRING("bedroom", store.get_room(13));
}

void test_unknown_pin_falls_back_to_study() {
    TEST_ASSERT_EQUAL_STRING("study", store.get_room(99));
}

// ── NVS storage ─────────────────────────────────

void test_set_and_get_room() {
    TEST_ASSERT_TRUE(store.set_room(4, "kitchen"));
    TEST_ASSERT_EQUAL_STRING("kitchen", store.get_room(4));
}

void test_overwrite_room() {
    store.set_room(4, "kitchen");
    store.set_room(4, "balcony");
    TEST_ASSERT_EQUAL_STRING("balcony", store.get_room(4));
}

void test_reset_clears_all() {
    store.set_room(4, "kitchen");
    store.set_room(5, "office");

    store.reset();

    // After reset, should fall back to defaults
    TEST_ASSERT_EQUAL_STRING("study", store.get_room(4));
    TEST_ASSERT_EQUAL_STRING("living", store.get_room(5));
}

void test_multiple_pins_independent() {
    store.set_room(4, "kitchen");
    store.set_room(5, "office");

    TEST_ASSERT_EQUAL_STRING("kitchen", store.get_room(4));
    TEST_ASSERT_EQUAL_STRING("office", store.get_room(5));
    // Unchanged pin still uses default
    TEST_ASSERT_EQUAL_STRING("cinema", store.get_room(12));
}

// ── Config-file defaults (Tier 2) ─────────────────

void test_config_default_overrides_hardcoded() {
    store.set_default_room(4, "garage");
    TEST_ASSERT_EQUAL_STRING("garage", store.get_room(4));
    // Unchanged pins still use hardcoded
    TEST_ASSERT_EQUAL_STRING("living", store.get_room(5));
}

void test_nvs_wins_over_config_default() {
    store.set_default_room(4, "garage");
    store.set_room(4, "kitchen");
    // NVS (Tier 1) beats config-file (Tier 2)
    TEST_ASSERT_EQUAL_STRING("kitchen", store.get_room(4));
}

void test_reset_falls_back_to_config_default() {
    store.set_default_room(4, "garage");
    store.set_room(4, "kitchen");
    store.reset();
    // After NVS clear, falls back to config-file default
    TEST_ASSERT_EQUAL_STRING("garage", store.get_room(4));
}

void test_clear_defaults() {
    store.set_default_room(4, "garage");
    TEST_ASSERT_EQUAL(1, store.defaults_count());
    store.clear_defaults();
    TEST_ASSERT_EQUAL(0, store.defaults_count());
    // After clear, falls back to hardcoded
    TEST_ASSERT_EQUAL_STRING("study", store.get_room(4));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_default_pin_4_is_study);
    RUN_TEST(test_default_pin_5_is_living);
    RUN_TEST(test_default_pin_12_is_cinema);
    RUN_TEST(test_default_pin_13_is_bedroom);
    RUN_TEST(test_unknown_pin_falls_back_to_study);

    RUN_TEST(test_set_and_get_room);
    RUN_TEST(test_overwrite_room);
    RUN_TEST(test_reset_clears_all);
    RUN_TEST(test_multiple_pins_independent);

    RUN_TEST(test_config_default_overrides_hardcoded);
    RUN_TEST(test_nvs_wins_over_config_default);
    RUN_TEST(test_reset_falls_back_to_config_default);
    RUN_TEST(test_clear_defaults);

    return UNITY_END();
}
