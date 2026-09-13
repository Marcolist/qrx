#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Common post-Genesis activation state machine. Target dates are policy
 * not-before dates, never wall-clock consensus switches. */
typedef enum {
    QRX_PROTOCOL_READINESS_LOCKED = 0,
    QRX_PROTOCOL_READINESS_NOT_READY = 1,
    QRX_PROTOCOL_READINESS_SOAKING = 2,
    QRX_PROTOCOL_READINESS_WAITING_TARGET_DATE = 3,
    QRX_PROTOCOL_READINESS_READY_FOR_GOVERNANCE = 4,
    QRX_PROTOCOL_READINESS_SCHEDULED = 5,
    QRX_PROTOCOL_READINESS_ACTIVE = 6
} QrxProtocolReadinessStatus;

#define QRX_PROTOCOL_READINESS_FEATURE_MAX 32
#define QRX_PROTOCOL_READINESS_REASON_MAX 160

/* DRIVE_V1 */
#define QRX_DRIVE_READINESS_MIN_PROVIDERS 14ULL
#define QRX_DRIVE_READINESS_MIN_OPERATORS 14ULL
#define QRX_DRIVE_READINESS_MIN_ASNS 4ULL
#define QRX_DRIVE_READINESS_MIN_REGIONS 3ULL
#define QRX_DRIVE_READINESS_MIN_PROVEN_BYTES (14ULL*1024ULL*1024ULL*1024ULL)
#define QRX_DRIVE_READINESS_MIN_AVAILABILITY_BPS 8000U
#define QRX_DRIVE_READINESS_MIN_PROOF_BPS 8000U
#define QRX_DRIVE_READINESS_MIN_HEALTH_SCORE 60U
#define QRX_DRIVE_READINESS_SOAK_BLOCKS 60480ULL /* ~7 days at 10 s */

/* QRX_NET_V1: requires a healthy active Drive plus real contract usage. */
#define QRX_NET_READINESS_MIN_PROVIDERS 14ULL
#define QRX_NET_READINESS_MIN_ASNS 4ULL
#define QRX_NET_READINESS_MIN_REGIONS 3ULL
#define QRX_NET_READINESS_MIN_ACTIVE_CONTRACTS 3ULL
#define QRX_NET_READINESS_MIN_AVAILABILITY_BPS 8500U
#define QRX_NET_READINESS_MIN_HEALTH_SCORE 70U
#define QRX_NET_READINESS_SOAK_BLOCKS 60480ULL /* Drive must run ~7 days */

/* ADVERTISING_V1: requires a live QRX-Net with real domains and stable storage. */
#define QRX_AD_READINESS_MIN_PROVIDERS 14ULL
#define QRX_AD_READINESS_MIN_ASNS 4ULL
#define QRX_AD_READINESS_MIN_REGIONS 3ULL
#define QRX_AD_READINESS_MIN_ACTIVE_DOMAINS 3ULL
#define QRX_AD_READINESS_MIN_AVAILABILITY_BPS 8500U
#define QRX_AD_READINESS_MIN_HEALTH_SCORE 70U
#define QRX_AD_READINESS_SOAK_BLOCKS 60480ULL /* QRX-Net must run ~7 days */

/* COMPUTE_POUC_V1: economic rewards stay disabled until provider identities
 * and all nine pre-existing PoUC safety/readiness predicates are present. */
#define QRX_COMPUTE_READINESS_MIN_PROVIDERS 8ULL
#define QRX_COMPUTE_READINESS_REQUIRED_SAFETY_FLAGS 9U
#define QRX_COMPUTE_READINESS_MIN_STORAGE_PROVIDERS 14ULL
#define QRX_COMPUTE_READINESS_MIN_ASNS 4ULL
#define QRX_COMPUTE_READINESS_MIN_REGIONS 3ULL
#define QRX_COMPUTE_READINESS_MIN_HEALTH_SCORE 70U
#define QRX_COMPUTE_READINESS_SOAK_BLOCKS 120960ULL /* ~14 days after dependencies */

typedef struct {
    char feature_flag[QRX_PROTOCOL_READINESS_FEATURE_MAX];
    QrxProtocolReadinessStatus status;
    uint64_t current_height;
    int64_t target_time;
    int64_t activation_height;

    uint32_t dependencies_required;
    uint32_t dependencies_active;
    uint64_t dependency_soak_blocks;
    uint64_t required_soak_blocks;

    uint32_t criteria_passed;
    uint32_t criteria_required;
    uint32_t readiness_bps;

    uint64_t serving_providers;
    uint64_t mature_attested_providers;
    uint64_t independent_operators;
    uint64_t independent_asns;
    uint64_t visible_regions;
    uint64_t proven_bytes;
    uint64_t active_contracts;
    uint64_t active_domains;
    uint64_t compute_providers;
    uint32_t compute_safety_flags;
    uint32_t avg_availability_bps;
    uint32_t avg_proof_success_bps;
    uint32_t health_score;
    uint64_t soak_blocks;

    char blocking_reason[QRX_PROTOCOL_READINESS_REASON_MAX];
} QrxProtocolActivationReadiness;

int qrx_protocol_activation_readiness(const char *chain_dir,const char *feature_flag,
                                      uint64_t current_height,int64_t now_unix,
                                      QrxProtocolActivationReadiness *out);
const char *qrx_protocol_readiness_status_name(QrxProtocolReadinessStatus s);

/* Backward-compatible DRIVE_V1 API retained for wallet/older tests. */
typedef enum {
    QRX_DRIVE_READINESS_NOT_READY = 0,
    QRX_DRIVE_READINESS_WAITING_SOAK = 1,
    QRX_DRIVE_READINESS_WAITING_TARGET_DATE = 2,
    QRX_DRIVE_READINESS_READY = 3,
    QRX_DRIVE_READINESS_ALREADY_SCHEDULED = 4,
    QRX_DRIVE_READINESS_ACTIVE = 5
} QrxDriveReadinessStatus;

typedef struct {
    QrxDriveReadinessStatus status;
    uint64_t current_height;
    int64_t target_time;
    int64_t activation_height;
    uint64_t serving_providers;
    uint64_t mature_attested_providers;
    uint64_t independent_operators;
    uint64_t independent_asns;
    uint64_t visible_regions;
    uint64_t proven_bytes;
    uint32_t avg_availability_bps;
    uint32_t avg_proof_success_bps;
    uint32_t health_score;
    uint64_t soak_blocks;
    uint64_t soak_blocks_required;
    uint32_t criteria_passed;
    uint32_t criteria_required;
} QrxDriveActivationReadiness;

int qrx_drive_activation_readiness(const char *chain_dir,uint64_t current_height,int64_t now_unix,QrxDriveActivationReadiness *out);
const char *qrx_drive_readiness_status_name(QrxDriveReadinessStatus s);
#ifdef __cplusplus
}
#endif
