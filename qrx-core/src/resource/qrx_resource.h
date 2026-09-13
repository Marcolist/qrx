#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* QRX 0.0.8 resource layer.
 *
 * DRIVE_V1 is deliberately NOT committed to Genesis. Mainnet activation is a
 * post-Genesis mandatory protocol upgrade. The final activation height is
 * scheduled through the existing threshold-signed PROTOCOL_UPGRADE mechanism
 * after observing real Mainnet block cadence. 2026-11-30 17:00 UTC is an
 * operational target only, never a wall-clock consensus switch. */
#define QRX_RESOURCE_PROTOCOL_VERSION 1
#define QRX_DRIVE_V1_MIN_PROTOCOL_VERSION 9LL
#define QRX_DRIVE_V1_FEATURE_FLAG "DRIVE_V1"
#define QRX_NET_V1_FEATURE_FLAG "QRX_NET_V1"
#define QRX_COMPUTE_POUC_V1_FEATURE_FLAG "COMPUTE_POUC_V1"
#define QRX_ADVERTISING_V1_FEATURE_FLAG "ADVERTISING_V1"
/* Genesis hardening (Finding 5): shielded value movement on Mainnet. */
#define QRX_PRIVACY_V1_FEATURE_FLAG "PRIVACY_V1"
#define QRX_DRIVE_V1_PLANNED_TARGET_TIME 1796058000LL   /* 2026-11-30 17:00 UTC */
#define QRX_NET_V1_PLANNED_TARGET_TIME 1796662800LL     /* 2026-12-07 17:00 UTC */
#define QRX_ADVERTISING_V1_PLANNED_TARGET_TIME 1797354000LL /* 2026-12-15 17:00 UTC */
#define QRX_COMPUTE_POUC_V1_PLANNED_TARGET_TIME 1801414800LL /* 2027-01-31 17:00 UTC */

#define QRX_BPS_DENOMINATOR 10000ULL
#define QRX_STORAGE_DEV_SHARE_BPS 50ULL
#define QRX_STORAGE_RESILIENCE_RESERVE_BPS 200ULL
#define QRX_STORAGE_PROVIDER_BUDGET_BPS (QRX_BPS_DENOMINATOR - QRX_STORAGE_DEV_SHARE_BPS - QRX_STORAGE_RESILIENCE_RESERVE_BPS)
#define QRX_STORAGE_PERFORMANCE_BONUS_CAP_BPS 1500ULL

#define QRX_STORAGE_FAST_FULL_REPLICAS 3
#define QRX_STORAGE_STANDARD_DATA_SHARDS 10
#define QRX_STORAGE_STANDARD_PARITY_SHARDS 4
#define QRX_STORAGE_ARCHIVE_DATA_SHARDS 16
#define QRX_STORAGE_ARCHIVE_PARITY_SHARDS 4

typedef enum {
    QRX_RESOURCE_STORAGE = 1,
    QRX_RESOURCE_COMPUTE = 2,
    QRX_RESOURCE_GPU = 3,
    QRX_RESOURCE_SPECIALIZED = 4
} QrxResourceType;

typedef struct {
    char provider_id[129];
    QrxResourceType resource_type;
    uint64_t capacity_total;
    uint64_t capacity_available;
    uint64_t bond_atoms;
    uint32_t reputation_bps;
    char failure_domain[129];
} QrxResourceProvider;

typedef struct {
    char offer_id[129];
    char provider_id[129];
    QrxResourceType resource_type;
    uint64_t units_available;
    uint64_t price_atoms_per_unit_epoch;
    uint64_t valid_from_height;
    uint64_t valid_until_height;
} QrxResourceOffer;

typedef struct {
    char contract_id[129];
    char owner[160];
    QrxResourceType resource_type;
    uint64_t units;
    uint64_t start_height;
    uint64_t end_height;
    uint64_t escrow_atoms;
} QrxResourceContract;

typedef struct {
    uint64_t development_atoms;
    uint64_t resilience_atoms;
    uint64_t provider_budget_atoms;
} QrxStorageContractSplit;

typedef struct {
    const char *name;
    unsigned data_shards;
    unsigned parity_shards;
    unsigned full_replicas;
} QrxStorageRedundancyProfile;

long long qrx_resource_activation_height(const char *chain_dir);
long long qrx_resource_target_time(const char *chain_dir);
int qrx_resource_drive_v1_scheduled(const char *chain_dir);
int qrx_resource_qrx_net_v1_scheduled(const char *chain_dir);
int qrx_resource_compute_pouc_v1_scheduled(const char *chain_dir);
int qrx_resource_advertising_v1_scheduled(const char *chain_dir);
int qrx_resource_protocol_enabled_at_height(const char *chain_dir, long long height);
int qrx_storage_protocol_enabled_at_height(const char *chain_dir, long long height);
int qrx_storage_preflight_tx_type(const char *tx_type);
int qrx_net_protocol_enabled_at_height(const char *chain_dir, long long height);

/* Genesis hardening (Finding 5): PRIVACY_V1 Mainnet gate.
 * Shielded value movement stays fail-closed on Mainnet until an external
 * cryptography audit has completed, its findings are fixed, 3-of-5 governance
 * approves, and a PRIVACY_V1 activation height is committed on-chain.
 * Governance and attester preparation remain possible before activation. */
long long qrx_privacy_activation_height(const char *chain_dir);
int qrx_privacy_protocol_enabled_at_height(const char *chain_dir, long long height);
int qrx_privacy_preflight_tx_type(const char *tx_type);
int qrx_compute_pouc_protocol_enabled_at_height(const char *chain_dir, long long height);
int qrx_advertising_protocol_enabled_at_height(const char *chain_dir, long long height);
long long qrx_compute_pouc_activation_height(const char *chain_dir);
long long qrx_advertising_activation_height(const char *chain_dir);
long long qrx_net_activation_height(const char *chain_dir);
long long qrx_net_target_time(const char *chain_dir);
long long qrx_advertising_target_time(const char *chain_dir);
long long qrx_compute_pouc_target_time(const char *chain_dir);
int qrx_resource_is_mainnet(const char *chain_dir);

int qrx_storage_split_contract_value(uint64_t contract_atoms,
                                     uint64_t development_bps,
                                     uint64_t resilience_bps,
                                     QrxStorageContractSplit *out);

const QrxStorageRedundancyProfile *qrx_storage_profile_by_name(const char *name);
uint64_t qrx_resource_capacity_weight(uint64_t proven_free_bytes);
const char *qrx_resource_type_name(QrxResourceType type);

#ifdef __cplusplus
}
#endif
