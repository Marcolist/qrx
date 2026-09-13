#pragma once
#include "net/qrx_net_site.h"
#include "storage/qrx_drive_prepare.h"
#include "resource/qrx_storage_consensus.h"
#include "qrxdb.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_NET_SITE_CATALOG_PATH_MAX 1024
#define QRX_NET_SITE_STANDARD_DATA_SHARDS 10u
#define QRX_NET_SITE_STANDARD_PARITY_SHARDS 4u
#define QRX_NET_SITE_STANDARD_ACTIVE_MIN 10u

typedef struct {char domain[254];uint64_t sequence;uint8_t manifest_root[64];char package_path[QRX_NET_SITE_CATALOG_PATH_MAX];} QrxNetSiteVersion;
typedef int (*QrxNetSiteVersionCallback)(const QrxNetSiteVersion*,void*);

/* Archives an already verified PUBLIC_SIGNED package immutably. Existing
 * sequence files are never overwritten. */
int qrx_net_publisher_archive(const char *catalog_root,const QrxNetPublishedSite *site,QrxNetSiteVersion *out);
int qrx_net_publisher_load_version(const char *catalog_root,const char *domain,uint64_t sequence,QrxNetPublishedSite *out);
int qrx_net_publisher_list_versions(const char *catalog_root,const char *domain,QrxNetSiteVersionCallback cb,void *ctx);
/* Resolves a rollback target without mutating chain state; caller must submit
 * DOMAIN_UPDATE with the returned manifest_root using the current sequence. */
int qrx_net_publisher_prepare_rollback(const char *catalog_root,const char *domain,uint64_t target_sequence,QrxNetSiteVersion *out);

#define QRX_NET_PUBLISH_STATUS_PREPARED 1
#define QRX_NET_PUBLISH_STATUS_STORING 2
#define QRX_NET_PUBLISH_STATUS_WAITING_STORAGE 3
#define QRX_NET_PUBLISH_STATUS_READY_TO_ACTIVATE 4
#define QRX_NET_PUBLISH_STATUS_ACTIVATION_SUBMITTED 5
#define QRX_NET_PUBLISH_STATUS_ACTIVE 6
#define QRX_NET_PUBLISH_STATUS_FAILED 7

typedef struct {
    char domain[254];
    uint64_t sequence;
    uint64_t published_height;
    uint8_t manifest_root[64];
    char package_path[QRX_NET_SITE_CATALOG_PATH_MAX];
    char distribution_path[QRX_NET_SITE_CATALOG_PATH_MAX];
    char prepare_dir[QRX_NET_SITE_CATALOG_PATH_MAX];
    char storage_contract_id[129];
    char transfer_id[129];
    char activation_txid[129];
    uint64_t activation_height;
    char storage_step_txid[129];
    uint64_t storage_step_height;
    uint32_t storage_step_kind;
    uint32_t storage_step_shard;
    char journal_path[QRX_NET_SITE_CATALOG_PATH_MAX];
    uint32_t required_active_shards;
    uint32_t active_shards;
    uint32_t total_shards;
    uint32_t status;
} QrxNetPublishJob;

/* Securely walks a website directory without following symlinks/reparse points,
 * streams each regular file into PUBLIC_SIGNED CAS, signs the canonical site
 * manifest with ML-DSA-65, archives an immutable version, exports a canonical
 * distribution blob and prepares STANDARD 10+4 provider shards. */
int qrx_net_publisher_publish_directory(const char *catalog_root,QrxStorageFs *fs,
                                        const char *web_root,const char *domain,
                                        uint64_t sequence,uint64_t height,
                                        EVP_PKEY *publishing_key,
                                        QrxNetPublishJob *out);
int qrx_net_publisher_job_save(const QrxNetPublishJob *job);
int qrx_net_publisher_job_load(const char *journal_path,QrxNetPublishJob *out);
/* Local state update helper; chain-authoritative callers should use
 * qrx_net_publisher_refresh_storage(). */
int qrx_net_publisher_job_set_active_shards(QrxNetPublishJob *job,uint32_t active_shards);

/* Loads and verifies the signed 10+4 prepared public payload referenced by the
 * publish job. */
int qrx_net_publisher_load_prepared(const QrxNetPublishJob *job,EVP_PKEY *publishing_public,QrxDrivePreparedUpload *out);

/* Deterministic PUBLIC_SIGNED storage contract orchestration. The contract's
 * manifest_root is the QRX-Net site manifest root (the same value committed by
 * DOMAIN_UPDATE), while assignments are bound to the prepared shard CAS/Merkle
 * commitments. */
typedef enum { QRX_NET_HOST_STEP_INVALID=0,QRX_NET_HOST_STEP_CREATE=1,QRX_NET_HOST_STEP_ASSIGN=2,QRX_NET_HOST_STEP_READY_UPLOAD=3,QRX_NET_HOST_STEP_WAIT_ACTIVE=4,QRX_NET_HOST_STEP_READY_ACTIVATE=5 } QrxNetHostStepKind;
typedef struct {
    QrxNetHostStepKind kind;
    char contract_id[129];
    char tx_type[48];
    char to[160];
    char payload[1800];
    uint64_t amount_atoms;
    uint64_t end_height;
    uint64_t base_atoms_per_gib_epoch;
    uint32_t shard_index;
    uint32_t assignments_present;
    uint32_t active_shards;
    uint32_t required_active_shards;
} QrxNetHostStep;
int qrx_net_publisher_host_next_step(QrxDB *db,const char *owner,const QrxNetPublishJob *job,
                                     const QrxDrivePreparedUpload *prepared,uint64_t current_height,
                                     uint64_t epochs,uint64_t base_atoms_per_gib_epoch,
                                     QrxNetHostStep *out);
/* Revalidates contract + every assignment against the prepared package and
 * updates the journal from authoritative QRXDB state. */
int qrx_net_publisher_refresh_storage(QrxDB *db,QrxNetPublishJob *job,const QrxDrivePreparedUpload *prepared);

/* Canonical QRXWEB1 archive support for browser fetch. The archive contains the
 * signed QRXNS01 package followed by the exact CAS bytes for all site files. */
int qrx_net_publisher_export_distribution(QrxStorageFs *fs,const QrxNetPublishedSite *site,const char *package_path,const char *out_path);
int qrx_net_publisher_extract_file(const char *distribution_path,const char *request_path,QrxNetPublishedSite *site_out,unsigned char **data_out,size_t *data_len_out);

#ifdef __cplusplus
}
#endif

/* 0.0.8.77: verifies the complete QRXWEB1 distribution before it may enter the
 * browser cache. Uses the ML-DSA public key embedded in QRXNS02 and binds it to
 * the chain publishing-key commitment. File verification is streaming/bounded. */
int qrx_net_publisher_verify_distribution(const char *distribution_path,
                                          const QrxDomainRecord *domain_record,
                                          uint64_t current_height,
                                          QrxNetPublishedSite *site_out);
