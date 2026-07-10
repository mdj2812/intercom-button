#include "ota_crypto.h"

#include <cstdio>
#include <cstring>

#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>

namespace OTACrypto {

void hexify(const uint8_t* hash, char* out) {
    for (int i = 0; i < 32; i++) {
        sprintf(out + i * 2, "%02x", hash[i]);
    }
    out[64] = '\0';
}

bool unhexify(const char* hex, uint8_t* out) {
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

bool verify_ecdsa_signature(const uint8_t* hash, const uint8_t* sig, const uint8_t* pubkey, size_t pubkey_len) {
    if (pubkey_len != 65)
        return false;

    mbedtls_ecdsa_context ecdsa;
    mbedtls_ecdsa_init(&ecdsa);

    int ret = mbedtls_ecp_group_load(&ecdsa.grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        mbedtls_ecdsa_free(&ecdsa);
        return false;
    }

    ret = mbedtls_ecp_point_read_binary(&ecdsa.grp, &ecdsa.Q, pubkey, 65);
    if (ret != 0) {
        mbedtls_ecdsa_free(&ecdsa);
        return false;
    }

    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    mbedtls_mpi_read_binary(&r, sig, 32);
    mbedtls_mpi_read_binary(&s, sig + 32, 32);

    ret = mbedtls_ecdsa_verify(&ecdsa.grp, hash, 32, &ecdsa.Q, &r, &s);

    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    mbedtls_ecdsa_free(&ecdsa);

    return ret == 0;
}

} // namespace OTACrypto
