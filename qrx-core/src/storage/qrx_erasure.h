#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned data_shards;
    unsigned parity_shards;
    size_t shard_size;
    size_t original_size;
    uint8_t **shards;
    uint8_t *present;
} QrxErasureSet;

int qrx_erasure_encode(const uint8_t *data, size_t data_len,
                       unsigned data_shards, unsigned parity_shards,
                       QrxErasureSet *out);
int qrx_erasure_reconstruct(QrxErasureSet *set);
int qrx_erasure_join(const QrxErasureSet *set, uint8_t **data_out, size_t *data_len_out);
void qrx_erasure_free(QrxErasureSet *set);
/* Bounded-memory file encoder using the same systematic RS matrix as
 * qrx_erasure_encode(). output_paths must contain k+m writable paths. */
int qrx_erasure_encode_file(const char *source_path,unsigned data_shards,unsigned parity_shards,
                            const char *const *output_paths,size_t stripe_bytes,
                            uint64_t *original_size_out,uint64_t *shard_size_out);
#ifdef __cplusplus
}
#endif
