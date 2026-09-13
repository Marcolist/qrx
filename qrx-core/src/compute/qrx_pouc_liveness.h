#pragma once
#include <stdint.h>
#include "qrxdb.h"
#include "compute/qrx_pouc_verifier.h"
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_POUC_LIVENESS_VERSION 1u
#define QRX_POUC_LIVENESS_MAX_EXCLUDED 64u
#define QRX_POUC_LIVENESS_MAX_TIMEOUTS 3u
#define QRX_POUC_LIVENESS_PREFIX "consensus:compute:verifier_liveness:"
#define QRX_POUC_LIVENESS_HISTORY_PREFIX "consensus:compute:verifier_liveness_history:"
#define QRX_POUC_MISS_COUNT_PREFIX "consensus:compute:verifier_miss_count:"

typedef enum { QRX_POUC_TIMEOUT_PRIMARY=1, QRX_POUC_TIMEOUT_CHALLENGE=2 } QrxPoucTimeoutPhase;
typedef struct {
 uint32_t version; char receipt_commitment[65]; uint32_t round; uint32_t excluded_count;
 char excluded[QRX_POUC_LIVENESS_MAX_EXCLUDED][QRX_POUC_VERIFIER_ADDRESS_MAX]; uint64_t last_reselection_height;
} QrxPoucLivenessState;
typedef struct {
 uint32_t version; QrxPoucTimeoutPhase phase; uint32_t round; uint64_t apply_height;
 char receipt_commitment[65]; QrxPoucVerifierSelection before; QrxPoucVerifierSelection after;
 uint32_t timeout_count; char timed_out[QRX_POUC_LIVENESS_MAX_TIMEOUTS][QRX_POUC_VERIFIER_ADDRESS_MAX];
 uint64_t miss_before[QRX_POUC_LIVENESS_MAX_TIMEOUTS]; uint64_t miss_after[QRX_POUC_LIVENESS_MAX_TIMEOUTS];
 uint8_t jail_applied[QRX_POUC_LIVENESS_MAX_TIMEOUTS]; char penalty_id[QRX_POUC_LIVENESS_MAX_TIMEOUTS][65];
 QrxPoucLivenessState state_before,state_after;
} QrxPoucReselectionEffect;

#define QRX_POUC_LIVENESS_PARAMS_VERSION 1u
typedef struct {
 uint32_t version; uint64_t verifier_miss_threshold; uint64_t verifier_miss_jail_blocks;
} QrxPoucLivenessParams;
int qrx_pouc_liveness_params_validate(const QrxPoucLivenessParams*);
int qrx_pouc_liveness_params_get(QrxDB*,QrxPoucLivenessParams*);
int qrx_pouc_liveness_params_stage(QrxDBBatch*,const QrxPoucLivenessParams*);

int qrx_pouc_liveness_get(QrxDB*,const char receipt[65],QrxPoucLivenessState*);
int qrx_pouc_reselection_prepare(QrxDB*,const char receipt[65],uint64_t apply_height,QrxPoucReselectionEffect*);
int qrx_pouc_reselection_stage(QrxDB*,QrxDBBatch*,const QrxPoucReselectionEffect*,const char *txid);
int qrx_pouc_liveness_revert_above_height(QrxDB*,uint64_t canonical_height,uint32_t *out_reverted);
#ifdef __cplusplus
}
#endif
