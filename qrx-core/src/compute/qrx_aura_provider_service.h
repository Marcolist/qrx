#pragma once
#include <stddef.h>
#include <stdint.h>
#include "compute/qrx_aura_remote_dispatch.h"
#ifdef __cplusplus
extern "C" {
#endif

/* QRX 0.0.9.39 – autonomous provider service + model-aware MoE contract. */
#define QRX_AURA_PROVIDER_SERVICE_VERSION 1u
#define QRX_AURA_PROVIDER_HOST_MAX 128u
#define QRX_AURA_EXPERT_MANIFEST_VERSION 1u
#define QRX_AURA_MOE_FRAGMENT_VERSION 1u
#define QRX_AURA_MOE_FRAGMENT_MAGIC "QRXMF39\0"
#define QRX_AURA_MOE_MAX_FRAGMENTS QRX_AURA_ADMISSION_MAX_PODS
#define QRX_AURA_MOE_MAX_EXPERTS 1048576u

#define QRX_AURA_FRAGMENT_SINGLE 1u
#define QRX_AURA_FRAGMENT_REPLICATED 2u
#define QRX_AURA_FRAGMENT_EXPERT_RANGE 3u

typedef struct {
    uint32_t version;
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char architecture[QRX_MODEL_MAX_ARCH];
    uint32_t layer_count;
    uint32_t expert_count;
    uint32_t experts_per_token;
} QrxAuraExpertManifest;

typedef struct {
    uint32_t fragment_index;
    uint32_t fragment_count;
    uint32_t expert_first;
    uint32_t expert_count;
    uint32_t layer_first;
    uint32_t layer_count;
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    char pod_id[QRX_MOE_MAX_POD_ID];
} QrxAuraMoeFragment;

typedef struct {
    uint32_t version;
    uint32_t strategy;
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char model_commitment[65];
    char expert_manifest_root[65];
    uint32_t layer_count;
    uint32_t expert_count;
    uint32_t experts_per_token;
    uint32_t fragment_count;
    QrxAuraMoeFragment fragments[QRX_AURA_MOE_MAX_FRAGMENTS];
    char plan_commitment[65];
} QrxAuraMoeFragmentPlan;

typedef struct {
    uint32_t version;
    char plan_commitment[65];
    uint32_t strategy;
    uint32_t fragment_index;
    uint32_t fragment_count;
    uint32_t expert_first;
    uint32_t expert_count;
    uint32_t layer_first;
    uint32_t layer_count;
    uint32_t experts_per_token;
    uint8_t *input;
    size_t input_len;
} QrxAuraMoeFragmentEnvelope;

int qrx_aura_expert_manifest_encode(const QrxAuraExpertManifest *manifest,uint8_t **out,size_t *out_len);
int qrx_aura_expert_manifest_decode(const uint8_t *in,size_t in_len,QrxAuraExpertManifest *manifest);
int qrx_aura_expert_manifest_root(const QrxAuraExpertManifest *manifest,char out_hex[65]);
int qrx_aura_expert_manifest_load(QrxStorageFs *fs,const QrxAiModelRecord *model,QrxAuraExpertManifest *out);
int qrx_aura_moe_fragment_plan_build(const QrxAiModelRecord *model,const QrxAuraExpertManifest *expert_manifest,const QrxAuraAdmission *admission,QrxAuraMoeFragmentPlan *out);
int qrx_aura_moe_fragment_plan_commitment(const QrxAuraMoeFragmentPlan *plan,char out_hex[65]);
int qrx_aura_moe_fragment_payload_encode(const QrxAuraMoeFragmentPlan *plan,uint32_t fragment_index,const void *input,size_t input_len,uint8_t **out,size_t *out_len);
int qrx_aura_moe_fragment_payload_decode(const uint8_t *in,size_t in_len,QrxAuraMoeFragmentEnvelope *out);
void qrx_aura_moe_fragment_envelope_free(QrxAuraMoeFragmentEnvelope *envelope);
/* Direct QrxAuraRemoteFragmentInputFn adapter: ctx is QrxAuraMoeFragmentPlan*. */
int qrx_aura_moe_fragment_input_adapter(void *ctx,uint32_t fragment_index,uint32_t fragment_count,const void *input,size_t input_bytes,uint8_t **fragment_out,size_t *fragment_bytes_out);

typedef int (*QrxAuraProviderAdvertiseFn)(void *ctx,const QrxAuraPodAnnouncement *announcement,const uint8_t *signature,size_t signature_len);

typedef struct {
    uint32_t version;
    QrxAuraRuntimeBinding *binding;
    QrxAuraModelCacheCatalog *model_cache;
    const QrxAiModelRegistry *model_registry;
    QrxAuraRemoteKeyLookupFn requester_key_lookup;
    void *requester_key_ctx;
    EVP_PKEY *provider_private_key;
    QrxAuraRemoteHeightFn height_fn;
    void *height_ctx;
    QrxAuraSecureSessionLookupFn secure_session_lookup;
    void *secure_session_ctx;
    uint8_t enable_pq_hybrid_sessions;
    EVP_PKEY *hybrid_kem_private_key; /* optional; generated when enabled and NULL */
    uint64_t pq_session_ttl_blocks;
    QrxAuraModelCacheFetchFn model_fetch;
    void *model_fetch_ctx;
    QrxAuraRemoteModelExecuteFn model_execute;
    void *model_execute_ctx;
    char listen_host[QRX_AURA_PROVIDER_HOST_MAX];
    uint16_t listen_port; /* 0 = OS-assigned */
    char lease_journal_path[1024];
    char job_journal_path[1024]; /* empty => <lease_journal_path>.jobs */
    uint8_t require_secure_dispatch;
    char relay_endpoint[QRX_AURA_GOSSIP_MAX_ENDPOINT]; /* optional qrxrelay://host:port/route */
    uint8_t relay_required;
    uint8_t auto_advertise;
    QrxAuraPodCapacity advertised_capacity;
    uint64_t advertisement_sequence;
    uint64_t advertisement_ttl_blocks;
    QrxAuraProviderAdvertiseFn advertise;
    void *advertise_ctx;
} QrxAuraProviderServiceConfig;

typedef struct QrxAuraProviderService QrxAuraProviderService;

int qrx_aura_provider_service_start(const QrxAuraProviderServiceConfig *config,QrxAuraProviderService **out);
int qrx_aura_provider_service_stop(QrxAuraProviderService *service);
void qrx_aura_provider_service_free(QrxAuraProviderService *service);
const char *qrx_aura_provider_service_endpoint(const QrxAuraProviderService *service);
uint16_t qrx_aura_provider_service_port(const QrxAuraProviderService *service);
uint32_t qrx_aura_provider_service_active_leases(const QrxAuraProviderService *service);
uint64_t qrx_aura_provider_service_announcement_sequence(const QrxAuraProviderService *service);
uint32_t qrx_aura_provider_service_pq_session_count(const QrxAuraProviderService *service);
const char *qrx_aura_provider_service_pq_kem_name(const QrxAuraProviderService *service);

#ifdef __cplusplus
}
#endif
