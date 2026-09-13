#pragma once
#include "storage/qrx_storage_fs.h"
#include <openssl/evp.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_DRIVE_OBJECT_VERSION 1u
#define QRX_DRIVE_OBJECT_DEFAULT_CHUNK_BYTES (8u * 1024u * 1024u)
#define QRX_DRIVE_OBJECT_MIN_CHUNK_BYTES (64u * 1024u)
#define QRX_DRIVE_OBJECT_MAX_CHUNK_BYTES (64u * 1024u * 1024u)
#define QRX_DRIVE_OBJECT_INDEX_FANOUT 64u

typedef struct {
    uint64_t plaintext_bytes;
    uint32_t chunk_bytes;
    uint32_t tree_level;
    uint64_t chunk_count;
    char object_id[65];
    char root_index_id[65];
} QrxDriveObjectInfo;

/* Stores an arbitrarily large logical file with bounded memory use.
 * The logical object has no product-level file-size cap: its length is a
 * uint64_t protocol value and is split into independently encrypted chunks.
 * chunk_bytes==0 selects QRX_DRIVE_OBJECT_DEFAULT_CHUNK_BYTES.
 */
int qrx_drive_object_put_file(QrxStorageFs *fs,
                              const char *source_path,
                              EVP_PKEY *recipient_public,
                              uint32_t chunk_bytes,
                              QrxDriveObjectInfo *out);

/* Streams and decrypts a logical object back to disk without loading the whole
 * object into memory. The supplied object_id is the content ID of the root
 * descriptor, not of any individual chunk. */
int qrx_drive_object_get_file(QrxStorageFs *fs,
                              const char *object_id,
                              EVP_PKEY *recipient_private,
                              const char *destination_path,
                              QrxDriveObjectInfo *out_info);

#ifdef __cplusplus
}
#endif
