#pragma once

#include "storage/qrx_merkle.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QRX_CAPACITY_PROOF_VERSION 1
#define QRX_CAPACITY_DEFAULT_SLOT_BYTES (64u * 1024u)

typedef struct {
    uint32_t version;
    char provider_id[129];
    uint64_t epoch;
    uint64_t slot_count;
    uint32_t slot_bytes;
    uint8_t merkle_root[QRX_MERKLE_HASH_BYTES];
} QrxCapacityCommitment;

typedef struct {
    uint64_t slot_index;
    QrxMerkleProof merkle_proof;
} QrxCapacityChallengeProof;

int qrx_capacity_commit(const char *provider_id,
                        uint64_t epoch,
                        const uint8_t *slots,
                        size_t slot_count,
                        size_t slot_bytes,
                        QrxCapacityCommitment *out);

uint64_t qrx_capacity_challenge_index(const QrxCapacityCommitment *commitment,
                                      const uint8_t finalized_block_hash[64]);

int qrx_capacity_build_challenge_proof(const QrxCapacityCommitment *commitment,
                                       const uint8_t *slots,
                                       size_t slot_count,
                                       size_t slot_bytes,
                                       const uint8_t finalized_block_hash[64],
                                       QrxCapacityChallengeProof *out);

int qrx_capacity_verify_challenge(const QrxCapacityCommitment *commitment,
                                  const uint8_t finalized_block_hash[64],
                                  const uint8_t *revealed_slot,
                                  size_t revealed_slot_len,
                                  const QrxCapacityChallengeProof *proof);

#ifdef __cplusplus
}
#endif
