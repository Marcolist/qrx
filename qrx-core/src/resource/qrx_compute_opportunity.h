#pragma once
#include <stdint.h>
#include "resource/qrx_resource_globe.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_COMPUTE_OPPORTUNITY_VERSION 1u
#define QRX_OPPORTUNITY_DEVICE_MAX 95u

#define QRX_OPP_HOST_STORAGE (1u<<0)
#define QRX_OPP_HOST_COMPUTE (1u<<1)
#define QRX_OPP_HOST_AI_ACCELERATOR (1u<<2)
#define QRX_OPP_HOST_MODEL_CACHE (1u<<3)
#define QRX_OPP_HOST_NETWORK (1u<<4)
#define QRX_OPP_HOST_CUDA (1u<<5)
#define QRX_OPP_HOST_METAL_MLX (1u<<6)
#define QRX_OPP_HOST_ALL ((1u<<7)-1u)

typedef enum {
    QRX_OPP_ACTION_NONE = 0,
    QRX_OPP_ACTION_STORAGE,
    QRX_OPP_ACTION_COMPUTE,
    QRX_OPP_ACTION_AI_ACCELERATOR,
    QRX_OPP_ACTION_MODEL_CACHE,
    QRX_OPP_ACTION_NETWORK
} QrxOpportunityAction;

typedef struct {
    uint32_t version;
    uint32_t resource_mask;
    uint32_t cpu_threads_total;
    uint32_t cpu_threads_available;
    uint64_t storage_available_bytes;
    uint64_t model_cache_available_bytes;
    uint64_t accelerator_memory_bytes;
    uint64_t network_egress_mbps;
    char accelerator_name[QRX_OPPORTUNITY_DEVICE_MAX+1]; /* optional display metadata */
} QrxOpportunityHostProfile;

typedef struct {
    uint32_t version;
    char region[QRX_GLOBE_REGION_MAX+1];
    uint32_t storage_score;
    uint32_t compute_score;
    uint32_t ai_score;
    uint32_t model_cache_score;
    uint32_t network_score;
    uint32_t composite_score;
    uint32_t expected_utilization_bps; /* deterministic planning estimate, not an earnings guarantee */
    uint64_t recommended_storage_bytes;
    uint64_t recommended_model_cache_bytes;
    uint32_t recommended_compute_threads;
    uint64_t recommended_network_egress_mbps;
    int recommend_ai_accelerator;
    int recommend_cuda;
    int recommend_metal_mlx;
    QrxOpportunityAction primary_action;
} QrxComputeOpportunityRecommendation;

int qrx_compute_opportunity_host_validate(const QrxOpportunityHostProfile *host);
int qrx_compute_opportunity_recommend(const QrxResourceGlobeCell *cell,
                                      const QrxOpportunityHostProfile *host,
                                      QrxComputeOpportunityRecommendation *out);
int qrx_compute_opportunity_commitment(const QrxComputeOpportunityRecommendation *r,
                                       char out_hex[65]);

#ifdef __cplusplus
}
#endif
