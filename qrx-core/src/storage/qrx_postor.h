#pragma once

#include "storage/qrx_merkle.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char contract_id[129];
    char provider_id[129];
    uint32_t shard_index;
    uint64_t epoch;
    uint64_t leaf_index;
} QrxPoStorChallenge;

int qrx_postor_derive_challenge(const char *contract_id,
                                const char *provider_id,
                                uint32_t shard_index,
                                uint64_t epoch,
                                uint64_t leaf_count,
                                const uint8_t finalized_block_hash[64],
                                QrxPoStorChallenge *out);

int qrx_postor_verify(const QrxPoStorChallenge *challenge,
                      const uint8_t *revealed_leaf,
                      size_t revealed_leaf_len,
                      const QrxMerkleProof *proof,
                      const uint8_t shard_root[64]);

#ifdef __cplusplus
}
#endif
