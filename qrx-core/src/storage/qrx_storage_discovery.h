#pragma once
#include "qrxdb.h"
#include "storage/qrx_storage_transport.h"
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_STORAGE_DISCOVERY_VERSION 1U
#define QRX_STORAGE_DISCOVERY_MAX_ENDPOINT 256U
#define QRX_STORAGE_DISCOVERY_MAX_ENTRIES 4096U
#define QRX_STORAGE_DISCOVERY_CAP_RANGE_READ (1ULL<<0)
#define QRX_STORAGE_DISCOVERY_CAP_RESUME     (1ULL<<1)
#define QRX_STORAGE_DISCOVERY_CAP_QUIC       (1ULL<<2)
#define QRX_STORAGE_DISCOVERY_CAP_PROOF      (1ULL<<3)
#define QRX_STORAGE_DISCOVERY_CAP_PUBLIC     (1ULL<<4)

typedef struct {
    uint32_t version;
    char provider_id[129];
    uint64_t sequence;
    uint64_t valid_from_height;
    uint64_t valid_until_height;
    uint64_t capabilities;
    uint64_t available_bytes;
    uint32_t observed_latency_ms;
    uint64_t observed_throughput_bps;
    uint32_t reliability_bps;
    char endpoint[QRX_STORAGE_DISCOVERY_MAX_ENDPOINT];
} QrxStorageProviderAnnouncement;

typedef struct {
    QrxStorageProviderAnnouncement announcement;
    uint8_t *signature;
    size_t signature_len;
    uint64_t learned_height;
} QrxStorageDiscoveryEntry;

typedef struct {
    QrxStorageDiscoveryEntry *entries;
    size_t count;
    size_t capacity;
} QrxStorageDiscoveryTable;

typedef int (*QrxStorageProviderKeyLookupFn)(void *ctx,const char *provider_id,EVP_PKEY **public_key_out);

int qrx_storage_announcement_canonical(const QrxStorageProviderAnnouncement *a,uint8_t **out,size_t *out_len);
int qrx_storage_announcement_hash(const QrxStorageProviderAnnouncement *a,uint8_t out[64]);
int qrx_storage_announcement_sign(EVP_PKEY *private_key,const QrxStorageProviderAnnouncement *a,uint8_t **sig,size_t *sig_len);
int qrx_storage_announcement_verify(EVP_PKEY *public_key,const QrxStorageProviderAnnouncement *a,const uint8_t *sig,size_t sig_len);
int qrx_storage_announcement_validate(const QrxStorageProviderAnnouncement *a,uint64_t current_height);

void qrx_storage_discovery_init(QrxStorageDiscoveryTable *t);
void qrx_storage_discovery_free(QrxStorageDiscoveryTable *t);
int qrx_storage_discovery_ingest(QrxStorageDiscoveryTable *t,QrxDB *db,
                                 const QrxStorageProviderAnnouncement *a,const uint8_t *sig,size_t sig_len,
                                 uint64_t current_height,QrxStorageProviderKeyLookupFn key_lookup,void *key_ctx);
size_t qrx_storage_discovery_prune(QrxStorageDiscoveryTable *t,uint64_t current_height);
int qrx_storage_discovery_merge(QrxStorageDiscoveryTable *dst,QrxDB *db,const QrxStorageDiscoveryTable *src,
                                uint64_t current_height,QrxStorageProviderKeyLookupFn key_lookup,void *key_ctx);

/* Resolves the authoritative provider assignments for one storage contract to
 * currently reachable, cryptographically authenticated discovery entries.
 * Unknown/stale/unassigned announcements are ignored. Sources are ranked by
 * qrx_storage_fetch_plan() before being returned. */
int qrx_storage_discovery_sources_for_contract(QrxStorageDiscoveryTable *t,QrxDB *db,const char *contract_id,
                                               uint32_t shard_count,uint64_t current_height,
                                               QrxShardProviderSource *out,size_t out_cap,size_t *out_count);
/* Upload staging may resolve PENDING/REPAIRING assignments; reads remain ACTIVE-only. */
int qrx_storage_discovery_sources_for_contract_upload(QrxStorageDiscoveryTable *t,QrxDB *db,const char *contract_id,
                                                      uint32_t shard_count,uint64_t current_height,
                                                      QrxShardProviderSource *out,size_t out_cap,size_t *out_count);
const QrxStorageProviderAnnouncement *qrx_storage_discovery_find(const QrxStorageDiscoveryTable *t,const char *provider_id);

#ifdef __cplusplus
}
#endif

/* 0.0.8.64 authenticated gossip/cache helpers. The wire format carries the
 * exact signed announcement; peers MUST call qrx_storage_discovery_ingest()
 * after decode, so gossip never bypasses chain/provider/key validation. */
int qrx_storage_discovery_wire_encode(const QrxStorageProviderAnnouncement *a,const uint8_t *sig,size_t sig_len,uint8_t **out,size_t *out_len);
int qrx_storage_discovery_wire_decode(const uint8_t *in,size_t in_len,QrxStorageProviderAnnouncement *a,uint8_t **sig,size_t *sig_len);
int qrx_storage_discovery_cache_save(const QrxStorageDiscoveryTable *t,const char *path);
int qrx_storage_discovery_cache_load(QrxStorageDiscoveryTable *t,QrxDB *db,const char *path,uint64_t current_height,QrxStorageProviderKeyLookupFn key_lookup,void *key_ctx);
