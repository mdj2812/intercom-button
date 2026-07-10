/**
 * @file test_ota_crypto.cpp
 * @brief Unit tests for OTACrypto — hexify, unhexify, ECDSA verify roundtrip.
 *
 * hexify/unhexify are pure functions tested directly.
 * ECDSA is tested via the sign_firmware.py script + openssl verification.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unity.h>

#include "ota_keys.h"

// ── Pure helpers (duplicated from ota_crypto.cpp for native testing) ──

static void hexify(const uint8_t* hash, char* out) {
    for (int i = 0; i < 32; i++)
        sprintf(out + i * 2, "%02x", hash[i]);
    out[64] = '\0';
}

static bool unhexify(const char* hex, uint8_t* out) {
    if (strlen(hex) != 64)
        return false;
    for (int i = 0; i < 32; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1)
            return false;
        out[i] = (uint8_t) byte;
    }
    return true;
}

// ── hexify / unhexify tests ────────────────────────────

void test_hexify_known() {
    uint8_t hash[32] = {0x8d, 0x17, 0x3f, 0x20, 0x3c, 0x21, 0xe1, 0x34, 0x23, 0x83, 0x50, 0xcf, 0x6a, 0x72, 0x1a, 0xec,
                        0xed, 0x20, 0x9c, 0x41, 0x89, 0xd4, 0xfa, 0x6e, 0xe3, 0x5b, 0xf2, 0x68, 0xc5, 0x33, 0x35, 0x20};
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
        original[i] = (uint8_t) (i * 7 + 13);

    char hex[65];
    hexify(original, hex);

    uint8_t decoded[32] = {};
    TEST_ASSERT_TRUE(unhexify(hex, decoded));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(original, decoded, 32);
}

void test_unhexify_bad_length() {
    uint8_t out[32] = {};
    TEST_ASSERT_FALSE(unhexify("abc", out));
    TEST_ASSERT_FALSE(unhexify("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef00", out));
}

void test_unhexify_bad_chars() {
    uint8_t out[32] = {};
    TEST_ASSERT_FALSE(unhexify("gg173f203c21e134238350cf6a721aeced209c4189d4fa6ee35bf268c5333520", out));
}

// ── ECDSA sign → verify roundtrip (via shell) ─────────

static const char* TEST_PAYLOAD = "hello world hello world hello world hello world hello world hello world "
                                  "hello world hello world hello world hello world hello world hello world "
                                  "hello world hello world hello world hello world hello world hello world "
                                  "hello world hello world hello world hello world hello world hello world "
                                  "hello wor";

void test_ecdsa_sign_verify_roundtrip() {
    // Write test payload
    FILE* f = fopen("/tmp/ota_test_payload.bin", "wb");
    fwrite(TEST_PAYLOAD, 1, strlen(TEST_PAYLOAD), f);
    fclose(f);

    // Sign with our script → produces raw 64-byte r||s
    // (exit code may be non-zero due to Python 3.13 subprocess bug in
    // the self-verification step; the .sig file is created before that)
    system("python3 scripts/sign_firmware.py "
           "/tmp/ota_test_payload.bin scripts/ota_private.pem "
           "/tmp/ota_test_payload.bin.sig 2>/dev/null");

    // Read the 64-byte signature
    uint8_t sig[64];
    f = fopen("/tmp/ota_test_payload.bin.sig", "rb");
    TEST_ASSERT_NOT_NULL(f);
    size_t n = fread(sig, 1, 64, f);
    fclose(f);
    TEST_ASSERT_EQUAL(64, n);

    // Convert raw r||s to DER for openssl verification
    // Raw: 32 bytes r, 32 bytes s
    FILE* der = fopen("/tmp/ota_test_sig.der", "wb");
    size_t r_len = 32, s_len = 32;
    // Trim leading zeros from r
    const uint8_t* r_ptr = sig;
    while (r_len > 0 && *r_ptr == 0) {
        r_ptr++;
        r_len--;
    }
    if (r_len > 0 && (*r_ptr & 0x80)) {
        r_ptr--;
        r_len++;
    }
    // Trim leading zeros from s
    const uint8_t* s_ptr = sig + 32;
    while (s_len > 0 && *s_ptr == 0) {
        s_ptr++;
        s_len--;
    }
    if (s_len > 0 && (*s_ptr & 0x80)) {
        s_ptr--;
        s_len++;
    }

    uint8_t der_buf[128];
    int der_pos = 0;
    der_buf[der_pos++] = 0x30; // SEQUENCE
    der_buf[der_pos++] = (uint8_t) (4 + r_len + s_len);
    der_buf[der_pos++] = 0x02; // INTEGER r
    der_buf[der_pos++] = (uint8_t) r_len;
    memcpy(der_buf + der_pos, r_ptr, r_len);
    der_pos += r_len;
    der_buf[der_pos++] = 0x02; // INTEGER s
    der_buf[der_pos++] = (uint8_t) s_len;
    memcpy(der_buf + der_pos, s_ptr, s_len);
    der_pos += s_len;
    fwrite(der_buf, 1, der_pos, der);
    fclose(der);

    // Extract public key
    system("openssl ec -in scripts/ota_private.pem -pubout "
           "-out /tmp/ota_test_pub.pem 2>/dev/null");

    // Verify with openssl
    char verify_cmd[512];
    snprintf(verify_cmd, sizeof(verify_cmd),
             "openssl dgst -sha256 -verify /tmp/ota_test_pub.pem "
             "-signature /tmp/ota_test_sig.der "
             "/tmp/ota_test_payload.bin 2>&1");
    FILE* vp = popen(verify_cmd, "r");
    char result[128] = {};
    fread(result, 1, sizeof(result) - 1, vp);
    pclose(vp);

    TEST_ASSERT_TRUE_MESSAGE(strstr(result, "Verified OK") != nullptr, result);
}

void test_ecdsa_bad_signature() {
    // Write test payload
    FILE* f = fopen("/tmp/ota_test_payload.bin", "wb");
    fwrite(TEST_PAYLOAD, 1, strlen(TEST_PAYLOAD), f);
    fclose(f);

    // Create a wrong DER signature (64 random bytes encoded as DER)
    uint8_t fake_der[128];
    int der_pos = 0;
    fake_der[der_pos++] = 0x30;
    fake_der[der_pos++] = 68; // length
    fake_der[der_pos++] = 0x02;
    fake_der[der_pos++] = 32; // r len
    for (int i = 0; i < 32; i++)
        fake_der[der_pos++] = (uint8_t) rand();
    fake_der[der_pos++] = 0x02;
    fake_der[der_pos++] = 32; // s len
    for (int i = 0; i < 32; i++)
        fake_der[der_pos++] = (uint8_t) rand();

    f = fopen("/tmp/ota_test_fake.der", "wb");
    fwrite(fake_der, 1, der_pos, f);
    fclose(f);

    // Should fail verification
    system("openssl ec -in scripts/ota_private.pem -pubout "
           "-out /tmp/ota_test_pub.pem 2>/dev/null");

    FILE* vp = popen("openssl dgst -sha256 -verify /tmp/ota_test_pub.pem "
                     "-signature /tmp/ota_test_fake.der "
                     "/tmp/ota_test_payload.bin 2>&1",
                     "r");
    char result[128] = {};
    fread(result, 1, sizeof(result) - 1, vp);
    int exit_code = pclose(vp);

    TEST_ASSERT_TRUE_MESSAGE(exit_code != 0 || strstr(result, "Verification Failure") != nullptr,
                             "Bad signature should not verify");
}

// ── Runner ────────────────────────────────────────────

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
