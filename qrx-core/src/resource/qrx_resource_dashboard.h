#pragma once
#include "resource/qrx_storage_atlas.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t active_contracts;
    uint64_t logical_user_bytes;
    uint64_t purchased_logical_bytes;
    uint64_t healthy_shards;
    uint64_t degraded_shards;
    uint64_t repairing_shards;
    uint64_t active_domains;
    uint64_t hosted_sites;
    uint64_t website_logical_bytes;
    uint64_t website_requests_24h;
    uint64_t website_cache_hits_24h;
} QrxResourceWorkloadStats;

typedef struct {
    QrxStorageNetworkCapacity storage;
    uint32_t health_score;
    uint32_t demand_factor_bps;
    uint32_t avg_proof_success_bps;
    uint32_t avg_availability_bps;
    uint32_t provider_diversity_bps;
    uint64_t serving_providers;
    uint64_t independent_asns;
    uint64_t visible_regions;
    uint64_t hidden_regions;
    uint64_t active_contracts;
    uint64_t purchased_logical_bytes;
    uint64_t active_domains;
    uint64_t hosted_sites;
    uint64_t website_logical_bytes;
    uint64_t website_requests_24h;
    uint32_t website_cache_hit_bps;
} QrxResourceDashboardSnapshot;

int qrx_resource_dashboard_build(const QrxStorageAtlasProvider *providers,size_t provider_count,
                                 const QrxResourceWorkloadStats *workload,
                                 const QrxStorageRedundancyProfile *profile,
                                 size_t privacy_min_providers,
                                 QrxResourceDashboardSnapshot *out,
                                 QrxStorageAtlasCell **cells_out,size_t *cell_count_out);
#ifdef __cplusplus
}
#endif
