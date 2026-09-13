#pragma once
#include <stddef.h>
#include <stdint.h>
#include "compute/qrx_compute.h"
#include "resource/qrx_resource_globe.h"
#ifdef __cplusplus
extern "C" {
#endif

/* QRX 0.0.9.35 – Adaptive AURA Model Fabric / Edge AI Bootstrap.
   This layer is deliberately benchmark-driven. Product names (Jetson, Mac,
   Ryzen, Raspberry Pi, ODROID, etc.) never determine eligibility by themselves.
   Any host that can produce a valid runtime/calibration profile may contribute. */
#define QRX_AURA_FABRIC_VERSION 1u
#define QRX_AURA_FABRIC_LOCAL_REGISTRY_MAX_PODS 256u
#define QRX_AURA_FABRIC_MAX_PODS 4096u /* global live fabric/gossip view */
#define QRX_AURA_FABRIC_MAX_MODELS 64u
#define QRX_AURA_FABRIC_MAX_ID 96u
#define QRX_AURA_FABRIC_MAX_FAMILY 64u
#define QRX_AURA_FABRIC_MAX_QUANT 32u
#define QRX_AURA_FABRIC_DEFAULT_PRIVACY_MIN_PROVIDERS 3u

#define QRX_AURA_POD_ROLE_INFERENCE   (1u<<0)
#define QRX_AURA_POD_ROLE_ROUTER      (1u<<1)
#define QRX_AURA_POD_ROLE_EMBEDDING   (1u<<2)
#define QRX_AURA_POD_ROLE_RAG         (1u<<3)
#define QRX_AURA_POD_ROLE_PREPOST     (1u<<4)
#define QRX_AURA_POD_ROLE_VERIFY      (1u<<5)
#define QRX_AURA_POD_ROLE_MODEL_CACHE (1u<<6)
#define QRX_AURA_POD_ROLE_ALL         ((1u<<7)-1u)

/* These are scheduling tiers, not model vendor contracts. K2/K3 are network
   capability classes for larger distributed MoE profiles. */
typedef enum {
    QRX_AURA_TIER_NONE=0,
    QRX_AURA_TIER_NANO=1,
    QRX_AURA_TIER_EDGE=2,
    QRX_AURA_TIER_LOCAL=3,
    QRX_AURA_TIER_CLUSTER=4,
    QRX_AURA_TIER_K2_CLASS=5,
    QRX_AURA_TIER_K3_CLASS=6
} QrxAuraModelTier;

typedef enum {
    QRX_AURA_ROUTE_AUTO=1,
    QRX_AURA_ROUTE_PINNED=2
} QrxAuraRouteMode;

typedef enum {
    QRX_AURA_TASK_CHAT=1,
    QRX_AURA_TASK_RAG=2,
    QRX_AURA_TASK_CODING=3,
    QRX_AURA_TASK_REASONING=4,
    QRX_AURA_TASK_TOOL_USE=5
} QrxAuraTaskClass;

typedef struct {
    uint32_t version;
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1]; /* internal only */
    char pod_id[QRX_MOE_MAX_POD_ID];            /* internal only */
    char region[QRX_GLOBE_REGION_MAX+1];        /* coarse cell */
    QrxMoeRuntimeBackend backend;
    uint32_t accelerator_features;
    uint32_t role_mask;
    uint64_t usable_memory_bytes;
    uint64_t free_memory_bytes;
    uint64_t model_cache_free_bytes;
    uint64_t measured_milli_tokens_per_second;
    uint32_t network_egress_mbps;
    uint32_t latency_ms;
    uint32_t utilization_bps;
    uint32_t reliability_bps;
    uint32_t cache_hit_bps;
    uint32_t expert_locality_bps;
    uint32_t max_context_tokens;
    uint32_t node_count; /* 1 = single-device pod; >1 = local pod */
    uint8_t available;
    char calibration_commitment[65]; /* optional all-zero/empty only for utility-only pods */
} QrxAuraPodCapacity;

typedef struct {
    uint32_t version;
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char family[QRX_AURA_FABRIC_MAX_FAMILY];
    char runtime_id[QRX_MODEL_MAX_RUNTIME];
    char quantization[QRX_AURA_FABRIC_MAX_QUANT];
    QrxAuraModelTier tier;
    uint64_t min_memory_bytes;
    uint64_t recommended_memory_bytes;
    uint64_t min_storage_bytes;
    uint64_t min_aggregate_milli_tokens_per_second;
    uint32_t min_pods;
    uint32_t min_network_mbps;
    uint32_t max_latency_ms;
    uint32_t min_reliability_bps;
    uint32_t min_cache_hit_bps;
    uint32_t min_expert_locality_bps;
    uint32_t max_context_tokens;
    uint32_t quality_bps;
    uint32_t coding_bps;
    uint32_t reasoning_bps;
    uint32_t rag_bps;
    uint32_t tool_use_bps;
    uint32_t required_accelerator_features;
    uint8_t is_moe;
    uint8_t supports_single_device;
} QrxAuraModelCapabilityProfile;

typedef struct {
    uint32_t version;
    QrxAuraPodCapacity pods[QRX_AURA_FABRIC_LOCAL_REGISTRY_MAX_PODS];
    uint32_t count;
    uint64_t revision;
} QrxAuraPodCapacityRegistry;

typedef struct {
    uint32_t version;
    char region[QRX_GLOBE_REGION_MAX+1];
    uint64_t provider_count;
    uint64_t pod_count;
    uint64_t inference_pods;
    uint64_t utility_pods;
    uint64_t tier_pods[7];
    uint64_t free_memory_bytes;
    uint64_t model_cache_free_bytes;
    uint64_t ai_milli_tokens_per_second;
    uint32_t avg_latency_ms;
    uint32_t avg_utilization_bps;
    uint32_t avg_reliability_bps;
    uint32_t avg_cache_hit_bps;
    uint32_t avg_expert_locality_bps;
    uint32_t k2_readiness_bps;
    uint32_t k3_readiness_bps;
    uint8_t publicly_visible;
} QrxAuraGlobeCell;

typedef struct {
    uint32_t version;
    uint64_t provider_count;
    uint64_t total_pods;
    uint64_t inference_pods;
    uint64_t utility_pods;
    uint64_t tier_pods[7];
    uint64_t free_memory_bytes;
    uint64_t model_cache_free_bytes;
    uint64_t ai_milli_tokens_per_second;
    uint32_t avg_latency_ms;
    uint32_t avg_utilization_bps;
    uint32_t avg_reliability_bps;
    uint32_t avg_cache_hit_bps;
    uint32_t avg_expert_locality_bps;
    uint32_t visible_regions;
    uint32_t hidden_regions;
    uint32_t k2_readiness_bps;
    uint32_t k3_readiness_bps;
    QrxAuraModelTier max_ready_tier;
} QrxAuraFabricSnapshot;

typedef struct {
    uint32_t version;
    QrxAuraRouteMode mode;
    QrxAuraTaskClass task_class;
    uint32_t complexity_bps;       /* 0..10000 */
    uint32_t min_quality_bps;      /* 0 means derive from complexity */
    uint32_t min_context_tokens;
    uint8_t allow_degradation;
    char pinned_model_id[QRX_MODEL_MAX_ID];
    char pinned_model_version[QRX_MODEL_MAX_VERSION];
} QrxAuraRouteRequest;

typedef struct {
    uint32_t version;
    QrxAuraRouteMode mode;
    QrxAuraTaskClass task_class;
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char runtime_id[QRX_MODEL_MAX_RUNTIME];
    QrxAuraModelTier tier;
    uint32_t fitness_bps;
    uint32_t readiness_bps;
    uint32_t task_quality_bps;
    uint32_t required_quality_bps;
    uint32_t selected_pods;
    uint64_t aggregate_milli_tokens_per_second;
    uint8_t degraded;
    char decision_commitment[65];
} QrxAuraRouteDecision;

int qrx_aura_pod_capacity_validate(const QrxAuraPodCapacity *pod);
int qrx_aura_model_profile_validate(const QrxAuraModelCapabilityProfile *model);
QrxAuraModelTier qrx_aura_pod_tier_classify(const QrxAuraPodCapacity *pod);
const char *qrx_aura_tier_name(QrxAuraModelTier tier);

int qrx_aura_pod_registry_init(QrxAuraPodCapacityRegistry *registry);
int qrx_aura_pod_registry_upsert(QrxAuraPodCapacityRegistry *registry, const QrxAuraPodCapacity *pod);
int qrx_aura_pod_registry_remove(QrxAuraPodCapacityRegistry *registry, const char *pod_id);
const QrxAuraPodCapacity *qrx_aura_pod_registry_find(const QrxAuraPodCapacityRegistry *registry, const char *pod_id);

int qrx_aura_pod_from_calibration(const char *provider_id, const char *pod_id, const char *region,
                                  const QrxMoeCalibrationProfile *calibration,
                                  uint32_t role_mask, uint64_t free_memory_bytes,
                                  uint64_t model_cache_free_bytes, uint32_t network_egress_mbps,
                                  uint32_t latency_ms, uint32_t utilization_bps,
                                  uint32_t reliability_bps, uint32_t cache_hit_bps,
                                  uint32_t expert_locality_bps, uint32_t max_context_tokens,
                                  uint32_t node_count, QrxAuraPodCapacity *out);

uint32_t qrx_aura_model_readiness_bps(const QrxAuraModelCapabilityProfile *model,
                                      const QrxAuraPodCapacity *pods, size_t pod_count,
                                      uint32_t *eligible_pods_out,
                                      uint64_t *aggregate_milli_tokens_per_second_out);

int qrx_aura_fabric_build(const QrxAuraPodCapacity *pods, size_t pod_count,
                          const QrxAuraModelCapabilityProfile *models, size_t model_count,
                          size_t privacy_min_providers,
                          QrxAuraFabricSnapshot *snapshot_out,
                          QrxAuraGlobeCell **cells_out, size_t *cell_count_out);
void qrx_aura_globe_free(QrxAuraGlobeCell *cells);

int qrx_aura_route_model(const QrxAuraRouteRequest *request,
                         const QrxAuraModelCapabilityProfile *models, size_t model_count,
                         const QrxAuraPodCapacity *pods, size_t pod_count,
                         QrxAuraRouteDecision *out);
int qrx_aura_route_decision_commitment(const QrxAuraRouteDecision *decision, char out_hex[65]);
int qrx_aura_route_apply_to_job_node(const QrxAuraRouteDecision *decision,
                                     const QrxAuraModelCapabilityProfile *models, size_t model_count,
                                     QrxComputeJobNode *node);

#ifdef __cplusplus
}
#endif
