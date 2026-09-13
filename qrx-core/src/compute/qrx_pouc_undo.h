#pragma once
#include <stdint.h>
#include "qrxdb.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_POUC_TX_UNDO_VERSION 1u
#define QRX_POUC_TX_UNDO_PREFIX "consensus:compute:tx_undo:"
#define QRX_POUC_TX_UNDO_MAX_REVERT 4096u
#define QRX_POUC_TX_UNDO_MAX_SNAPSHOT_VALUE (1024u * 1024u)

typedef struct {
    uint32_t version;
    uint64_t apply_height;
    uint64_t apply_generation;
    uint32_t snapshot_count;
    char txid[129];
    char tx_type[64];
} QrxPoucTxUndoEntry;

/* Capture the authoritative pre-state of every key already staged in the
 * outer applytx batch, then append the undo journal to THAT SAME batch.
 * Call this only after all transaction state, fee, nonce and tx-index writes
 * have been staged and before qrxdb_batch_commit(). */
int qrx_pouc_tx_undo_stage(QrxDB *db,
                           QrxDBBatch *batch,
                           const char *txid,
                           const char *tx_type,
                           uint64_t apply_height);

/* Restore every 0.0.9.34+ PoUC transaction above canonical_height in one
 * QRXDB/WAL batch, newest generation first. Journal and snapshot keys are
 * deleted as part of the same commit, making repeated rollback idempotent. */
int qrx_pouc_tx_undo_revert_above_height(QrxDB *db,
                                         uint64_t canonical_height,
                                         uint32_t *out_reverted);

#ifdef __cplusplus
}
#endif
