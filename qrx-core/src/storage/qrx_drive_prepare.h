#pragma once
#include "storage/qrx_drive_manifest.h"
#include "storage/qrx_storage_transport.h"
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_DRIVE_PREPARE_MAX_SHARDS 32
#define QRX_DRIVE_POSTOR_LEAF_BYTES (64u * 1024u)
#define QRX_DRIVE_SUITE_PRIVATE_PQ "qrx-drive-pq-v1"
#define QRX_DRIVE_SUITE_PUBLIC_SIGNED "qrx-net-public-signed-v1"
typedef struct {
 char prepare_id[129];
 char package_dir[1024];
 char encrypted_path[1200];
 uint64_t plaintext_bytes,ciphertext_bytes,shard_bytes;
 unsigned data_shards,parity_shards,total_shards;
 char shard_object_ids[QRX_DRIVE_PREPARE_MAX_SHARDS][65];
 uint8_t shard_merkle_roots[QRX_DRIVE_PREPARE_MAX_SHARDS][64];
 uint64_t shard_leaf_counts[QRX_DRIVE_PREPARE_MAX_SHARDS];
 char shard_paths[QRX_DRIVE_PREPARE_MAX_SHARDS][1200];
 QrxDriveManifest manifest;
 uint8_t manifest_hash[64];
 uint8_t *manifest_signature; size_t manifest_signature_len;
} QrxDrivePreparedUpload;
int qrx_drive_prepare_private_upload(const char *root,const char *source,const char *profile,unsigned k,unsigned m,EVP_PKEY *kem_public,EVP_PKEY *signer_private,QrxDrivePreparedUpload *out);
/* PUBLIC_SIGNED payload preparation: no encryption is applied; the caller must
 * pass an already signed/verified public artifact. Erasure coding and PoStor
 * commitments are generated with bounded-memory file I/O. */
int qrx_drive_prepare_public_signed_upload(const char *root,const char *source,const char *profile,unsigned k,unsigned m,EVP_PKEY *signer_private,QrxDrivePreparedUpload *out);
void qrx_drive_prepared_upload_free(QrxDrivePreparedUpload *p);
int qrx_drive_prepared_upload_save(const QrxDrivePreparedUpload *p);
int qrx_drive_prepared_upload_load(const char *package_dir,EVP_PKEY *signer_public,QrxDrivePreparedUpload *out);
int qrx_drive_prepared_upload_verify_files(const QrxDrivePreparedUpload *p);
int qrx_drive_prepared_upload_verify_sources(const QrxDrivePreparedUpload *p,const QrxShardProviderSource *sources,size_t count);
#ifdef __cplusplus
}
#endif
