#pragma once
/**
 * @file ota_crypto.h
 * @brief Pure cryptographic helpers for OTA firmware verification.
 *
 * Zero hardware dependencies — testable natively with PlatformIO native env.
 */

#include <cstddef>
#include <cstdint>

namespace OTACrypto {

/** Convert 32-byte binary hash to lowercase 64-char hex string. */
void hexify(const uint8_t* hash, char* out);

/** Parse 64-char hex string to 32-byte binary. Returns false on parse error. */
bool unhexify(const char* hex, uint8_t* out);

/**
 * Verify an ECDSA secp256r1 signature against a 32-byte SHA-256 hash.
 *
 * @param hash  32-byte SHA-256 digest of the firmware binary
 * @param sig   64-byte raw ECDSA signature (r || s, 32 bytes each)
 * @param pubkey  65-byte uncompressed public key (0x04 || X || Y)
 * @param pubkey_len  must be 65
 * @return true if signature is valid
 */
bool verify_ecdsa_signature(const uint8_t* hash, const uint8_t* sig, const uint8_t* pubkey, size_t pubkey_len);

} // namespace OTACrypto
