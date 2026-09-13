#pragma once
#include "net/qrx_net_name.h"
#include "storage/qrx_storage_fs.h"
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_NET_SITE_PATH_MAX 255
#define QRX_NET_SITE_MAX_FILES 512

typedef struct {const char *path;const uint8_t *data;size_t data_len;} QrxNetSiteFileInput;
typedef struct {char path[QRX_NET_SITE_PATH_MAX+1];char object_id[65];uint64_t size;uint8_t content_hash[64];uint8_t merkle_root[64];} QrxNetSiteFileEntry;
typedef struct {uint32_t version;char domain[254];uint64_t sequence;uint64_t published_height;uint32_t file_count;uint8_t bundle_root[64];} QrxNetSiteManifest;
typedef struct {QrxNetSiteManifest manifest;QrxNetSiteFileEntry *files;uint8_t *signature;size_t signature_len;uint8_t *publishing_public_der;size_t publishing_public_der_len;} QrxNetPublishedSite;

int qrx_net_site_publish(QrxStorageFs *fs,const char *domain,uint64_t sequence,uint64_t height,
                         const QrxNetSiteFileInput *files,size_t file_count,EVP_PKEY *publishing_key,
                         QrxNetPublishedSite *out);
/* Builds/signs a site from already persisted, integrity-committed entries. The
 * entries are copied and canonicalized; duplicate paths fail closed. */
int qrx_net_site_publish_entries(const char *domain,uint64_t sequence,uint64_t height,
                                 const QrxNetSiteFileEntry *entries,size_t file_count,
                                 EVP_PKEY *publishing_key,QrxNetPublishedSite *out);
int qrx_net_site_manifest_hash(const QrxNetSiteManifest *m,uint8_t out[64]);
int qrx_net_site_verify(const QrxNetPublishedSite *site,const QrxDomainRecord *domain_record,EVP_PKEY *publishing_public,uint64_t height);
int qrx_net_site_verify_file(const QrxNetSiteFileEntry *entry,const uint8_t *data,size_t len);
void qrx_net_published_site_free(QrxNetPublishedSite *s);
#ifdef __cplusplus
}
#endif

/* Persistent PUBLIC_SIGNED package format (QRXNS02; QRXNS01 read-compatible). The package contains the
 * immutable manifest, sorted file entries and ML-DSA signature. */
int qrx_net_site_package_save(const QrxNetPublishedSite *site,const char *path);
int qrx_net_site_package_load(const char *path,QrxNetPublishedSite *out);
/* Verifies package membership + signature + domain record + all locally
 * available CAS objects. */
int qrx_net_site_verify_storage(QrxStorageFs *fs,const QrxNetPublishedSite *site,
                                const QrxDomainRecord *domain_record,
                                EVP_PKEY *publishing_public,uint64_t height);
