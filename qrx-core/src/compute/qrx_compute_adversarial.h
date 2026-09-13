#pragma once
#include <stdint.h>
#include "compute/qrx_compute.h"
#include "resource/qrx_resource_provider_runtime.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_COMPUTE_ADVERSARIAL_VERSION 1u
#define QRX_COMPUTE_ADVERSARIAL_MAX_PEERS 32u
#define QRX_COMPUTE_ADVERSARIAL_ID_MAX 96u

typedef enum {
    QRX_ADV_FAKE_COMPUTE = 1,
    QRX_ADV_RESULT_MISMATCH = 2,
    QRX_ADV_MODEL_RUNTIME_MISMATCH = 3,
    QRX_ADV_VERIFIER_COLLUSION = 4,
    QRX_ADV_SYBIL_PROVIDER_SET = 5,
    QRX_ADV_CACHE_FRAUD = 6,
    QRX_ADV_SLOW_WORKER = 7,
    QRX_ADV_QUEUE_STARVATION = 8,
    QRX_ADV_WAN_PARTITION = 9,
    QRX_ADV_POD_FAILURE = 10
} QrxComputeAdversarialAttack;

typedef enum {
    QRX_ADV_SEVERITY_INFO = 1,
    QRX_ADV_SEVERITY_LOW = 2,
    QRX_ADV_SEVERITY_MEDIUM = 3,
    QRX_ADV_SEVERITY_HIGH = 4,
    QRX_ADV_SEVERITY_CRITICAL = 5
} QrxComputeAdversarialSeverity;

#define QRX_ADV_ACTION_CHALLENGE        (1u<<0)
#define QRX_ADV_ACTION_REJECT_RESULT    (1u<<1)
#define QRX_ADV_ACTION_FREEZE_PAYOUT    (1u<<2)
#define QRX_ADV_ACTION_QUARANTINE       (1u<<3)
#define QRX_ADV_ACTION_SLASH_CANDIDATE  (1u<<4)
#define QRX_ADV_ACTION_REROUTE          (1u<<5)
#define QRX_ADV_ACTION_DRAIN_PROVIDER   (1u<<6)
#define QRX_ADV_ACTION_RETRY_REDUNDANT  (1u<<7)

/* Evidence is intentionally protocol-level and deterministic. It does not
   contain IP addresses, exact coordinates, host paths, or wallet secrets. */
typedef struct {
    uint32_t version;
    char scenario_id[QRX_COMPUTE_ADVERSARIAL_ID_MAX+1];
    QrxComputeAdversarialAttack attack;
    char provider_id[QRX_RESOURCE_PROVIDER_ID_MAX+1];
    char expected_model_commitment[65];
    char observed_model_commitment[65];
    char expected_runtime[QRX_POUC_MAX_RUNTIME];
    char observed_runtime[QRX_POUC_MAX_RUNTIME];
    char expected_result_commitment[65];
    char observed_result_commitment[65];
    uint64_t claimed_compute_atoms;
    uint64_t measured_compute_atoms;
    uint32_t verifier_count;
    uint32_t matching_verifiers;
    uint32_t distinct_operator_count;
    uint32_t distinct_provider_count;
    uint32_t timeout_bps;
    uint32_t queue_wait_bps;
    uint32_t cache_hit_claim_bps;
    uint32_t cache_hit_verified_bps;
    uint32_t unavailable_peer_bps;
    uint32_t unavailable_pod_bps;
} QrxComputeAdversarialEvidence;

typedef struct {
    uint32_t version;
    QrxComputeAdversarialAttack attack;
    QrxComputeAdversarialSeverity severity;
    uint32_t action_mask;
    uint32_t risk_bps;
    uint32_t challenge_bps;
    uint8_t detected;
    uint8_t block_new_work;
    uint8_t payout_allowed;
    uint8_t consensus_slash_applied; /* always 0 in 0.0.9.26 testnet layer */
    char evidence_commitment[65];
} QrxComputeAdversarialVerdict;

int qrx_compute_adversarial_evidence_validate(const QrxComputeAdversarialEvidence *evidence);
int qrx_compute_adversarial_evidence_commitment(const QrxComputeAdversarialEvidence *evidence, char out_hex[65]);
int qrx_compute_adversarial_evaluate(const QrxComputeAdversarialEvidence *evidence, QrxComputeAdversarialVerdict *out);

#ifdef __cplusplus
}
#endif
