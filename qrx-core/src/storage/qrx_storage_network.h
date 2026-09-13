#pragma once
#include "storage/qrx_storage_transport.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define QRX_STORAGE_NET_MAX_RANGE (128U*1024U)
typedef struct { const char *contract_id; uint32_t connect_timeout_ms; uint32_t io_timeout_ms; } QrxStorageNetworkFetchCtx;
int qrx_storage_network_fetch_range(void *ctx,const QrxShardProviderSource *source,const uint8_t object_id[64],uint64_t offset,size_t length,uint8_t **out,size_t *out_len);
int qrx_storage_network_upload_file(const QrxShardProviderSource *source,const char *contract_id,const char *shard_path);
int qrx_storage_network_upload_many(const QrxShardProviderSource *sources,const char *const *shard_paths,size_t count,const char *contract_id,size_t *successes);
int qrx_storage_qrxp2p_serve_fd(int connected_fd, void *db, void *fs, const char *provider_id);
int qrx_storage_qrxp2p_serve_once(int listen_fd, void *db, void *fs, const char *provider_id);
#ifdef __cplusplus
}
#endif
