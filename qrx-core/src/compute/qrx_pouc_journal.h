#pragma once
#include <stdint.h>
#include <stddef.h>
#include "qrxdb.h"
#include "compute/qrx_pouc_mainnet.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_POUC_JOURNAL_VERSION 1u
#define QRX_POUC_JOURNAL_MAX_REVERT 256u

typedef enum {
    QRX_POUC_JOURNAL_NONE = 0,
    QRX_POUC_JOURNAL_FROZEN = 1,
    QRX_POUC_JOURNAL_PAYOUT = 2,
    QRX_POUC_JOURNAL_REJECTED = 3,
    QRX_POUC_JOURNAL_REVERTED = 4
} QrxPoucJournalState;

typedef struct {
    uint32_t version;
    QrxPoucJournalState state;
    uint64_t applied_height;
    uint64_t settlement_nonce;
    uint64_t payout_compute_atoms;
    uint64_t refund_atoms;
    uint8_t slash_candidate;
    uint8_t jail_candidate;
    char replay_key[65];
    char receipt_commitment[65];
    char decision_commitment[65];
    char graph_commitment[65];
    char chain_id[QRX_POUC_MAINNET_CHAIN_ID_MAX+1];
    char genesis_hash[65];
    char protocol_version[QRX_POUC_MAINNET_PROTOCOL_MAX+1];
} QrxPoucJournalEntry;

/*
 * Atomically persists a settlement decision and the resulting escrow snapshot
 * in QRXDB/WAL. Frozen -> terminal is allowed for the same replay key; a
 * terminal replay key is rejected. This journal does not transfer wallet
 * balances and does not execute consensus slashing itself.
 */
int qrx_pouc_journal_apply(QrxDB *db,
                           const QrxPoucSettlementRequest *req,
                           const QrxPoucSettlementDecision *decision,
                           QrxComputeEscrow *escrow);

/* Stage the same journal transition into an already-open outer QRXDB batch.
 * This is used by the authoritative applytx/block state transition so journal,
 * replay marker, escrow snapshot and wallet/accounting effects can share one
 * WAL commit. No commit is performed here. */
int qrx_pouc_journal_stage(QrxDB *db,
                           QrxDBBatch *batch,
                           const QrxPoucSettlementRequest *req,
                           const QrxPoucSettlementDecision *decision,
                           const QrxComputeEscrow *escrow_before,
                           QrxComputeEscrow *escrow_after);

/* Canonical compute-market integration helpers. The market/lock transaction
 * can seed/update the same escrow snapshot later consumed by PoUC settlement. */
int qrx_pouc_journal_stage_escrow(QrxDBBatch *batch, const QrxComputeEscrow *escrow);
int qrx_pouc_journal_put_escrow(QrxDB *db, const QrxComputeEscrow *escrow);

int qrx_pouc_journal_get(QrxDB *db, const char *replay_key, QrxPoucJournalEntry *out);
int qrx_pouc_journal_get_escrow(QrxDB *db, const char *graph_commitment, QrxComputeEscrow *out);
int qrx_pouc_journal_replay_consumed(QrxDB *db, const char *replay_key);

/*
 * Reorg helper: journal entries above canonical_height become REVERTED and
 * their pre-transition escrow snapshots are restored atomically. A reverted
 * replay key may be applied again on the new canonical branch.
 */
int qrx_pouc_journal_revert_above_height(QrxDB *db, uint64_t canonical_height, uint32_t *out_reverted);

const char *qrx_pouc_journal_state_name(QrxPoucJournalState s);

#ifdef __cplusplus
}
#endif
