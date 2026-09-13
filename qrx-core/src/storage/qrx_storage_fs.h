#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct QrxStorageFs QrxStorageFs;

typedef struct {
    uint64_t max_usage_bytes;
    uint64_t min_free_space_bytes;
    uint64_t used_bytes;
    uint64_t reserved_bytes;
    uint64_t quota_available_bytes;
    uint64_t filesystem_free_bytes;
    uint64_t object_count;
} QrxStorageFsStats;

/* Opens/creates a local QRX Drive filesystem backend.
 * max_usage_bytes == 0 means no QRX quota (filesystem reserve still applies).
 * min_free_space_bytes is always kept outside QRX-managed storage. */
int qrx_storage_fs_open(const char *root,
                        uint64_t max_usage_bytes,
                        uint64_t min_free_space_bytes,
                        QrxStorageFs **out);
void qrx_storage_fs_close(QrxStorageFs *fs);

/* Removes abandoned partial writes and reconciles actual object bytes with the
 * persisted usage counter. Safe to call after an unclean shutdown. */
int qrx_storage_fs_recover(QrxStorageFs *fs);

/* Stores content by SHA3-256 content ID. Existing identical objects are
 * idempotent and do not consume quota twice. out_object_id_hex needs 65 bytes. */
int qrx_storage_fs_put(QrxStorageFs *fs,
                       const void *data,
                       size_t len,
                       char out_object_id_hex[65]);
int qrx_storage_fs_put_file(QrxStorageFs *fs,
                            const char *source_path,
                            char out_object_id_hex[65]);

int qrx_storage_fs_has(QrxStorageFs *fs, const char *object_id_hex);
int qrx_storage_fs_read(QrxStorageFs *fs,
                        const char *object_id_hex,
                        unsigned char **out,
                        size_t *out_len);
/* Bounded range read for resumable P2P transfers. length==0 succeeds with an
 * empty result; offsets beyond EOF fail closed. */
int qrx_storage_fs_read_range(QrxStorageFs *fs,
                              const char *object_id_hex,
                              uint64_t offset,
                              size_t length,
                              unsigned char **out,
                              size_t *out_len);
int qrx_storage_fs_get_file(QrxStorageFs *fs,
                            const char *object_id_hex,
                            const char *destination_path);
int qrx_storage_fs_delete(QrxStorageFs *fs, const char *object_id_hex);
int qrx_storage_fs_stats(QrxStorageFs *fs, QrxStorageFsStats *out);

const char *qrx_storage_fs_backend_name(void);

#ifdef __cplusplus
}
#endif
