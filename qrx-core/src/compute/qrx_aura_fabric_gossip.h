#pragma once
#include <stddef.h>
#include <stdint.h>
#include <openssl/evp.h>
#include "compute/qrx_aura_model_fabric.h"
#ifdef __cplusplus
extern "C" {
#endif

/* QRX 0.0.9.36 – signed AURA pod gossip, live Resource Globe AI telemetry
   and model capability-profile distribution. Provider/pod identifiers remain
   internal to gossip/routing; public globe cells are still privacy aggregated. */
#define QRX_AURA_GOSSIP_VERSION 1u
#define QRX_AURA_GOSSIP_MAX_PODS 4096u
#define QRX_AURA_GOSSIP_MAX_MODELS 256u
#define QRX_AURA_GOSSIP_MAX_ENDPOINT 256u
#define QRX_AURA_GOSSIP_MAX_SIGNATURE 16384u
#define QRX_AURA_GOSSIP_MAX_WIRE (64u*1024u)
#define QRX_AURA_GOSSIP_DEFAULT_TTL_BLOCKS 120u

typedef struct {
    uint32_t version;
    uint64_t sequence;
    uint64_t valid_from_height;
    uint64_t valid_until_height;
    char service_endpoint[QRX_AURA_GOSSIP_MAX_ENDPOINT];
    QrxAuraPodCapacity capacity;
} QrxAuraPodAnnouncement;

typedef struct {
    QrxAuraPodAnnouncement announcement;
    uint8_t *signature;
    size_t signature_len;
    uint64_t learned_height;
} QrxAuraPodGossipEntry;

typedef struct {
    QrxAuraPodGossipEntry *entries;
    size_t count;
    size_t capacity;
    uint64_t revision;
} QrxAuraPodGossipTable;

typedef struct {
    uint32_t version;
    char publisher_id[QRX_GLOBE_PROVIDER_MAX+1];
    uint64_t sequence;
    uint64_t valid_from_height;
    uint64_t valid_until_height;
    char model_registry_commitment[65]; /* identity binding supplied by publisher */
    QrxAuraModelCapabilityProfile profile;
} QrxAuraModelProfileAnnouncement;

typedef struct {
    QrxAuraModelProfileAnnouncement announcement;
    uint8_t *signature;
    size_t signature_len;
    uint64_t learned_height;
} QrxAuraModelGossipEntry;

typedef struct {
    QrxAuraModelGossipEntry *entries;
    size_t count;
    size_t capacity;
    uint64_t revision;
} QrxAuraModelGossipTable;

typedef int (*QrxAuraGossipKeyLookupFn)(void *ctx,const char *identity,EVP_PKEY **public_key_out);
typedef int (*QrxAuraModelPublisherAuthorizeFn)(void *ctx,const char *publisher_id);

int qrx_aura_pod_announcement_validate(const QrxAuraPodAnnouncement *a,uint64_t current_height);
int qrx_aura_pod_announcement_hash(const QrxAuraPodAnnouncement *a,uint8_t out[32]);
int qrx_aura_pod_announcement_sign(EVP_PKEY *private_key,const QrxAuraPodAnnouncement *a,uint8_t **sig,size_t *sig_len);
int qrx_aura_pod_announcement_verify(EVP_PKEY *public_key,const QrxAuraPodAnnouncement *a,const uint8_t *sig,size_t sig_len);
int qrx_aura_pod_wire_encode(const QrxAuraPodAnnouncement *a,const uint8_t *sig,size_t sig_len,uint8_t **out,size_t *out_len);
int qrx_aura_pod_wire_decode(const uint8_t *in,size_t in_len,QrxAuraPodAnnouncement *a,uint8_t **sig,size_t *sig_len);

void qrx_aura_pod_gossip_init(QrxAuraPodGossipTable *t);
void qrx_aura_pod_gossip_free(QrxAuraPodGossipTable *t);
int qrx_aura_pod_gossip_ingest(QrxAuraPodGossipTable *t,const QrxAuraPodAnnouncement *a,const uint8_t *sig,size_t sig_len,
                               uint64_t current_height,QrxAuraGossipKeyLookupFn key_lookup,void *key_ctx);
size_t qrx_aura_pod_gossip_prune(QrxAuraPodGossipTable *t,uint64_t current_height);
const QrxAuraPodAnnouncement *qrx_aura_pod_gossip_find(const QrxAuraPodGossipTable *t,const char *pod_id);
int qrx_aura_pod_gossip_cache_save(const QrxAuraPodGossipTable *t,const char *path);
int qrx_aura_pod_gossip_cache_load(QrxAuraPodGossipTable *t,const char *path,uint64_t current_height,QrxAuraGossipKeyLookupFn key_lookup,void *key_ctx);

int qrx_aura_model_announcement_validate(const QrxAuraModelProfileAnnouncement *a,uint64_t current_height);
int qrx_aura_model_announcement_hash(const QrxAuraModelProfileAnnouncement *a,uint8_t out[32]);
int qrx_aura_model_announcement_sign(EVP_PKEY *private_key,const QrxAuraModelProfileAnnouncement *a,uint8_t **sig,size_t *sig_len);
int qrx_aura_model_announcement_verify(EVP_PKEY *public_key,const QrxAuraModelProfileAnnouncement *a,const uint8_t *sig,size_t sig_len);
int qrx_aura_model_wire_encode(const QrxAuraModelProfileAnnouncement *a,const uint8_t *sig,size_t sig_len,uint8_t **out,size_t *out_len);
int qrx_aura_model_wire_decode(const uint8_t *in,size_t in_len,QrxAuraModelProfileAnnouncement *a,uint8_t **sig,size_t *sig_len);

void qrx_aura_model_gossip_init(QrxAuraModelGossipTable *t);
void qrx_aura_model_gossip_free(QrxAuraModelGossipTable *t);
int qrx_aura_model_gossip_ingest(QrxAuraModelGossipTable *t,const QrxAuraModelProfileAnnouncement *a,const uint8_t *sig,size_t sig_len,
                                 uint64_t current_height,QrxAuraGossipKeyLookupFn key_lookup,void *key_ctx,
                                 QrxAuraModelPublisherAuthorizeFn authorize,void *auth_ctx);
size_t qrx_aura_model_gossip_prune(QrxAuraModelGossipTable *t,uint64_t current_height);
const QrxAuraModelProfileAnnouncement *qrx_aura_model_gossip_find(const QrxAuraModelGossipTable *t,const char *model_id,const char *model_version);
int qrx_aura_model_gossip_cache_save(const QrxAuraModelGossipTable *t,const char *path);
int qrx_aura_model_gossip_cache_load(QrxAuraModelGossipTable *t,const char *path,uint64_t current_height,QrxAuraGossipKeyLookupFn key_lookup,void *key_ctx,
                                     QrxAuraModelPublisherAuthorizeFn authorize,void *auth_ctx);

/* Builds the scheduling/globe view only from announcements that are still
   valid at current_height. The public cells retain the 0.0.9.35 unique-provider
   privacy threshold and never emit provider IDs or service endpoints. */
int qrx_aura_gossip_live_snapshot(const QrxAuraPodGossipTable *pods,const QrxAuraModelGossipTable *models,
                                  uint64_t current_height,size_t privacy_min_providers,
                                  QrxAuraFabricSnapshot *snapshot_out,QrxAuraGlobeCell **cells_out,size_t *cell_count_out);
int qrx_aura_gossip_live_route(const QrxAuraPodGossipTable *pods,const QrxAuraModelGossipTable *models,
                               uint64_t current_height,const QrxAuraRouteRequest *request,QrxAuraRouteDecision *out);
int qrx_aura_gossip_resource_globe(const QrxAuraPodGossipTable *pods,uint64_t current_height,size_t privacy_min_providers,
                                   QrxResourceGlobeCell **cells_out,size_t *cell_count_out);

typedef struct {
    uint32_t version;
    uint64_t pod_count;
    uint64_t model_count;
    char pod_root[65];
    char model_root[65];
    char combined_root[65];
} QrxAuraGossipDigest;

int qrx_aura_gossip_digest(const QrxAuraPodGossipTable *pods,const QrxAuraModelGossipTable *models,
                           uint64_t current_height,QrxAuraGossipDigest *out);
int qrx_aura_gossip_anti_entropy_merge(QrxAuraPodGossipTable *dst_pods,QrxAuraModelGossipTable *dst_models,
                                       const QrxAuraPodGossipTable *src_pods,const QrxAuraModelGossipTable *src_models,
                                       uint64_t current_height,QrxAuraGossipKeyLookupFn pod_key_lookup,void *pod_key_ctx,
                                       QrxAuraGossipKeyLookupFn model_key_lookup,void *model_key_ctx,
                                       QrxAuraModelPublisherAuthorizeFn authorize,void *auth_ctx,
                                       size_t *pod_updates,size_t *model_updates);

#ifdef __cplusplus
}
#endif
