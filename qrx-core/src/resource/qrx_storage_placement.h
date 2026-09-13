#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QRX_STORAGE_MAX_PROVIDER_ID 128
#define QRX_STORAGE_MAX_FAILURE_ID 96

typedef struct {
    char provider_id[QRX_STORAGE_MAX_PROVIDER_ID+1];
    char operator_id[QRX_STORAGE_MAX_FAILURE_ID+1];
    char asn[QRX_STORAGE_MAX_FAILURE_ID+1];
    char region[QRX_STORAGE_MAX_FAILURE_ID+1];
    uint64_t proven_free_bytes;
    uint64_t bond_atoms;
    uint64_t price_atoms_per_gib_epoch;
    uint32_t availability_bps;
    uint32_t proof_success_bps;
    uint32_t performance_bps; /* 10000 baseline; capped to 11500 for placement */
    int active;
    int failure_domain_attested;
} QrxStoragePlacementCandidate;

typedef struct {
    const char **provider_ids; size_t provider_count;
    const char **operator_ids; size_t operator_count;
    const char **asns; size_t asn_count;
    const char **regions; size_t region_count;
} QrxStoragePlacementExclusions;

uint64_t qrx_storage_placement_score(const QrxStoragePlacementCandidate *c,
                                     const uint8_t randomness[64]);
int qrx_storage_select_provider(const QrxStoragePlacementCandidate *candidates,
                                size_t candidate_count,
                                const QrxStoragePlacementExclusions *exclude,
                                uint64_t min_free_bytes,
                                uint64_t min_bond_atoms,
                                const uint8_t randomness[64],
                                size_t *selected_index_out);

#ifdef __cplusplus
}
#endif
