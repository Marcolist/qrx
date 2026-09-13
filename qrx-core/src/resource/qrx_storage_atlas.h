#pragma once
#include "resource/qrx_storage_capacity.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_ATLAS_REGION_MAX 63

typedef struct {
    char provider_id[129];
    char region[QRX_ATLAS_REGION_MAX+1];
    char asn[64];
    int network_attested;
    QrxStorageProviderCapacity capacity;
} QrxStorageAtlasProvider;

typedef struct {
    char region[QRX_ATLAS_REGION_MAX+1];
    uint64_t provider_count;
    uint64_t proven_bytes;
    uint64_t allocated_bytes;
    uint64_t free_bytes;
    uint64_t unique_asns;
    uint32_t utilization_bps;
    uint32_t avg_availability_bps;
    uint32_t avg_proof_success_bps;
    uint32_t opportunity_score;
    uint32_t diversity_bonus_bps;
    int publicly_visible;
} QrxStorageAtlasCell;

typedef struct {
    char region[QRX_ATLAS_REGION_MAX+1];
    uint64_t wanted_additional_bytes;
    uint64_t wanted_additional_providers;
    uint32_t opportunity_score;
    uint32_t incentive_factor_bps;
} QrxStorageMission;

uint32_t qrx_storage_network_health_score(const QrxStorageNetworkCapacity *n,
                                          uint32_t avg_proof_success_bps,
                                          uint32_t provider_diversity_bps);
uint32_t qrx_storage_dynamic_demand_factor_bps(const QrxStorageNetworkCapacity *n);
int qrx_storage_atlas_build(const QrxStorageAtlasProvider *providers,size_t count,
                            size_t privacy_min_providers,
                            QrxStorageAtlasCell **cells_out,size_t *cell_count_out);
void qrx_storage_atlas_free(QrxStorageAtlasCell *cells);
int qrx_storage_missions_from_atlas(const QrxStorageAtlasCell *cells,size_t count,
                                    uint64_t target_free_bytes_per_region,
                                    uint64_t target_providers_per_region,
                                    QrxStorageMission **missions_out,size_t *mission_count_out);
int qrx_storage_public_missions_from_atlas(const QrxStorageAtlasCell *cells,size_t count,
                                           uint64_t target_free_bytes_per_region,
                                           uint64_t target_providers_per_region,
                                           QrxStorageMission **missions_out,size_t *mission_count_out);
void qrx_storage_missions_free(QrxStorageMission *missions);
#ifdef __cplusplus
}
#endif
