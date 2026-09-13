#pragma once
#include <stddef.h>
#include <stdint.h>
#include "compute/qrx_aura_model_distribution.h"
#include "compute/qrx_aura_provider_service.h"
#include "storage/qrx_storage_fs.h"
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_AURA_ORIGIN_VERSION 1u
#define QRX_AURA_ORIGIN_MAX_FILES 256u
#define QRX_AURA_ORIGIN_PATH_MAX 384u
#define QRX_AURA_ORIGIN_REPO_MAX 160u
#define QRX_AURA_ORIGIN_URL_MAX 1024u
#define QRX_AURA_ORIGIN_REV_MAX 64u
#define QRX_AURA_ORIGIN_FILTER_MAX 256u

/* Data-only bootstrap catalog. New upstream models can be added to the catalog
 * without changing consensus or recompiling QRX. Exact imported bytes are later
 * bound by the QRX SHA3 content roots and signed model-catalog governance. */
typedef struct {
    uint32_t version;
    char model_id[QRX_MODEL_MAX_ID];
    char family[QRX_AURA_FABRIC_MAX_FAMILY];
    char repo_id[QRX_AURA_ORIGIN_REPO_MAX];
    char revision[QRX_AURA_ORIGIN_REV_MAX]; /* channel/tag/SHA; resolved to immutable SHA */
    char expected_license[QRX_AURA_MODEL_LICENSE_MAX];
    char architecture_hint[QRX_MODEL_MAX_ARCH];
    char runtime_id[QRX_MODEL_MAX_RUNTIME];
    char quantization[QRX_AURA_FABRIC_MAX_QUANT];
    QrxAuraModelTier tier;
    uint8_t recommended;
    uint8_t moe_hint;
    /* V2 bootstrap policy fields. Zero means use the conservative legacy default. */
    uint64_t min_memory_bytes;
    uint64_t recommended_memory_bytes;
    uint64_t min_milli_tokens_per_second;
    uint32_t quality_bps;
    uint32_t coding_bps;
    uint32_t reasoning_bps;
    uint32_t rag_bps;
    uint32_t tool_use_bps;
    uint8_t auto_default;      /* eligible for zero-touch AUTO */
    uint8_t gated_optional;    /* never AUTO unless access was explicitly enabled */
    char file_filter[QRX_AURA_ORIGIN_FILTER_MAX]; /* exact weight filename or * */
} QrxAuraOriginModelSpec;

typedef struct {
    char path[QRX_AURA_ORIGIN_PATH_MAX];
    uint64_t bytes;
} QrxAuraOriginRemoteFile;

typedef struct {
    uint32_t version;
    char repo_id[QRX_AURA_ORIGIN_REPO_MAX];
    char requested_revision[QRX_AURA_ORIGIN_REV_MAX];
    char resolved_revision[QRX_AURA_ORIGIN_REV_MAX];
    char license_id[QRX_AURA_MODEL_LICENSE_MAX];
    uint8_t gated;
    uint32_t file_count;
    QrxAuraOriginRemoteFile files[QRX_AURA_ORIGIN_MAX_FILES];
} QrxAuraOriginRepoMetadata;

typedef struct {
    uint32_t version;
    char hf_base_url[QRX_AURA_ORIGIN_URL_MAX];     /* default https://huggingface.co */
    char hf_api_base_url[QRX_AURA_ORIGIN_URL_MAX]; /* default https://huggingface.co/api */
    char hf_token[512];                            /* optional; never persisted in manifests */
    char tls_ca_bundle[1024];                      /* optional explicit CA bundle for HTTPS origins */
    char work_dir[1024];
    uint32_t connect_timeout_seconds;
    uint32_t transfer_timeout_seconds; /* 0 = no total timeout for huge model shards */
    uint64_t max_asset_bytes;          /* 0 = no per-asset limit */
    uint8_t allow_gated;
} QrxAuraOriginConfig;

typedef struct {
    char path[QRX_AURA_ORIGIN_PATH_MAX];
    char content_root[65];
    uint64_t bytes;
    QrxAuraModelAssetKind kind;
    uint32_t layer_first,layer_count;
    uint32_t expert_first,expert_count;
} QrxAuraOriginSeededFile;

typedef struct {
    uint32_t version;
    char repo_id[QRX_AURA_ORIGIN_REPO_MAX];
    char revision[QRX_AURA_ORIGIN_REV_MAX];
    uint32_t file_count;
    QrxAuraOriginSeededFile files[QRX_AURA_ORIGIN_MAX_FILES];
} QrxAuraOriginSourceMap;

typedef struct {
    QrxAuraOriginRepoMetadata upstream;
    QrxAuraOriginSourceMap source_map;
    QrxAuraModelManifest distribution_manifest;
    char distribution_manifest_root[65];
    QrxAuraModelBundleManifest drive_bundle;
    char drive_bundle_root[65];
    QrxAiModelRecord registry_record;
    QrxAuraExpertManifest expert_manifest;
    uint8_t has_expert_manifest;
    char source_map_root[65];
} QrxAuraOriginSeedResult;

typedef void (*QrxAuraOriginProgressFn)(void *ctx,const char *phase,const char *path,uint64_t done,uint64_t total);

int qrx_aura_origin_config_defaults(QrxAuraOriginConfig *out);
int qrx_aura_origin_catalog_load(const char *path,QrxAuraOriginModelSpec *out,size_t cap,size_t *count_out);

typedef struct {
    uint32_t version;
    uint32_t spec_index;
    QrxAuraOriginModelSpec spec;
    QrxAuraModelCapabilityProfile profile;
    uint8_t degraded;
} QrxAuraOriginAutoSelection;

/* Build a scheduler profile directly from the signed/bootstrap catalog before
 * the model has been downloaded. This is what lets a fresh Pi/Mac choose a
 * safe model without asking the user about model names, formats or runtimes. */
int qrx_aura_origin_spec_profile(const QrxAuraOriginModelSpec *spec,QrxAuraModelCapabilityProfile *out);
int qrx_aura_origin_auto_select(const QrxAuraOriginModelSpec *specs,size_t count,
                                const QrxAuraPodCapacity *pod,QrxAuraTaskClass task,
                                uint32_t complexity_bps,uint8_t allow_gated,
                                QrxAuraOriginAutoSelection *out);
int qrx_aura_hf_repo_resolve(const QrxAuraOriginConfig *cfg,const QrxAuraOriginModelSpec *spec,QrxAuraOriginRepoMetadata *out);
int qrx_aura_origin_source_map_encode(const QrxAuraOriginSourceMap *map,uint8_t **out,size_t *out_len);
int qrx_aura_origin_source_map_decode(const uint8_t *in,size_t in_len,QrxAuraOriginSourceMap *out);
int qrx_aura_hf_seed_to_qrx_drive(const QrxAuraOriginConfig *cfg,const QrxAuraOriginModelSpec *spec,QrxStorageFs *seed_fs,
                                  QrxAuraOriginProgressFn progress,void *progress_ctx,QrxAuraOriginSeedResult *out);
int qrx_aura_origin_materialize(const QrxAuraOriginSourceMap *map,QrxStorageFs *fs,const char *directory);
int qrx_aura_origin_catalog_announcement(const QrxAuraOriginSeedResult *seed,const QrxAuraOriginModelSpec *spec,
                                         const char *publisher_id,uint64_t sequence,uint64_t valid_from_height,uint64_t valid_until_height,
                                         QrxAuraModelCatalogAnnouncement *out);
int qrx_aura_origin_availability_announcement(const QrxAuraOriginSeedResult *seed,const char *provider_id,const char *pod_id,
                                              const char *region,const char *endpoint,uint64_t sequence,
                                              uint64_t valid_from_height,uint64_t valid_until_height,
                                              uint32_t bandwidth_mbps,uint32_t latency_ms,uint32_t reliability_bps,
                                              QrxAuraModelAvailabilityAnnouncement *out);

typedef struct {
    QrxAuraDriveModelFetchContext *drive; /* optional: tried first */
    const QrxAuraModelCatalog *catalog;   /* signed/governed catalog */
    QrxAuraOriginConfig origin;
    QrxStorageFs *local_cache;
    uint8_t allow_origin_fallback;
} QrxAuraOriginFallbackFetchContext;

/* QrxAuraModelCacheFetchFn adapter: QRX Drive first, immutable HF origin only
 * when the governed manifest explicitly permits and binds that origin. */
int qrx_aura_model_fetch_drive_then_origin(void *ctx,const QrxAiModelRecord *model,
                                           const QrxAuraModelCachePlacement *placement,
                                           char verified_model_commitment_out[65]);

#ifdef __cplusplus
}
#endif
