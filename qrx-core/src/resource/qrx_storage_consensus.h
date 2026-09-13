#pragma once
#include "protocol/qrx_service_effects.h"
#include "qrxdb.h"
#include <stdint.h>
#include <openssl/evp.h>
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_STORAGE_PROOF_WINDOW_BLOCKS 720ULL
#define QRX_STORAGE_ATTEST_EPOCH_BLOCKS 8640ULL
#define QRX_STORAGE_ATTEST_QUORUM 3U

typedef enum {
    QRX_STORAGE_CONTRACT_ACTIVE = 1,
    QRX_STORAGE_CONTRACT_COMPLETE = 2,
    QRX_STORAGE_CONTRACT_REFUNDED = 3
} QrxStorageContractStatus;

typedef struct {
    char owner[160];
    char profile[16];
    uint64_t logical_bytes;
    uint64_t shard_bytes;
    uint32_t shard_count;
    uint64_t start_height;
    uint64_t end_height;
    uint64_t original_atoms;
    uint64_t development_atoms;
    uint64_t resilience_atoms;
    uint64_t provider_escrow_atoms;
    uint64_t provider_paid_atoms;
    uint64_t repair_paid_atoms;
    uint64_t egress_paid_atoms;
    uint64_t base_atoms_per_gib_epoch;
    uint8_t manifest_root[64];
    QrxStorageContractStatus status;
} QrxStorageContractState;

typedef struct {
    char owner[160];
    uint32_t status;
    uint64_t bond_atoms;
    uint64_t proven_capacity_bytes;
    uint64_t allocated_bytes;
    uint64_t exit_height;
    uint32_t availability_bps;
    uint32_t proof_success_bps;
    uint32_t performance_bps;
    uint32_t reputation_bps;
    char operator_id[129];
    char claimed_asn[64];
    char claimed_region[96];
    char attested_asn[64];
    char attested_region[96];
    uint64_t attestation_epoch;
} QrxStorageProviderState;

typedef struct {
    char provider_id[129];
    uint32_t state;
    uint64_t physical_bytes;
    char object_id[65];
    uint8_t merkle_root[64];
    uint64_t leaf_count;
    uint64_t accepted_height;
    uint64_t last_proof_epoch;
    uint64_t last_proof_height;
    uint64_t last_settled_epoch;
} QrxStorageAssignmentRecord;

int qrx_storage_provider_get(QrxDB *db,const char *provider_id,QrxStorageProviderState *out);
int qrx_storage_contract_get(QrxDB *db,const char *contract_id,QrxStorageContractState *out);
int qrx_storage_assignment_get(QrxDB *db,const char *contract_id,uint32_t shard,QrxStorageAssignmentRecord *out);

typedef struct {
    char old_provider_id[129];
    char replacement_provider_id[129];
    uint32_t state;
    uint64_t start_height;
    uint64_t physical_bytes;
    char object_id[65];
    uint8_t merkle_root[64];
    uint64_t leaf_count;
    uint64_t accepted_height;
} QrxStorageRepairRecord;

int qrx_storage_repair_get(QrxDB *db,const char *contract_id,uint32_t shard,QrxStorageRepairRecord *out);
/* 0.0.8.65: consensus-bound ML-DSA discovery identity lookup. ctx must be QrxDB*. */
int qrx_storage_provider_discovery_key_lookup(void *ctx,const char *provider_id,EVP_PKEY **public_key_out);
/* Deterministic next-assignment provider. Selection randomness is bound to the last finalized block (inclusion_height-1), so wallets can construct a valid transaction before the next block exists. */
int qrx_storage_assignment_provider_for_height(QrxDB *db,const char *contract_id,uint64_t shard_bytes,uint64_t inclusion_height,char out[129]);

int qrx_storage_consensus_prepare(const char *chain_dir,const char *tx_type,const char *from,const char *to,
                                  uint64_t amount_atoms,const char *payload,const char *txid,uint64_t height,
                                  QrxServiceEconomicEffect *effect);
int qrx_storage_consensus_stage(QrxDB *db,QrxDBBatch *batch,const char *chain_dir,const char *tx_type,
                                const char *from,const char *to,uint64_t amount_atoms,const char *payload,
                                const char *txid,uint64_t height);

#ifdef __cplusplus
}
#endif
