#pragma once

#include "resource/qrx_resource.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char provider_id[129];
    uint64_t configured_bytes;          /* operator max_usage */
    uint64_t physical_eligible_bytes;   /* local disk space eligible after reserve */
    uint64_t proven_bytes;              /* capacity accepted by Capacity Proof */
    uint64_t allocated_physical_bytes;  /* actual shards stored for contracts */
    uint64_t reserve_committed_bytes;   /* proven free bytes reserved for pending placements */
    uint32_t availability_bps;
    uint32_t proof_success_bps;
} QrxStorageProviderCapacity;

typedef struct {
    uint64_t configured_bytes;
    uint64_t raw_eligible_bytes;
    uint64_t proven_bytes;
    uint64_t allocated_physical_bytes;
    uint64_t logical_user_bytes;
    uint64_t free_proven_physical_bytes;
    uint64_t available_usable_bytes;
    uint64_t usable_capacity_bytes;
    uint64_t reserve_committed_bytes;
    uint64_t providers_total;
    uint64_t providers_proven;
    uint64_t healthy_shards;
    uint64_t degraded_shards;
    uint64_t repairing_shards;
    uint32_t utilization_bps;
    uint32_t resilience_headroom_bps;
    uint32_t redundancy_x10000; /* 1.40x == 14000 */
} QrxStorageNetworkCapacity;

/* Validates monotonic provider capacity constraints. */
int qrx_storage_provider_capacity_validate(const QrxStorageProviderCapacity *p);

/* Converts physical free capacity into logical user capacity for a redundancy
 * profile. FAST 3x => physical/3; STANDARD 10+4 => physical*10/14. */
uint64_t qrx_storage_logical_capacity_for_profile(uint64_t physical_bytes,
                                                  const QrxStorageRedundancyProfile *profile);

/* Aggregates provider capacity plus contract-level logical data. Logical data
 * is deliberately supplied once at network/contract level so erasure shards
 * are not double-counted by summing per-provider reports. */
int qrx_storage_capacity_aggregate(const QrxStorageProviderCapacity *providers,
                                   size_t provider_count,
                                   uint64_t logical_user_bytes,
                                   uint64_t healthy_shards,
                                   uint64_t degraded_shards,
                                   uint64_t repairing_shards,
                                   const QrxStorageRedundancyProfile *availability_profile,
                                   QrxStorageNetworkCapacity *out);

#ifdef __cplusplus
}
#endif
