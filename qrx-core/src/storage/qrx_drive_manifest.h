#pragma once
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_DRIVE_MANIFEST_VERSION 1
#define QRX_DRIVE_MANIFEST_MAX_SHARDS 32

typedef struct {
    uint32_t version;
    char crypto_suite[32];
    char profile[16];
    uint64_t plaintext_bytes;
    uint64_t ciphertext_bytes;
    uint32_t data_shards;
    uint32_t parity_shards;
    uint8_t ciphertext_root[64];
    uint8_t shard_roots[QRX_DRIVE_MANIFEST_MAX_SHARDS][64];
} QrxDriveManifest;

int qrx_drive_manifest_canonical(const QrxDriveManifest *m,uint8_t **out,size_t *out_len);
int qrx_drive_manifest_hash(const QrxDriveManifest *m,uint8_t out[64]);
int qrx_drive_manifest_generate_signing_key(EVP_PKEY **out);
int qrx_drive_manifest_sign(EVP_PKEY *private_key,const QrxDriveManifest *m,uint8_t **sig,size_t *sig_len);
int qrx_drive_manifest_verify(EVP_PKEY *public_key,const QrxDriveManifest *m,const uint8_t *sig,size_t sig_len);
#ifdef __cplusplus
}
#endif
