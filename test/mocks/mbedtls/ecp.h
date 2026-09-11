#pragma once
#include <cstddef>

#define MBEDTLS_ECP_DP_SECP256R1 0

struct mbedtls_ecp_group {};
struct mbedtls_ecp_point {};

inline int mbedtls_ecp_group_load(mbedtls_ecp_group*, int) {
    return -1;
}

inline int mbedtls_ecp_point_read_binary(mbedtls_ecp_group*, mbedtls_ecp_point*, const unsigned char*, size_t) {
    return -1;
}
