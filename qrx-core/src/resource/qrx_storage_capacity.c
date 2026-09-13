#include "resource/qrx_storage_capacity.h"

#include <limits.h>
#include <string.h>

static int add_u64(uint64_t *dst, uint64_t v) {
    if (UINT64_MAX - *dst < v) return -1;
    *dst += v;
    return 0;
}

static uint64_t mul_ratio_floor(uint64_t value, uint64_t num, uint64_t den) {
    if (!den) return 0;
    /* Avoid overflow: q*den+r representation. */
    uint64_t q = value / den;
    uint64_t r = value % den;
    if (num && q > UINT64_MAX / num) return UINT64_MAX;
    uint64_t a = q * num;
    uint64_t b = (r * num) / den; /* r < den; profile ratios are tiny */
    return UINT64_MAX - a < b ? UINT64_MAX : a + b;
}

static uint32_t ratio_bps(uint64_t numerator, uint64_t denominator) {
    if (!denominator) return 0;
    uint64_t v = mul_ratio_floor(numerator, 10000ULL, denominator);
    return v > UINT32_MAX ? UINT32_MAX : (uint32_t)v;
}

int qrx_storage_provider_capacity_validate(const QrxStorageProviderCapacity *p) {
    if (!p || !p->provider_id[0]) return -1;
    uint64_t raw = p->configured_bytes < p->physical_eligible_bytes ? p->configured_bytes : p->physical_eligible_bytes;
    if (p->proven_bytes > raw) return -1;
    if (p->allocated_physical_bytes > p->proven_bytes) return -1;
    if (p->reserve_committed_bytes > p->proven_bytes - p->allocated_physical_bytes) return -1;
    if (p->availability_bps > 10000U || p->proof_success_bps > 10000U) return -1;
    return 0;
}

uint64_t qrx_storage_logical_capacity_for_profile(uint64_t physical_bytes,
                                                  const QrxStorageRedundancyProfile *profile) {
    if (!profile) return 0;
    if (profile->full_replicas) return physical_bytes / profile->full_replicas;
    uint64_t total = (uint64_t)profile->data_shards + (uint64_t)profile->parity_shards;
    if (!profile->data_shards || !total) return 0;
    return mul_ratio_floor(physical_bytes, profile->data_shards, total);
}

int qrx_storage_capacity_aggregate(const QrxStorageProviderCapacity *providers,
                                   size_t provider_count,
                                   uint64_t logical_user_bytes,
                                   uint64_t healthy_shards,
                                   uint64_t degraded_shards,
                                   uint64_t repairing_shards,
                                   const QrxStorageRedundancyProfile *availability_profile,
                                   QrxStorageNetworkCapacity *out) {
    if (!out || (provider_count && !providers) || !availability_profile) return -1;
    memset(out, 0, sizeof(*out));
    out->logical_user_bytes = logical_user_bytes;
    out->healthy_shards = healthy_shards;
    out->degraded_shards = degraded_shards;
    out->repairing_shards = repairing_shards;
    out->providers_total = (uint64_t)provider_count;

    for (size_t i = 0; i < provider_count; ++i) {
        const QrxStorageProviderCapacity *p = &providers[i];
        if (qrx_storage_provider_capacity_validate(p) != 0) return -1;
        uint64_t raw = p->configured_bytes < p->physical_eligible_bytes ? p->configured_bytes : p->physical_eligible_bytes;
        if (add_u64(&out->configured_bytes, p->configured_bytes) != 0 ||
            add_u64(&out->raw_eligible_bytes, raw) != 0 ||
            add_u64(&out->proven_bytes, p->proven_bytes) != 0 ||
            add_u64(&out->allocated_physical_bytes, p->allocated_physical_bytes) != 0 ||
            add_u64(&out->reserve_committed_bytes, p->reserve_committed_bytes) != 0)
            return -1;
        if (p->proven_bytes) out->providers_proven++;
    }

    if (out->allocated_physical_bytes > out->proven_bytes) return -1;
    out->free_proven_physical_bytes = out->proven_bytes - out->allocated_physical_bytes;
    out->available_usable_bytes = qrx_storage_logical_capacity_for_profile(out->free_proven_physical_bytes,
                                                                           availability_profile);
    if (UINT64_MAX - logical_user_bytes < out->available_usable_bytes) return -1;
    out->usable_capacity_bytes = logical_user_bytes + out->available_usable_bytes;
    out->utilization_bps = ratio_bps(out->allocated_physical_bytes, out->proven_bytes);
    out->resilience_headroom_bps = ratio_bps(out->free_proven_physical_bytes, out->proven_bytes);
    out->redundancy_x10000 = logical_user_bytes ? ratio_bps(out->allocated_physical_bytes, logical_user_bytes) : 0;
    return 0;
}
