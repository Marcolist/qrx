#pragma once
#include <openssl/evp.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_DRIVE_PRIVATE_PQ_CHUNK (1024u*1024u)
int qrx_drive_private_pq_encrypt_file(const char *source,const char *container,EVP_PKEY *kem_public,EVP_PKEY *signer_private,uint64_t *plaintext_bytes,uint64_t *ciphertext_bytes);
int qrx_drive_private_pq_decrypt_file(const char *container,const char *destination,EVP_PKEY *kem_private,EVP_PKEY *signer_public,uint64_t *plaintext_bytes);
#ifdef __cplusplus
}
#endif
