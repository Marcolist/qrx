#pragma once
#include <stdint.h>
#include "qrxdb.h"
#include "compute/qrx_pouc_journal.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_POUC_CONSENSUS_VERSION 1u
#define QRX_POUC_CHALLENGE_PENDING 1u
#define QRX_POUC_CHALLENGE_PASSED  2u
#define QRX_POUC_CHALLENGE_FAILED  3u

/* Prepared read-only effect for one POUC_SETTLEMENT transaction. Wallet/account
 * deltas are intentionally explicit so qrx.c can merge them with the ordinary
 * transaction fee debit before one authoritative QRXDB/WAL batch is committed. */
typedef struct {
    uint32_t version;
    QrxPoucSettlementRequest request;
    QrxPoucSettlementDecision decision;
    QrxComputeEscrow escrow_before;
    QrxComputeEscrow escrow_after;
    char provider[QRX_POUC_MAX_PROVIDER];
    char owner[160];
    uint64_t provider_credit_atoms;
    uint64_t owner_credit_atoms;
    uint64_t development_credit_atoms;
    uint64_t network_credit_atoms;
    uint64_t escrow_pool_debit_atoms;
} QrxPoucConsensusEffect;

int qrx_pouc_consensus_prepare(QrxDB *db,
                               const char *chain_id,
                               const char *genesis_hash,
                               const char *protocol_version,
                               const char *tx_type,
                               const char *from,
                               const char *to,
                               uint64_t amount_atoms,
                               const char *payload,
                               int protocol_activation_ready,
                               uint64_t apply_height,
                               QrxPoucConsensusEffect *out);

/* Stages journal/replay/escrow, pool debit and slash/jail evidence into an
 * already-open applytx/block batch. Wallet balance credits are represented by
 * QrxPoucConsensusEffect and are staged by the caller in the same batch. */
int qrx_pouc_consensus_stage(QrxDB *db,
                             QrxDBBatch *batch,
                             const QrxPoucConsensusEffect *effect,
                             const char *txid,
                             uint64_t apply_height);

/* Integration primitives for the compute-market / verifier pipeline. They are
 * deterministic state writers, not privileged RPCs. */
int qrx_pouc_consensus_stage_escrow_lock(QrxDB *db, QrxDBBatch *batch, const QrxComputeEscrow *escrow);
int qrx_pouc_consensus_stage_readiness(QrxDBBatch *batch, const QrxPoucMainnetReadiness *readiness);
int qrx_pouc_consensus_stage_challenge(QrxDBBatch *batch, const char receipt_commitment[65], uint32_t status);
int qrx_pouc_consensus_stage_adversarial(QrxDBBatch *batch, const char receipt_commitment[65], const QrxComputeAdversarialVerdict *verdict);

#ifdef __cplusplus
}
#endif
