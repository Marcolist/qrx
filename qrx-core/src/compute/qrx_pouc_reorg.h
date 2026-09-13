#pragma once
#include <stdint.h>
#include "qrxdb.h"
#include "compute/qrx_pouc_pipeline.h"
#include "compute/qrx_pouc_undo.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_POUC_REORG_VERSION 1u
#define QRX_POUC_REWARD_JOURNAL_PREFIX "consensus:compute:reward_journal:"
#define QRX_POUC_REWARD_JOURNAL_MAX_REVERT 512u

typedef struct {
    uint32_t version;
    uint64_t apply_height;
    uint8_t role; /* 1 primary, 2 challenger */
    uint64_t reward_atoms;
    uint64_t pool_before;
    uint64_t pool_after;
    uint64_t verification_spent_before;
    uint64_t verification_spent_after;
    char txid[129];
    char receipt_commitment[65];
    char graph_commitment[65];
    char verifier[QRX_POUC_VERIFIER_ADDRESS_MAX];
} QrxPoucRewardJournalEntry;

typedef struct {
    uint32_t version;
    uint64_t canonical_height;
    uint32_t atomic_tx_reverted;
    uint32_t settlement_reverted;
    uint32_t liveness_reverted;
    uint32_t reward_reverted;
} QrxPoucCanonicalReorgReport;

/* Stage the economic undo record in the SAME QRXDB batch as a VERIFY or
 * CHALLENGE reward. The caller remains responsible for staging the wallet
 * balance credit itself in that same outer applytx batch. */
int qrx_pouc_reward_journal_stage(QrxDB *db,
                                  QrxDBBatch *batch,
                                  const QrxPoucPipelineEffect *effect,
                                  const char *txid,
                                  uint64_t apply_height);
int qrx_pouc_reward_journal_revert_above_height(QrxDB *db,
                                                uint64_t canonical_height,
                                                uint32_t *out_reverted);

/* Canonical PoUC disconnect/reorg hook. Ordering is intentional: settlement
 * is restored first, then liveness/penalties, then earlier verification
 * reward debits/credits. This prevents a settlement snapshot from
 * re-introducing verification_reward_spent after reward rollback. */
int qrx_pouc_canonical_reorg_hook(QrxDB *db,
                                  uint64_t canonical_height,
                                  QrxPoucCanonicalReorgReport *out);

#ifdef __cplusplus
}
#endif
