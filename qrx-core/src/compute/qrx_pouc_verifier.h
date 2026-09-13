#pragma once
#include <stdint.h>
#include "qrxdb.h"
#include "compute/qrx_compute.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_POUC_VERIFIER_SELECTION_VERSION 1u
#define QRX_POUC_VERIFIER_MAX_PRIMARY 3u
#define QRX_POUC_VERIFIER_MAX_CHALLENGE 3u
#define QRX_POUC_VERIFIER_MAX_CANDIDATES 512u
#define QRX_POUC_VERIFIER_ADDRESS_MAX 160u
#define QRX_POUC_VERIFY_WINDOW_BLOCKS 20u
#define QRX_POUC_CHALLENGE_WINDOW_BLOCKS 20u
#define QRX_POUC_PRIMARY_REWARD_WEIGHT 2u
#define QRX_POUC_CHALLENGE_REWARD_WEIGHT 1u

#define QRX_POUC_VERIFIER_SELECTION_PREFIX "consensus:compute:verifier_selection:"
#define QRX_POUC_COMPUTE_JAIL_PREFIX "consensus:compute:jail_until_height:"
#define QRX_POUC_COMPUTE_SLASH_POOL_KEY "consensus:compute:slashing_pool"
#define QRX_POUC_FRAUD_SLASH_BPS_KEY "consensus:compute:params:fraud_slash_bps"
#define QRX_POUC_FRAUD_JAIL_BLOCKS_KEY "consensus:compute:params:fraud_jail_blocks"

#define QRX_POUC_MISS_THRESHOLD_KEY "consensus:compute:params:verifier_miss_threshold"
#define QRX_POUC_MISS_JAIL_BLOCKS_KEY "consensus:compute:params:verifier_miss_jail_blocks"
#define QRX_POUC_PENALTY_JOURNAL_PREFIX "consensus:compute:penalty_journal:"

/* Selection is produced from a finalized ancestor block hash + receipt
 * commitment and a chain-authoritative bonded-validator set. Provider and
 * owner are excluded. Primary and challenger sets never overlap. */
typedef struct {
    uint32_t version;
    char receipt_commitment[65];
    uint64_t entropy_height;
    char entropy_block_hash[129];
    uint32_t verification_mode;
    uint32_t target_verifiers;
    uint32_t primary_count;
    uint32_t challenge_count;
    char primary[QRX_POUC_VERIFIER_MAX_PRIMARY][QRX_POUC_VERIFIER_ADDRESS_MAX];
    char challenger[QRX_POUC_VERIFIER_MAX_CHALLENGE][QRX_POUC_VERIFIER_ADDRESS_MAX];
    uint64_t primary_reward_atoms;
    uint64_t challenge_reward_atoms;
    uint64_t verification_deadline_height;
    uint64_t challenge_deadline_height;
    char selection_commitment[65];
} QrxPoucVerifierSelection;

int qrx_pouc_verifier_selection_build(QrxDB *db,
                                      const char receipt_commitment[65],
                                      const char *owner,
                                      const char *provider,
                                      uint32_t verification_mode,
                                      uint64_t entropy_height,
                                      uint64_t receipt_apply_height,
                                      uint64_t verification_reward_atoms,
                                      QrxPoucVerifierSelection *out);
int qrx_pouc_verifier_selection_stage(QrxDBBatch *batch,const QrxPoucVerifierSelection *selection);
int qrx_pouc_verifier_selection_get(QrxDB *db,const char receipt_commitment[65],QrxPoucVerifierSelection *out);
int qrx_pouc_verifier_is_primary(const QrxPoucVerifierSelection *selection,const char *validator);
int qrx_pouc_verifier_is_challenger(const QrxPoucVerifierSelection *selection,const char *validator);
int qrx_pouc_verifier_is_compute_jailed(QrxDB *db,const char *validator,uint64_t height);
int qrx_pouc_verifier_selection_commitment(const QrxPoucVerifierSelection *selection,char out_hex[65]);

/* Applies a configured fraud slash/jail to authoritative QRXDB staking state.
 * No hidden default slash economics are used: slash bps and jail blocks must
 * be present in canonical consensus parameters when a slash/jail candidate is
 * executed. Slashed stake is moved to a protocol slashing pool rather than
 * minted/burned implicitly. */
int qrx_pouc_stage_consensus_penalty(QrxDB *db,
                                     QrxDBBatch *batch,
                                     const char *validator,
                                     const char receipt_commitment[65],
                                     uint64_t apply_height,
                                     uint8_t slash,
                                     uint8_t jail,
                                     uint64_t *out_slashed_atoms,
                                     uint64_t *out_jail_until_height);

int qrx_pouc_stage_consensus_penalty_custom(QrxDB *db,
                                            QrxDBBatch *batch,
                                            const char *validator,
                                            const char penalty_id[65],
                                            const char receipt_commitment[65],
                                            uint64_t apply_height,
                                            uint64_t slash_bps,
                                            uint64_t jail_blocks,
                                            uint64_t *out_slashed_atoms,
                                            uint64_t *out_jail_until_height);
int qrx_pouc_penalty_journal_revert_above_height(QrxDB *db,uint64_t canonical_height,uint32_t *out_reverted);

#ifdef __cplusplus
}
#endif
