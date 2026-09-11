#pragma once
#include "ecp.h"
#include <cstddef>

struct mbedtls_mpi {};

struct mbedtls_ecdsa_context {
    mbedtls_ecp_group grp;
    mbedtls_ecp_point Q;
};

inline void mbedtls_ecdsa_init(mbedtls_ecdsa_context*) {}
inline void mbedtls_ecdsa_free(mbedtls_ecdsa_context*) {}
inline void mbedtls_mpi_init(mbedtls_mpi*) {}
inline void mbedtls_mpi_free(mbedtls_mpi*) {}
inline int mbedtls_mpi_read_binary(mbedtls_mpi*, const unsigned char*, size_t) {
    return 0;
}
inline int mbedtls_ecdsa_verify(mbedtls_ecp_group*, const unsigned char*, size_t, mbedtls_ecp_point*, mbedtls_mpi*,
                                mbedtls_mpi*) {
    return -1;
}
