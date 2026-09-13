#pragma once
#include <stdint.h>
#include <stddef.h>
#include <openssl/evp.h>
#include "qrxdb.h"
#include "protocol/qrx_service_effects.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_COMPUTE_PROVIDER_IDENTITY_VERSION 1u
#define QRX_COMPUTE_PROVIDER_ID_MAX 128u
#define QRX_COMPUTE_PROVIDER_SCHEME_MAX 32u
#define QRX_COMPUTE_PROVIDER_KEY_B64_MAX 16384u
#define QRX_COMPUTE_PROVIDER_IDENTITY_PREFIX "compute/provider-identity/"
#define QRX_COMPUTE_PROVIDER_KEY_SCHEME "ML-DSA-65"
#define QRX_COMPUTE_PROVIDER_BIND_TX "COMPUTE_PROVIDER_BIND_IDENTITY"

typedef struct {
    uint32_t version;
    char provider_id[QRX_COMPUTE_PROVIDER_ID_MAX+1];
    char owner[160];
    char key_scheme[QRX_COMPUTE_PROVIDER_SCHEME_MAX];
    char public_key_pem_b64[QRX_COMPUTE_PROVIDER_KEY_B64_MAX];
    uint64_t sequence;
    uint64_t bound_height;
} QrxComputeProviderIdentity;

int qrx_compute_provider_identity_validate(const QrxComputeProviderIdentity *identity);
int qrx_compute_provider_identity_get(QrxDB *db,const char *provider_id,QrxComputeProviderIdentity *out);
int qrx_compute_provider_identity_key_lookup(void *ctx,const char *provider_id,EVP_PKEY **public_key_out);
int qrx_compute_provider_identity_prepare(QrxDB *db,const char *tx_type,const char *from,const char *to,
                                          uint64_t amount_atoms,const char *payload,uint64_t height,
                                          QrxServiceEconomicEffect *effect);
int qrx_compute_provider_identity_stage(QrxDB *db,QrxDBBatch *batch,const char *tx_type,const char *from,const char *to,
                                        uint64_t amount_atoms,const char *payload,uint64_t height);
int qrx_compute_provider_identity_commitment(const QrxComputeProviderIdentity *identity,char out_hex[65]);

#ifdef __cplusplus
}
#endif
