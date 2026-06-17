#include <Arduino.h>
#include <unity.h>

/// Smoke test: verify Arduino mock compiles and works.
void test_mock_arduino_compiles() {
    Serial.begin(115200);
    Serial.println("hello from mock Arduino!");

    pinMode(4, INPUT_PULLUP);
    TEST_ASSERT_EQUAL(INPUT_PULLUP, pinModePinParam[4]);

    digitalReadPin[4] = HIGH;
    int btn = digitalRead(4);
    TEST_ASSERT_EQUAL(HIGH, btn); // default: not pressed

    digitalReadPin[4] = LOW;
    btn = digitalRead(4);
    TEST_ASSERT_EQUAL(LOW, btn); // pressed
}

void test_mock_millis() {
    mock_set_millis(1000);
    TEST_ASSERT_EQUAL(1000, millis());

    mock_advance_millis(500);
    TEST_ASSERT_EQUAL(1500, millis());
}

void test_mock_delay() {
    mock_set_millis(0);
    delay(100);
    TEST_ASSERT_EQUAL(100, millis());
}

void test_mock_string() {
    String s("hello");
    TEST_ASSERT_EQUAL_STRING("hello", s.c_str());
    TEST_ASSERT_EQUAL(5, (int) s.length());

    String s2 = s;
    TEST_ASSERT_EQUAL_STRING("hello", s2.c_str());

    int idx = s.indexOf("ll");
    TEST_ASSERT_EQUAL(2, idx);

    idx = s.indexOf("zz");
    TEST_ASSERT_EQUAL(-1, idx);
}

void test_mock_serial_printf() {
    Serial.printf("test number: %d, string: %s\n", 42, "ok");
    TEST_ASSERT_TRUE(true); // just verify it doesn't crash
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_mock_arduino_compiles);
    RUN_TEST(test_mock_millis);
    RUN_TEST(test_mock_delay);
    RUN_TEST(test_mock_string);
    RUN_TEST(test_mock_serial_printf);
    return UNITY_END();
}
