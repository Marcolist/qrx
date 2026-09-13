#pragma once
#include <stdint.h>
#include "resource/qrx_compute_opportunity.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_RESOURCE_PROVIDER_VERSION 1u
#define QRX_RESOURCE_PROVIDER_ID_MAX 128u

#define QRX_PROVIDER_ENABLE_STORAGE (1u<<0)
#define QRX_PROVIDER_ENABLE_COMPUTE (1u<<1)
#define QRX_PROVIDER_ENABLE_AI (1u<<2)
#define QRX_PROVIDER_ENABLE_MODEL_CACHE (1u<<3)
#define QRX_PROVIDER_ENABLE_NETWORK (1u<<4)
#define QRX_PROVIDER_ENABLE_ALL ((1u<<5)-1u)

/* A provider profile is an explicit user policy. Building a plan never starts
   a worker, publishes an endpoint, signs a transaction or spends QUB. */
typedef struct {
    uint32_t version;
    char provider_id[QRX_RESOURCE_PROVIDER_ID_MAX+1];
    uint32_t enabled_mask;
    uint64_t max_storage_bytes;
    uint64_t max_model_cache_bytes;
    uint32_t max_compute_threads;
    uint64_t max_network_egress_mbps;
    uint8_t allow_accelerator;
    uint8_t allow_cuda;
    uint8_t allow_metal_mlx;
    uint8_t require_wallet_approval;
} QrxResourceProviderPolicy;

typedef struct {
    uint32_t version;
    char region[QRX_GLOBE_REGION_MAX+1];
    uint32_t enabled_mask;
    uint64_t storage_bytes;
    uint64_t model_cache_bytes;
    uint32_t compute_threads;
    uint64_t network_egress_mbps;
    uint8_t accelerator_enabled;
    uint8_t cuda_enabled;
    uint8_t metal_mlx_enabled;
    uint8_t requires_wallet_approval;
    uint32_t primary_action;
    uint32_t opportunity_score;
    uint32_t expected_utilization_bps;
} QrxResourceProviderPlan;

int qrx_resource_provider_policy_validate(const QrxResourceProviderPolicy *policy,
                                          const QrxOpportunityHostProfile *host);
int qrx_resource_provider_plan(const QrxResourceProviderPolicy *policy,
                               const QrxOpportunityHostProfile *host,
                               const QrxComputeOpportunityRecommendation *recommendation,
                               QrxResourceProviderPlan *out);
int qrx_resource_provider_plan_commitment(const QrxResourceProviderPlan *plan,
                                          char out_hex[65]);

#ifdef __cplusplus
}
#endif
