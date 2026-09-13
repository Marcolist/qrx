#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QRX_MERKLE_HASH_BYTES 64
#define QRX_MERKLE_MAX_DEPTH 64

typedef struct {
    uint64_t leaf_index;
    uint64_t leaf_count;
    uint32_t depth;
    uint8_t siblings[QRX_MERKLE_MAX_DEPTH][QRX_MERKLE_HASH_BYTES];
} QrxMerkleProof;

int qrx_merkle_leaf_hash(uint64_t index,
                         const uint8_t *data,
                         size_t data_len,
                         uint8_t out[QRX_MERKLE_HASH_BYTES]);

int qrx_merkle_root_from_leaves(const uint8_t *leaf_hashes,
                                size_t leaf_count,
                                uint8_t out[QRX_MERKLE_HASH_BYTES]);

int qrx_merkle_root_from_chunks(const uint8_t *data,
                                size_t data_len,
                                size_t chunk_size,
                                uint8_t out[QRX_MERKLE_HASH_BYTES],
                                size_t *leaf_count_out);

int qrx_merkle_build_proof_from_leaves(const uint8_t *leaf_hashes,
                                       size_t leaf_count,
                                       size_t leaf_index,
                                       QrxMerkleProof *out);

int qrx_merkle_verify_proof(const uint8_t leaf_hash[QRX_MERKLE_HASH_BYTES],
                            const QrxMerkleProof *proof,
                            const uint8_t expected_root[QRX_MERKLE_HASH_BYTES]);

#ifdef __cplusplus
}
#endif
