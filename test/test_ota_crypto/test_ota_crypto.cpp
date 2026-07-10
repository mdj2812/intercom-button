/**
 * @file test_ota_crypto.cpp
 * @brief Unit tests for OTACrypto — hexify, unhexify, ECDSA verify roundtrip.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unity.h>

static void hexify(const uint8_t* hash, char* out) {
    for (int i = 0; i < 32; i++)
        sprintf(out + i * 2, "%02x", hash[i]);
    out[64] = '\0';
}

static bool unhexify(const char* hex, uint8_t* out) {
    if (strlen(hex) != 64) return false;
    for (int i = 0; i < 32; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return false;
        out[i] = (uint8_t) byte;
    }
    return true;
}

// ── hexify / unhexify tests ───────────────────────────

void test_hexify_known() {
    uint8_t hash[32] = {
        0x8d, 0x17, 0x3f, 0x20, 0x3c, 0x21, 0xe1, 0x34,
        0x23, 0x83, 0x50, 0xcf, 0x6a, 0x72, 0x1a, 0xec,
        0xed, 0x20, 0x9c, 0x41, 0x89, 0xd4, 0xfa, 0x6e,
        0xe3, 0x5b, 0xf2, 0x68, 0xc5, 0x33, 0x35, 0x20
    };
    char out[65];
    hexify(hash, out);
    TEST_ASSERT_EQUAL_STRING("8d173f203c21e134238350cf6a721aeced209c4189d4fa6ee35bf268c5333520", out);
}

void test_hexify_zero() {
    uint8_t hash[32] = {};
    char out[65];
    hexify(hash, out);
    TEST_ASSERT_EQUAL_STRING("0000000000000000000000000000000000000000000000000000000000000000", out);
}

void test_unhexify_known() {
    uint8_t out[32] = {};
    TEST_ASSERT_TRUE(unhexify("8d173f203c21e134238350cf6a721aeced209c4189d4fa6ee35bf268c5333520", out));
    TEST_ASSERT_EQUAL_UINT8(0x8d, out[0]);
    TEST_ASSERT_EQUAL_UINT8(0x17, out[1]);
    TEST_ASSERT_EQUAL_UINT8(0x20, out[31]);
}

void test_unhexify_roundtrip() {
    uint8_t original[32];
    for (int i = 0; i < 32; i++)
        original[i] = (uint8_t)(i * 7 + 13);
    char hex[65];
    hexify(original, hex);
    uint8_t decoded[32] = {};
    TEST_ASSERT_TRUE(unhexify(hex, decoded));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(original, decoded, 32);
}

void test_unhexify_bad_length() {
    uint8_t out[32] = {};
    TEST_ASSERT_FALSE(unhexify("abc", out));
    TEST_ASSERT_FALSE(unhexify(
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef00", out));
}

void test_unhexify_bad_chars() {
    uint8_t out[32] = {};
    TEST_ASSERT_FALSE(unhexify(
        "gg173f203c21e134238350cf6a721aeced209c4189d4fa6ee35bf268c5333520", out));
}

// ── ECDSA tests (via openssl shell) ───────────────────

static const char* TEST_PAYLOAD =
    "hello world hello world hello world hello world hello world hello world "
    "hello world hello world hello world hello world hello world hello world "
    "hello world hello world hello world hello world hello world hello world "
    "hello world hello world hello world hello world hello world hello world "
    "hello wor";

void test_ecdsa_sign_verify_roundtrip() {
    FILE* f = fopen("/tmp/ota_test_payload.bin", "wb");
    fwrite(TEST_PAYLOAD, 1, strlen(TEST_PAYLOAD), f);
    fclose(f);

    // Generate ephemeral key pair for this test
    system("openssl ecparam -name prime256v1 -genkey "
           "-out /tmp/ota_test_key.pem 2>/dev/null");
    system("openssl ec -in /tmp/ota_test_key.pem -pubout "
           "-out /tmp/ota_test_pub.pem 2>/dev/null");
    system("openssl dgst -sha256 -sign /tmp/ota_test_key.pem "
           "-out /tmp/ota_test_sig.der "
           "/tmp/ota_test_payload.bin 2>/dev/null");

    FILE* vp = popen(
        "openssl dgst -sha256 -verify /tmp/ota_test_pub.pem "
        "-signature /tmp/ota_test_sig.der "
        "/tmp/ota_test_payload.bin 2>&1", "r");
    char result[128] = {};
    fread(result, 1, sizeof(result) - 1, vp);
    int exit_code = pclose(vp);

    TEST_ASSERT_TRUE_MESSAGE(
        exit_code == 0 && strstr(result, "Verified OK") != nullptr,
        result);
}

void test_ecdsa_bad_signature() {
    // Write a fake DER signature
    uint8_t fake_der[128];
    int pos = 0;
    fake_der[pos++] = 0x30;
    fake_der[pos++] = 68;
    fake_der[pos++] = 0x02;
    fake_der[pos++] = 32;
    for (int i = 0; i < 32; i++) fake_der[pos++] = (uint8_t)rand();
    fake_der[pos++] = 0x02;
    fake_der[pos++] = 32;
    for (int i = 0; i < 32; i++) fake_der[pos++] = (uint8_t)rand();

    FILE* f = fopen("/tmp/ota_test_payload.bin", "wb");
    fwrite(TEST_PAYLOAD, 1, strlen(TEST_PAYLOAD), f);
    fclose(f);

    f = fopen("/tmp/ota_test_fake.der", "wb");
    fwrite(fake_der, 1, pos, f);
    fclose(f);

    system("openssl ecparam -name prime256v1 -genkey "
           "-out /tmp/ota_test_key2.pem 2>/dev/null");
    system("openssl ec -in /tmp/ota_test_key2.pem -pubout "
           "-out /tmp/ota_test_pub.pem 2>/dev/null");

    FILE* vp = popen(
        "openssl dgst -sha256 -verify /tmp/ota_test_pub.pem "
        "-signature /tmp/ota_test_fake.der "
        "/tmp/ota_test_payload.bin 2>&1", "r");
    char result[128] = {};
    fread(result, 1, sizeof(result) - 1, vp);
    int exit_code = pclose(vp);

    TEST_ASSERT_TRUE_MESSAGE(
        exit_code != 0 || strstr(result, "Verification Failure") != nullptr,
        "Bad signature should not verify");
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_hexify_known);
    RUN_TEST(test_hexify_zero);
    RUN_TEST(test_unhexify_known);
    RUN_TEST(test_unhexify_roundtrip);
    RUN_TEST(test_unhexify_bad_length);
    RUN_TEST(test_unhexify_bad_chars);
    RUN_TEST(test_ecdsa_sign_verify_roundtrip);
    RUN_TEST(test_ecdsa_bad_signature);
    return UNITY_END();
}
