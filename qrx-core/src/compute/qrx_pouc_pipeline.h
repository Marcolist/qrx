#pragma once
#include <stdint.h>
#include "qrxdb.h"
#include "compute/qrx_pouc_journal.h"
#include "compute/qrx_compute_adversarial.h"
#include "compute/qrx_pouc_verifier.h"
#include "compute/qrx_pouc_liveness.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_POUC_PIPELINE_VERSION 1u
#define QRX_POUC_ASSIGNMENT_VERSION 1u
#define QRX_POUC_VERIFY_AGG_VERSION 1u
#define QRX_POUC_CHALLENGE_QUORUM 3u

typedef enum {
    QRX_POUC_PIPELINE_NONE=0,
    QRX_POUC_PIPELINE_ESCROW_LOCK=1,
    QRX_POUC_PIPELINE_ASSIGN=2,
    QRX_POUC_PIPELINE_RECEIPT=3,
    QRX_POUC_PIPELINE_VERIFY=4,
    QRX_POUC_PIPELINE_CHALLENGE=5,
    QRX_POUC_PIPELINE_RESELECT=6
} QrxPoucPipelineOp;

typedef struct {
    uint32_t version;
    char graph_commitment[65];
    char owner[160];
    char provider[QRX_POUC_MAX_PROVIDER];
    char quote_id[129];
    char runtime_id[QRX_POUC_MAX_RUNTIME];
    char model_commitment[65];
    uint32_t node_id;
    uint64_t quote_price_atoms;
    uint64_t quote_expiry_height;
    uint32_t verification_mode;
    uint64_t assignment_nonce;
    uint64_t assigned_height;
    char assignment_commitment[65];
} QrxPoucAssignment;

typedef struct {
    uint32_t version;
    uint32_t verifier_count;
    uint32_t matching_verifiers;
    uint32_t mismatching_verifiers;
    uint32_t target_verifiers;
    uint8_t finalized;
} QrxPoucVerificationAggregate;

typedef struct {
    uint32_t version;
    QrxPoucPipelineOp op;
    uint64_t debit_atoms;
    QrxComputeEscrow escrow_before;
    QrxComputeEscrow escrow_after;
    QrxPoucAssignment assignment;
    QrxPoucReceipt receipt;
    char receipt_commitment[65];
    QrxPoucVerificationAggregate verification;
    uint32_t challenge_status;
    uint32_t challenge_votes;
    uint32_t challenge_pass_votes;
    uint32_t challenge_fail_votes;
    char verifier[160];
    char observed_result_commitment[65];
    QrxPoucVerifierSelection selection;
    uint64_t reward_credit_atoms;
    uint8_t reward_role; /* 1=primary verifier, 2=challenger */
    uint8_t has_adversarial_verdict;
    QrxComputeAdversarialVerdict adversarial;
    QrxPoucReselectionEffect reselection;
} QrxPoucPipelineEffect;

int qrx_pouc_pipeline_prepare(QrxDB *db,
                              const char *tx_type,
                              const char *from,
                              const char *to,
                              uint64_t amount_atoms,
                              const char *payload,
                              uint64_t apply_height,
                              QrxPoucPipelineEffect *out);

int qrx_pouc_pipeline_stage(QrxDB *db,
                            QrxDBBatch *batch,
                            const QrxPoucPipelineEffect *effect,
                            const char *txid,
                            uint64_t apply_height);

int qrx_pouc_pipeline_get_assignment(QrxDB *db,const char graph_commitment[65],QrxPoucAssignment *out);
int qrx_pouc_pipeline_get_receipt(QrxDB *db,const char receipt_commitment[65],QrxPoucReceipt *out);
int qrx_pouc_pipeline_get_receipt_for_graph(QrxDB *db,const char graph_commitment[65],char receipt_commitment[65]);
int qrx_pouc_pipeline_get_verification(QrxDB *db,const char receipt_commitment[65],QrxPoucVerificationAggregate *out);
uint32_t qrx_pouc_pipeline_target_verifiers(uint32_t verification_mode);
const char *qrx_pouc_pipeline_op_name(QrxPoucPipelineOp op);

#ifdef __cplusplus
}
#endif
