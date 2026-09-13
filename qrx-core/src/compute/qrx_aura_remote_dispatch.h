#pragma once
#include <stddef.h>
#include <stdint.h>
#include <openssl/evp.h>
#include "compute/qrx_aura_live_dispatch.h"
#include "storage/qrx_storage_fs.h"
#include "compute/qrx_aura_job_journal.h"
#ifdef __cplusplus
extern "C" {
#endif

/* QRX 0.0.9.38 – authenticated remote dispatch, provider-side reservation
 * leases, QRX Drive/CAS model bundle fetch and AURA gossip anti-entropy. */
#define QRX_AURA_REMOTE_VERSION 1u
#define QRX_AURA_REMOTE_MAX_PAYLOAD (16u*1024u*1024u)
#define QRX_AURA_REMOTE_MAX_SIGNATURE 16384u
#define QRX_AURA_REMOTE_MAX_LEASES 1024u
#define QRX_AURA_REMOTE_MAX_PEERS 32u
#define QRX_AURA_REMOTE_MAX_BUNDLE_OBJECTS 256u
#define QRX_AURA_REMOTE_DEFAULT_LEASE_BLOCKS 16u
#define QRX_AURA_REMOTE_MAX_LEASE_BLOCKS 256u
#define QRX_AURA_REMOTE_IO_MAGIC "QRXARD38"

#define QRX_AURA_SECURE_CHANNEL_VERSION 1u
#define QRX_AURA_SECURE_KEY_BYTES 32u
#define QRX_AURA_SECURE_SESSION_ID_HEX 64u
#define QRX_AURA_SECURE_ENVELOPE_MAGIC "QRXSEC39"
#define QRX_AURA_SECURE_NONCE_BYTES 12u
#define QRX_AURA_SECURE_TAG_BYTES 16u

#define QRX_AURA_PQ_HYBRID_KEM_NAME "X25519MLKEM768"
#define QRX_AURA_PQ_SESSION_CACHE_MAX 256u
#define QRX_AURA_PQ_SESSION_DEFAULT_TTL_BLOCKS 128u
#define QRX_AURA_PQ_SESSION_MAX_TTL_BLOCKS 4096u


#define QRX_AURA_REMOTE_KIND_LEASE_OPEN    1u
#define QRX_AURA_REMOTE_KIND_LEASE_RENEW   2u
#define QRX_AURA_REMOTE_KIND_LEASE_RELEASE 3u
#define QRX_AURA_REMOTE_KIND_DISPATCH      4u
#define QRX_AURA_REMOTE_KIND_SESSION_KEM_GET 5u
#define QRX_AURA_REMOTE_KIND_SESSION_OPEN    6u

#define QRX_AURA_REMOTE_STATUS_OK          0u
#define QRX_AURA_REMOTE_STATUS_INVALID     1u
#define QRX_AURA_REMOTE_STATUS_UNAUTHORIZED 2u
#define QRX_AURA_REMOTE_STATUS_BUSY        3u
#define QRX_AURA_REMOTE_STATUS_EXPIRED     4u
#define QRX_AURA_REMOTE_STATUS_EXECUTION   5u
#define QRX_AURA_REMOTE_STATUS_UNSUPPORTED 6u

#define QRX_AURA_LEASE_ACTIVE   1u
#define QRX_AURA_LEASE_RELEASED 2u
#define QRX_AURA_LEASE_EXPIRED  3u

#define QRX_AURA_MODEL_BUNDLE_VERSION 1u
#define QRX_AURA_MODEL_OBJECT_WEIGHTS 1u
#define QRX_AURA_MODEL_OBJECT_TOKENIZER 2u
#define QRX_AURA_MODEL_OBJECT_CONFIG 3u
#define QRX_AURA_MODEL_OBJECT_EXPERT_PACK 4u
#define QRX_AURA_MODEL_OBJECT_EXPERT_MANIFEST 5u
#define QRX_AURA_MODEL_OBJECT_SOURCE_MAP 6u

/* Signed wire frame. Payload is committed in the signature and may contain
 * opaque runtime input/output bytes. The same framing is used for requests and
 * responses so the client can authenticate provider results. */
typedef struct {
    uint32_t version;
    uint32_t kind;
    uint32_t status;
    char signer_id[QRX_GLOBE_PROVIDER_MAX+1];
    uint64_t sequence;
    uint64_t height;
    char admission_commitment[65];
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    char pod_id[QRX_MOE_MAX_POD_ID];
    char lease_id[65];
    uint64_t lease_expires_height;
    uint32_t compute_threads;
    uint64_t network_egress_mbps;
    uint32_t node_id;
    uint64_t required_memory_bytes;
    uint64_t max_output_bytes;
    uint32_t fragment_index;
    uint32_t fragment_count;
    char runtime_id[QRX_MODEL_MAX_RUNTIME];
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    char model_commitment[65];
    char payload_commitment[65];
    uint8_t *payload;
    size_t payload_len;
} QrxAuraRemoteFrame;

typedef int (*QrxAuraRemoteKeyLookupFn)(void *ctx,const char *identity,EVP_PKEY **public_key_out);
typedef uint64_t (*QrxAuraRemoteHeightFn)(void *ctx);

typedef struct {
    uint32_t version;
    char session_id[65];
    uint8_t key[QRX_AURA_SECURE_KEY_BYTES];
} QrxAuraSecureSession;

typedef int (*QrxAuraSecureSessionLookupFn)(void *ctx,const char *local_id,const char *remote_id,QrxAuraSecureSession *session_out);

typedef struct {
    char local_id[QRX_GLOBE_PROVIDER_MAX+1];
    char remote_id[QRX_GLOBE_PROVIDER_MAX+1];
    QrxAuraSecureSession session;
    uint64_t created_height;
    uint64_t expires_height;
} QrxAuraPqSessionEntry;

typedef struct {
    QrxAuraPqSessionEntry entries[QRX_AURA_PQ_SESSION_CACHE_MAX];
    uint32_t count;
    uint64_t revision;
} QrxAuraPqSessionCache;

int qrx_aura_pq_hybrid_kem_generate(EVP_PKEY **private_key_out);
int qrx_aura_pq_hybrid_encapsulate(EVP_PKEY *remote_public,const char *requester_id,const char *provider_id,
                                   uint8_t **ciphertext_out,size_t *ciphertext_len_out,QrxAuraSecureSession *session_out);
int qrx_aura_pq_hybrid_decapsulate(EVP_PKEY *local_private,const void *ciphertext,size_t ciphertext_len,
                                   const char *requester_id,const char *provider_id,QrxAuraSecureSession *session_out);
void qrx_aura_pq_session_cache_init(QrxAuraPqSessionCache *cache);
int qrx_aura_pq_session_cache_put(QrxAuraPqSessionCache *cache,const char *local_id,const char *remote_id,
                                  const QrxAuraSecureSession *session,uint64_t created_height,uint64_t expires_height);
int qrx_aura_pq_session_cache_get(QrxAuraPqSessionCache *cache,const char *local_id,const char *remote_id,
                                  uint64_t current_height,QrxAuraSecureSession *session_out);
size_t qrx_aura_pq_session_cache_prune(QrxAuraPqSessionCache *cache,uint64_t current_height);

/* Confidential application payload envelope. The signed remote frame remains
 * authoritative for identity/integrity; AES-256-GCM adds confidentiality.
 * 0.0.9.40 can negotiate the abstract session key with OpenSSL's hybrid
 * X25519MLKEM768 KEM. The older X25519 helper remains available as a classical
 * compatibility/bootstrap path and is not itself a PQC claim. */
int qrx_aura_secure_session_from_secret(const void *secret,size_t secret_len,const char *requester_id,const char *provider_id,QrxAuraSecureSession *out);
int qrx_aura_secure_session_x25519(EVP_PKEY *local_private,EVP_PKEY *remote_public,const char *requester_id,const char *provider_id,QrxAuraSecureSession *out);
int qrx_aura_secure_payload_is_envelope(const void *payload,size_t payload_len);
int qrx_aura_secure_payload_seal(const QrxAuraSecureSession *session,const QrxAuraRemoteFrame *frame,const void *plaintext,size_t plaintext_len,uint8_t **sealed_out,size_t *sealed_len_out);
int qrx_aura_secure_payload_open(const QrxAuraSecureSession *session,const QrxAuraRemoteFrame *frame,const void *sealed,size_t sealed_len,uint8_t **plaintext_out,size_t *plaintext_len_out);

void qrx_aura_remote_frame_free(QrxAuraRemoteFrame *frame);
int qrx_aura_remote_frame_commitment(const QrxAuraRemoteFrame *frame,char out_hex[65]);
int qrx_aura_remote_frame_sign(EVP_PKEY *private_key,const QrxAuraRemoteFrame *frame,uint8_t **sig,size_t *sig_len);
int qrx_aura_remote_frame_verify(EVP_PKEY *public_key,const QrxAuraRemoteFrame *frame,const uint8_t *sig,size_t sig_len);
int qrx_aura_remote_wire_encode(const QrxAuraRemoteFrame *frame,const uint8_t *sig,size_t sig_len,uint8_t **out,size_t *out_len);
int qrx_aura_remote_wire_decode(const uint8_t *in,size_t in_len,QrxAuraRemoteFrame *frame,uint8_t **sig,size_t *sig_len);
int qrx_aura_remote_send_fd(int fd,const QrxAuraRemoteFrame *frame,EVP_PKEY *signer_private);
int qrx_aura_remote_recv_fd(int fd,QrxAuraRemoteFrame *frame,uint8_t **sig,size_t *sig_len);

/* Provider-side lease table. LEASE_OPEN reserves the real provider runtime.
 * Expiry/release always returns the same resources exactly once. */
typedef struct {
    uint32_t version;
    uint32_t state;
    char lease_id[65];
    char requester_id[QRX_GLOBE_PROVIDER_MAX+1];
    char admission_commitment[65];
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    char pod_id[QRX_MOE_MAX_POD_ID];
    char model_commitment[65];
    uint32_t node_id;
    uint64_t opened_height;
    uint64_t expires_height;
    uint64_t last_sequence; /* strict per-lease replay barrier */
    uint32_t compute_threads;
    uint64_t network_egress_mbps;
    uint64_t required_memory_bytes;
    uint64_t max_output_bytes;
    uint32_t fragment_index;
    uint32_t fragment_count;
    char runtime_id[QRX_MODEL_MAX_RUNTIME];
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
} QrxAuraReservationLease;

typedef struct {
    QrxAuraReservationLease entries[QRX_AURA_REMOTE_MAX_LEASES];
    uint32_t count;
    uint64_t revision;
} QrxAuraReservationLeaseTable;

void qrx_aura_reservation_leases_init(QrxAuraReservationLeaseTable *table);
const QrxAuraReservationLease *qrx_aura_reservation_lease_find(const QrxAuraReservationLeaseTable *table,const char *lease_id);
int qrx_aura_reservation_lease_open(QrxAuraReservationLeaseTable *table,QrxAuraRuntimeBinding *binding,
                                    const QrxAuraRemoteFrame *request,uint64_t current_height,
                                    QrxAuraReservationLease *lease_out);
int qrx_aura_reservation_lease_renew(QrxAuraReservationLeaseTable *table,const QrxAuraRemoteFrame *request,
                                     uint64_t current_height,QrxAuraReservationLease *lease_out);
int qrx_aura_reservation_lease_release(QrxAuraReservationLeaseTable *table,QrxAuraRuntimeBinding *binding,
                                       const QrxAuraRemoteFrame *request);
size_t qrx_aura_reservation_leases_reap(QrxAuraReservationLeaseTable *table,QrxAuraRuntimeBinding *bindings,
                                        size_t binding_count,uint64_t current_height);

/* Crash-durable lease journal. Save is atomic (temp + fsync + replace). Recovery
 * re-reserves only still-live ACTIVE leases on freshly initialized provider
 * runtimes and rolls back all recovered reservations if any entry cannot be
 * restored exactly. */
int qrx_aura_reservation_leases_journal_save(const char *path,const QrxAuraReservationLeaseTable *table);
int qrx_aura_reservation_leases_journal_load(const char *path,QrxAuraReservationLeaseTable *table);
int qrx_aura_reservation_leases_journal_recover(const char *path,QrxAuraReservationLeaseTable *table,QrxAuraRuntimeBinding *bindings,size_t binding_count,uint64_t current_height,size_t *recovered_out,size_t *expired_out);
int qrx_aura_reservation_leases_reap_durable(QrxAuraReservationLeaseTable *table,QrxAuraRuntimeBinding *bindings,size_t binding_count,uint64_t current_height,const char *journal_path,size_t *reaped_out);

typedef int (*QrxAuraRemoteModelExecuteFn)(void *ctx,const QrxAiModelRecord *model,const QrxAuraRemoteFrame *request,
                                           const void *input,size_t input_bytes,void *output,size_t output_capacity,size_t *output_bytes);

/* Remote provider endpoint. The server verifies requester signatures, manages
 * leases and executes only against an active lease bound to this provider/pod. */
typedef struct {
    QrxAuraRuntimeBinding *binding;
    QrxAuraReservationLeaseTable *leases;
    QrxAuraModelCacheCatalog *model_cache;
    QrxAuraRemoteKeyLookupFn requester_key_lookup;
    void *requester_key_ctx;
    EVP_PKEY *provider_private_key;
    QrxAuraRemoteHeightFn height_fn;
    void *height_ctx;
    const char *lease_journal_path;
    uint8_t require_durable_leases;
    QrxAuraJobJournal *job_journal;
    const char *job_journal_path;
    uint8_t require_durable_jobs;
    QrxAuraSecureSessionLookupFn secure_session_lookup;
    void *secure_session_ctx;
    QrxAuraPqSessionCache *pq_session_cache;
    EVP_PKEY *hybrid_kem_private_key;
    uint64_t pq_session_ttl_blocks;
    uint8_t require_secure_dispatch;
    const QrxAiModelRegistry *model_registry;
    QrxAuraModelCacheFetchFn model_fetch;
    void *model_fetch_ctx;
    QrxAuraRemoteModelExecuteFn model_execute;
    void *model_execute_ctx;
} QrxAuraRemoteServerContext;

int qrx_aura_remote_serve_fd(int connected_fd,QrxAuraRemoteServerContext *server);

/* Client-side peer map and distributed-dispatch adapter. The adapter is a
 * direct QrxAuraDistributedDispatchFn implementation for 0.0.9.37. */
typedef struct {
    char provider_id[QRX_GLOBE_PROVIDER_MAX+1];
    char pod_id[QRX_MOE_MAX_POD_ID];
    char endpoint[QRX_AURA_GOSSIP_MAX_ENDPOINT];
    QrxAuraSecureSession secure_session;
    EVP_PKEY *hybrid_kem_public_key; /* owned by client peer after set/fetch */
    uint8_t secure_enabled;
    uint8_t auto_hybrid_session;
} QrxAuraRemotePeer;

typedef int (*QrxAuraRemoteFragmentInputFn)(void *ctx,uint32_t fragment_index,uint32_t fragment_count,
                                            const void *input,size_t input_bytes,
                                            uint8_t **fragment_out,size_t *fragment_bytes_out);
typedef int (*QrxAuraRemoteAggregateFn)(void *ctx,const uint8_t *const *fragment_outputs,const size_t *fragment_sizes,
                                       uint32_t fragment_count,void *output,size_t output_capacity,size_t *output_bytes);

typedef struct {
    char requester_id[QRX_GLOBE_PROVIDER_MAX+1];
    EVP_PKEY *requester_private_key;
    QrxAuraRemoteKeyLookupFn provider_key_lookup;
    void *provider_key_ctx;
    QrxAuraRemotePeer peers[QRX_AURA_REMOTE_MAX_PEERS];
    uint32_t peer_count;
    uint64_t sequence;
    uint64_t current_height;
    uint64_t lease_blocks;
    uint8_t require_secure_peers;
    QrxAuraRemoteFragmentInputFn fragment_input;
    void *fragment_input_ctx;
    QrxAuraRemoteAggregateFn aggregate;
    void *aggregate_ctx;
} QrxAuraRemoteClientContext;

int qrx_aura_remote_client_add_peer(QrxAuraRemoteClientContext *client,const char *provider_id,const char *pod_id,const char *endpoint);
int qrx_aura_remote_client_set_peer_secure_session(QrxAuraRemoteClientContext *client,const char *provider_id,const char *pod_id,const QrxAuraSecureSession *session);
int qrx_aura_remote_client_enable_peer_pq_hybrid(QrxAuraRemoteClientContext *client,const char *provider_id,const char *pod_id,int enable);
int qrx_aura_remote_client_set_peer_hybrid_kem_public(QrxAuraRemoteClientContext *client,const char *provider_id,const char *pod_id,EVP_PKEY *public_key);
void qrx_aura_remote_client_cleanup(QrxAuraRemoteClientContext *client);
int qrx_aura_remote_distributed_dispatch(void *ctx,const QrxAuraAdmission *admission,const QrxComputeJobNode *node,
                                         const void *input,size_t input_bytes,void *output,size_t output_capacity,size_t *output_bytes);

/* QRX Drive model bundle format. Model bytes are content-addressed objects;
 * only roots/metadata are in the bundle manifest. */
typedef struct {
    uint32_t kind;
    uint64_t bytes;
    char object_root[65];
} QrxAuraModelBundleObject;

typedef struct {
    uint32_t version;
    char model_id[QRX_MODEL_MAX_ID];
    char model_version[QRX_MODEL_MAX_VERSION];
    /* Intentionally does not contain QrxAiModelRecord commitment: that
     * commitment already binds manifest_root, so embedding it here would
     * create a hash cycle (model commitment -> manifest root -> bundle ->
     * model commitment). The chain registry binds this exact bundle by
     * manifest_root instead. */
    uint64_t total_bytes;
    uint32_t object_count;
    QrxAuraModelBundleObject objects[QRX_AURA_REMOTE_MAX_BUNDLE_OBJECTS];
} QrxAuraModelBundleManifest;

typedef int (*QrxAuraDriveCasStatFn)(void *ctx,const char *object_root,uint64_t *bytes_out);
typedef int (*QrxAuraDriveCasRangeFetchFn)(void *ctx,const char *object_root,uint64_t offset,size_t length,
                                          uint8_t **out,size_t *out_len);

typedef struct {
    QrxStorageFs *local_cache;
    QrxAuraDriveCasStatFn stat;
    QrxAuraDriveCasRangeFetchFn fetch_range;
    void *remote_ctx;
    size_t range_bytes;
} QrxAuraDriveModelFetchContext;

int qrx_aura_model_bundle_encode(const QrxAuraModelBundleManifest *manifest,uint8_t **out,size_t *out_len);
int qrx_aura_model_bundle_decode(const uint8_t *in,size_t in_len,QrxAuraModelBundleManifest *manifest);
int qrx_aura_model_bundle_root(const QrxAuraModelBundleManifest *manifest,char out_hex[65]);
int qrx_aura_drive_model_fetch(void *ctx,const QrxAiModelRecord *model,const QrxAuraModelCachePlacement *placement,
                               char verified_model_commitment_out[65]);
/* Fetch one content-addressed model asset into local_cache and verify its
 * declared byte size. Used by the autonomous replication executor. */
int qrx_aura_drive_asset_fetch(QrxAuraDriveModelFetchContext *ctx,const char *object_root,uint64_t declared_bytes);

#ifdef __cplusplus
}
#endif
