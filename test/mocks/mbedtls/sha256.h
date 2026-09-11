#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

struct mbedtls_sha256_context {};

inline void mbedtls_sha256_init(mbedtls_sha256_context*) {}
inline void mbedtls_sha256_free(mbedtls_sha256_context*) {}
inline int mbedtls_sha256_starts_ret(mbedtls_sha256_context*, int) {
    return 0;
}
inline int mbedtls_sha256_update_ret(mbedtls_sha256_context*, const uint8_t*, size_t) {
    return 0;
}
inline int mbedtls_sha256_finish_ret(mbedtls_sha256_context*, uint8_t out[32]) {
    if (out)
        std::memset(out, 0, 32);
    return 0;
}
