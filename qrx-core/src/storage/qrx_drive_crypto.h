#pragma once

#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QRX_DRIVE_FILE_KEY_BYTES 32
#define QRX_DRIVE_GCM_NONCE_BYTES 12
#define QRX_DRIVE_GCM_TAG_BYTES 16
#define QRX_DRIVE_KEK_BYTES 32
#define QRX_DRIVE_KEM_NAME "X25519MLKEM768"
#define QRX_DRIVE_CRYPTO_SUITE "qrx-drive-pq-v1"

typedef struct {
    uint8_t *ciphertext;
    size_t ciphertext_len;
    uint8_t nonce[QRX_DRIVE_GCM_NONCE_BYTES];
    uint8_t tag[QRX_DRIVE_GCM_TAG_BYTES];
} QrxDriveCiphertext;

typedef struct {
    uint8_t *kem_ciphertext;
    size_t kem_ciphertext_len;
    uint8_t wrapped_file_key[QRX_DRIVE_FILE_KEY_BYTES];
    uint8_t nonce[QRX_DRIVE_GCM_NONCE_BYTES];
    uint8_t tag[QRX_DRIVE_GCM_TAG_BYTES];
} QrxDriveKeyEnvelope;

int qrx_drive_generate_recipient_key(EVP_PKEY **out);
int qrx_drive_random_file_key(uint8_t out[QRX_DRIVE_FILE_KEY_BYTES]);

int qrx_drive_encrypt(const uint8_t key[QRX_DRIVE_FILE_KEY_BYTES],
                      const uint8_t *plaintext, size_t plaintext_len,
                      const uint8_t *aad, size_t aad_len,
                      QrxDriveCiphertext *out);
int qrx_drive_decrypt(const uint8_t key[QRX_DRIVE_FILE_KEY_BYTES],
                      const QrxDriveCiphertext *ciphertext,
                      const uint8_t *aad, size_t aad_len,
                      uint8_t **plaintext_out, size_t *plaintext_len_out);
void qrx_drive_ciphertext_free(QrxDriveCiphertext *v);

int qrx_drive_wrap_file_key(EVP_PKEY *recipient_public,
                            const uint8_t file_key[QRX_DRIVE_FILE_KEY_BYTES],
                            QrxDriveKeyEnvelope *out);
int qrx_drive_unwrap_file_key(EVP_PKEY *recipient_private,
                              const QrxDriveKeyEnvelope *env,
                              uint8_t file_key_out[QRX_DRIVE_FILE_KEY_BYTES]);
void qrx_drive_key_envelope_free(QrxDriveKeyEnvelope *env);

#ifdef __cplusplus
}
#endif
