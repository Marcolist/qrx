#pragma once
#include <stddef.h>
#include <stdint.h>
#include "compute/qrx_aura_fabric_gossip.h"
#include "resource/qrx_resource_provider_runtime.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_AURA_LIVE_DISPATCH_VERSION 1u
#define QRX_AURA_ADMISSION_MAX_PODS 16u
#define QRX_AURA_CACHE_MAX_ENTRIES 256u
#define QRX_AURA_CACHE_PLAN_MAX_PLACEMENTS 16u

typedef struct {
    uint32_t version;
    char pod_id[QRX_MOE_MAX_POD_ID];
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    QrxResourceProviderRuntime *provider_runtime;
    QrxMoeRuntimeDevice device;
    QrxMoeWorkerAdapter adapter;
    uint8_t local;
} QrxAuraRuntimeBinding;

typedef struct {
    uint32_t version;
    char pod_id[QRX_MOE_MAX_POD_ID];
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char model_commitment[65];
    uint64_t bytes_present;
    uint64_t verified_height;
    uint8_t verified;
} QrxAuraModelCacheEntry;

typedef struct {
    QrxAuraModelCacheEntry entries[QRX_AURA_CACHE_MAX_ENTRIES];
    uint32_t count;
    uint64_t revision;
} QrxAuraModelCacheCatalog;

typedef struct {
    char pod_id[QRX_MOE_MAX_POD_ID];
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    uint64_t bytes_to_place;
} QrxAuraModelCachePlacement;

typedef struct {
    uint32_t version;
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char model_commitment[65];
    char manifest_root[65];
    uint64_t required_bytes;
    uint64_t planned_bytes;
    QrxAuraModelCachePlacement placements[QRX_AURA_CACHE_PLAN_MAX_PLACEMENTS];
    uint32_t placement_count;
    char plan_commitment[65];
} QrxAuraModelCachePlan;

typedef int (*QrxAuraModelCacheFetchFn)(void *ctx,const QrxAiModelRecord *model,
                                        const QrxAuraModelCachePlacement *placement,
                                        char verified_model_commitment_out[65]);

typedef struct {
    char pod_id[QRX_MOE_MAX_POD_ID];
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    uint32_t compute_threads;
    uint64_t network_egress_mbps;
    uint64_t model_cache_bytes;
} QrxAuraAdmissionPod;

typedef struct {
    uint32_t version;
    QrxAuraRouteDecision route;
    char model_commitment[65];
    char graph_commitment[65];
    uint32_t node_id;
    uint64_t admitted_height;
    uint64_t expires_height;
    QrxAuraAdmissionPod pods[QRX_AURA_ADMISSION_MAX_PODS];
    uint32_t pod_count;
    uint8_t active;
    char admission_commitment[65];
} QrxAuraAdmission;

typedef int (*QrxAuraDistributedDispatchFn)(void *ctx,const QrxAuraAdmission *admission,
                                            const QrxComputeJobNode *node,
                                            const void *input,size_t input_bytes,
                                            void *output,size_t output_capacity,size_t *output_bytes);

typedef struct {
    uint32_t version;
    char admission_commitment[65];
    char model_commitment[65];
    char runtime_id[QRX_MODEL_MAX_RUNTIME];
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    char pod_id[QRX_MOE_MAX_POD_ID];
    uint32_t pod_count;
    uint64_t output_bytes;
    char output_commitment[65];
    char result_commitment[65];
} QrxAuraDispatchResult;

int qrx_aura_model_profile_registry_verify(const QrxAuraModelProfileAnnouncement *announcement,
                                           const QrxAiModelRegistry *registry,
                                           QrxAiModelRecord *record_out);
void qrx_aura_model_cache_catalog_init(QrxAuraModelCacheCatalog *catalog);
const QrxAuraModelCacheEntry *qrx_aura_model_cache_find(const QrxAuraModelCacheCatalog *catalog,
                                                        const char *pod_id,const char *model_id,const char *model_version);
int qrx_aura_model_cache_plan_build(const QrxAuraModelProfileAnnouncement *announcement,
                                    const QrxAiModelRegistry *registry,
                                    const QrxAuraPodGossipTable *pods,uint64_t current_height,
                                    uint32_t min_placements,QrxAuraModelCachePlan *out);
int qrx_aura_model_cache_plan_commitment(const QrxAuraModelCachePlan *plan,char out_hex[65]);
int qrx_aura_model_cache_plan_execute(const QrxAuraModelCachePlan *plan,const QrxAiModelRegistry *registry,
                                      QrxAuraModelCacheCatalog *catalog,uint64_t current_height,
                                      QrxAuraModelCacheFetchFn fetch,void *fetch_ctx);

int qrx_aura_live_scheduler_admit(const QrxAuraPodGossipTable *pods,const QrxAuraModelGossipTable *models,
                                  const QrxAiModelRegistry *registry,QrxAuraModelCacheCatalog *cache,
                                  const QrxAuraRouteRequest *route_request,const QrxComputeJobGraph *graph,
                                  uint32_t node_id,QrxAuraRuntimeBinding *bindings,size_t binding_count,
                                  uint64_t current_height,uint64_t ttl_blocks,QrxAuraAdmission *out);
int qrx_aura_admission_commitment(const QrxAuraAdmission *admission,char out_hex[65]);
int qrx_aura_live_scheduler_release(QrxAuraAdmission *admission,QrxAuraRuntimeBinding *bindings,size_t binding_count);

int qrx_aura_runtime_dispatch_execute(const QrxAuraAdmission *admission,const QrxComputeJobNode *node,
                                      QrxAuraRuntimeBinding *bindings,size_t binding_count,
                                      const void *input,size_t input_bytes,void *output,size_t output_capacity,size_t *output_bytes,
                                      QrxAuraDistributedDispatchFn distributed_dispatch,void *dispatch_ctx,
                                      QrxAuraDispatchResult *result_out);
int qrx_aura_dispatch_result_commitment(const QrxAuraDispatchResult *result,char out_hex[65]);
int qrx_aura_dispatch_build_pouc_receipt(const QrxAuraAdmission *admission,const QrxAuraDispatchResult *result,
                                         const char input_commitment[65],uint64_t verified_compute_atoms,
                                         uint64_t started_height,uint64_t completed_height,uint32_t verification_mode,
                                         QrxPoucReceipt *receipt_out);

#ifdef __cplusplus
}
#endif
