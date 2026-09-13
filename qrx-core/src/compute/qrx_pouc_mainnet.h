#pragma once
#include <stdint.h>
#include "compute/qrx_compute.h"
#include "compute/qrx_compute_adversarial.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_POUC_MAINNET_VERSION 1u
#define QRX_POUC_MAINNET_CHAIN_ID_MAX 64u
#define QRX_POUC_MAINNET_PROTOCOL_MAX 32u
#define QRX_POUC_MAINNET_REPLAY_SLOTS 128u

typedef struct {
    uint32_t version;
    uint8_t verification_stable;
    uint8_t rewards_simulated;
    uint8_t fasttrack_fair;
    uint8_t standard_tasks_protected;
    uint8_t fasttrack_dev_share_transparent;
    uint8_t model_integrity_verified;
    uint8_t provider_fraud_controlled;
    uint8_t resource_globe_privacy_checked;
    uint8_t server_independence_verified;
} QrxPoucMainnetReadiness;

typedef struct {
    uint32_t version;
    char chain_id[QRX_POUC_MAINNET_CHAIN_ID_MAX+1];
    char genesis_hash[65];
    char protocol_version[QRX_POUC_MAINNET_PROTOCOL_MAX+1];
} QrxPoucChainBinding;

typedef enum {
    QRX_POUC_SETTLEMENT_REJECTED = 1,
    QRX_POUC_SETTLEMENT_FROZEN = 2,
    QRX_POUC_SETTLEMENT_PAYOUT = 3
} QrxPoucSettlementOutcome;

typedef struct {
    uint32_t version;
    QrxPoucChainBinding chain;
    QrxPoucMainnetReadiness readiness;
    QrxPoucReceipt receipt;
    QrxPoucVerification verification;
    uint8_t challenge_finalized;
    uint8_t challenge_passed;
    uint8_t has_adversarial_verdict;
    QrxComputeAdversarialVerdict adversarial;
    uint64_t settlement_nonce;
    uint64_t current_height;
} QrxPoucSettlementRequest;

typedef struct {
    uint32_t version;
    QrxPoucSettlementOutcome outcome;
    uint8_t activation_ready;
    uint8_t payout_allowed;
    uint8_t slash_candidate;
    uint8_t jail_candidate;
    uint64_t payout_compute_atoms;
    uint64_t refund_atoms;
    char receipt_commitment[65];
    char replay_key[65];
    char decision_commitment[65];
} QrxPoucSettlementDecision;

typedef struct {
    uint32_t version;
    uint32_t count;
    char replay_keys[QRX_POUC_MAINNET_REPLAY_SLOTS][65];
} QrxPoucReplayGuard;

int qrx_pouc_mainnet_readiness_validate(const QrxPoucMainnetReadiness *r);
int qrx_pouc_chain_binding_validate(const QrxPoucChainBinding *b);
int qrx_pouc_mainnet_decide(const QrxPoucSettlementRequest *req, const QrxComputeEscrow *escrow, QrxPoucSettlementDecision *out);
int qrx_pouc_mainnet_apply_payout(const QrxPoucSettlementDecision *decision, QrxComputeEscrow *escrow, uint64_t current_height);
int qrx_pouc_replay_guard_init(QrxPoucReplayGuard *g);
int qrx_pouc_replay_guard_accept(QrxPoucReplayGuard *g, const QrxPoucSettlementDecision *d);
const char *qrx_pouc_settlement_outcome_name(QrxPoucSettlementOutcome o);

#ifdef __cplusplus
}
#endif
