#pragma once
#include <stddef.h>
#include <stdint.h>
#include <openssl/evp.h>
#include "compute/qrx_aura_model_fabric.h"
#ifdef __cplusplus
extern "C" {
#endif

/* QRX 0.0.9.42 – zero-config model catalog, content-addressed availability,
 * replication planning and hardware-aware local placement. Vendor/model names
 * are metadata only: consensus does not hard-code Qwen/Kimi/Llama/etc. */
#define QRX_AURA_MODEL_DIST_VERSION 1u
#define QRX_AURA_MODEL_MANIFEST_MAX_ASSETS 256u
#define QRX_AURA_MODEL_CATALOG_MAX 256u
#define QRX_AURA_MODEL_PROVIDER_MAX 4096u
#define QRX_AURA_MODEL_AVAILABILITY_BITS (QRX_AURA_MODEL_MANIFEST_MAX_ASSETS/8u)
#define QRX_AURA_MODEL_FORMAT_MAX 32u
#define QRX_AURA_MODEL_LICENSE_MAX 128u
#define QRX_AURA_MODEL_ORIGIN_MAX 512u
#define QRX_AURA_MODEL_PROVIDER_ENDPOINT_MAX 256u
#define QRX_AURA_MODEL_SIGNATURE_MAX 16384u
#define QRX_AURA_MODEL_MIN_GLOBAL_REPLICAS 14u
#define QRX_AURA_MODEL_MIN_REGIONS 4u

typedef enum {
    QRX_AURA_ASSET_CONFIG=1,
    QRX_AURA_ASSET_TOKENIZER=2,
    QRX_AURA_ASSET_SHARED_WEIGHTS=3,
    QRX_AURA_ASSET_WEIGHT_CHUNK=4,
    QRX_AURA_ASSET_EXPERT_PACK=5,
    QRX_AURA_ASSET_ADAPTER=6
} QrxAuraModelAssetKind;

typedef struct {
    uint32_t index;
    QrxAuraModelAssetKind kind;
    char content_root[65];
    uint64_t bytes;
    uint32_t layer_first;
    uint32_t layer_count;
    uint32_t expert_first;
    uint32_t expert_count;
    uint8_t required_for_boot;
} QrxAuraModelAsset;

typedef struct {
    uint32_t version;
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char family[QRX_AURA_FABRIC_MAX_FAMILY];
    char architecture[QRX_MODEL_MAX_ARCH];
    char format[QRX_AURA_MODEL_FORMAT_MAX];
    char quantization[QRX_AURA_FABRIC_MAX_QUANT];
    char license_id[QRX_AURA_MODEL_LICENSE_MAX];
    char origin_uri[QRX_AURA_MODEL_ORIGIN_MAX]; /* optional bootstrap/fallback */
    uint32_t max_context_tokens;
    uint32_t asset_count;
    uint64_t total_bytes;
    QrxAuraModelAsset assets[QRX_AURA_MODEL_MANIFEST_MAX_ASSETS];
} QrxAuraModelManifest;

typedef struct {
    uint32_t version;
    char publisher_id[QRX_GLOBE_PROVIDER_MAX+1];
    uint64_t sequence;
    uint64_t valid_from_height;
    uint64_t valid_until_height;
    char manifest_root[65];
    QrxAuraModelManifest manifest;
    QrxAuraModelCapabilityProfile capability;
    uint32_t popularity_bps;
    uint32_t priority_bps;
    uint8_t recommended;
} QrxAuraModelCatalogAnnouncement;

typedef struct {
    QrxAuraModelCatalogAnnouncement announcement;
    uint8_t *signature;
    size_t signature_len;
} QrxAuraModelCatalogEntry;

typedef struct {
    QrxAuraModelCatalogEntry entries[QRX_AURA_MODEL_CATALOG_MAX];
    uint32_t count;
    uint64_t revision;
} QrxAuraModelCatalog;

typedef struct {
    uint32_t version;
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    char pod_id[QRX_MOE_MAX_POD_ID];
    char region[QRX_GLOBE_REGION_MAX+1];
    char endpoint[QRX_AURA_MODEL_PROVIDER_ENDPOINT_MAX];
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char manifest_root[65];
    uint64_t sequence;
    uint64_t valid_from_height;
    uint64_t valid_until_height;
    uint32_t bandwidth_mbps;
    uint32_t latency_ms;
    uint32_t reliability_bps;
    uint8_t full_model;
    uint8_t asset_bitmap[QRX_AURA_MODEL_AVAILABILITY_BITS];
} QrxAuraModelAvailabilityAnnouncement;

typedef struct {
    QrxAuraModelAvailabilityAnnouncement announcement;
    uint8_t *signature;
    size_t signature_len;
} QrxAuraModelAvailabilityEntry;

typedef struct {
    QrxAuraModelAvailabilityEntry entries[QRX_AURA_MODEL_PROVIDER_MAX];
    uint32_t count;
    uint64_t revision;
} QrxAuraModelProviderIndex;

typedef int (*QrxAuraModelDistKeyLookupFn)(void *ctx,const char *identity,EVP_PKEY **public_key_out);
typedef int (*QrxAuraModelCatalogAuthorizeFn)(void *ctx,const char *publisher_id);

typedef struct {
    uint32_t asset_index;
    uint32_t provider_replicas;
    uint32_t region_replicas;
    uint32_t target_replicas;
    uint32_t deficit;
    uint32_t demand_bps;
    uint32_t priority_score;
} QrxAuraModelAssetHealth;

typedef enum {
    QRX_AURA_POWER_ECO=1,
    QRX_AURA_POWER_BALANCED=2,
    QRX_AURA_POWER_PERFORMANCE=3
} QrxAuraPowerProfile;

typedef struct {
    uint32_t version;
    uint64_t disk_budget_bytes;
    QrxAuraPowerProfile power_profile;
    uint8_t automatic_models;
    uint8_t predictive_prefetch;
    uint8_t allow_external_origin;
} QrxAuraZeroConfigProfile;

typedef struct {
    uint32_t asset_index;
    uint64_t bytes;
    uint32_t priority_score;
    uint8_t boot_required;
} QrxAuraModelPlacementItem;

typedef struct {
    uint32_t version;
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char manifest_root[65];
    QrxAuraModelPlacementItem items[QRX_AURA_MODEL_MANIFEST_MAX_ASSETS];
    uint32_t item_count;
    uint64_t total_bytes;
} QrxAuraModelPlacementPlan;

typedef enum {
    QRX_AURA_SOURCE_LOCAL_CACHE=1,
    QRX_AURA_SOURCE_LAN_PEER=2,
    QRX_AURA_SOURCE_POD_PEER=3,
    QRX_AURA_SOURCE_NEARBY_PROVIDER=4,
    QRX_AURA_SOURCE_QRX_DRIVE=5,
    QRX_AURA_SOURCE_EXTERNAL_ORIGIN=6
} QrxAuraModelSourceTier;

typedef struct {
    QrxAuraModelSourceTier tier;
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    char endpoint[QRX_AURA_MODEL_PROVIDER_ENDPOINT_MAX];
    uint32_t latency_ms;
    uint32_t bandwidth_mbps;
    uint32_t reliability_bps;
    uint32_t score;
} QrxAuraModelSource;

typedef struct {
    uint32_t version;
    uint32_t asset_index;
    QrxAuraModelSource sources[32];
    uint32_t source_count;
} QrxAuraModelDownloadPlan;

int qrx_aura_model_manifest_validate(const QrxAuraModelManifest *manifest);
int qrx_aura_model_manifest_commitment(const QrxAuraModelManifest *manifest,char out_hex[65]);
int qrx_aura_model_catalog_announcement_hash(const QrxAuraModelCatalogAnnouncement *a,uint8_t out[32]);
int qrx_aura_model_catalog_announcement_sign(EVP_PKEY *key,const QrxAuraModelCatalogAnnouncement *a,uint8_t **sig,size_t *sig_len);
int qrx_aura_model_catalog_announcement_verify(EVP_PKEY *key,const QrxAuraModelCatalogAnnouncement *a,const uint8_t *sig,size_t sig_len);
void qrx_aura_model_catalog_init(QrxAuraModelCatalog *catalog);
void qrx_aura_model_catalog_free(QrxAuraModelCatalog *catalog);
int qrx_aura_model_catalog_ingest(QrxAuraModelCatalog *catalog,const QrxAuraModelCatalogAnnouncement *a,const uint8_t *sig,size_t sig_len,
                                  uint64_t current_height,QrxAuraModelDistKeyLookupFn key_lookup,void *key_ctx,
                                  QrxAuraModelCatalogAuthorizeFn authorize,void *auth_ctx);
const QrxAuraModelCatalogAnnouncement *qrx_aura_model_catalog_find(const QrxAuraModelCatalog *catalog,const char *model_id,const char *model_version);
size_t qrx_aura_model_catalog_profiles(const QrxAuraModelCatalog *catalog,uint64_t current_height,QrxAuraModelCapabilityProfile *out,size_t cap);

int qrx_aura_model_availability_validate(const QrxAuraModelAvailabilityAnnouncement *a,uint64_t current_height);
int qrx_aura_model_availability_hash(const QrxAuraModelAvailabilityAnnouncement *a,uint8_t out[32]);
int qrx_aura_model_availability_sign(EVP_PKEY *key,const QrxAuraModelAvailabilityAnnouncement *a,uint8_t **sig,size_t *sig_len);
int qrx_aura_model_availability_verify(EVP_PKEY *key,const QrxAuraModelAvailabilityAnnouncement *a,const uint8_t *sig,size_t sig_len);
void qrx_aura_model_provider_index_init(QrxAuraModelProviderIndex *index);
void qrx_aura_model_provider_index_free(QrxAuraModelProviderIndex *index);
int qrx_aura_model_provider_index_ingest(QrxAuraModelProviderIndex *index,const QrxAuraModelAvailabilityAnnouncement *a,const uint8_t *sig,size_t sig_len,
                                         uint64_t current_height,QrxAuraModelDistKeyLookupFn key_lookup,void *key_ctx);
size_t qrx_aura_model_provider_index_prune(QrxAuraModelProviderIndex *index,uint64_t current_height);
int qrx_aura_model_availability_set_asset(QrxAuraModelAvailabilityAnnouncement *a,uint32_t asset_index,int present);
int qrx_aura_model_availability_has_asset(const QrxAuraModelAvailabilityAnnouncement *a,uint32_t asset_index);

uint32_t qrx_aura_model_target_replicas(const QrxAuraModelCatalogAnnouncement *catalog_entry,const QrxAuraModelAsset *asset,uint32_t demand_bps);
int qrx_aura_model_asset_health(const QrxAuraModelManifest *manifest,const QrxAuraModelProviderIndex *index,uint64_t current_height,
                                uint32_t asset_index,uint32_t demand_bps,const QrxAuraModelCatalogAnnouncement *catalog_entry,
                                QrxAuraModelAssetHealth *out);
int qrx_aura_model_zero_config_profile(uint64_t free_disk_bytes,QrxAuraPowerProfile power,QrxAuraZeroConfigProfile *out);
int qrx_aura_model_placement_plan(const QrxAuraModelCatalogAnnouncement *catalog_entry,const QrxAuraModelProviderIndex *index,
                                  uint64_t current_height,const QrxAuraPodCapacity *local_pod,const QrxAuraZeroConfigProfile *profile,
                                  const uint8_t local_asset_bitmap[QRX_AURA_MODEL_AVAILABILITY_BITS],QrxAuraModelPlacementPlan *out);
int qrx_aura_model_download_plan(const QrxAuraModelManifest *manifest,const QrxAuraModelProviderIndex *index,uint64_t current_height,
                                 uint32_t asset_index,const char *local_region,uint32_t lan_latency_ceiling_ms,uint8_t allow_external_origin,
                                 QrxAuraModelDownloadPlan *out);
#ifdef __cplusplus
}
#endif
