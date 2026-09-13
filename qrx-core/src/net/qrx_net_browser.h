#pragma once
#include "net/qrx_net_registry.h"
#include "net/qrx_net_publisher.h"
#include "storage/qrx_storage_discovery.h"
#include "storage/qrx_storage_transport.h"
#include "qrxdb.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_NET_BROWSER_CACHE_PATH_MAX 1400

typedef struct {
    char domain[254];
    char request_path[QRX_NET_SITE_PATH_MAX+1];
    char contract_id[129];
    char manifest_root_hex[129];
    char distribution_cache_path[QRX_NET_BROWSER_CACHE_PATH_MAX];
    char file_cache_path[QRX_NET_BROWSER_CACHE_PATH_MAX];
    uint64_t site_sequence;
    uint64_t bytes_received;
    uint32_t active_sources;
    uint32_t from_cache;
} QrxNetBrowserFetchResult;

/* Resolves the currently committed .qrx website from chain state, reconstructs
 * its QRXWEB distribution from >=10 authenticated ACTIVE providers when needed,
 * verifies embedded ML-DSA key -> chain commitment -> signature -> manifest ->
 * every file CAS/content/Merkle commitment, and only then promotes it to cache.
 * Existing cache entries are revalidated against the current DomainRecord. */
int qrx_net_browser_fetch(QrxDB *db,QrxStorageDiscoveryTable *discovery,
                          const char *domain,const char *request_path,
                          uint64_t current_height,const char *cache_root,
                          QrxNetBrowserFetchResult *out);
#ifdef __cplusplus
}
#endif
