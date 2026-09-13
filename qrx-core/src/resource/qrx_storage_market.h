#pragma once
#include "resource/qrx_resource.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_STORAGE_REPUTATION_INITIAL_BPS 8000u
#define QRX_STORAGE_REPUTATION_MIN_BPS 1000u
#define QRX_STORAGE_BOND_BASE_ATOMS 1000000ULL
#define QRX_STORAGE_BOND_WEIGHT_ATOMS 1000ULL
#define QRX_STORAGE_EXIT_DELAY_BLOCKS 8640ULL

typedef struct {
    uint32_t availability_bps;
    uint32_t proof_success_bps;
    uint32_t performance_bps;
    uint32_t reputation_bps;
    uint64_t proofs_ok;
    uint64_t proofs_failed;
    uint64_t bytes_served;
    uint64_t repairs_completed;
} QrxStorageProviderReputation;

typedef enum {
    QRX_PROVIDER_REGISTERED = 1,
    QRX_PROVIDER_ACTIVE = 2,
    QRX_PROVIDER_EXITING = 3,
    QRX_PROVIDER_WITHDRAWN = 4,
    QRX_PROVIDER_SLASHED = 5
} QrxStorageProviderStatus;

typedef struct {
    QrxStorageProviderStatus status;
    uint64_t bond_atoms;
    uint64_t proven_capacity_bytes;
    uint64_t allocated_bytes;
    uint64_t exit_height;
} QrxStorageBondState;

typedef struct {
    uint64_t original_atoms;
    uint64_t development_atoms;
    uint64_t resilience_atoms;
    uint64_t provider_escrow_atoms;
    uint64_t provider_paid_atoms;
    uint64_t repair_paid_atoms;
    uint64_t egress_paid_atoms;
} QrxStorageFinancialState;

typedef struct {
    uint64_t stored_bytes;
    uint64_t epochs;
    uint64_t base_atoms_per_gib_epoch;
    uint32_t availability_bps;
    uint32_t proof_factor_bps;
    uint32_t performance_bps;
} QrxStorageRewardInput;

void qrx_storage_reputation_init(QrxStorageProviderReputation *r);
int qrx_storage_reputation_record_proof(QrxStorageProviderReputation *r,int success,uint32_t latency_score_bps);
uint64_t qrx_storage_required_bond(uint64_t proven_capacity_bytes);
int qrx_storage_bond_register(QrxStorageBondState *s,uint64_t proven_capacity_bytes,uint64_t bond_atoms);
int qrx_storage_bond_activate(QrxStorageBondState *s);
int qrx_storage_bond_begin_exit(QrxStorageBondState *s,uint64_t current_height);
int qrx_storage_bond_withdrawable(const QrxStorageBondState *s,uint64_t current_height);
int qrx_storage_bond_withdraw(QrxStorageBondState *s,uint64_t current_height,uint64_t *atoms_out);
int qrx_storage_bond_slash(QrxStorageBondState *s,uint64_t atoms,uint64_t *slashed_out);

int qrx_storage_financial_init(uint64_t contract_atoms,QrxStorageFinancialState *out);
uint64_t qrx_storage_provider_reward(const QrxStorageRewardInput *in);
int qrx_storage_financial_pay_provider(QrxStorageFinancialState *s,uint64_t atoms);
int qrx_storage_financial_pay_repair(QrxStorageFinancialState *s,uint64_t atoms);
int qrx_storage_financial_pay_egress(QrxStorageFinancialState *s,uint64_t atoms);
uint64_t qrx_storage_financial_refundable(const QrxStorageFinancialState *s);

#ifdef __cplusplus
}
#endif
